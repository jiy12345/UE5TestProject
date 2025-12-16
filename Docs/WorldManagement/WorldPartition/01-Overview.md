# World Partition 개요

> **작성일**: 2025-12-16
> **엔진 버전**: Unreal Engine 5.7

## World Partition이란?

World Partition은 언리얼 엔진 5에서 도입된 **대규모 오픈 월드를 위한 자동화된 스트리밍 시스템**입니다. 기존의 수동 Level Streaming 방식을 대체하며, 공간 기반의 자동 셀 분할과 스트리밍을 제공합니다.

### 핵심 개념

```
Traditional Level Streaming          World Partition
┌────────────────────────┐          ┌────────────────────────┐
│   Level_A (manual)     │          │   Automatic Grid       │
│   ┌─────┬─────┐        │          │  ┌──┬──┬──┬──┐        │
│   │ Map │ Map │        │          │  │A1│A2│A3│A4│        │
│   ├─────┼─────┤        │          │  ├──┼──┼──┼──┤        │
│   │ Map │ Map │        │    →     │  │B1│B2│B3│B4│        │
│   └─────┴─────┘        │          │  ├──┼──┼──┼──┤        │
│   Level_B (manual)     │          │  │C1│C2│C3│C4│        │
│   ┌─────┬─────┐        │          │  └──┴──┴──┴──┘        │
│   │ Map │ Map │        │          │   (Runtime Hash Grid)  │
│   └─────┴─────┘        │          └────────────────────────┘
└────────────────────────┘
   수동 관리 필요                      자동 관리
   레벨 경계 명확                      연속적인 월드
   액터 이동 제한                      자유로운 액터 이동
```

## 주요 특징

### 1. 자동 셀 분할 (Automatic Cell Division)

**기존 방식 (Level Streaming)**:
```cpp
// 개발자가 수동으로 레벨을 분할하고 관리
LevelA.umap: { Actor1, Actor2, Actor3 }
LevelB.umap: { Actor4, Actor5, Actor6 }
// 레벨 경계를 넘으면 액터 이동 복잡
```

**World Partition**:
```cpp
// 엔진이 자동으로 Grid로 분할
WorldPartition:
  RuntimeHash: Spatial Hash Grid (100m x 100m)
  Cell(0,0): { Actor1, Actor2 }
  Cell(0,1): { Actor3 }
  Cell(1,0): { Actor4, Actor5 }
  // 액터는 자유롭게 이동 가능, 자동으로 Cell 업데이트
```

**소스 코드 참조**:
- `Engine/Source/Runtime/Engine/Public/WorldPartition/WorldPartitionRuntimeSpatialHash.h`
- Grid 크기, Cell 분할 로직

### 2. 공간 기반 스트리밍 (Spatial Streaming)

**Streaming Source** (보통 플레이어):
```cpp
// WorldPartitionStreamingSource.h:37
USTRUCT(BlueprintType)
struct FStreamingSourceShape
{
    // 플레이어 주변 로딩 범위
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = StreamingSource)
    float LoadingRange = 20000.0f;  // 20km

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = StreamingSource)
    FName Name;
};
```

**자동 로딩/언로딩**:
```
Player Position: (5000, 5000)
Loading Range: 10000

Loaded Cells:
┌────┬────┬────┐
│    │Load│    │  ← Player 주변 Cell 자동 로드
├────┼────┼────┤
│Load│P   │Load│  ← P: Player
├────┼────┼────┤
│    │Load│    │
└────┴────┴────┘

Unload distant cells automatically
```

### 3. Actor Descriptor (경량 메타데이터)

**문제**: 대규모 월드에서 모든 액터를 메모리에 올리면 메모리 부족
**해결**: Actor Descriptor - 액터의 경량 메타데이터

```cpp
// WorldPartitionActorDesc.h:24
struct FWorldPartitionActorDescInitData
{
    UClass* NativeClass;      // 액터 타입
    FName PackageName;        // 패키지 경로
    FSoftObjectPath ActorPath; // 액터 참조
    TArray<uint8> SerializedData; // 직렬화된 데이터
};

// WorldPartitionActorDesc.h:70
struct FWorldPartitionRelativeBounds
{
    FVector Center;   // 중심 좌표
    FQuat Rotation;   // 회전
    FVector Extents;  // 범위
    bool bIsValid;
};
```

**메모리 효율**:
```
Traditional:
  Actor (Full Object): ~10 KB/actor
  100,000 actors = 1 GB

World Partition:
  Actor Descriptor: ~100 bytes/actor
  100,000 actors = 10 MB  ← 100배 절약!

  실제 Actor는 필요할 때만 로드
```

## Level Streaming과의 비교

### Level Streaming (UE4 방식)

**장점**:
- 명확한 레벨 경계
- 수동 제어 가능
- 작은 프로젝트에 적합

**단점**:
- ❌ 수동 분할 및 관리 필요
- ❌ 레벨 경계를 넘는 액터 이동 복잡
- ❌ 대규모 월드에서 관리 어려움
- ❌ 팀 협업 시 충돌 가능성 높음

### World Partition (UE5 방식)

**장점**:
- ✅ 완전 자동화된 스트리밍
- ✅ 연속적인 오픈 월드
- ✅ 액터가 자유롭게 이동 가능
- ✅ One File Per Actor (OFPA) - 협업 용이
- ✅ 자동 HLOD 생성
- ✅ Data Layers 통합

**단점**:
- ⚠️ 복잡한 설정 (처음 학습 곡선)
- ⚠️ UE5 이상 필요
- ⚠️ 기존 Level Streaming 프로젝트 마이그레이션 필요

## 언제 사용해야 하는가?

### World Partition 사용 권장

✅ **대규모 오픈 월드 게임**
- 맵 크기: 5km² 이상
- 예: 오픈월드 RPG, 샌드박스 게임

✅ **동적 스트리밍이 필요한 경우**
- 플레이어가 자유롭게 이동
- 예: 비행, 차량 운전

✅ **팀 협업 프로젝트**
- 여러 개발자가 동시에 작업
- One File Per Actor로 Git 충돌 최소화

### Level Streaming 사용 권장

✅ **작은 규모의 게임**
- 맵 크기: 1km² 미만
- 명확한 구역 분리 (던전, 방 단위)

✅ **선형 레벨 디자인**
- 명확한 진행 순서
- 예: 선형 액션 게임, 퍼즐 게임

✅ **UE4 프로젝트**
- 기존 Level Streaming 프로젝트 유지

## 주요 구성 요소

### 1. UWorldPartition
- **파일**: `WorldPartition.h:141`
- 월드의 모든 액터를 관리하는 메인 클래스
- Actor Descriptor 컨테이너

### 2. Runtime Hash
- **파일**: `WorldPartitionRuntimeHash.h:59`
- 공간을 Grid로 분할
- Streaming Cell 생성 및 관리

### 3. Streaming Source
- **파일**: `WorldPartitionStreamingSource.h`
- 스트리밍의 중심점 (보통 플레이어)
- 로딩 범위 정의

### 4. Runtime Cell
- **파일**: `WorldPartitionRuntimeCell.h`
- Grid의 각 셀
- Loaded/Unloaded 상태 관리

### 5. Data Layers
- **파일**: `DataLayer/DataLayerInstance.h`
- 레이어 기반 스트리밍 제어
- Runtime 활성화/비활성화

## 간단한 예제

### C++에서 Data Layer 제어

```cpp
#include "WorldPartition/DataLayer/DataLayerSubsystem.h"
#include "WorldPartition/DataLayer/DataLayerAsset.h"

void AMyGameMode::LoadEnvironmentLayer()
{
    UDataLayerSubsystem* DataLayerSubsystem =
        GetWorld()->GetSubsystem<UDataLayerSubsystem>();

    if (DataLayerSubsystem)
    {
        // Data Layer 활성화
        UDataLayerAsset* EnvironmentLayer =
            LoadObject<UDataLayerAsset>(nullptr, TEXT("/Game/DataLayers/Environment"));

        DataLayerSubsystem->SetDataLayerRuntimeState(
            EnvironmentLayer,
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
  - Data Layer: "Environment"
  - State: Activated
  - Recursive: true
```

## 다음 단계

World Partition의 전체 개념을 이해했다면, 다음 문서로 진행하세요:

1. **[02-Architecture.md](02-Architecture.md)** - 시스템 아키텍처 이해
2. **[03-CoreClasses.md](03-CoreClasses.md)** - 핵심 클래스 분석
3. **[04-StreamingSystem.md](04-StreamingSystem.md)** - 스트리밍 메커니즘 심화

## 참고 자료

- **공식 문서**: [World Partition Overview](https://docs.unrealengine.com/5.7/en-US/world-partition-in-unreal-engine/)
- **소스 코드**: `Engine/Source/Runtime/Engine/Public/WorldPartition/`
- **샘플**: `Engine/Content/WorldPartition/` (엔진 샘플 맵)
