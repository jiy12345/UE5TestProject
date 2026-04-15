# 04. 소스 코드 심층 분석

> **작성일**: 2026-03-23
> **엔진 버전**: UE 5.7

## 1. NavMesh 빌드 파이프라인 상세

### 1-1. 진입점: RebuildDirtyAreas()

**파일**: `Engine/Source/Runtime/NavigationSystem/Private/NavMesh/RecastNavMeshGenerator.cpp`

```
UNavigationSystemV1::Tick()
    └── ANavigationData::RebuildDirtyAreas(DirtyAreas)
         └── FRecastNavMeshGenerator::RebuildDirtyAreas()
              └── MarkDirtyTiles(DirtyAreas)     ← Dirty Area → 타일 목록 변환
                   └── PendingDirtyTiles 큐 추가
```

### 1-2. MarkDirtyTiles() — Dirty Area 필터링

**파일**: `RecastNavMeshGenerator.cpp` (약 라인 6560-6672)

```cpp
void FRecastNavMeshGenerator::MarkDirtyTiles(const TArray<FNavigationDirtyArea>& DirtyAreas)
{
    const bool bGameStaticNavMesh = IsGameStaticNavMesh(DestNavMesh);

    for (const FNavigationDirtyArea& DirtyArea : DirtyAreas)
    {
        // DynamicModifiersOnly 모드 필터 (5번 절 참조)
        if (bGameStaticNavMesh &&
            (!DirtyArea.HasFlag(ENavigationDirtyFlag::DynamicModifier) ||
              DirtyArea.HasFlag(ENavigationDirtyFlag::NavigationBounds)))
        {
            continue;
        }

        // 타일 재빌드 범위 결정 (약 라인 6672)
        Element.bRebuildGeometry = !bGameStaticNavMesh &&
            (DirtyArea.HasFlag(ENavigationDirtyFlag::Geometry) ||
             DirtyArea.HasFlag(ENavigationDirtyFlag::NavigationBounds));

        // Dirty Area와 교차하는 타일 목록 계산 → PendingDirtyTiles에 추가
    }
}
```

#### DynamicModifiersOnly 모드 판별 — `IsGameStaticNavMesh()`

**파일**: `RecastNavMeshGenerator.cpp:5105`

```cpp
static bool IsGameStaticNavMesh(ARecastNavMesh* InNavMesh)
{
    return (InNavMesh->GetWorld()->IsGameWorld() &&
            InNavMesh->GetRuntimeGenerationMode() != ERuntimeGenerationType::Dynamic);
}
```

- `IsGameWorld()`: PIE, 독립 실행, 쿠킹 빌드 모두 `true`
- `RuntimeGeneration`이 **`Static` 또는 `DynamicModifiersOnly`면 true** (즉 "게임 스태틱"이라는 용어는 `DynamicModifiersOnly`를 포함)
- 이 값이 `true`면 `Geometry` 플래그만 있는 Dirty Area는 건너뛰고, `DynamicModifier` 플래그가 있는 것만 처리

**Runtime 모드 분기 요약:**
| RuntimeGeneration | bGameStaticNavMesh | 지오메트리 Dirty | Modifier Dirty |
|-------------------|:------------------:|:----------------:|:--------------:|
| `Static` | true | 무시 | 무시 (애초에 DynamicModifier flag 없음) |
| `DynamicModifiersOnly` | true | 무시 | 처리 (압축 캐시 경로) |
| `Dynamic` | false | 처리 (전체 재빌드) | 처리 (전체 재빌드) |

**핵심 포인트**: `DynamicModifiersOnly` 모드에서는 `DynamicModifier` 플래그가 있는 Dirty Area만 처리되고, **지오메트리 재래스터화는 생략**됩니다. Nav Modifier Volume이 아닌 일반 액터의 이동은 무시됩니다.

### 1-3. TickAsyncBuild() — 비동기 빌드 루프

**파일**: `RecastNavMeshGenerator.cpp`

```
FRecastNavMeshGenerator::TickAsyncBuild(DeltaSeconds)
    ├── 완료된 RunningDirtyTiles 처리
    │    └── AddGeneratedTilesAndGetUpdatedTiles()   ← Game Thread
    │         └── dtNavMesh::addTile() or removeTile()
    └── PendingDirtyTiles에서 새 작업 시작
         └── FRecastTileGenerator 생성 → 비동기 태스크 실행 (Worker Thread)
```

#### 비동기 태스크와 Modifier 반영 지연

비동기 빌드 중에 Modifier가 수정되면:
1. **Modifier 변경 → 새로운 Dirty Area 추가**: `FNavigationOctree`가 재등록되고 `MarkDirtyTiles()`가 호출되어 해당 타일이 `PendingDirtyTiles`에 다시 올라감.
2. **진행 중인 기존 Task는 취소되지 않음**: Task가 완료되면 구 Modifier 기준의 `dtMeshTile`이 `dtNavMesh`에 등록됨.
3. **직후 새 Task가 실행**되어 최신 Modifier 기준으로 동일 타일을 다시 빌드. 2-3 프레임 내에 최종 상태로 수렴.

따라서 **짧은 기간 동안 "반영이 지연된 상태의 쿼리"가 실제로 발생할 수 있습니다.** 이 기간은 일반적으로 타일 하나의 빌드 시간(수 ms ~ 수십 ms) 정도.

**완화 방법:**
- `EnsureBuildCompletion()` — 동기 대기 (게임 스레드 블로킹). 컷신 시작 등 빌드 완료 보장이 필요한 시점에 사용.
- `DynamicModifiersOnly` 모드는 압축 레이어 경로를 쓰므로 일반 `Dynamic`보다 타일당 비용이 작아 수렴도 빠름.

### 1-4. FRecastTileGenerator — 타일 빌드 단계

**파일**: `RecastNavMeshGenerator.cpp` (약 라인 1600-2000)

#### DynamicModifiersOnly에서의 특수 경로

```cpp
// 라인 1800-1815 (근사)
const bool bGeometryChanged = (DirtyAreas.Num() == 0);

if (!bGeometryChanged)
{
    // Modifier 업데이트: 기존 압축 레이어에서 지오메트리 복원
    CompressedLayers = ParentGenerator.GetOwner()->GetTileCacheLayers(TileX, TileY);
}

// 압축 레이어가 없으면 지오메트리 재빌드 시도
bRegenerateCompressedLayers = (bGeometryChanged ||
                                TileConfig.bGenerateLinks ||
                                CompressedLayers.Num() == 0);
```

**중요**: `CompressedTileCacheLayers`가 비어있으면 (`Num() == 0`) 지오메트리 재빌드를 시도하는데,
`DynamicModifiersOnly` + `bSkipNavGeometry`이면 지오메트리 수집이 없어 빈 타일이 생성됩니다.
→ 이것이 쿠킹 빌드에서 Data Layer Nav Modifier 미동작의 핵심 원인입니다.

#### 전체 빌드 단계 (일반 경로)

**파일**: `RecastNavMeshGenerator.cpp:2061, 6434` (`GatherGeometryFromSources()`, `DoWork()`)

```
DoWork():
    1. GatherGeometryFromSources()       ← FNavigationOctree에서 지오메트리 수집
    2. GenerateCompressedLayers()        ← Recast 빌드
         ├── rcCreateHeightfield()       복셀 그리드 생성
         ├── rcRasterizeTriangles()      삼각형 → 복셀 변환
         ├── rcFilterLowHangingWalkable() 필터링 (낮은 천장 등)
         ├── rcBuildCompactHeightfield() 압축 높이필드
         ├── rcBuildDistanceField()      거리 필드
         ├── rcBuildRegions()            영역 분할 (Watershed/Monotone)
         ├── rcBuildContours()           윤곽선 추출
         ├── rcBuildPolyMesh()           폴리곤 메시 생성
         ├── rcBuildPolyMeshDetail()     세부 메시 생성
         └── dtCreateNavMeshData()       Detour 타일 데이터 생성
    3. ApplyModifiers() / MarkDynamicAreas()  ← Nav Modifier 영역 적용
    4. dtTileCache::buildNavMeshTilesAt()    ← 최종 dtMeshTile 생성
```

#### GatherGeometryFromSources() — 수집 기준

**파일**: `RecastNavMeshGenerator.cpp:2061`

```cpp
NavOctreeInstance->FindElementsWithBoundsTest(
    ParentGenerator.GrowBoundingBox(TileBB, /*bIncludeAgentHeight*/ false),
    [&](const FNavigationOctreeElement& Element)
    {
        // 가상 필터링 모드면 NavDataConfig 기반 검사
        if (bUseVirtualGeometryFilteringAndDirtying)
            if (!ShouldGenerateGeometryForOctreeElement(Element, NavDataConfig)) return;
        else
            if (!Element.ShouldUseGeometry(NavDataConfig)) return;

        // 3가지 조건 중 하나라도 만족하면 수집
        if (bGeometryChanged && (Element.Data->HasGeometry() ||
                                 Element.Data->IsPendingLazyGeometryGathering()))
            NavigationRelevantData.Add(Element.Data);  // 지오메트리 수집

        if (Element.Data->NeedAnyPendingLazyModifiersGathering() ||
            Element.Data->Modifiers.HasMetaAreas() ||
            !Element.Data->Modifiers.IsEmpty())
            NavigationRelevantData.Add(Element.Data);  // 모디파이어 수집
    });
```

**수집 기준 요약:**
1. **공간적 기준**: 타일 바운딩 박스(`TileBB`)와 교차하는 엔트리만. 에이전트 높이 만큼 확장은 아님 (`bIncludeAgentHeight=false`).
2. **NavRelevance**: `ShouldUseGeometry(NavDataConfig)` — 엔트리가 현재 NavData의 에이전트 프로퍼티에 해당하는지.
3. **데이터 존재 여부**: `HasGeometry()`, `HasDynamicModifiers()`, `IsPendingLazyGeometryGathering()` 등.
4. **bLoadedData 필터**: `bExcludeLoadedData`가 true면 스트리밍으로 로드된 엔트리 제외 (아래 2-1 참조).

#### 복셀(Voxel)이란?

**Voxel = Volume + Pixel** (볼륨 픽셀). 픽셀이 2D의 최소 단위라면, 복셀은 **3D의 최소 단위**입니다.

```
2D: Pixel                         3D: Voxel
   ┌───┬───┬───┐                     ┌───┬───┐
   │ ■ │ □ │ ■ │                    ╱│ ■ │ □ ╲
   ├───┼───┼───┤                   ╱ ├───┼───┤╲
   │ □ │ ■ │ □ │                  ┌──┤ ■ │ □ │─┐
   └───┴───┴───┘                  │ ■│───│───│ │
   각 칸 = Pixel                   │ □│ □ │ ■ │ │
   (x, y) 좌표 + 값                └──┴───┴───┴─┘
                                  각 정육면체 = Voxel
                                  (x, y, z) 좌표 + 값
```

현실 예시: MRI/CT 스캔(인체를 3D 복셀로 샘플링), 마인크래프트(월드 전체가 복셀 그리드), 의료 영상 렌더링 등.

#### Recast의 "복셀" — 실제로는 Heightfield (2.5D)

Recast는 **완전한 3D 복셀 그리드를 쓰지 않고**, `rcHeightfield`라는 **2.5D 구조**를 씁니다. 메모리 효율 + 네비메시 목적에 최적화된 변형.

```
완전 3D 복셀 그리드 (Recast가 안 씀):
   (x, y, z) 3차원 배열, 100³ = 100만 셀
   → 대부분이 빈 공간이라 낭비 큼

Recast Heightfield (`rcHeightfield`):
   (x, y) 2차원 그리드 + 각 칸에 "수직 스팬 리스트"

     위에서 본 그리드                  한 칸(x,y)의 수직 단면
   ┌───┬───┬───┬───┐
   │ * │ * │   │ * │                     z=50 ─┐
   ├───┼───┼───┼───┤                           │ 공중
   │   │ * │ * │   │                     z=30 ─┤
   ├───┼───┼───┼───┤                           │ ■ 천장 (지붕)
   │ * │   │   │ * │                     z=25 ─┤
   └───┴───┴───┴───┘                           │ 공중
                                         z=10 ─┤
                                               │ ■ 지면 (바닥)
                                          z=0 ─┘
```

**`rcSpan` 구조**:
```cpp
struct rcSpan {
    unsigned int smin : 13;   // 스팬 시작 Z
    unsigned int smax : 13;   // 스팬 끝 Z
    unsigned int area : 6;    // Area ID (걸을 수 있는지 등)
    rcSpan* next;             // 같은 (x,y)의 다음 스팬
};
```

같은 (x, y) 위치에 **여러 수평 면**을 표현 가능:
- `span[0]: z=0 ~ z=10, walkable` (바닥)
- `span[1]: z=25 ~ z=30, walkable` (2층 바닥)
- `span[2]: z=50 ~ z=52, unwalkable` (천장 고정물)

다리+다리 아래 길, 2층 건물, 동굴 속 동굴 같은 **다층 지형**을 자연스럽게 표현.

**메모리 효율:** 100×100 × 평균 2~3 스팬 = 2~3만 스팬 → 완전 3D 대비 1/30~1/50.

#### 왜 복셀화(Heightfield로 변환)가 필요한가?

Recast는 임의의 삼각형 수프에서 걷기 가능 영역을 안정적으로 추출하기 위해 **복셀화(voxelization)** 를 사용합니다.

1. **균일 그리드 기반 필터링**: 경사각, 스텝 높이, 저공간 판정을 각 셀의 `rcSpan`에 대한 일관된 규칙으로 적용 가능. 삼각형 직접 처리로는 "걷기 가능한 이웃 영역" 판단이 복잡해짐.
   ```cpp
   // 예: 스텝 높이 이상의 공간이 있으면 걸을 수 있다
   if (span.smax - nextSpan.smin >= walkableHeight)
       span.area = RC_WALKABLE;
   ```
2. **여러 층 지원**: `rcHeightfield`의 스팬 리스트는 같은 (x, y)에 여러 높이를 저장 가능 (예: 다리 + 다리 아래 바닥). 삼각형 기반 접근으로는 이를 표현하기 어려움.
3. **영역 분할 가능성**: 연결된 걷기 가능 셀들을 Watershed/Monotone 알고리즘으로 깔끔하게 분할 (`rcBuildRegions()`)하려면 픽셀/복셀 도메인이 필수.
4. **결정론적 결과**: 동일 입력 → 동일 출력. 삼각형 교차 계산은 부동소수 오차에 민감.

#### 용어 정리

| 용어 | 의미 |
|------|------|
| Pixel | 2D 격자 셀 (x, y) + 값 |
| Voxel | 3D 격자 셀 (x, y, z) + 값 |
| `rcHeightfield` | 2D 그리드 × 수직 스팬 리스트 — Recast가 실제 사용하는 2.5D 형태 |
| `rcSpan` | 한 (x, y) 칸의 수직 한 조각 (zmin~zmax, 걸을 수 있음/없음) |
| `rcCompactHeightfield` | 걸을 수 있는 스팬만 추린 최적화 형태 (이후 단계에서 사용) |

**"복셀 그리드 생성 + 복셀 변환"의 정확한 의미**:
월드 지오메트리 삼각형들을 `rcHeightfield`(2.5D 공간 격자)에 래스터화해서, 각 (x, y) 칸의 수직 스팬 리스트로 변환하는 작업. 엄밀히는 3D 복셀이 아닌 heightfield지만 학술/업계 관행상 "voxelization"으로 지칭.

**빌드 단계 대응:**
- `rcCreateHeightfield()`: 빈 복셀 그리드 생성 (`width × height × spans`).
- `rcRasterizeTriangles()`: 입력 삼각형을 스팬으로 래스터화 (각 셀의 Z 범위별 걷기 가능 플래그).
- 필터 단계들: 스팬 단위로 플래그 조정 (낮은 천장 제거, 경사 제거, 저공간 마킹 등).
- `rcBuildCompactHeightfield()`: 실제로 걷기 가능한 스팬만 압축한 형태로 재구성.
- `rcBuildRegions()` → `rcBuildContours()` → `rcBuildPolyMesh()`: 복셀 영역을 폴리곤으로 변환.

#### ApplyModifiers() / MarkDynamicAreas() — 영역 적용의 범위

**파일**: `RecastNavMeshGenerator.cpp:4812-4891, 4102`

```cpp
// MarkDynamicAreas() — 압축 레이어 상에서 영역만 재할당
dtMarkCylinderArea(layer, bmin, bmax, center, radius, height, areaId);
dtReplaceBoxArea(layer, ..., oldAreaId, newAreaId);
dtReplaceConvexArea(layer, ..., areaId);
```

**핵심**: 모디파이어 적용은 **기존 폴리곤/타일 생성의 Area ID(또는 필터)만 교체**하는 작업입니다. 복셀이나 지오메트리 재래스터화는 일어나지 않습니다.

- `dtMarkCylinderArea()` 등은 압축 레이어의 `areas[]` 배열에서 해당 셀의 Area ID를 덮어씀.
- 이후 `dtTileCache::buildNavMeshTile()`이 해당 레이어 기준으로 `dtMeshTile`을 재생성하면서, `dtPoly::areaAndtype`의 하위 6비트에 Area ID가 반영됨.
- 쿼리 시 `dtQueryFilter`가 Area ID별 비용을 조회하여 경로 비용에 반영 (`RECAST_UNWALKABLE_POLY_COST`를 주면 우회, 낮은 비용을 주면 선호).

**결과적으로:**
- DynamicModifier 경로는 **복셀화 생략 → 빠름**.
- 다만 **Modifier로 물리 지오메트리를 추가할 수는 없음** (예: 문이 닫히는 효과 = `Dynamic` 모드 필요).

> **소스 확인 위치**
> - `Engine/Source/Runtime/NavigationSystem/Private/NavMesh/RecastNavMeshGenerator.cpp:5105` — `IsGameStaticNavMesh()`
> - `Engine/Source/Runtime/NavigationSystem/Private/NavMesh/RecastNavMeshGenerator.cpp:5824, 6589, 6672` — `MarkDirtyTiles()`, DynamicModifiersOnly 필터, `bRebuildGeometry` 분기
> - `Engine/Source/Runtime/NavigationSystem/Private/NavMesh/RecastNavMeshGenerator.cpp:2061` — `GatherGeometryFromSources()` 내 `FindElementsWithBoundsTest` 람다
> - `Engine/Source/Runtime/NavigationSystem/Private/NavMesh/RecastNavMeshGenerator.cpp:4812-4891` — `MarkDynamicAreas()`
> - `Engine/Source/Runtime/Navmesh/Public/Recast/Recast.h:956,1064` — `rcCreateHeightfield()`, `rcRasterizeTriangles()`
> - `Engine/Source/Runtime/NavigationSystem/Public/NavMesh/RecastNavMeshGenerator.h:361` — `FRecastTileGenerator` 클래스 선언

---

## 2. NavigationOctree — 지오메트리 공간 분할

**파일**: `Engine/Source/Runtime/NavigationSystem/Public/NavigationOctree.h:172`

```cpp
class FNavigationOctree : public TOctree2<FNavigationOctreeElement, FNavigationOctreeSemantics>
{
    // 엔트리 저장 방식
    // NavigationDataStoreMode에 따라:
    // - ENavigationDataGatheringMode::Instant  : 즉시 지오메트리 저장
    // - ENavigationDataGatheringMode::Lazy     : 필요시 수집 (Lazy Gathering)
};
// 리프당 최대 16 요소, 최대 깊이 12
```

### 2-0. 왜 옥트리로 공간 분할하는가?

타일 제너레이터가 반복적으로 **"이 타일 바운딩 박스에 겹치는 지오메트리/모디파이어 엔트리가 뭐냐"** 를 조회합니다. 등록된 엔트리 수가 수천 개에 달하는 대형 레벨에서 매 타일마다 O(n) 스캔을 하면 빌드 시간이 폭발합니다. 옥트리로 분할하면 바운딩 기반 범위 질의가 **O(log n + k)** (k는 반환 요소 수)로 떨어집니다.

또한 옥트리는 NavRelevant 오브젝트의 이동/추가/제거 이벤트에 반응해 **엔트리를 갱신**하는 중앙 허브 역할도 합니다. Nav Modifier가 런타임에 등록/해제되는 `DynamicModifiersOnly` 흐름도 옥트리를 통해 Dirty Area로 연결됩니다.

`FRecastTileGenerator::GatherGeometryFromSources()`에서 `FindElementsWithBoundsTest()`로 타일 영역에 포함된 엔트리를 수집합니다 (위 1-4의 수집 기준 참조).

### 2-1. 옥트리의 소유권 모델 — "스냅샷 + 약한 참조"

옥트리는 **NavRelevant 액터/컴포넌트를 직접 소유하지 않습니다**. 스냅샷 데이터만 자기 힙에 보관하고, 원본 UObject에 대해서는 `TWeakObjectPtr` 약한 참조만 가집니다.

#### 소유 구조

```
FNavigationOctree                          ← UNavigationSystemV1 소유
  └── TOctree2<FNavigationOctreeElement>
        └── FNavigationOctreeElement       ← 옥트리가 값으로 보유
              ├── FBoxSphereBounds Bounds  ← 캐시된 바운딩
              └── TSharedRef<FNavigationRelevantData> Data   ← 공유 소유
                    ├── Modifiers (FCompositeNavModifier)    ← 스냅샷
                    ├── CollisionData                         ← 지오메트리 복사본
                    ├── NavLinks
                    └── TSharedRef<FNavigationElement> SourceElement
                          ├── TWeakObjectPtr<UObject> OwnerUObject   ← 약한 참조!
                          ├── TWeakObjectPtr<UBodySetup> BodySetup
                          └── NavigationParent (weak)
```

**파일 근거:**
- `NavigationOctree.h:33-56` — `FNavigationOctreeElement`가 `TSharedRef<FNavigationRelevantData>`로 데이터 보유
- `NavigationRelevantData.h:79` — `TSharedRef<const FNavigationElement> SourceElement`
- `NavigationElement.h:257-278` — 멤버 전부 `TWeakObjectPtr`

#### 핵심 원칙

**1) 옥트리는 UObject를 소유하지 않음** — 액터가 destroy되면:
- GC가 정상적으로 수거
- 옥트리 내 `WeakObjectPtr`는 자동 invalid
- 옥트리 엔트리는 여전히 잔존 (별도 정리 로직 필요)

**2) 옥트리가 소유하는 건 "스냅샷"** — `FNavigationRelevantData`에 복사된:
- `CollisionData` (버텍스, 삼각형 인덱스)
- `Modifiers` (Nav Modifier 정보)
- `Bounds` (바운딩)

원본 액터가 사라져도 스냅샷은 유지됨.

**3) 원본 라이프사이클은 UWorld/ULevel 소관** — `UWorld`가 액터 배열을 소유, 가비지 컬렉터가 수거. 옥트리는 독립적.

#### 이 설계의 이유

| 이유 | 효과 |
|------|------|
| **약한 참조** | 옥트리가 액터를 살려두지 않음 → GC/레벨 스트리밍/순환 참조 문제 없음 |
| **스냅샷 캐싱** | 쿼리 시 액터 재접근 없이 빠르게 데이터 사용 |
| **멀티스레드 안전** | `TSharedRef`로 Worker 스레드가 참조 카운트 올리고 안전 읽기 가능 |
| **원본 액터와 분리** | 비동기 타일 빌드 중 게임 스레드 액터 변경이 Worker 쪽 데이터에 영향 없음 |

### 2-2. 이벤트 기반 동기화 — NavigationSystem이 허브

옥트리는 **수동적(passive) 데이터 구조**입니다. 스스로 액터의 상태 변화를 감지하지 않고, `UNavigationSystemV1`이 엔진 이벤트를 받아서 옥트리에 명령을 내리는 방식으로 동기화됩니다.

#### 옥트리가 반응해야 할 이벤트

| 이벤트 | 필요한 동작 |
|--------|-------------|
| 액터 **등록** (BeginPlay, SpawnActor) | 옥트리 엔트리 추가 |
| 액터 **해제** (Destroy, EndPlay) | 엔트리 삭제 + 과거 바운드 Dirty Area 생성 |
| 액터 **이동** (SetActorLocation) | 엔트리 재배치 + Dirty Area 2개 (옛/새 바운드) |
| **컴포넌트 등록/해제** | 엔트리 추가/삭제 |
| **지오메트리 변경** (스태틱 메시 교체) | 스냅샷 재생성 |
| **Nav Modifier 변경** (Area Class 교체) | Data만 갱신 |
| **월드 스트리밍** 로드/언로드 | 대량 엔트리 배치 처리 |

#### 이벤트 파이프라인

```
[엔진 이벤트 소스]
    │ UWorld::OnActorRegisteredEvent
    │ AActor::RegisterAllComponents()
    │ UActorComponent::OnRegister()
    │ USceneComponent::OnTransformUpdated
    │ INavRelevantInterface::UpdateNavigationRelevantData()
    ▼
[허브: UNavigationSystemV1]
    │ OnActorRegistered / OnActorUnregistered
    │ OnComponentRegistered / OnComponentUnregistered
    │ UpdateActorInNavOctree / UpdateComponentInNavOctree
    │ UpdateNavOctreeAfterMove / UpdateNavOctreeBounds
    ▼ (라우팅)
┌──────────────────────┬──────────────────────────┐
▼                      ▼                          ▼
[FNavigationOctree]    [FNavigationDirtyAreas]    [ANavigationData]
 엔트리 add/remove/move  Dirty Area 누적           RebuildDirtyAreas
```

#### INavRelevantInterface — 네비 연관성 선언

```cpp
// NavRelevantInterface.h
class INavRelevantInterface
{
    virtual void GetNavigationData(FNavigationRelevantData& Data) const;
    virtual FBox GetNavigationBounds() const;
    virtual bool IsNavigationRelevant() const;
    virtual UObject* GetNavigationParent() const;
};
```

액터/컴포넌트가 이 인터페이스를 구현하면 NavigationSystem이 등록 시점에 옥트리에 반영합니다.

#### 이벤트 누락 시 증상

**1) 엔트리 누락 (월드에 있지만 옥트리엔 없음)**
- 타일 빌드 시 해당 지오메트리 무시 → 네비메시 누락 영역 발생
- 증상: 벽이 있는데 AI가 통과

**2) 엔트리 잔존 (월드에서 사라졌지만 옥트리에 남음)**
- `TWeakObjectPtr` invalid → 접근 시 skip되지만 **스냅샷 Data는 유효**
- 증상: 부숴진 벽인데 AI가 여전히 우회
- 해결: 주기적 정리 로직 / `ApplyAreaChangesAndUpdateNavOctree`

**3) Bounds 불일치 (이동했지만 옥트리엔 옛 위치)**
- 공간 질의가 엉뚱한 타일에 속한 것처럼 계산
- 증상: 액터를 옮겼는데 네비메시가 그대로
- 해결: `UpdateNavOctreeAfterMove` 호출 보장

#### 대안 방식과의 비교

| 방식 | 장점 | 단점 |
|------|------|------|
| **이벤트 기반 (현재)** | 변경 시점에만 갱신, 즉각 반영, 효율적 | 이벤트 누락 시 조용히 불일치 |
| Polling (매 틱 전수 스캔) | 단순, 누락 없음 | O(n) 매 프레임 = 성능 낭비 |
| Lazy (쿼리 시 재확인) | 등록 비용 없음 | 쿼리마다 느림 |

엔진은 **이벤트 기반**을 택했고, 결과적으로 옥트리가 "NavigationSystem의 명령에 반응하는 인덱스"가 되었습니다.

#### 세 가지 원칙의 종합

옥트리 아키텍처는 세 가지가 맞물려 있습니다:

1. **소유 안 함 (Weak Ref)** → 라이프사이클 간섭 없음, GC/스트리밍 자유
2. **스냅샷 보유** → 빠른 쿼리, 멀티스레드 안전
3. **이벤트 구독** → 실제 상태와 동기화 유지

이 중 하나라도 빠지면 구조가 깨집니다. 예를 들어 이벤트 없이 스냅샷만 있으면 **갱신 안 되는 고정 사본**이 되고, 스냅샷 없이 weak ref만 있으면 **매 쿼리마다 원본 재접근 필요**해서 성능 저하.

> **소스 확인 위치**
> - `Engine/Source/Runtime/NavigationSystem/Public/NavigationOctree.h:33-56,172` — `FNavigationOctreeElement`, `FNavigationOctree`
> - `Engine/Source/Runtime/Engine/Classes/AI/Navigation/NavigationRelevantData.h:79` — `SourceElement` SharedRef
> - `Engine/Source/Runtime/Engine/Classes/AI/Navigation/NavigationElement.h:257-278` — 전부 `TWeakObjectPtr`
> - `Engine/Source/Runtime/NavigationSystem/Public/NavigationSystem.h` — `OnActorRegistered`, `UpdateActorInNavOctree`, `UpdateNavOctreeAfterMove`
> - `Engine/Source/Runtime/Engine/Classes/AI/Navigation/NavRelevantInterface.h` — `INavRelevantInterface`

### 2-3. bExcludeLoadedData 필터

**파일**: `Engine/Source/Runtime/NavigationSystem/Public/NavigationRelevantData.h:26`

```cpp
/** fail if from level loading (only valid in WP dynamic mode) */
uint32 bExcludeLoadedData : 1;
```

**설정 지점**: `NavigationDataHandler.cpp:171`

```cpp
OctreeElement.Data->bLoadedData =
    DirtyElement.bIsFromVisibilityChange ||
    NavigationElement.IsFromLevelVisibilityChange();
```

**World Partition에서의 문제 시나리오**:
1. Data Layer 로드 → Nav Modifier 컴포넌트 등록 → `bLoadedData = true` 설정
2. Dirty Area 생성 → `MarkDirtyTiles()` → 타일 재빌드 시작
3. `GatherGeometryFromSources()` → `bExcludeLoadedData=true`로 수집
4. **Nav Modifier가 `bLoadedData=true`이므로 수집에서 제외됨** → Modifier 효과 없음

이 필터는 `bUseWorldPartitionedDynamicMode` 경로에서만 활성화됩니다. Non-WP 월드에서는 적용되지 않습니다.

---

## 3. World Partition 스트리밍 — AttachTiles / DetachTiles

**파일**: `Engine/Source/Runtime/NavigationSystem/Private/NavMesh/RecastNavMeshDataChunk.cpp:203`

이 섹션은 **World Partition (또는 레벨 스트리밍) 기반 NavMesh**에서만 의미가 있습니다. 일반 정적 월드에서는 청크가 생성되지 않고 네비메시 데이터 전체가 레벨 액터로 직렬화됩니다.

### AttachTiles() 흐름

```cpp
void URecastNavMeshDataChunk::AttachTiles()
{
    ARecastNavMesh* NavMesh = ...;

    for (each Tile in RawTiles)
    {
        // 게임 월드: 소유권 이전 (NavMesh가 메모리 소유)
        const bool bKeepCopyOfCacheData = !NavMesh->GetWorld()->IsGameWorld();

        // dtNavMesh에 타일 추가
        NavMesh->GetRecastMesh()->addTile(tileData, dataSize, flags, 0, &tileRef);

        // TileCache 레이어도 등록 (DynamicModifiersOnly용)
        if (HasCacheLayers)
            NavMesh->AddTileCacheLayer(TileX, TileY, LayerIdx, LayerData);
    }
}
```

### DetachTiles() 흐름

```cpp
void URecastNavMeshDataChunk::DetachTiles()
{
    for (each attached TileRef)
    {
        NavMesh->GetRecastMesh()->removeTile(tileRef, &data, &dataSize);
        NavMesh->RemoveTileCacheLayer(TileX, TileY, LayerIdx);

        // 게임 월드: 소유권 다시 청크로 반환
        if (bTakeCacheDataOwnership)
            RawTiles[i].RawData = data;  // 재사용 가능하도록 보관
    }
}
```

**호출 측**: World Partition의 Streaming Source / ALevelStreaming 이 청크 Load 완료 후 호출합니다. `UNavigationSystemV1`은 이 과정을 감독(Invoker/ActiveTiles 관리)하지만 실제 Attach/Detach는 청크가 수행합니다.

---

## 4. NavigationDirtyAreasController — WP 최적화

**파일**: `Engine/Source/Runtime/NavigationSystem/Private/NavigationDirtyAreasController.cpp:148-163`

**전제 조건**: 이 필터는 **`bUseWorldPartitionedDynamicMode`가 true일 때만** 동작합니다. Non-WP 월드에서는 이 블록 전체가 skip 되고, 모든 visibility 변화 dirty는 그대로 처리됩니다.

```cpp
if (bUseWorldPartitionedDynamicMode)
{
    if (const bool bIsFromVisibilityChange =
            (DirtyElement && DirtyElement->bIsFromVisibilityChange) ||
            (SourceElement && SourceElement->IsFromLevelVisibilityChange()))
    {
        if (const bool bIsIncludedInBaseNavmesh =
                (DirtyElement && DirtyElement->bIsInBaseNavmesh) ||
                (SourceElement && SourceElement->IsInBaseNavigationData()))
        {
            UE_LOG(LogNavigationDirtyArea, VeryVerbose,
                TEXT("Ignoring dirtyness (visibility changed and in base navmesh)..."));
            return;  // Dirty area IGNORED
        }
    }
}
```

### 4-0. Base Navmesh란?

**"Base Navmesh"** 는 WP 동적 모드에서 **사전 베이크된 정적 부분**의 네비메시를 가리킵니다. `WorldSettings->BaseNavmeshDataLayers`에 등록된 DataLayer에 속한 액터는 "Base에 이미 반영된 것"으로 취급되어, 해당 DataLayer가 런타임에 visibility 변경(로드/언로드)될 때 **이미 베이크된 Base NavMesh에 중복 반영되어 있으므로 dirty 처리 불필요**라는 최적화입니다.

즉 "DataLayer로 가시성이 토글되는 오브젝트 중 Base에 포함된 것들은 네비메시 관점에서 이미 존재하는 것처럼 동작"시키는 사전 지식입니다.

### 4-1. IsInBaseNavmesh() 판단

**파일**: `Engine/Source/Runtime/Engine/Private/AI/Navigation/NavigationTypes.cpp:50-77`

```cpp
bool IsInBaseNavmesh(const UObject* Object)
{
    const UActorComponent* ObjectAsComponent = Cast<UActorComponent>(Object);
    const AActor* Actor = ObjectAsComponent ? ObjectAsComponent->GetOwner()
                                             : Cast<AActor>(Object);
    if (!Actor) return false;

    if (!Actor->HasDataLayers())
        return true;  // 데이터 레이어 없으면 항상 Base로 취급

    const AWorldSettings* WS = Actor->GetWorld()->GetWorldSettings();
    for (const UDataLayerAsset* BaseLayer : WS->BaseNavmeshDataLayers)
    {
        if (Actor->ContainsDataLayer(BaseLayer))
            return true;  // BaseNavmeshDataLayers에 속하면 Base
    }
    return false;  // Data Layer 소속이지만 Base 아님
}
```

**각 시나리오별 동작:**

| 시나리오 | `IsInBaseNavmesh()` | 처리 |
|----------|:-------------------:|------|
| **A.** 액터가 `BaseNavmeshDataLayers`에 등록된 레이어에 속함 (visibility 변경) | true | **Dirty 무시** (Base에 이미 반영됨) |
| **B.** 액터에 DataLayer 자체가 없음 (visibility 변경) | true | **Dirty 무시** (Base의 일부로 간주) |
| **C.** 액터가 비-Base DataLayer에 속함 (visibility 변경) | false | **Dirty 처리** (로드/언로드 반영 필요) |
| **D.** WP가 아님 or visibility 변경이 아님 | - | **Dirty 처리** (필터 skip) |

### 4-2. DataLayer 없는 Modifier의 Dirty Area 처리 — 스트리밍/Non-WP 시나리오

리뷰 질문: "모디파이어가 속한 DataLayer가 없을 때도 Dirty Area가 무시될 수 있는가? 스트리밍/언로드 시나리오는? Non-WP에서 네비메시는 전부 로드되어 있는데 모디파이어만 로드/언로드되는 경우는?"

**답:**

**시나리오 A — WP 동적 모드, DataLayer 없는 모디파이어가 레벨 visibility로 등장/소멸:**
- `IsInBaseNavmesh()` 시나리오 B에 해당 → **true 반환 → Dirty 무시**
- 의미: WP에서 DataLayer 없는 일반 액터의 visibility 변경은 Base 소속으로 간주되어 네비메시를 건드리지 않음
- **이 동작은 Nav Modifier Volume에도 동일하게 적용되어, WP + DataLayer 없이 레벨 visibility로 Modifier를 토글하면 원하는 만큼 반영되지 않을 수 있음** — 실무에서는 Modifier는 DataLayer로 관리하거나 런타임 스폰 방식을 사용해야 함

**시나리오 B — WP 동적 모드, DataLayer로 Modifier 토글:**
- 해당 DataLayer가 `BaseNavmeshDataLayers`에 **없으면** → 시나리오 C → Dirty 정상 처리
- `BaseNavmeshDataLayers`에 **있으면** → 시나리오 A → Dirty 무시 (이미 Base에 반영되었다고 가정)

**시나리오 C — Non-WP 월드, DataLayer 설정 안 된 Modifier가 레벨 스트리밍으로 등장/소멸:**
- `bUseWorldPartitionedDynamicMode = false` → `IsInBaseNavmesh` 기반 필터 자체가 skip
- 모든 visibility 변경 dirty가 그대로 전달됨
- 이후 `MarkDirtyTiles()` 단에서 `IsGameStaticNavMesh()` 필터를 만나지만, **`DynamicModifier` flag는 통과** → 타일 재빌드 발생
- **※ 이 시나리오의 구조적 비효율:**
  - Non-WP 네비메시는 persistent level에 속한 단일 글로벌 에셋 — 게임 시작 시 **이미 전부 로드**됨
  - 에디트 시점에 해당 Modifier가 서브레벨에 있었고 bake 시 visible이었다면, **baked 타일에 이미 Modifier 효과 반영됨**
  - 런타임에 서브레벨이 스트리밍되며 Modifier 액터가 재등록되어도 **네비메시는 이미 해당 Modifier를 반영한 상태**
  - 따라서 이때 생성되는 dirty와 뒤따르는 타일 재빌드는 **"같은 결과를 다시 만드는 작업"** — 아키텍처적으로 불필요
  - 엔진에 `bIsFromVisibilityChange` 플래그는 존재하고 WP 경로에서는 `BaseNavmeshDataLayers` 기반 스킵에 활용되지만, **Non-WP 경로에서는 활용되지 않음**
- **실무 영향**: WP 월드 + Non-WP 네비메시 조합(장거리 자동 이동이 필요해 흔히 선택됨) + Modifier 다수 배치 시 스트리밍 빈도에 비례한 프레임 스파이크 원인. 투자 대비 효과가 큰 최적화 여지 존재 (#15 참조).

**시나리오 D — Non-WP + 런타임 스폰된 NavModifierVolume:**
- visibility 변화가 아니라 컴포넌트 등록/해제 이벤트 → `OctreeUpdate_Modifiers` → `DynamicModifier` flag
- `DynamicModifiersOnly` 모드에서 **정상 처리** — 이것이 의도된 기본 사용 방식
- 시나리오 C와 달리 이 경우는 "baked에 없던 것이 새로 추가"되는 것이므로 dirty 처리가 **정당**

**요약**:
- DataLayer 없는 Modifier를 WP 월드의 레벨 visibility로 토글하는 방식은 **WP 환경에서 의도치 않게 무시될 수 있음** (시나리오 A). 안정적 방식은 DataLayer 기반 관리(+ Base에 넣지 않기) 또는 런타임 스폰.
- 반면 **Non-WP 네비메시 + WP 월드 조합**에서는 시나리오 C의 구조적 비효율이 나타나 프레임 비용 증가 가능성. 이 조합은 장거리 경로 탐색 요구가 있을 때 흔히 선택되므로, 해당 최적화 여지를 별도 이슈로 추적 (#15).

---

## 5. IsGameStaticNavMesh() — 핵심 분기

**파일**: `Engine/Source/Runtime/NavigationSystem/Private/NavMesh/RecastNavMeshGenerator.cpp:5105`

```cpp
static bool IsGameStaticNavMesh(ARecastNavMesh* InNavMesh)
{
    return (InNavMesh->GetWorld()->IsGameWorld() &&
            InNavMesh->GetRuntimeGenerationMode() != ERuntimeGenerationType::Dynamic);
}
```

### 5-1. 이 분기가 결정하는 것

`MarkDirtyTiles()` 내부에서 두 가지 동작을 결정합니다:

1. **Dirty Area skip 조건** (약 라인 6589):
   ```cpp
   if (bGameStaticNavMesh &&
       (!HasFlag(DynamicModifier) || HasFlag(NavigationBounds)))
       continue;
   ```
   → `Static` / `DynamicModifiersOnly`에서 지오메트리 dirty 무시.

2. **타일 재빌드 시 지오메트리 재래스터화 여부** (약 라인 6672):
   ```cpp
   Element.bRebuildGeometry = !bGameStaticNavMesh &&
       (HasFlag(Geometry) || HasFlag(NavigationBounds));
   ```
   → `Dynamic`에서만 지오메트리 재래스터화 허용. `DynamicModifiersOnly`는 **압축 레이어 경로**로 빌드.

### 5-2. 호출자

- `FRecastNavMeshGenerator::MarkDirtyTiles()` — 유일한 직접 호출자.
- `RebuildDirtyAreas()` → `MarkDirtyTiles()` 로 이어지므로, 매 프레임 Dirty Area 처리 시점마다 평가됨.

`IsGameWorld()`: PIE, 독립 실행, 쿠킹 빌드 모두 `true`. 에디터 전용 월드(`EWorldType::Editor`)는 `false` → 에디터에서는 `bGameStaticNavMesh`가 항상 false가 되어 모든 빌드가 허용됨.

---

## 6. GetDirtyFlag() — Modifier 플래그 결정

**파일**: `Engine/Source/Runtime/NavigationSystem/Public/NavigationRelevantData.h:135`

```cpp
ENavigationDirtyFlag FNavigationRelevantData::GetDirtyFlag() const
{
    const bool bSetGeometryFlag =
        HasGeometry() ||
        IsPendingLazyGeometryGathering() ||
        Modifiers.GetFillCollisionUnderneathForNavmesh() ||
        Modifiers.GetMaskFillCollisionUnderneathForNavmesh() ||
        (Modifiers.GetNavMeshResolution() != ENavigationDataResolution::Invalid);

    return (bSetGeometryFlag ? ENavigationDirtyFlag::Geometry : ENavigationDirtyFlag::None)
         | ((HasDynamicModifiers() || NeedAnyPendingLazyModifiersGathering())
             ? ENavigationDirtyFlag::DynamicModifier : ENavigationDirtyFlag::None)
         | (Modifiers.HasAgentHeightAdjust()
             ? ENavigationDirtyFlag::UseAgentHeight : ENavigationDirtyFlag::None);
}
```

### 6-1. 각 플래그의 의미와 효과

| 플래그 | 의미 | 주요 발생 상황 | `DynamicModifiersOnly`에서의 처리 |
|--------|------|----------------|------------------------------------|
| `None` (0) | 변경 없음 | 무의미한 Dirty | skip |
| `Geometry` (1) | 지오메트리 변경 — 복셀 재래스터화 필요 | 액터 이동, 콜리전 변경, `FillCollisionUnderneathForNavmesh`/`NavMeshResolution` 설정된 Modifier 등록 | **skip** (IsGameStaticNavMesh 필터) |
| `DynamicModifier` (2) | Nav Modifier만 변경 — 압축 레이어 경로 가능 | NavModifierVolume, NavModifierComponent의 등록/해제/AreaClass 변경 | **처리** |
| `UseAgentHeight` (4) | 에이전트 높이 필터 적용 필요 | Modifier에 `bIncludeAgentHeight=true` 설정 | Modifier와 함께 처리 |
| `NavigationBounds` (8) | 네비메시 경계 변경 | `ANavMeshBoundsVolume` 이동/추가/제거 | **skip** (IsGameStaticNavMesh는 bounds 변경은 전체 재빌드 경로이므로 배제) |

### 6-2. 플래그 조합 시나리오

- **순수 Nav Modifier Volume 이동** → `DynamicModifier`만 → 정상 처리
- **Modifier에 `FillCollisionUnderneathForNavmesh=true`** → `Geometry | DynamicModifier` → **skip** (Geometry 플래그가 있으면 `DynamicModifiersOnly`에서 무시됨!)
- **Modifier에 `NavMeshResolution` 변경** → `Geometry | DynamicModifier` → skip

**실무 주의사항**: `DynamicModifiersOnly` 모드로 런타임 Modifier를 쓴다면 `FillCollisionUnderneathForNavmesh`, `NavMeshResolution` 옵션은 피해야 합니다. 이 옵션들은 내부적으로 `Geometry` 플래그를 붙여 런타임 업데이트를 막습니다.

> **소스 확인 위치**
> - `Engine/Source/Runtime/NavigationSystem/Public/NavigationDirtyArea.h:12-20` — `ENavigationDirtyFlag` 열거형
> - `Engine/Source/Runtime/NavigationSystem/Public/NavigationRelevantData.h:135` — `GetDirtyFlag()`
> - `Engine/Source/Runtime/NavigationSystem/Private/NavigationDataHandler.cpp:20-25` — 업데이트 플래그 → Dirty 플래그 매핑
