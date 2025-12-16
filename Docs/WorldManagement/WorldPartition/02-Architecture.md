# World Partition 아키텍처

> **작성일**: 2025-12-16
> **엔진 버전**: Unreal Engine 5.7

## 시스템 전체 구조

World Partition은 여러 서브시스템이 유기적으로 연결된 복잡한 아키텍처입니다.

```
┌─────────────────────────────────────────────────────────────────┐
│                        AWorldDataLayers                          │
│                      (Data Layer Container)                      │
│  ┌─────────────────────────────────────────────────────────┐   │
│  │              UDataLayerManager                           │   │
│  │          (Layer Runtime State Control)                   │   │
│  └─────────────────────────────────────────────────────────┘   │
└───────────────────────────────┬─────────────────────────────────┘
                                │
┌───────────────────────────────┴─────────────────────────────────┐
│                        UWorldPartition                           │
│                      (Main System Class)                         │
│  ┌──────────────────────────────────────────────────────────┐  │
│  │         FActorDescContainerInstanceCollection            │  │
│  │              (Actor Descriptor Storage)                  │  │
│  │  ┌────────────────────────────────────────────────┐     │  │
│  │  │   TMap<FGuid, FWorldPartitionActorDesc*>       │     │  │
│  │  │   - ActorGuid → Descriptor mapping             │     │  │
│  │  │   - Lightweight metadata only                  │     │  │
│  │  └────────────────────────────────────────────────┘     │  │
│  └──────────────────────────────────────────────────────────┘  │
│                                                                  │
│  ┌──────────────────────────────────────────────────────────┐  │
│  │          UWorldPartitionRuntimeHash                      │  │
│  │          (Spatial Hash Grid System)                      │  │
│  │  ┌────────────────────────────────────────────────┐     │  │
│  │  │  Grid Configuration                             │     │  │
│  │  │  - Cell Size (e.g., 12800cm = 128m)           │     │  │
│  │  │  - Loading Range                               │     │  │
│  │  └────────────────────────────────────────────────┘     │  │
│  │  ┌────────────────────────────────────────────────┐     │  │
│  │  │  TArray<UWorldPartitionRuntimeCell*>           │     │  │
│  │  │  - Runtime Cells                               │     │  │
│  │  └────────────────────────────────────────────────┘     │  │
│  └──────────────────────────────────────────────────────────┘  │
│                                                                  │
│  ┌──────────────────────────────────────────────────────────┐  │
│  │     UWorldPartitionStreamingPolicy                       │  │
│  │     (Streaming Decision Logic)                           │  │
│  │  ┌────────────────────────────────────────────────┐     │  │
│  │  │  EvaluateStreamingSources()                     │     │  │
│  │  │  - Check player position                        │     │  │
│  │  │  - Calculate distance                           │     │  │
│  │  │  - Determine load/unload                        │     │  │
│  │  └────────────────────────────────────────────────┘     │  │
│  └──────────────────────────────────────────────────────────┘  │
└──────────────────────────────────────────────────────────────────┘
                                │
                                ▼
┌──────────────────────────────────────────────────────────────────┐
│              UWorldPartitionSubsystem                             │
│              (World Subsystem - Runtime)                          │
│  ┌──────────────────────────────────────────────────────────┐   │
│  │  Tick() - Called every frame                             │   │
│  │  - Update streaming sources                              │   │
│  │  - Update cell states                                    │   │
│  │  - Process load/unload requests                          │   │
│  └──────────────────────────────────────────────────────────┘   │
└──────────────────────────────────────────────────────────────────┘
```

## 핵심 컴포넌트

### 1. UWorldPartition (worldPartition.h:141)

메인 시스템 클래스로, World Partition의 중앙 허브 역할을 합니다.

```cpp
UCLASS(AutoExpandCategories=(WorldPartition), MinimalAPI)
class UWorldPartition final : public UObject,
                               public FActorDescContainerInstanceCollection,
                               public IWorldPartitionCookPackageGenerator
{
    // Runtime Hash - 공간 분할 시스템
    UPROPERTY()
    TObjectPtr<UWorldPartitionRuntimeHash> RuntimeHash;

    // Streaming Policy - 스트리밍 로직
    UPROPERTY()
    TObjectPtr<UWorldPartitionStreamingPolicy> StreamingPolicy;

    // Editor Hash (에디터 전용)
#if WITH_EDITORONLY_DATA
    UPROPERTY()
    TObjectPtr<UWorldPartitionEditorHash> EditorHash;
#endif
};
```

**책임**:
- Actor Descriptor 관리
- Runtime Hash 소유 및 초기화
- Streaming Policy 실행
- Data Layer 통합

### 2. UWorldPartitionRuntimeHash (WorldPartitionRuntimeHash.h:59)

공간을 Grid로 분할하고 Runtime Cell을 관리하는 추상 클래스입니다.

```cpp
UCLASS(Abstract, MinimalAPI)
class URuntimeHashExternalStreamingObjectBase : public UObject,
                                                 public IWorldPartitionCookPackageObject,
                                                 public IDataLayerInstanceProvider
{
    // Streaming Cells
    UPROPERTY()
    TArray<TObjectPtr<UWorldPartitionRuntimeCell>> StreamingCells;

    // External Data Layer (optional)
    UPROPERTY()
    TObjectPtr<UExternalDataLayerInstance> RootExternalDataLayerInstance;
};
```

**주요 구현체**:
- `UWorldPartitionRuntimeSpatialHash` - 그리드 기반 공간 해싱

### 3. UWorldPartitionRuntimeCell

Grid의 개별 셀을 나타냅니다.

```cpp
// WorldPartitionRuntimeCell.h
UCLASS(Abstract, MinimalAPI)
class UWorldPartitionRuntimeCell : public UObject
{
    // Cell의 월드 공간 바운드
    UPROPERTY()
    FBox ContentBounds;

    // Cell에 속한 Actor Descriptor들
    UPROPERTY()
    TArray<FName> DataLayers;

    // 현재 스트리밍 상태
    EWorldPartitionRuntimeCellState CurrentState;
};
```

**Cell States**:
```cpp
enum class EWorldPartitionRuntimeCellState : uint8
{
    Unloaded,    // 메모리에 없음
    Loaded,      // 메모리에 로드됨 (숨김)
    Activated    // 메모리에 로드되고 표시됨
};
```

### 4. FWorldPartitionActorDesc (WorldPartitionActorDesc.h)

액터의 경량 메타데이터 구조입니다.

```cpp
struct FWorldPartitionActorDescInitData
{
    UClass* NativeClass;             // 액터 클래스
    FName PackageName;               // 패키지 경로
    FSoftObjectPath ActorPath;       // 액터 레퍼런스
    TArray<uint8> SerializedData;    // 직렬화된 데이터
};

// WorldPartitionActorDesc.h:70
struct FWorldPartitionRelativeBounds
{
    FVector Center;    // 중심 좌표
    FQuat Rotation;    // 회전
    FVector Extents;   // 범위 (Half Extent)
    bool bIsValid;     // 유효성 플래그
};
```

**메모리 레이아웃**:
```
Full Actor Object (Not Loaded):
┌────────────────────────────────────┐
│ FWorldPartitionActorDesc           │  ~100-200 bytes
│ ┌────────────────────────────────┐ │
│ │ GUID                           │ │
│ │ Bounds (FWorldPartitionBounds) │ │
│ │ NativeClass pointer            │ │
│ │ Data Layers TArray             │ │
│ │ Runtime Grid                   │ │
│ └────────────────────────────────┘ │
└────────────────────────────────────┘

                VS

Loaded AActor Object:
┌────────────────────────────────────┐
│ AActor                             │  ~10-50 KB
│ ┌────────────────────────────────┐ │
│ │ USceneComponent tree           │ │
│ │ Materials                      │ │
│ │ Physics state                  │ │
│ │ AI state                       │ │
│ │ Network replication            │ │
│ └────────────────────────────────┘ │
└────────────────────────────────────┘
```

### 5. UWorldPartitionStreamingPolicy

스트리밍 결정 로직을 담당합니다.

```cpp
// WorldPartitionStreamingPolicy.h
UCLASS(MinimalAPI)
class UWorldPartitionStreamingPolicy : public UObject
{
public:
    // 매 프레임 호출: Streaming Sources 기반으로 Cell 로드/언로드 결정
    virtual void UpdateStreamingState() PURE_VIRTUAL(UWorldPartitionStreamingPolicy::UpdateStreamingState,);

    // Streaming Source 추가 (보통 플레이어)
    virtual void AddStreamingSource(const FWorldPartitionStreamingSource& Source);
};
```

### 6. FWorldPartitionStreamingSource (WorldPartitionStreamingSource.h)

스트리밍의 중심점입니다 (보통 플레이어 캐릭터).

```cpp
USTRUCT(BlueprintType)
struct FStreamingSourceShape
{
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float LoadingRange = 20000.0f;  // 20km 기본값

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FName Name;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bUseGridLoadingRange = true;
};
```

## 시스템 초기화 흐름

```
World Load
    │
    ▼
UWorldPartition::Initialize()
    │
    ├─→ LoadRuntimeHash()
    │   └─→ UWorldPartitionRuntimeSpatialHash 생성
    │       └─→ Grid 설정 (Cell Size, Loading Range)
    │
    ├─→ InitializeActorDescContainers()
    │   └─→ Actor Descriptor 로드 (전체 월드 메타데이터)
    │       └─→ TMap<FGuid, FWorldPartitionActorDesc*> 구축
    │
    ├─→ InitializeDataLayers()
    │   └─→ UDataLayerManager 초기화
    │
    └─→ CreateStreamingPolicy()
        └─→ UWorldPartitionStreamingPolicy 생성

Runtime (Game Start)
    │
    ▼
UWorldPartitionSubsystem::Initialize()
    │
    └─→ RegisterStreamingSources()
        └─→ PlayerController → StreamingSource 등록
```

## Runtime 스트리밍 흐름

```
Every Tick:
    │
    ▼
UWorldPartitionSubsystem::Tick()
    │
    ├─→ UpdateStreamingSources()
    │   └─→ Get Player Positions
    │
    ├─→ StreamingPolicy->UpdateStreamingState()
    │   │
    │   ├─→ For each Streaming Source:
    │   │   │
    │   │   ├─→ RuntimeHash->GetCellsInRange(Position, LoadingRange)
    │   │   │   └─→ Spatial Hash Query
    │   │   │       └─→ Return nearby cells
    │   │   │
    │   │   └─→ Determine Cell States:
    │   │       ├─→ Within LoadingRange → Load
    │   │       ├─→ Within ActivationRange → Activate
    │   │       └─→ Outside Range → Unload
    │   │
    │   └─→ DataLayerManager->UpdateRuntimeState()
    │       └─→ Apply Data Layer filtering
    │
    └─→ ProcessStreamingRequests()
        │
        ├─→ Load Cells:
        │   └─→ UWorldPartitionRuntimeLevelStreamingCell::Load()
        │       └─→ ULevelStreamingDynamic::LoadLevel()
        │           └─→ Load actors from package
        │
        ├─→ Activate Cells:
        │   └─→ SetVisibility(true)
        │       └─→ Actors become visible
        │
        └─→ Unload Cells:
            └─→ ULevelStreamingDynamic::UnloadLevel()
                └─→ Destroy actors, free memory
```

## Data Layer 통합

World Partition은 Data Layers와 긴밀하게 통합됩니다.

```
UWorldPartition
    ↓
UDataLayerManager (WorldPartition 내부)
    ↓
TMap<FName, UDataLayerInstance*>
    ├─→ "Environment" (Activated)
    ├─→ "Buildings" (Loaded)
    └─→ "NPCs" (Unloaded)

Streaming Decision:
    Cell should load? → YES
    AND
    Actor's Data Layer activated? → Check
        If all layers activated → Load Actor
        Else → Skip Actor
```

**코드 예시**:
```cpp
// DataLayerManager.h:83
bool SetDataLayerInstanceRuntimeState(
    const UDataLayerInstance* InDataLayerInstance,
    EDataLayerRuntimeState InState,
    bool bInIsRecursive = false
);

// 재귀적으로 자식 레이어도 변경 가능
```

## HLOD (Hierarchical Level of Detail) 통합

```
World Partition
    │
    ├─→ Near Cells (0-5km)
    │   └─→ Full detail actors
    │
    ├─→ Mid Cells (5-10km)
    │   └─→ HLOD Level 1 (Merged meshes)
    │
    └─→ Far Cells (10-20km)
        └─→ HLOD Level 2 (Proxy meshes)

HLOD 자동 생성 (Cook time):
    WorldPartition → HLOD Builder
        └─→ Merge actors per cell
            └─→ Generate HLOD assets
```

**HLOD 플러그인 위치**:
- `Engine/Plugins/Editor/WorldPartitionHLODUtilities/`

## 설계 철학

### 1. 완전한 자동화
수동 관리를 최소화하고 엔진이 자동으로 처리합니다.

### 2. 공간 기반 (Spatial-First)
레벨 경계가 아닌 3D 공간 좌표 기반으로 스트리밍합니다.

### 3. 메타데이터 우선 (Metadata-First)
Actor Descriptor로 전체 월드 정보를 경량으로 유지하고, 필요할 때만 실제 액터를 로드합니다.

### 4. 확장 가능성 (Extensibility)
- Runtime Hash: 커스텀 구현 가능
- Streaming Policy: 게임별로 커스터마이징 가능
- Data Layers: 동적 레이어 추가/제거

### 5. 협업 친화적 (Collaboration-Friendly)
One File Per Actor (OFPA) 방식으로 Git 충돌을 최소화합니다.

## 성능 특징

### 메모리 효율
```
100,000 actors world:
- Actor Descriptors: ~10 MB (항상 메모리에)
- Loaded actors: ~1-2 GB (플레이어 주변만)
Total: ~2 GB instead of 10+ GB
```

### 스트리밍 성능
- Spatial Hash: O(1) cell lookup
- Async loading: 백그라운드 스레드에서 로드
- Incremental unloading: GC 부담 분산

## 다음 단계

아키텍처를 이해했다면 다음 문서로 진행하세요:

1. **[03-CoreClasses.md](03-CoreClasses.md)** - 각 클래스의 세부 구현
2. **[04-StreamingSystem.md](04-StreamingSystem.md)** - 스트리밍 메커니즘 상세 분석
3. **[05-ActorDescriptor.md](05-ActorDescriptor.md)** - Actor Descriptor 시스템 심화

## 참고 자료

- **소스 코드**:
  - `Engine/Source/Runtime/Engine/Public/WorldPartition/WorldPartition.h`
  - `Engine/Source/Runtime/Engine/Public/WorldPartition/WorldPartitionRuntimeHash.h`
  - `Engine/Source/Runtime/Engine/Public/WorldPartition/WorldPartitionStreamingPolicy.h`
