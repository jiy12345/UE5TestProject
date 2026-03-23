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

**파일**: `RecastNavMeshGenerator.cpp` (약 라인 6560-6620)

```cpp
void FRecastNavMeshGenerator::MarkDirtyTiles(const TArray<FNavigationDirtyArea>& DirtyAreas)
{
    const bool bGameStaticNavMesh = IsGameStaticNavMesh(DestNavMesh);

    for (const FNavigationDirtyArea& DirtyArea : DirtyAreas)
    {
        // DynamicModifiersOnly 모드: DynamicModifier 플래그 없으면 건너뜀
        if (bGameStaticNavMesh &&
            (!DirtyArea.HasFlag(ENavigationDirtyFlag::DynamicModifier) ||
              DirtyArea.HasFlag(ENavigationDirtyFlag::NavigationBounds)))
        {
            continue;
        }

        // Dirty Area와 교차하는 타일 목록 계산
        // ... TileX, TileY 범위 계산 후 PendingDirtyTiles에 추가
    }
}
```

**핵심 포인트**: `DynamicModifiersOnly` 모드에서는 `DynamicModifier` 플래그가 있는 Dirty Area만 처리됩니다. Nav Modifier Volume이 아닌 일반 액터의 이동은 무시됩니다.

### 1-3. TickAsyncBuild() — 비동기 빌드 루프

**파일**: `RecastNavMeshGenerator.cpp`

```
FRecastNavMeshGenerator::TickAsyncBuild(DeltaSeconds)
    ├── 완료된 RunningDirtyTiles 처리
    │    └── AddGeneratedTilesAndGetUpdatedTiles()
    │         └── dtNavMesh::addTile() or removeTile()
    └── PendingDirtyTiles에서 새 작업 시작
         └── FRecastTileGenerator 생성 → 비동기 태스크 실행
```

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
    3. ApplyModifiers()                  ← Nav Modifier 영역 적용
    4. dtTileCache::buildNavMeshTilesAt() ← 최종 dtMeshTile 생성
```

---

## 2. NavigationOctree — 지오메트리 공간 분할

**파일**: `Engine/Source/Runtime/NavigationSystem/Public/NavigationOctree.h`

씬의 모든 NavRelevant 오브젝트(지오메트리, Nav Modifier 등)를 공간 분할 구조로 관리합니다.

```cpp
class FNavigationOctree : public TOctree2<FNavigationOctreeElement, ...>
{
    // 엔트리 저장 방식
    // NavigationDataStoreMode에 따라:
    // - ENavigationDataGatheringMode::Instant  : 즉시 지오메트리 저장
    // - ENavigationDataGatheringMode::Lazy     : 필요시 수집 (Lazy Gathering)
};
```

`FRecastTileGenerator::GatherGeometryFromSources()`에서 Octree를 순회하여 타일 영역에 포함된 엔트리를 수집합니다.

### bExcludeLoadedData 필터

**파일**: `Engine/Source/Runtime/NavigationSystem/Private/NavigationSystem.cpp`

```cpp
// FindElementsInNavOctree 호출 시
bool bExcludeLoadedData = true;

// FNavigationOctreeElement::IsAccessible()
bool IsAccessible() const
{
    return !bExcludeLoadedData || !bLoadedData;
    // bLoadedData=true인 엔트리(스트리밍으로 로드된)는 수집에서 제외
}
```

**World Partition에서의 문제 시나리오**:
1. Data Layer 로드 → Nav Modifier 컴포넌트 등록 → `bLoadedData = true` 설정
2. Dirty Area 생성 → `MarkDirtyTiles()` → 타일 재빌드 시작
3. `GatherGeometryFromSources()` → `bExcludeLoadedData=true`로 수집
4. **Nav Modifier가 `bLoadedData=true`이므로 수집에서 제외됨** → Modifier 효과 없음

---

## 3. World Partition 스트리밍 — AttachTiles / DetachTiles

**파일**: `Engine/Source/Runtime/NavigationSystem/Private/NavMesh/RecastNavMeshDataChunk.cpp:203`

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

---

## 4. NavigationDirtyAreasController — WP 최적화

**파일**: `Engine/Source/Runtime/NavigationSystem/Private/NavigationDirtyAreasController.cpp:148`

World Partition Dynamic 모드에서 불필요한 Dirty Area를 걸러냅니다.

```cpp
void FNavigationDirtyAreasController::AddArea(const FBox& Area, ENavigationDirtyFlag Flags,
                                              const UObject* OptionalObject)
{
    if (bUseWorldPartitionedDynamicMode)
    {
        const bool bIsFromVisibilityChange = EnumHasAnyFlags(Flags, ENavigationDirtyFlag::Visibility);
        const bool bIsIncludedInBaseNavmesh = IsInBaseNavmesh(OptionalObject);

        if (bIsFromVisibilityChange && bIsIncludedInBaseNavmesh)
        {
            // Base NavMesh에 포함된 오브젝트의 visibility 변경은 무시
            // → 사전 베이크된 NavMesh에 이미 반영되어 있다고 가정
            return;
        }
    }
    // ... 일반 Dirty Area 추가
}
```

### IsInBaseNavmesh() 판단

**파일**: `Engine/Source/Runtime/NavigationSystem/Private/NavigationTypes.cpp:50`

```cpp
bool IsInBaseNavmesh(const UObject* Object)
{
    const AActor* Actor = Cast<AActor>(Object);
    if (!Actor || !Actor->HasDataLayers())
        return true;  // 데이터 레이어 없으면 항상 Base

    const AWorldSettings* WS = Actor->GetWorldSettings();
    for (const UDataLayerAsset* BaseLayer : WS->BaseNavmeshDataLayers)
    {
        if (Actor->ContainsDataLayer(BaseLayer))
            return true;  // BaseNavmeshDataLayers에 속하면 Base
    }
    return false;  // Data Layer 소속이지만 Base 아님
}
```

**주의**: 만약 Nav Modifier가 속한 Data Layer가 `WorldSettings->BaseNavmeshDataLayers`에 등록되어 있으면, 해당 Modifier의 Dirty Area가 무시될 수 있습니다.

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

- `IsGameWorld()`: PIE, 독립 실행, 쿠킹 빌드 모두 `true`
- `Static`과 `DynamicModifiersOnly` 모두 "게임 스태틱"으로 취급
- 차이점: `DynamicModifiersOnly`는 `DynamicModifier` 플래그 Dirty Area는 처리함

---

## 6. GetDirtyFlag() — Modifier 플래그 결정

**파일**: `Engine/Source/Runtime/NavigationSystem/Public/NavigationTypes.h:135`

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
         | (HasDynamicModifiers() ? ENavigationDirtyFlag::DynamicModifier : ENavigationDirtyFlag::None)
         | ...;
}
```

**중요**: `FillCollisionUnderneathForNavmesh`나 `NavMeshResolution`이 설정된 Modifier는 `Geometry` 플래그도 포함됩니다. `DynamicModifiersOnly` 모드에서 `Geometry` 플래그를 가진 Dirty Area는 처리되지 않으므로, 이런 설정의 Modifier는 런타임 업데이트가 되지 않습니다.
