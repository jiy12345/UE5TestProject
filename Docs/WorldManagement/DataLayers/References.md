# Data Layers 참고 자료

> **작성일**: 2025-12-16
> **엔진 버전**: Unreal Engine 5.7

## 엔진 소스 코드

### Runtime 핵심 파일

**Data Layer 시스템**:
- `Engine/Source/Runtime/Engine/Public/WorldPartition/DataLayer/DataLayerInstance.h` - UDataLayerInstance 클래스
- `Engine/Source/Runtime/Engine/Public/WorldPartition/DataLayer/DataLayerAsset.h` - UDataLayerAsset 클래스
- `Engine/Source/Runtime/Engine/Public/WorldPartition/DataLayer/DataLayerManager.h` - UDataLayerManager 클래스
- `Engine/Source/Runtime/Engine/Public/WorldPartition/DataLayer/DataLayerSubsystem.h` - UDataLayerSubsystem 클래스

**Actor Integration**:
- `Engine/Source/Runtime/Engine/Public/WorldPartition/DataLayer/ActorDataLayer.h` - FActorDataLayer 구조체
- `Engine/Source/Runtime/Engine/Public/WorldPartition/DataLayer/WorldDataLayers.h` - AWorldDataLayers 액터

**External Data Layers**:
- `Engine/Source/Runtime/Engine/Public/WorldPartition/DataLayer/ExternalDataLayerAsset.h`
- `Engine/Source/Runtime/Engine/Public/WorldPartition/DataLayer/ExternalDataLayerInstance.h`
- `Engine/Source/Runtime/Engine/Public/WorldPartition/DataLayer/ExternalDataLayerManager.h`

**Loading Policy**:
- `Engine/Source/Runtime/Engine/Public/WorldPartition/DataLayer/DataLayerLoadingPolicy.h`

**Utilities**:
- `Engine/Source/Runtime/Engine/Public/WorldPartition/DataLayer/DataLayerUtils.h`
- `Engine/Source/Runtime/Engine/Public/WorldPartition/DataLayer/DataLayerType.h`

### Editor 파일

**에디터 모듈**:
- `Engine/Source/Editor/DataLayerEditor/` - 전체 Data Layer 에디터
- `Engine/Source/Editor/UnrealEd/Public/WorldPartition/DataLayer/` - 에디터 통합

**에디터 Context**:
- `Engine/Source/Runtime/Engine/Public/WorldPartition/DataLayer/DataLayerEditorContext.h`

## 공식 문서

### Unreal Engine 공식 문서
- [Data Layers in World Partition](https://docs.unrealengine.com/5.7/en-US/world-partition-data-layers-in-unreal-engine/)
- [Working with Data Layers](https://docs.unrealengine.com/5.7/en-US/working-with-data-layers/)
- [Data Layer Runtime Control](https://docs.unrealengine.com/5.7/en-US/data-layer-runtime-control-unreal-engine/)

### API Reference
- [UDataLayerInstance API](https://docs.unrealengine.com/5.7/en-US/API/Runtime/Engine/WorldPartition/DataLayer/UDataLayerInstance/)
- [UDataLayerManager API](https://docs.unrealengine.com/5.7/en-US/API/Runtime/Engine/WorldPartition/DataLayer/UDataLayerManager/)
- [UDataLayerSubsystem API](https://docs.unrealengine.com/5.7/en-US/API/Runtime/Engine/WorldPartition/DataLayer/UDataLayerSubsystem/)

## 학습 자료

### Epic Games 공식 비디오
- [Data Layers Quick Start Guide](https://dev.epicgames.com/community/learning/tutorials/)
- [Dynamic World Building with Data Layers](https://www.youtube.com/c/UnrealEngine)

### 커뮤니티 자료
- [Unreal Engine Forums - Data Layers](https://forums.unrealengine.com/tags/c/development-discussion/world-building/16/data-layers)
- [Data Layers Best Practices](https://forums.unrealengine.com/)

## 샘플 프로젝트

### 엔진 내장 샘플
- `Engine/Content/WorldPartition/` - Data Layer 샘플
- Content Examples 프로젝트의 World Partition 예제

### Epic Games 샘플
- Lyra Starter Game - Data Layers 활용 (Day/Night 시스템)
- City Sample - 대규모 Data Layers 사용 예제

## 사용 예제

### C++ 예제

**Data Layer 상태 변경**:
```cpp
#include "WorldPartition/DataLayer/DataLayerSubsystem.h"

void AMyGameMode::ActivateLayer(UDataLayerAsset* Layer)
{
    if (UDataLayerSubsystem* Subsystem = GetWorld()->GetSubsystem<UDataLayerSubsystem>())
    {
        Subsystem->SetDataLayerRuntimeState(Layer, EDataLayerRuntimeState::Activated);
    }
}
```

**Data Layer 쿼리**:
```cpp
#include "WorldPartition/DataLayer/DataLayerManager.h"

bool AMyActor::IsLayerActive(const FName& LayerName) const
{
    if (UDataLayerManager* Manager = UDataLayerManager::GetDataLayerManager(this))
    {
        if (const UDataLayerInstance* Instance = Manager->GetDataLayerInstanceFromName(LayerName))
        {
            EDataLayerRuntimeState State = Manager->GetDataLayerInstanceRuntimeState(Instance);
            return State == EDataLayerRuntimeState::Activated;
        }
    }
    return false;
}
```

### Blueprint 예제

**레이어 활성화**:
```
Event BeginPlay
  ↓
Get Data Layer Subsystem
  ↓
Set Data Layer Runtime State
  - Data Layer: "Environment"
  - State: Activated
```

**델리게이트 바인딩**:
```
Event BeginPlay
  ↓
Get Data Layer Manager
  ↓
Bind Event to OnDataLayerRuntimeStateChanged
  ↓
Custom Event: HandleLayerChanged
  - Data Layer: DataLayerInstance
  - New State: EDataLayerRuntimeState
```

## 관련 시스템 문서

### World Partition
- [../WorldPartition/README.md](../WorldPartition/README.md) - World Partition 시스템

### Level Streaming
- [Level Streaming Documentation](https://docs.unrealengine.com/5.7/en-US/level-streaming-in-unreal-engine/)

## 개발 도구

### 디버깅 명령어

콘솀에서 사용 가능한 명령어:

```
# Data Layer 시각화
wp.Runtime.ToggleDrawDataLayers

# Data Layer 상태 로그
wp.Runtime.DataLayers.LogRuntimeState

# Data Layer 목록 출력
wp.Editor.DumpDataLayers

# 특정 레이어 강제 활성화 (디버그)
wp.Runtime.SetDataLayerRuntimeState [LayerName] [State]
# State: 0=Unloaded, 1=Loaded, 2=Activated
```

### Editor UI

**Data Layers Panel**:
- Window → World Partition → Data Layers
- 레이어 생성, 편집, 계층 구조 관리

**Outliner Integration**:
- World Outliner에서 Data Layer 필터링
- 우클릭 → Data Layers → Assign to Layer

**Details Panel**:
- Actor 선택 시 Data Layers 섹션
- 레이어 추가/제거

### Project Settings

**Project Settings → World Partition → Data Layers**:
- Default Data Layer Type
- Enable Runtime Validation
- Data Layer Editor Settings

## 문제 해결

### 공통 이슈

**이슈**: Data Layer가 활성화되지 않음
- **해결**: Data Layer Type이 Runtime인지 확인
- **해결**: World Partition이 활성화되어 있는지 확인

**이슈**: Actor가 표시되지 않음
- **해결**: 모든 할당된 Data Layer가 Activated 상태인지 확인
- **해결**: Boolean Logic Operator 설정 확인 (AND/OR)

**이슈**: 네트워크에서 동기화 안 됨
- **해결**: Data Layer Load Filter 확인 (Client/Server/None)
- **해결**: 서버에서 상태 변경하는지 확인

**이슈**: 성능 저하
- **해결**: 한 번에 너무 많은 레이어를 활성화하지 않기
- **해결**: Loaded 상태로 Pre-load 후 점진적으로 Activate

### 로그 카테고리

```cpp
// DataLayer 관련 로그
DECLARE_LOG_CATEGORY_EXTERN(LogDataLayer, Log, All);
DECLARE_LOG_CATEGORY_EXTERN(LogWorldPartitionDataLayer, Log, All);
```

**로그 활성화**:
```
# DefaultEngine.ini or Console
[Core.Log]
LogDataLayer=VeryVerbose
LogWorldPartitionDataLayer=Verbose
```

## 베스트 프랙티스

### 레이어 구조 설계

**좋은 예**:
```
Environment (Parent)
  ├─ Buildings
  ├─ Vegetation
  └─ Props

Gameplay (Parent)
  ├─ Enemies
  └─ Objectives
```

**나쁜 예**:
```
Layer1, Layer2, Layer3... (의미 없는 이름)
Everything_In_One_Layer (너무 큰 레이어)
```

### 명명 규칙
- 명확하고 설명적인 이름 사용
- 접두사로 카테고리 표시 (예: `ENV_Buildings`, `GP_Enemies`)
- 소문자와 언더스코어 또는 파스칼케이스 사용

### 성능 최적화
- 자주 전환되는 레이어는 Loaded ↔ Activated 사용
- 큰 레이어는 여러 작은 레이어로 분할
- Load Filter로 불필요한 Client/Server 로딩 방지

## 업데이트 이력

- **UE 5.0**: Data Layers 도입 (실험적 기능)
- **UE 5.1**: Data Layers 안정화, API 개선
- **UE 5.2**: External Data Layers 추가
- **UE 5.3**: Load Filter 기능 추가
- **UE 5.7**: 현재 버전 (안정화, 성능 개선)

## 관련 자료

### 블로그 포스트
- [Epic Games Dev Community - Data Layers](https://dev.epicgames.com/community/)
- [Unreal Engine Blog - World Building](https://www.unrealengine.com/en-US/blog)

### 비디오 튜토리얼
- YouTube - "Unreal Engine 5 Data Layers Tutorial"
- Unreal Online Learning - Data Layers Course

## 기여 및 피드백

이 문서에 대한 피드백이나 추가 자료가 있다면:
- GitHub Issues: [프로젝트 Issue 페이지]
- 직접 수정 후 Pull Request

---

**마지막 업데이트**: 2025-12-16
**문서 버전**: 1.0
