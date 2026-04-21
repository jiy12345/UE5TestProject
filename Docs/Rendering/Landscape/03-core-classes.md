# 03. 핵심 클래스 레퍼런스

> **작성일**: 2026-04-21
> **엔진 버전**: UE 5.7

이 문서는 Landscape 시스템의 주요 클래스·구조체를 **빠르게 찾아보기 위한 레퍼런스 카드**입니다. 동작 원리는 관련 문서 링크를 따라가세요.

## 1. 액터 클래스 (자세한 것은 [02-architecture.md](02-architecture.md))

### ALandscapeProxy — 공통 지형 타일 액터
- **파일**: `Engine/Source/Runtime/Landscape/Classes/LandscapeProxy.h`
- **부모**: `APartitionActor → AActor`
- **역할**: 모든 지형 액터의 공통 기반. 컴포넌트/재질/스트리밍 설정 보유
- **주요 멤버** (발췌):
  - `TArray<TObjectPtr<ULandscapeComponent>> LandscapeComponents`
  - `TArray<TObjectPtr<ULandscapeHeightfieldCollisionComponent>> CollisionComponents`
  - `TObjectPtr<UMaterialInterface> LandscapeMaterial`
  - `TObjectPtr<ULandscapeNaniteComponent> LandscapeNaniteComponent`
  - `bUseNanite`, `bEnableNanite`, `NaniteLODIndex`
- **주요 API**: `GetLandscapeActor()`, `GetLandscapeMaterial(LODIndex)`, `RecreateComponentsState()`, `ImportFromFile(...)`

### ALandscape — 마스터 액터
- **파일**: `Engine/Source/Runtime/Landscape/Classes/Landscape.h:275`
- **부모**: `ALandscapeProxy`
- **역할**: 논리적으로 하나인 Landscape의 편집 권한·공유 속성 권한 보유
- **주요 멤버**:
  - `TArray<FLandscapeLayer> LandscapeEditLayers` (private, 아래 §5 참고)
  - `FGuid EditingLayer` (현재 편집 중인 레이어)
  - `bool bAreNewLandscapeActorsSpatiallyLoaded` (WP 기본값)
- **주요 API** (편집):
  - `CreateLayer(FName, TSubclassOf<ULandscapeEditLayerBase>)` — 새 에디트 레이어
  - `ReorderLayer(StartIdx, DestIdx)`
  - `SetEditingLayer(FGuid)`
  - `RegenerateLayersHeightmaps()` → BatchedMerge 진입 ([05-edit-layers.md](05-edit-layers.md))
  - `RenderHeightmap(Transform, Extents, OutRT)` / `RenderWeightmap(...)` — 블루프린트에서 최종 텍스처 덤프

### ALandscapeStreamingProxy — WP 스트리밍 타일
- **파일**: `Engine/Source/Runtime/Landscape/Classes/LandscapeStreamingProxy.h:18`
- **부모**: `ALandscapeProxy`, `UCLASS(notplaceable)`
- **역할**: World Partition 그리드 셀 단위 지형 타일
- **주요 멤버**:
  - `TSoftObjectPtr<ALandscape> LandscapeActorRef` — 마스터 soft 참조
  - `TSet<FName> OverriddenSharedProperties` — 마스터 기본값 오버라이드 속성
- **주요 API**: `IsValidLandscapeActor(ALandscape*)` (GUID 검증), `SetLandscapeActor(ALandscape*)`

## 2. ULandscapeInfo — 월드 레지스트리
- **파일**: `Engine/Source/Runtime/Landscape/Classes/LandscapeInfo.h:107`
- **부모**: `UObject` (Transient)
- **역할**: 월드 내 특정 GUID에 해당하는 Landscape의 모든 프록시·컴포넌트 좌표 인덱스
- **주요 멤버**:
  - `TWeakObjectPtr<ALandscape> LandscapeActor`
  - `FGuid LandscapeGuid`
  - `int32 ComponentSizeQuads`, `SubsectionSizeQuads`, `ComponentNumSubsections`
  - `TMap<FIntPoint, ULandscapeComponent*> XYtoComponentMap`
  - `TMap<FIntPoint, ULandscapeHeightfieldCollisionComponent*> XYtoCollisionComponentMap`
- **주요 API**:
  - `static Find(World, Guid)` / `static FindOrCreate(World, Guid)`
  - `RegisterActor(Proxy)` / `UnregisterActor(Proxy)`
  - `RegisterActorComponent(Comp)` / `RegisterCollisionComponent(Comp)`
  - `GetSortedStreamingProxies()` — 정렬된 TArray 반환
  - `ForEachLandscapeProxy(Fn)` — 마스터 + 모든 스트리밍 프록시 순회
  - `GetOverlappedComponents(Transform, Extents, OutMap, OutBounds)` — AABB 겹치는 컴포넌트

## 3. ULandscapeSubsystem — 월드 서비스
- **파일**: `Engine/Source/Runtime/Landscape/Public/LandscapeSubsystem.h`
- **부모**: `UTickableWorldSubsystem`
- **역할**: 월드 레벨에서 Landscape 액터들을 관리·틱, 그래스와 텍스처 스트리밍 매니저 보유
- **주요 멤버**:
  - `TArray<TWeakObjectPtr<ALandscapeProxy>> Proxies` (활성 프록시 레지스트리)
  - `FLandscapeGrassMapsBuilder* GrassMapsBuilder` — 비동기 그래스 생성 상태 머신
  - `FLandscapeTextureStreamingManager* TextureStreamingManager` — 텍스처 스트리밍 우선순위
- **주요 API**:
  - `Tick(DeltaTime)` — 그래스/스트리밍 갱신 구동
  - `RegisterActor(Proxy)` / `UnregisterActor(Proxy)`
  - 전역 유틸: `UE::Landscape::HasModifiedLandscapes()`, `BuildGrassMaps(BuildFlags)`, `BuildNanite(...)`, `BuildAll(...)`
- **⚠️ 이 서브시스템이 하지 않는 것 — 프록시 스트리밍**:
  `ULandscapeSubsystem`은 `ALandscapeStreamingProxy`의 **스폰·Despawn을 담당하지 않습니다**. 이름에 "StreamingManager"가 들어간 멤버는 **텍스처 스트리밍(Heightmap/Weightmap 밉 우선순위)** 을 뜻하며, 액터 스트리밍이 아닙니다.
  
  - **프록시 로드/언로드의 주체**: **World Partition** (그리드 셀 로딩 시스템). WP가 셀 패키지를 로드하면 프록시가 스폰되고, 스폰 후 `RegisterActor`가 이 서브시스템에 호출되어 **레지스트리에 이름만 등록**됩니다.
  - **서브시스템이 하는 일**: 이미 스폰된 프록시들을 "알고 있는 상태"로 유지 + 프록시 그룹핑(LOD 조정) + 그래스 빌드 오케스트레이션 + 텍스처 스트리밍 우선순위 힌트 제공 + 편집 관련 유틸(BuildAll 등).
  - 자세한 분업은 [02-architecture.md §3.6](02-architecture.md) 및 [07-streaming-wp.md §4](07-streaming-wp.md) 참고.

## 4. ULandscapeComponent — 타일 하나의 데이터 컨테이너
- **파일**: `Engine/Source/Runtime/Landscape/Classes/LandscapeComponent.h`
- **부모**: `UPrimitiveComponent`
- **역할**: 섹션 하나의 Heightmap/Weightmap/재질 인스턴스/렌더 프록시를 보유하는 Landscape의 원자 단위
- **"모든 걸 다 처리하는가?"에 대한 명확화**:
  "컴포넌트 하나가 런타임 Landscape 동작을 다 책임진다"는 단순화된 표현이지만, 실제로는 **데이터 소유자**일 뿐이고 각 관심사는 **전문 컴포넌트/헬퍼에 위임**됩니다:
  
  | 관심사 | 실제 처리 주체 | ULandscapeComponent의 역할 |
  |-------|-------------|--------------------------|
  | 데이터 보유 | **ULandscapeComponent 자신** | Heightmap/Weightmap 텍스처, 머티리얼 인스턴스, 레이어 할당 |
  | 렌더링 | **`FLandscapeComponentSceneProxy`** (렌더 스레드) | 컴포넌트가 `CreateSceneProxy()` 시점에 이 프록시를 생성·위임 |
  | 물리/콜리전 | **`ULandscapeHeightfieldCollisionComponent`** (별도 컴포넌트) | 컴포넌트가 `CollisionComponentRef`로 짝 관리 |
  | LOD 선택 | **`FLandscapeRenderSystem`** (전역) + Scene Proxy | 공유 속성과 거리 정보만 제공 |
  | Nanite 렌더 | **`ULandscapeNaniteComponent`** (선택적, StaticMesh 기반) | 컴포넌트는 소스 데이터만 제공, Nanite 빌드 경로는 별도 |
  | 편집 시점 데이터 | `LayersData: TMap<FGuid, FLandscapeLayerComponentData>` | 각 편집 레이어별 기여분을 컴포넌트에 저장 |
  | 최종 텍스처 병합 | **`ALandscape`의 BatchedMerge 파이프라인** | 병합 결과가 `HeightmapTexture`/`WeightmapTextures`에 resolve됨 |
  | 그래스 생성 | **`ULandscapeSubsystem::GrassMapsBuilder`** | 컴포넌트는 `GrassData` 캐시만 제공 |
  
  즉 컴포넌트는 **"타일 하나의 모든 데이터를 한 곳에 모은 컨테이너 + 전문 헬퍼 스폰 지점"**이고, 실제 처리 로직은 Scene Proxy / Collision Component / Subsystem / 마스터 `ALandscape`의 머지 파이프라인에 분산됩니다.
- **섹션·서브섹션 설정**:
  - `int32 ComponentSizeQuads` — 이 컴포넌트의 quad 개수 (예: 63, 127)
  - `int32 SubsectionSizeQuads` — 서브섹션 quad 수 (`+1`이 2의 거듭제곱)
  - `int32 NumSubsections` — 1 (1×1) 또는 2 (2×2), LOD 전환 부드럽게 하기 위함
- **텍스처 데이터**:
  - `TObjectPtr<UTexture2D> HeightmapTexture` — 이 컴포넌트의 Heightmap (단일)
  - `TArray<TObjectPtr<UTexture2D>> WeightmapTextures` — Weightmap(들) (여러 개)
  - `TArray<FWeightmapLayerAllocationInfo> WeightmapLayerAllocations` — 어떤 레이어가 어느 텍스처·채널에 있는지 매핑
- **재질**:
  - `TArray<TObjectPtr<UMaterialInstanceConstant>> MaterialInstances` — LOD별 재질 인스턴스
  - `TArray<FLandscapePerLODMaterialOverride> PerLODOverrideMaterials`
- **그래스 데이터** (에디터):
  - `FLandscapeComponentGrassData GrassData` — 비동기 그래스 빌드용 캐시
- **주요 API**:
  - `GetLandscapeProxy()` — 소속 프록시
  - `GetLandscapeInfo()` — 소속 Info
  - `RecreateCollision()` — 콜리전 재생성 트리거
  - `UpdateMaterialInstances()` — 재질 갱신
  - `GetSectionBase()` / `GetComponentSize()` — 월드 좌표/크기

## 5. Edit Layers 관련 타입 (자세한 것은 [05-edit-layers.md](05-edit-layers.md))

### FLandscapeLayer — 레이어 + 브러시 보유 구조
- **파일**: `Engine/Source/Runtime/Landscape/Classes/Landscape.h:164`
- **역할**: 하나의 편집 레이어에 연결된 브러시 배열 + 실제 `ULandscapeEditLayerBase` 인스턴스
- **주요 멤버**:
  - `TArray<FLandscapeLayerBrush> Brushes` — 블루프린트 브러시 목록
  - `TObjectPtr<ULandscapeEditLayerBase> EditLayer` — 실제 레이어 구현

### ULandscapeEditLayerBase — 편집 레이어 추상 기반
- **파일**: `Engine/Source/Runtime/Landscape/Classes/LandscapeEditLayer.h`
- **역할**: Edit Layer의 추상 인터페이스. 각 구현체가 "이 레이어가 Heightmap/Weightmap을 어떻게 기여하는지" 정의
- **주요 서브클래스**:
  - `ULandscapeEditLayerPersistent` — 일반 사용자 편집 레이어 (텍스처 저장)
  - `ULandscapeEditLayerSplines` — Landscape Spline이 기여하는 레이어
  - `ULandscapeEditLayerProcedural` — 절차적 레이어
- **주요 API**: `SupportsTargetType(Type)`, `NeedsPersistentTextures()`, `SupportsEditingTools()`, `SupportsBeingCollapsedAway()`

### FMergeContext — BatchedMerge 메타데이터
- **파일**: `Engine/Source/Runtime/Landscape/Public/LandscapeEditLayerMergeContext.h:31`
- **역할**: 머지 범위의 대상 레이어 이름 ↔ 인덱스 매핑 제공
- **주요 API**: `GetTargetLayerIndexForName(Name)`, `IsValidTargetLayerName(Name)`, `ConvertTargetLayerNamesToBitIndices(...)`

### FMergeRenderContext / FMergeRenderParams / FMergeRenderBatch
- **파일**: `Engine/Source/Runtime/Landscape/Public/LandscapeEditLayerMergeRenderContext.h`
- **역할**: GPU 머지의 실제 렌더 상태. 대상 컴포넌트 집합, 활성 레이어, 웨이트맵 이름, 렌더 단계들을 구성

### FEditLayerRendererState — 레이어 활성/지원 상태
- **파일**: `Engine/Source/Runtime/Landscape/Public/LandscapeEditLayerRendererState.h:25`
- **역할**: 각 레이어가 지원하는 타겟 타입(`SupportedTargetTypeState`, 불변) vs 현재 활성화된 것(`EnabledTargetTypeState`, 가변)
- **주요 API**: `EnableTargetType`, `DisableWeightmap`, `IsTargetActive`, `GetActiveTargetWeightmaps`

## 6. Heightmap/Weightmap 관련 (자세한 것은 [04-heightmap-weightmap.md](04-heightmap-weightmap.md))

### FWeightmapLayerAllocationInfo — 레이어 할당 정보
- **파일**: `Engine/Source/Runtime/Landscape/Classes/LandscapeComponent.h:141`
- **역할**: "어떤 레이어가 어느 Weightmap 텍스처의 어느 채널에 들어있는가"
- **멤버**:
  - `TObjectPtr<ULandscapeLayerInfoObject> LayerInfo`
  - `uint8 WeightmapTextureIndex` (`WeightmapTextures[]` 인덱스)
  - `uint8 WeightmapTextureChannel` (0=R, 1=G, 2=B, 3=A)
- **특수값**: `255` = 비할당 (`IsAllocated()`가 false)

### ULandscapeLayerInfoObject — 페인트 레이어 에셋
- **파일**: `Engine/Source/Runtime/Landscape/Classes/LandscapeLayerInfoObject.h`
- **역할**: 페인트 레이어의 메타데이터 (이름, 물리 재질, 하드코딩 플래그)
- **멤버**: `LayerName`, `PhysMaterial`, `Hardness`, `LayerUsageDebugColor`
- **사용처**: 아티스트가 만들어 Content Browser에 저장, Weightmap 레이어마다 하나씩 할당

### LandscapeDataAccess.h — 텍스처 인코딩 상수
- **파일**: `Engine/Source/Runtime/Landscape/Public/LandscapeDataAccess.h`
- **주요 상수/함수**:
  - `LANDSCAPE_ZSCALE = 1.0f / 128.0f` — 16비트 높이 → 월드 Z 변환 스케일
  - `LANDSCAPE_INV_ZSCALE` — 역변환
  - Height 패킹/언패킹 함수 (R<<8 | G → uint16)

## 7. 콜리전/물리 관련 (자세한 것은 [08-collision-physics.md](08-collision-physics.md))

### ULandscapeHeightfieldCollisionComponent
- **파일**: `Engine/Source/Runtime/Landscape/Classes/LandscapeHeightfieldCollisionComponent.h:39`
- **부모**: `UPrimitiveComponent`
- **역할**: 컴포넌트당 Heightfield 기반 콜리전 (Chaos `FHeightField` 사용)
- **주요 멤버**:
  - `CollisionSizeQuads` — 콜리전 해상도 (렌더 컴포넌트보다 보통 낮음)
  - `SimpleCollisionSizeQuads` — Simple Collision용 별도 해상도
  - `CollisionHeightData` — 16비트 높이 BulkData
  - `TArray<ULandscapeLayerInfoObject*> ComponentLayerInfos` — 물리 재질용 레이어 매핑

### ULandscapeMeshCollisionComponent (DEPRECATED 5.7)
- **파일**: `Engine/Source/Runtime/Landscape/Classes/LandscapeMeshCollisionComponent.h`
- **역할**: XY 오프셋 지원 시절의 TriMesh 콜리전. 5.7에서 edit-layer 이행하며 deprecated
- **주석**: 새 프로젝트에서 사용 금지, 구 프로젝트 호환용으로만 유지

### ULandscapePhysicalMaterial
- **파일**: `Engine/Source/Runtime/Landscape/Classes/LandscapePhysicalMaterial.h`
- **역할**: Landscape 레이어 → 물리 재질 매핑
- **동작**: 특정 위치의 물리 재질 쿼리 시, 해당 위치의 Weightmap 레이어 중 Dominant 레이어의 `LayerInfo->PhysMaterial` 반환

## 8. 렌더링 관련 (자세한 것은 [06-rendering-pipeline.md](06-rendering-pipeline.md))

### FLandscapeComponentSceneProxy
- **파일**: `Engine/Source/Runtime/Landscape/Public/LandscapeRender.h:701`
- **부모**: `FPrimitiveSceneProxy`
- **역할**: `ULandscapeComponent`의 렌더 스레드 프록시
- **주요 멤버**:
  - `LODScreenRatioSquared[8]` — 거리 기반 LOD 선택 테이블 (`LANDSCAPE_LOD_LEVELS` = 8)
  - 공유 버텍스 버퍼 참조 (`FLandscapeSharedBuffers`)
  - Heightmap/Normalmap 바인딩

### FLandscapeSharedBuffers
- **파일**: `Engine/Source/Runtime/Landscape/Public/LandscapeRender.h` (근처)
- **역할**: 같은 `(ComponentSize, NumSubsections)` 조합의 컴포넌트들이 **정점 인덱스 버퍼를 공유**하도록 하는 싱글톤 맵
- **이유**: 수백 개 컴포넌트가 동일 구조라면 인덱스 버퍼는 한 벌만 있으면 됨 (메모리 절약)

### ULandscapeNaniteComponent
- **파일**: `Engine/Source/Runtime/Landscape/Classes/LandscapeNaniteComponent.h:81`
- **부모**: `UStaticMeshComponent`
- **역할**: Landscape를 **Nanite 메시**로 빌드한 결과를 들고 있는 컴포넌트 (선택 사항)
- **빌드**: `UE::Landscape::Nanite::FAsyncBuildData` 구조체로 비동기 빌드, `FMeshDescription` → StaticMesh → 스왑
- **런타임 선택**: Nanite 지원 플랫폼에서 클래식 프록시 대신 이 경로로 렌더

### ULandscapeMaterialInstanceConstant
- **파일**: `Engine/Source/Runtime/Landscape/Classes/LandscapeMaterialInstanceConstant.h:10`
- **부모**: `UMaterialInstanceConstant`
- **역할**: Landscape 전용 MIC — 텍스처별 스트리밍 우선순위 정보를 추가로 보유
- **주요 멤버**: `TArray<FTextureStreamingInfo> TextureStreamingInfo`

## 9. 유틸리티/편집 타입

### ULandscapeHeightmapTextureEdgeFixup
- **파일**: `Engine/Source/Runtime/Landscape/Classes/LandscapeEdgeFixup.h`
- **역할**: 인접 컴포넌트 경계 정점 공유 규칙 유지 유틸

### FLandscapeVertexRef
- **파일**: `Engine/Source/Runtime/Landscape/Classes/LandscapeComponent.h:118`
- **역할**: 컴포넌트 내 정점을 `(X, Y, SubX, SubY)` 4-tuple로 식별하는 경량 참조

### FLandscapeGroup
- **파일**: `Engine/Source/Runtime/Landscape/Classes/LandscapeComponent.h` (forward decl; 구현은 Private)
- **역할**: 컴포넌트 그룹화 (동일 Shared Buffers 공유 결정 등 내부 최적화 단위)

### ULandscapeBlueprintBrushBase
- **파일**: `Engine/Source/Runtime/Landscape/Public/LandscapeBlueprintBrushBase.h`
- **역할**: 블루프린트로 확장 가능한 브러시 베이스 (Edit Layers에서 호출됨). 상세는 별도 이슈 분리 예정

## 10. 한눈에 보는 의존 관계

```
ALandscape (master)
  └─ FLandscapeLayer[]
       ├─ FLandscapeLayerBrush[]
       └─ ULandscapeEditLayerBase
            ├─ ULandscapeEditLayerPersistent
            ├─ ULandscapeEditLayerSplines
            └─ ULandscapeEditLayerProcedural

ALandscapeProxy / ALandscapeStreamingProxy
  ├─ ULandscapeComponent[]
  │    ├─ HeightmapTexture (UTexture2D)
  │    ├─ WeightmapTextures[] (UTexture2D)
  │    ├─ WeightmapLayerAllocations[] (FWeightmapLayerAllocationInfo)
  │    ├─ MaterialInstances[] (ULandscapeMaterialInstanceConstant)
  │    └─ FLandscapeComponentSceneProxy (렌더 스레드)
  ├─ CollisionComponents[]
  │    └─ ULandscapeHeightfieldCollisionComponent
  │         └─ ComponentLayerInfos[] (ULandscapeLayerInfoObject)
  └─ LandscapeNaniteComponent (선택)

ULandscapeInfo (Transient, 월드+GUID 기준)
  ├─ XYtoComponentMap
  ├─ XYtoCollisionComponentMap
  └─ StreamingProxies (sorted)

ULandscapeSubsystem (World Subsystem)
  ├─ Proxies (활성 액터 레지스트리)
  ├─ GrassMapsBuilder
  └─ TextureStreamingManager
```

각 화살표가 **Owning** 관계이며, `ULandscapeInfo`는 외부에서 좌표/GUID로 조회만 하는 **인덱스** 역할입니다.
