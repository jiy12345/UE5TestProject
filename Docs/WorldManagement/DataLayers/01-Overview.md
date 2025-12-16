# Data Layers 개요

> **작성일**: 2025-12-16
> **엔진 버전**: Unreal Engine 5.7

## Data Layers란?

Data Layers는 **World Partition과 함께 사용되는 레이어 기반 액터 그룹화 및 스트리밍 제어 시스템**입니다. 액터를 논리적 레이어로 분류하고, 런타임에 동적으로 로드/언로드 및 표시/숨김을 제어할 수 있습니다.

## 핵심 개념

### 1. Runtime State

Data Layer는 세 가지 런타임 상태를 가집니다:

```cpp
// DataLayerInstance.h:25
enum class EDataLayerRuntimeState : uint8
{
    Unloaded,   // 메모리에 없음 (액터 존재하지 않음)
    Loaded,     // 메모리에 로드됨 (액터 숨김 상태)
    Activated   // 메모리에 로드되고 가시화됨
};
```

**State 전환 예시**:
```
Unloaded → Loaded:
  - 액터를 메모리에 로드
  - 하지만 렌더링하지 않음 (SetActorHiddenInGame(true))
  - 용도: 빠른 활성화를 위한 Pre-loading

Loaded → Activated:
  - 액터를 표시 (SetActorHiddenInGame(false))
  - 렌더링 시작
  - Tick 활성화

Activated → Loaded:
  - 액터를 숨김
  - 메모리에는 유지 (빠른 재활성화 가능)

Loaded → Unloaded:
  - 액터를 메모리에서 제거
  - 완전히 파괴
```

### 2. 계층 구조 (Hierarchical Layers)

Data Layers는 Parent-Child 관계를 지원합니다:

```
World
├─ Environment (Parent)
│  ├─ Buildings (Child)
│  ├─ Vegetation (Child)
│  └─ Props (Child)
├─ Gameplay (Parent)
│  ├─ Enemies (Child)
│  └─ Pickups (Child)
└─ Audio (Parent)
   ├─ Ambient (Child)
   └─ Music (Child)
```

**재귀적 상태 변경**:
```cpp
// Parent를 Unloaded로 변경하면 모든 자식도 Unloaded
DataLayerManager->SetDataLayerInstanceRuntimeState(
    EnvironmentLayer,
    EDataLayerRuntimeState::Unloaded,
    true  // bInIsRecursive = true
);
// → Buildings, Vegetation, Props 모두 Unloaded
```

### 3. World Partition 통합

Data Layers는 World Partition의 Cell 스트리밍과 함께 작동합니다:

```
Streaming Decision:

Step 1: World Partition Cell 로딩 결정
  - Player position: (5000, 5000)
  - Loading range: 10000
  - Cell(0,0), Cell(0,1), Cell(1,0) 로드 필요

Step 2: Data Layer 필터링
  For each Actor in Cell:
    Check Actor's Data Layers:
      If ALL layers are Activated → Load Actor
      Else → Skip Actor

Example:
  Actor_1: DataLayers = ["Environment", "Buildings"]
    Environment: Activated ✅
    Buildings: Activated ✅
    → Load Actor_1 ✅

  Actor_2: DataLayers = ["Gameplay", "Enemies"]
    Gameplay: Activated ✅
    Enemies: Unloaded ❌
    → Skip Actor_2 ❌
```

**Boolean Logic Operator** (AND/OR):
```cpp
// WorldPartition.h:88
enum class EWorldPartitionDataLayersLogicOperator : uint8
{
    Or,   // 하나라도 Activated면 로드
    And   // 모두 Activated일 때만 로드 (기본값)
};
```

## 주요 사용 사례

### 1. 동적 환경 변화

**시간대 시스템**:
```
Morning Layer (06:00-12:00)
  ├─ Bright sunlight
  ├─ Morning NPCs
  └─ Open shops

Afternoon Layer (12:00-18:00)
  ├─ Moderate sunlight
  ├─ Afternoon NPCs
  └─ Busy market

Night Layer (18:00-06:00)
  ├─ Moon & street lights
  ├─ Night NPCs
  └─ Closed shops
```

**구현**:
```cpp
void ATimeManager::UpdateTimeOfDay(float CurrentTime)
{
    UDataLayerSubsystem* Subsystem = GetWorld()->GetSubsystem<UDataLayerSubsystem>();

    if (CurrentTime >= 6.0f && CurrentTime < 12.0f)
    {
        // Morning
        Subsystem->SetDataLayerRuntimeState(MorningLayer, EDataLayerRuntimeState::Activated);
        Subsystem->SetDataLayerRuntimeState(AfternoonLayer, EDataLayerRuntimeState::Unloaded);
        Subsystem->SetDataLayerRuntimeState(NightLayer, EDataLayerRuntimeState::Unloaded);
    }
    // ... 다른 시간대
}
```

### 2. 퀘스트 시스템

```
Quest_01_Initial (퀘스트 시작 전)
  └─ NPC standing idle

Quest_01_Active (퀘스트 진행 중)
  ├─ Quest giver NPC (different dialogue)
  ├─ Quest objective markers
  └─ Enemy spawns

Quest_01_Complete (퀘스트 완료 후)
  ├─ Reward chest
  └─ Environment changes (destroyed wall, etc.)
```

### 3. 난이도별 콘텐츠

```
Easy_Difficulty
  └─ Fewer enemies, more health pickups

Normal_Difficulty
  └─ Standard enemies

Hard_Difficulty
  └─ More enemies, less resources

Nightmare_Difficulty
  └─ Elite enemies, no hints
```

### 4. 플랫폼별 최적화

```
High_Quality_Layer (PC, High-end console)
  ├─ Extra vegetation
  ├─ Detailed props
  └─ Particle effects

Low_Quality_Layer (Mobile, Low-end console)
  ├─ Simplified meshes
  └─ Reduced particles
```

## UE4 Layers와의 차이점

| 특징 | UE4 Layers | UE5 Data Layers |
|------|-----------|-----------------|
| **주요 용도** | 에디터 가시성 제어 | 런타임 스트리밍 제어 |
| **런타임 변경** | ❌ 불가능 | ✅ 동적 변경 가능 |
| **계층 구조** | ❌ Flat 구조 | ✅ Parent-Child 지원 |
| **World Partition** | ❌ 통합 없음 | ✅ 완전 통합 |
| **네트워크** | ❌ | ✅ 리플리케이션 |
| **초기 상태** | ❌ | ✅ InitialRuntimeState 설정 |
| **Blueprint 지원** | ⚠️ 제한적 | ✅ 완전 지원 |
| **Load Filter** | ❌ | ✅ Client/Server 분리 |

## Data Layer 구성 요소

### 1. UDataLayerAsset

Data Layer의 정의 (에셋):

```cpp
// DataLayerAsset.h
UCLASS(BlueprintType, MinimalAPI)
class UDataLayerAsset : public UObject
{
    // Data Layer 타입
    UPROPERTY(EditAnywhere, Category = DataLayer)
    EDataLayerType Type;  // Runtime or Editor

    // 디버그 색상
    UPROPERTY(EditAnywhere, Category = DataLayer)
    FColor DebugColor;
};
```

### 2. UDataLayerInstance

월드 내 Data Layer 인스턴스:

```cpp
// DataLayerInstance.h:61
UCLASS(MinimalAPI)
class UDataLayerInstance : public UObject
{
    // Data Layer Asset 참조
    UPROPERTY()
    TObjectPtr<UDataLayerAsset> DataLayerAsset;

    // Parent Layer
    UPROPERTY()
    TObjectPtr<UDataLayerInstance> Parent;

    // 초기 런타임 상태
    UPROPERTY(EditAnywhere, Category = "Data Layer|Runtime")
    EDataLayerRuntimeState InitialRuntimeState;

    // Slow Streaming 차단 옵션
    UPROPERTY(EditAnywhere, Category = "Data Layer|Advanced|Runtime")
    EOverrideBlockOnSlowStreaming OverrideBlockOnSlowStreaming;
};
```

### 3. UDataLayerManager

Data Layer 관리 및 제어:

```cpp
// DataLayerManager.h:47
UCLASS(Within = WorldPartition)
class UDataLayerManager : public UObject
{
    // Runtime 상태 변경
    UFUNCTION(BlueprintCallable)
    bool SetDataLayerInstanceRuntimeState(
        const UDataLayerInstance* InDataLayerInstance,
        EDataLayerRuntimeState InState,
        bool bInIsRecursive = false
    );

    // 현재 상태 조회
    UFUNCTION(BlueprintCallable)
    EDataLayerRuntimeState GetDataLayerInstanceRuntimeState(
        const UDataLayerInstance* InDataLayerInstance
    ) const;
};
```

## Actor에 Data Layer 할당

### 에디터에서

1. Actor 선택
2. Details 패널 → Data Layers
3. "+" 버튼으로 Data Layer 추가

### C++에서

```cpp
// Actor 생성 시
AActor* SpawnedActor = GetWorld()->SpawnActor<AActor>(ActorClass, Transform);

// Data Layer 설정
FActorDataLayer DataLayer;
DataLayer.Name = FName("Environment");
SpawnedActor->DataLayers.Add(DataLayer);
```

### Blueprint에서

```
Spawn Actor from Class
  ↓
Set Actor Data Layers
  - Target: SpawnedActor
  - Data Layers: ["Environment", "Buildings"]
```

## Load Filter (Client/Server 분리)

Data Layer는 Client/Server에서 독립적으로 제어할 수 있습니다:

```cpp
// DataLayerAsset.h
enum class EDataLayerLoadFilter : uint8
{
    None,            // 제한 없음 (서버에서 제어)
    ClientOnly,      // 클라이언트에서만 로드
    ServerOnly       // 서버에서만 로드
};
```

**사용 예**:
```
Audio_Layer: ClientOnly
  └─ 클라이언트에서만 사운드 재생, 서버는 불필요

AI_Perception_Layer: ServerOnly
  └─ AI 계산은 서버에서만, 클라이언트 불필요

Gameplay_Layer: None
  └─ 서버에서 제어, 클라이언트에 리플리케이션
```

## 성능 고려사항

### 메모리 관리

```
State: Unloaded
  Memory: 0 bytes
  CPU: 0%

State: Loaded (Hidden)
  Memory: Full actor data
  CPU: ~5% (physics, minimal tick)

State: Activated
  Memory: Full actor data
  CPU: 100% (rendering, full tick)
```

**권장 사항**:
- 자주 전환되는 레이어: Loaded ↔ Activated (빠름)
- 드물게 사용되는 레이어: Unloaded ↔ Activated (메모리 절약)

### Streaming 성능

```
Bad: 모든 레이어를 동시에 Activate
  → 스파이크 발생, 프레임 드랍

Good: 레이어를 점진적으로 Activate
  Frame 1: Essential Layer → Activated
  Frame 10: Decoration Layer → Loaded
  Frame 20: Decoration Layer → Activated
```

## 다음 단계

Data Layers의 개념을 이해했다면 다음 문서로 진행하세요:

1. **[02-CoreClasses.md](02-CoreClasses.md)** - 클래스 구현 상세 분석
2. **[03-RuntimeControl.md](03-RuntimeControl.md)** - 런타임 제어 방법
3. **[04-WorldPartitionIntegration.md](04-WorldPartitionIntegration.md)** - World Partition과의 통합
4. **[05-PracticalGuide.md](05-PracticalGuide.md)** - 실전 활용 가이드

## 참고 자료

- **공식 문서**: [Data Layers](https://docs.unrealengine.com/5.7/en-US/world-partition-data-layers-in-unreal-engine/)
- **소스 코드**: `Engine/Source/Runtime/Engine/Public/WorldPartition/DataLayer/`
