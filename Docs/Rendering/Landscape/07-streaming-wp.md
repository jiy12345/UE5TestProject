# 07. World Partition 스트리밍

> **작성일**: 2026-04-21
> **엔진 버전**: UE 5.7

## 1. 두 가지 월드 구성

Landscape가 월드에서 "어떻게 존재하는가"는 크게 둘로 갈립니다:

| 구성 | 설명 | 프록시 수 |
|------|------|----------|
| **비파티션(Non-partitioned)** | 전통적인 퍼시스턴트 레벨 또는 월드 컴포지션 | `ALandscape` 1개가 모든 컴포넌트 직접 보유 |
| **월드 파티션(World Partition, Grid-based)** | WP가 활성화된 월드 | `ALandscape` 1개 + `ALandscapeStreamingProxy` N개 (그리드 셀당 1개) |

이 문서는 주로 **후자(WP)**를 다룹니다. 전자는 "모든 지형이 항상 로드됨"이라 스트리밍 이슈가 없습니다.

`ULandscapeSubsystem::IsGridBased()`가 현재 월드가 파티션 모드인지 알려줍니다:

```cpp
// LandscapeSubsystem.h:171
LANDSCAPE_API bool IsGridBased() const;
```

> **소스 확인 위치**
> - `Engine/Source/Runtime/Landscape/Public/LandscapeSubsystem.h:171-173` — `IsGridBased`, `ChangeGridSize`, `FindOrAddLandscapeProxy`

## 2. 그리드 크기 — 몇 개 컴포넌트씩 프록시로 묶을 것인가

WP는 월드를 **정사각 그리드 셀**로 나누고, 각 셀에 포함된 액터를 한 패키지로 관리합니다. Landscape도 그리드 셀 단위로 `ALandscapeStreamingProxy`를 만들지만, **그리드 셀의 크기(월드 단위 cm)는 사용자 설정**이며, 이를 **"몇 컴포넌트씩 한 프록시에 넣을지"**와 연결하는 것이 `GetGridSize`입니다.

### 2.0 WP 그리드가 BatchedMerge 분할 기준에 영향을 주는가

**간접적으로 영향을 줍니다 — 단, 직접 결정하지는 않습니다.**

두 분할의 관계:

| 분할 | 적용 시점 | 결정 주체 | 단위 |
|------|---------|---------|------|
| **WP 그리드** | 에디터 저장 + 런타임 스트리밍 | 사용자 설정 (`GridSizeInComponents`) | 공간 영역(월드 cm) + 컴포넌트 집합 |
| **BatchedMerge 배치** | 편집 시점 머지 | 머지 파이프라인 내부 휴리스틱 | 영향받은 컴포넌트 집합 + RT 해상도 제약 |

**영향 관계**:
- **공유**: 두 분할 모두 "공간 근접성"을 기반으로 컴포넌트를 묶는다는 점은 동일
- **간접 영향**: BatchedMerge는 **현재 로드된 컴포넌트 집합**에서만 동작 가능. WP가 셀을 로드·언로드하므로 머지 대상 집합의 경계가 그리드 셀 경계와 자연스럽게 일치하는 경향
- **독립**: 다만 BatchedMerge의 실제 배치 경계는 RT 해상도 한계와 3×3 이웃 요구로 추가 분할될 수 있어, 항상 그리드 셀과 1:1 대응하지는 않음. 한 셀 내부에서도 여러 배치로 쪼개질 수 있고, 경계 이웃이 필요하면 다음 셀의 컴포넌트까지 참조 가능 (단 로드되어 있어야)

**예시 비교**:

```
 WP 그리드:  ┌─────────┬─────────┐   (2×2 컴포넌트씩 한 셀)
             │ Cell A  │ Cell B  │
             │ 4 comps │ 4 comps │
             ├─────────┼─────────┤
             │ Cell C  │ Cell D  │
             │ 4 comps │ 4 comps │
             └─────────┴─────────┘
 
 편집 후 BatchedMerge:
 사용자가 Cell A 가운데 두 컴포넌트를 편집 →
 영향 컴포넌트 = A의 그 2개 + 노멀 계산용 이웃 8개 (= 3×3 전체)
 → 배치 구성에는 Cell A, B, C의 일부 컴포넌트가 포함될 수 있음
   (B, D가 로드되어 있지 않으면 그 방향의 노멀 계산은 지연)
```

즉 **그리드 크기는 BatchedMerge에 대한 "가능한 컴포넌트 집합"을 결정하는 상한**이지만, 배치 경계 자체는 머지 내부 로직이 다시 정합니다. 따라서 WP 그리드 조정이 머지 성능·응답성에 영향을 미칠 수 있지만, BatchedMerge의 분할 알고리즘 자체를 바꾸는 것은 아닙니다.

### 2.1 GridSizeInComponents

```cpp
// LandscapeInfo.h:375
LANDSCAPE_API uint32 GetGridSize(uint32 InGridSizeInComponents) const;
```

**`GridSizeInComponents`**: 그리드 셀 하나에 들어갈 컴포넌트 수 (한 축당). 예를 들어 이 값이 `2`면 2×2 = 4개 컴포넌트가 한 프록시에 들어갑니다.

`GetGridSize(InGridSizeInComponents)`는 이 값을 월드 단위(cm)로 변환 — 컴포넌트 하나의 월드 크기 × `InGridSizeInComponents`.

예시:

```
ComponentSizeQuads = 63, 컴포넌트당 월드 크기 = 63 × 100cm = 6300cm (기본 Scale)

GridSizeInComponents = 2 → GetGridSize(2) = 12600cm = 126m
→ 한 StreamingProxy가 126×126m 영역을 담당
→ 4개 컴포넌트 포함
```

`GridSizeInComponents`를 크게 하면:
- **프록시 수↓** (메모리·오버헤드 감소)
- **한 번에 스트리밍되는 영역↑** (메모리 피크 증가, 언로드 지연)

작게 하면 그 반대. 프로젝트 스케일에 따라 튜닝하는 값입니다.

### 2.2 ChangeGridSize — 에디터에서의 재구성 (런타임 작업 아님)

```cpp
// LandscapeSubsystem.h:172
LANDSCAPE_API void ChangeGridSize(ULandscapeInfo* LandscapeInfo, uint32 NewGridSizeInComponents);
```

**중요 명확화**: 이 함수는 **에디터 전용**이며, **게임 런타임에는 호출되지 않습니다**. 이름이나 맥락 때문에 "런타임에 동적으로 그리드가 바뀌는 작업"으로 오해하기 쉬우나, 실제로는:

- **에디터에서 사용자가 Landscape 설정을 변경**할 때 (Details 패널에서 `GridSizeInComponents` 값 조정 → Apply)
- 함수가 **모든 기존 스트리밍 프록시 액터를 Destroy**하고, **새 그리드 크기에 맞게 다시 스폰**, **디스크의 `.umap`/`.uasset` 패키지를 재작성**
- 이미 저장된 프록시 패키지 수십~수백 개를 다시 쓰는 매우 **무거운 "원타임 리팩토링 작업"**
- 게임 빌드에는 이 코드가 컴파일되지 않거나, 컴파일되어도 호출되지 않음

즉 **"미리 구워둬야 한다"는 말이 정확히 맞습니다**. 그리드 크기는 프로젝트 초기에 결정하고, `ChangeGridSize`는 정말 필요한 경우에만 에디터에서 수동으로 호출하는 비상 수단입니다.

**권장 워크플로우**:
1. 프로젝트 시작 시 Landscape 스케일·컴포넌트 크기·예상 지형 범위를 고려해 `GridSizeInComponents` 결정 (보통 1, 2, 4 중 하나)
2. 이후 편집 세션에서는 이 값을 건드리지 않음
3. 정말 불가피하게 변경해야 하면 `ChangeGridSize` 호출 → 빌드·저장 → VCS 커밋 (디스크 변화량 큼)
4. 게임 빌드·쿠킹 시에는 이미 확정된 그리드 구조로 패키징

**에디터 API의 맥락**:
- `IsGridBased()`도 에디터 모드 구분 용도 (WP 맵 여부)
- `FindOrAddLandscapeProxy(Info, SectionBase)`도 에디터에서 "컴포넌트를 적절한 프록시에 추가"할 때 쓰임
- 이 모든 API가 `#if WITH_EDITOR` 가드 하에 있습니다

따라서 "런타임"이라는 표현은 이 서브섹션 제목에서 제거하는 게 더 정확했습니다. 실질적 의미는 **"Landscape가 로드된 상태에서 에디터 세션 중 실행되는 재구성"**입니다.

### 2.3 ShouldIncludeGridSizeInName

서로 다른 그리드 크기로 저장된 프록시가 섞이면 이름 충돌이 납니다. 그래서:

```cpp
// LandscapeStreamingProxy.h:49
virtual bool ShouldIncludeGridSizeInName(UWorld* InWorld, const FActorPartitionIdentifier& InIdentifier) const override;
```

이 함수가 true를 반환하면 프록시 이름에 **그리드 크기 ID가 포함**됩니다. 예: `LandscapeStreamingProxy_G2_0_0` 같은 형태.

> **소스 확인 위치**
> - `Engine/Source/Runtime/Landscape/Classes/LandscapeInfo.h:375` — `GetGridSize`
> - `Engine/Source/Runtime/Landscape/Public/LandscapeSubsystem.h:172` — `ChangeGridSize`
> - `Engine/Source/Runtime/Landscape/Classes/LandscapeStreamingProxy.h:49` — `ShouldIncludeGridSizeInName`

## 3. 공유 속성 — 마스터가 권위를 가진다

`ALandscape`는 "이 Landscape 전체의 기본 설정"을 소유하고, `ALandscapeStreamingProxy`는 그걸 **상속**해서 씁니다. 예:

| 공유 속성 (마스터가 정함) | 예시 |
|------|------|
| `LandscapeMaterial` | 기본 지형 재질 |
| LOD 설정 (LODDistributionSetting, LOD0DistributionSetting 등) | 연속 LOD 튜닝 |
| Nanite 설정 (`bEnableNanite`, 포지션 정밀도, 스커트) | 전체적으로 Nanite 쓸지 |
| `CastShadow`, `bCastStaticShadow` 등 그림자 플래그 | 전체 그림자 동작 |

스트리밍 프록시는 이들 속성의 **로컬 사본**을 가지지만 기본적으로는 마스터값을 그대로 받습니다.

### 3.1 OverriddenSharedProperties — 개별 오버라이드

특정 프록시가 다른 값을 쓰고 싶을 때:

```cpp
// LandscapeStreamingProxy.h:35-36
UPROPERTY()
TSet<FName> OverriddenSharedProperties;

// LandscapeStreamingProxy.h:67-68
virtual bool IsSharedPropertyOverridden(const FName& InPropertyName) const override;
virtual void SetSharedPropertyOverride(const FName& InPropertyName, const bool bIsOverridden) override;
```

프로퍼티 이름이 이 집합에 있으면 "마스터 값 무시, 로컬 값 사용"이고, 없으면 마스터 값을 따릅니다. 이 메커니즘으로 **특정 셀만 다른 재질/설정을 적용**할 수 있지만, 과도하게 쓰면 공유의 이점이 무너지므로 예외적 사용 권장.

### 3.2 OnProxyFixupSharedData 델리게이트

마스터가 바뀌면 스트리밍 프록시들이 알아야 합니다:

```cpp
// LandscapeProxy.h:83
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnLandscapeProxyFixupSharedDataDelegate,
    ALandscapeProxy* /*InProxy*/, const FOnLandscapeProxyFixupSharedDataParams& /*InParams*/);
```

프록시가 로드될 때 `FixupOverriddenSharedProperties()`를 통해 마스터 기본값을 다시 흡수하며, 이미 업그레이드된 경우 `bUpgradePropertiesPerformed` 플래그로 중복을 방지.

## 4. 스트리밍 인/아웃 플로우

### 4.1 로드 시점

플레이어/카메라가 그리드 셀로 진입 → WP가 패키지를 로드 → `ALandscapeStreamingProxy` 스폰. 이후 시퀀스:

```mermaid
sequenceDiagram
    participant WP as WorldPartition
    participant Proxy as ALandscapeStreamingProxy
    participant Master as ALandscape
    participant Info as ULandscapeInfo
    participant Sub as ULandscapeSubsystem
    participant Comp as ULandscapeComponent
    participant Render as 렌더 스레드
    
    WP->>Proxy: Spawn & 패키지 로드
    Proxy->>Proxy: PostLoad / PostRegisterAllComponents
    
    Proxy->>Master: LandscapeActorRef.Resolve() (Soft → Hard)
    alt 마스터가 로드되어 있음
        Proxy->>Proxy: IsValidLandscapeActor(Master) → GUID 매칭
        Proxy->>Proxy: FixupOverriddenSharedProperties()
    else 마스터가 없음 (비정상)
        Proxy->>Proxy: 렌더/콜리전 비활성, 로그 경고
    end
    
    Proxy->>Info: FindOrCreate(World, LandscapeGuid)
    Proxy->>Info: RegisterActor(this)
    Info->>Info: GetSortedStreamingProxies()에 정렬 삽입
    
    loop 각 ULandscapeComponent
        Comp->>Info: RegisterActorComponent(this)
        Info->>Info: XYtoComponentMap[XY] = comp
    end
    
    Proxy->>Sub: RegisterActor(this) → Proxies[] 추가
    Sub->>Sub: LandscapeGroup에 프록시 등록 (LOD 그룹 키 기반)
    
    Comp->>Render: SceneProxy 생성 (Classic 또는 Nanite)
    Comp->>Comp: CollisionComponent::RecreateCollision()
    
    Note over Render: 프록시가 프러스텀에 들어오면 렌더링 시작
```

### 4.2 언로드 시점

플레이어가 셀에서 멀어지면 역순:

1. `ULandscapeSubsystem::UnregisterActor(Proxy)` — Proxies 배열에서 제거
2. 각 컴포넌트가 `ULandscapeInfo::UnregisterActorComponent` 호출
3. `ULandscapeInfo::UnregisterActor(Proxy)` — 정렬 배열에서 제거
4. 프록시 액터가 Destroy → SceneProxy 해제, CollisionComponent 해제
5. WP가 패키지 언로드

**주의**: 언로드된 프록시의 컴포넌트가 다른 프록시의 **3×3 이웃**에 있으면, 남은 프록시의 노멀 계산이 완료되지 못하고 경계 부분이 미완성 상태로 남을 수 있습니다. 이는 [05-edit-layers.md](05-edit-layers.md) §6에서 언급한 "이웃 없는 컴포넌트는 노멀 갱신 지연" 이슈로 이어집니다.

> **소스 확인 위치**
> - `LandscapeInfo.h:406, 409` — `RegisterActor` / `UnregisterActor`
> - `LandscapeSubsystem.h:110-111` — Subsystem의 `RegisterActor` / `UnregisterActor`
> - `LandscapeSubsystem.h:141-142` — `RegisterComponent` / `UnregisterComponent`

## 5. LandscapeGroup — LOD 그룹핑

`ULandscapeSubsystem`은 프록시들을 **LODGroupKey 기반으로 그룹화**해 `FLandscapeGroup`에 담습니다:

```cpp
// LandscapeSubsystem.h:293
// LODGroupKey --> Landscape Group
TMap<uint32, FLandscapeGroup*> Groups;

// LandscapeSubsystem.h:296-297
// list of streaming proxies that need to re-register with their group because they moved, or changed their LODGroupKey
UPROPERTY(Transient, DuplicateTransient, NonTransactional)
TSet<TObjectPtr<ALandscapeStreamingProxy>> StreamingProxiesNeedingReregister;
```

**목적**: LOD 계산 시 "같은 그룹에 속한 이웃 프록시들"을 참조해 LOD 차이를 제한. 같은 `ALandscape` 밑의 프록시들은 기본적으로 같은 그룹에 있고, `SetLODGroupKey`로 다른 그룹에 붙일 수 있습니다 (고급 사용 케이스).

프록시가 이동하거나 그룹 키가 바뀌면 `StreamingProxiesNeedingReregister`에 쌓이고 다음 틱에 그룹 재등록.

> **소스 확인 위치**
> - `Engine/Source/Runtime/Landscape/Classes/Landscape.h:336-337` — `SetLODGroupKey` / `GetLODGroupKey`
> - `LandscapeSubsystem.h:293, 297` — Groups 맵, 재등록 대기 집합
> - `LandscapeSubsystem.h:265-266` — `GetLandscapeGroupForProxy` / `GetLandscapeGroupForComponent`

## 6. Heightmap 텍스처 스트리밍

프록시가 로드되었다고 해서 **Heightmap 텍스처가 즉시 풀 해상도로 로드되는 건 아닙니다.** 텍스처는 일반 스트리밍 시스템을 따라 **저해상도 밉부터 로드**되며, 카메라 거리에 따라 고해상도 밉이 스트리밍됩니다.

### 6.1 FLandscapeTextureStreamingManager

```cpp
// LandscapeSubsystem.h:127
FLandscapeTextureStreamingManager* GetTextureStreamingManager() { return TextureStreamingManager; }
```

Landscape 전용 스트리밍 매니저가 "이 컴포넌트는 지금 화면에 얼마나 보이는가"를 계산해 텍스처 스트리밍 시스템에 **우선순위 힌트**를 제공합니다. 일반 MIC보다 정교한 이유는 Landscape 컴포넌트의 실제 화면 기여가 LOD + 모핑 + 서브섹션 단위로 변하기 때문.

### 6.2 OnHeightmapStreamed — 스트리밍 완료 알림

```cpp
// LandscapeSubsystem.h:68-98
struct FOnHeightmapStreamedContext
{
    const ALandscape* Landscape;
    const FBox2D& UpdateRegion;
    const TSet<class ULandscapeComponent*>& LandscapeComponentsInvolved;
};

DECLARE_MULTICAST_DELEGATE_OneParam(FOnHeightmapStreamedDelegate, const FOnHeightmapStreamedContext&);

// LandscapeSubsystem.h:198
FOnHeightmapStreamedDelegate::RegistrationType& OnHeightmapStreamed();
```

Heightmap 고해상도 밉이 스트리밍 인되면 이 델리게이트가 호출됩니다. **에디터 전용**이고, 블루프린트 브러시 등이 "지금 이 영역의 데이터가 완전히 로드되었으니 내 로직을 다시 실행해라"처럼 사용.

### 6.3 런타임 프리스트림 — PrepareTextureResources

```cpp
// Landscape.h:564
LANDSCAPE_API bool PrepareTextureResources(bool bInWaitForStreaming);
```

"이 시점에 필요한 텍스처가 다 로드될 때까지 기다릴지" 결정하는 API. 주로 블루프린트 `RenderHeightmap`처럼 **CPU가 텍스처 데이터를 필요로 하는 시점**에 호출됩니다.

## 7. 비파티션 월드에서의 대안

WP가 아닌 **월드 컴포지션(Legacy)**을 쓰는 프로젝트도 여전히 `ALandscapeStreamingProxy`를 사용할 수 있습니다. 차이는 "그리드 배치가 WP가 아니라 월드 컴포지션 타일"로 결정된다는 점뿐. 내부 동작(공유 속성, 프록시 등록, LOD 그룹)은 동일합니다.

완전히 비스트리밍 환경(작은 월드)에서는 **스트리밍 프록시 없이** `ALandscape` 하나만 두는 것이 가장 간단합니다.

## 8. 에디터 측 프록시 생성 API

```cpp
// LandscapeSubsystem.h:173
LANDSCAPE_API ALandscapeProxy* FindOrAddLandscapeProxy(ULandscapeInfo* LandscapeInfo, const FIntPoint& SectionBase);
```

주어진 `SectionBase`(그리드 좌표)에 해당하는 프록시가 있으면 반환, 없으면 **새로 생성**합니다. 에디터에서 "이 영역에 지형을 늘려라" 같은 작업 시 호출됩니다.

### 8.1 MoveComponentsToLevel / MoveComponentsToProxy

```cpp
// LandscapeInfo.h:348-351
LANDSCAPE_API ALandscapeProxy* MoveComponentsToLevel(
    const TArray<ULandscapeComponent*>& InComponents, ULevel* TargetLevel, FName NewProxyName = NAME_None);

LANDSCAPE_API ALandscapeProxy* MoveComponentsToProxy(
    const TArray<ULandscapeComponent*>& InComponents, ALandscapeProxy* LandscapeProxy,
    bool bSetPositionAndOffset = false, ULevel* TargetLevel = nullptr);
```

컴포넌트를 다른 레벨이나 프록시로 **이적**시키는 API. 그리드 재구성이나 에디터 도구에서 사용. 단 스트리밍 프록시 간 이적은 그리드 경계를 깨뜨릴 수 있어 신중히 써야 합니다.

## 9. 공간 로딩 플래그

```cpp
// Landscape.h:646-648
/** Landscape actor has authority on default streaming behavior for new actors : LandscapeStreamingProxies & LandscapeSplineActors */
UPROPERTY(EditAnywhere, Category = WorldPartition)
bool bAreNewLandscapeActorsSpatiallyLoaded = true;
```

**새로 생성되는 스트리밍 프록시와 스플라인 액터가 기본적으로 공간 로딩 대상이 되는지**를 마스터가 결정. 이 플래그를 끄면 새 프록시들이 모두 "항상 로드" 상태가 되어 WP 이점이 사라집니다 — 디버그나 특수 시나리오에서만 사용.

`ALandscape` 자신은 이 플래그와 무관하게 **항상 공간 로딩 불가**:

```cpp
// Landscape.h:324
virtual bool CanChangeIsSpatiallyLoadedFlag() const override { return false; }
```

## 10. 요약

| 질문 | 답 |
|------|---|
| 누가 그리드 셀당 Landscape 프록시를 만드나? | WP가 사용자 설정 `GridSizeInComponents`에 맞춰 생성 |
| 프록시와 마스터는 어떻게 연결되나? | `LandscapeActorRef` (Soft) + GUID 매칭 (`IsValidLandscapeActor`) |
| 프록시가 독립적 설정을 가지려면? | `OverriddenSharedProperties`에 프로퍼티 이름 추가 |
| 로드되면 어디에 등록되나? | `ULandscapeInfo` (좌표 매핑) + `ULandscapeSubsystem`의 `Proxies`와 `Groups` |
| LOD는 전역적으로 어떻게 조율되나? | `FLandscapeGroup`이 같은 LODGroupKey의 프록시들을 묶어 차이 제한 |
| 텍스처 로딩은? | 일반 스트리밍 + `FLandscapeTextureStreamingManager`의 힌트 |
| 스트리밍 완료를 언제 알 수 있나? | `OnHeightmapStreamed` 델리게이트 (에디터) |

다음 문서에서는 프록시에 붙어 있는 **충돌 컴포넌트와 물리 재질**을 다룹니다 → [08-collision-physics.md](08-collision-physics.md).
