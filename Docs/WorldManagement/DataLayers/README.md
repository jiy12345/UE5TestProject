# Data Layers 시스템 분석

> **작성일**: 2025-12-16
> **엔진 버전**: Unreal Engine 5.7
> **분석 기준**: 엔진 소스 코드

## 개요

Data Layers는 **World Partition과 함께 사용되는 레이어 기반 스트리밍 제어 시스템**입니다. 액터를 논리적 레이어로 그룹화하고, 런타임에 동적으로 활성화/비활성화할 수 있습니다.

## 목차

1. [01-Overview.md](01-Overview.md) - Data Layers 개념 소개
   - Data Layers란?
   - 사용 사례 및 장점
   - UE4 Layer와의 차이점

2. [02-CoreClasses.md](02-CoreClasses.md) - 핵심 클래스 분석
   - UDataLayerInstance
   - UDataLayerManager
   - UDataLayerAsset
   - Runtime State 관리

3. [03-RuntimeControl.md](03-RuntimeControl.md) - 런타임 제어
   - Runtime State 전환
   - Client/Server 동기화
   - Load Filter 시스템

4. [04-WorldPartitionIntegration.md](04-WorldPartitionIntegration.md) - World Partition 통합
   - Streaming과의 관계
   - Actor 필터링 메커니즘
   - 계층 구조

5. [05-PracticalGuide.md](05-PracticalGuide.md) - 실전 활용 가이드
   - Data Layer 설정
   - Blueprint/C++ 제어
   - 디버깅 방법

6. [References.md](References.md) - 참고 자료

## 주요 개념 빠른 참조

### EDataLayerRuntimeState

Data Layer의 세 가지 상태:

```cpp
// DataLayerInstance.h:25
enum class EDataLayerRuntimeState : uint8
{
    Unloaded,   // 메모리에 없음
    Loaded,     // 메모리에 로드됨 (보이지 않음)
    Activated   // 메모리에 로드되고 가시화됨
};
```

### UDataLayerInstance (DataLayerInstance.h:61)

개별 Data Layer를 나타내는 클래스:

```cpp
UCLASS(Config = Engine, BlueprintType, MinimalAPI)
class UDataLayerInstance : public UObject
{
    // Parent-Child 계층 구조
    UPROPERTY()
    TObjectPtr<UDataLayerInstance> Parent;

    UPROPERTY()
    TArray<TObjectPtr<UDataLayerInstance>> Children;

    // 초기 상태
    UPROPERTY(EditAnywhere, Category = "Data Layer|Runtime")
    EDataLayerRuntimeState InitialRuntimeState;
};
```

### UDataLayerManager (DataLayerManager.h:47)

Data Layer의 런타임 상태를 관리:

```cpp
UCLASS(Config = Engine, Within = WorldPartition, MinimalAPI)
class UDataLayerManager : public UObject
{
    // Runtime 상태 변경
    UFUNCTION(BlueprintCallable, Category = DataLayers)
    bool SetDataLayerInstanceRuntimeState(
        const UDataLayerInstance* InDataLayerInstance,
        EDataLayerRuntimeState InState,
        bool bInIsRecursive = false
    );
};
```

## 사용 시나리오

### 1. 시간대별 콘텐츠

```
Day Layer (Activated)
  ├─→ NPCs walking
  ├─→ Market stalls open
  └─→ Bright lighting

Night Layer (Activated)
  ├─→ NPCs sleeping
  ├─→ Market stalls closed
  └─→ Street lights on
```

### 2. 게임 진행 단계

```
Quest01_Layer (Activated when quest starts)
  ├─→ Quest NPCs spawn
  ├─→ Quest objects visible
  └─→ Quest-specific enemies

Quest01_Complete_Layer (Activated when quest completes)
  ├─→ Reward chest
  └─→ Changed environment
```

### 3. 성능 최적화

```
High_Detail_Layer (PC only)
  └─→ Extra decorative objects

Low_Detail_Layer (Console/Mobile)
  └─→ Simplified versions
```

## 소스 코드 위치

### Runtime 코드
- `Engine/Source/Runtime/Engine/Public/WorldPartition/DataLayer/`
  - `DataLayerInstance.h` - Data Layer 인스턴스
  - `DataLayerManager.h` - 관리자 클래스
  - `DataLayerSubsystem.h` - World Subsystem
  - `WorldDataLayers.h` - Container Actor

### Editor 코드
- `Engine/Source/Editor/UnrealEd/Public/WorldPartition/DataLayer/`
- `Engine/Source/Editor/DataLayerEditor/` - Data Layer 에디터 UI

## 간단한 사용 예제

### C++에서 Data Layer 제어

```cpp
#include "WorldPartition/DataLayer/DataLayerSubsystem.h"

void AMyGameMode::ActivateNightMode()
{
    UDataLayerSubsystem* Subsystem =
        GetWorld()->GetSubsystem<UDataLayerSubsystem>();

    if (Subsystem)
    {
        // Day Layer 비활성화
        UDataLayerAsset* DayLayer =
            LoadObject<UDataLayerAsset>(nullptr, TEXT("/Game/DataLayers/Day"));
        Subsystem->SetDataLayerRuntimeState(
            DayLayer,
            EDataLayerRuntimeState::Unloaded
        );

        // Night Layer 활성화
        UDataLayerAsset* NightLayer =
            LoadObject<UDataLayerAsset>(nullptr, TEXT("/Game/DataLayers/Night"));
        Subsystem->SetDataLayerRuntimeState(
            NightLayer,
            EDataLayerRuntimeState::Activated
        );
    }
}
```

### Blueprint에서 Data Layer 제어

```
Event BeginPlay
  ↓
Get Data Layer Manager
  ↓
Set Data Layer Runtime State
  - Data Layer Asset: "Night"
  - New State: Activated
  - Is Recursive: true
```

## Data Layers vs UE4 Layers

| 특징 | UE4 Layers | UE5 Data Layers |
|------|-----------|-----------------|
| 용도 | 에디터 가시성 제어 | 런타임 스트리밍 제어 |
| 런타임 | ❌ 런타임 변경 불가 | ✅ 런타임 동적 변경 |
| 계층 구조 | ❌ Flat | ✅ Parent-Child |
| World Partition | ❌ 통합 없음 | ✅ 완전 통합 |
| 네트워크 | ❌ | ✅ 리플리케이션 지원 |

## 주요 특징

✅ **런타임 제어**: 게임 중 동적으로 활성화/비활성화
✅ **계층 구조**: Parent-Child 관계로 복잡한 레이어 구성
✅ **World Partition 통합**: 스트리밍과 함께 작동
✅ **클라이언트/서버 분리**: Load Filter로 클라이언트 전용, 서버 전용 설정
✅ **Blueprint 지원**: 코드 없이도 제어 가능
✅ **초기 상태 설정**: 게임 시작 시 자동 로드

## 학습 순서

1. **기초**: [01-Overview.md](01-Overview.md)로 개념 이해
2. **구현**: [02-CoreClasses.md](02-CoreClasses.md)로 클래스 분석
3. **제어**: [03-RuntimeControl.md](03-RuntimeControl.md)로 런타임 제어 학습
4. **통합**: [04-WorldPartitionIntegration.md](04-WorldPartitionIntegration.md)로 World Partition과의 관계 파악
5. **실전**: [05-PracticalGuide.md](05-PracticalGuide.md)로 활용법 습득

## 시작하기

Data Layers를 처음 접한다면 [01-Overview.md](01-Overview.md)부터 시작하세요.

## 관련 시스템

- [World Partition](../WorldPartition/) - Data Layers와 함께 사용되는 스트리밍 시스템
- Level Streaming - 기존 스트리밍 방식 (Data Layers가 더 유연함)
