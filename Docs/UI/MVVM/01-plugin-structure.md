# 플러그인 구조 분석

## 모듈 구성

ModelViewViewModel 플러그인은 총 **6개의 모듈**로 구성됩니다:

### 1. ModelViewViewModel (Runtime)
**역할**: 핵심 런타임 모듈
**타입**: Runtime
**의존성**:
- Public: Core, CoreUObject, Engine, **FieldNotification**
- Private: SlateCore, Slate, UMG

### 2. ModelViewViewModelBlueprint (UncookedOnly)
**역할**: 블루프린트 컴파일 지원
**타입**: UncookedOnly
**기능**: Blueprint에서 MVVM을 사용할 수 있도록 컴파일러 확장

### 3. ModelViewViewModelEditor (Editor)
**역할**: 에디터 통합
**타입**: Editor
**기능**: UMG Designer에서 MVVM 바인딩 설정 UI 제공

### 4. ModelViewViewModelDebugger (UncookedOnly)
**역할**: 디버깅 기능
**타입**: UncookedOnly
**기능**: 런타임 바인딩 추적 및 디버깅 지원

### 5. ModelViewViewModelDebuggerEditor (Editor)
**역할**: 디버거 에디터 UI
**타입**: Editor
**기능**: 디버거 시각화 및 인터페이스

### 6. ModelViewViewModelAssetSearch
**역할**: 에셋 검색 기능
**기능**: MVVM 관련 에셋 검색 및 참조 추적

---

## 핵심 클래스 계층 구조

```
UObject
    └─ UMVVMViewModelBase
        ├─ INotifyFieldValueChanged (Interface)
        └─ UE::MVVM::FMVVMFieldNotificationDelegates (Member)

UUserWidgetExtension
    └─ UMVVMView
        ├─ UMVVMViewClass* GeneratedViewClass (Shared Data)
        ├─ TArray<FMVVMView_Source> Sources (Instance Data)
        └─ TArray<FBoundEvent> BoundEvents

UObject
    └─ UMVVMViewClass
        ├─ FMVVMCompiledBindingLibrary BindingLibrary
        ├─ TArray<FMVVMViewClass_Source> Sources
        ├─ TArray<FMVVMViewClass_Binding> Bindings
        └─ TArray<FMVVMViewClass_Event> Events

UEngineSubsystem
    ├─ UMVVMSubsystem (Engine-level utilities)
    └─ UMVVMBindingSubsystem (Delayed/Tick binding management)

UGameInstanceSubsystem
    └─ UMVVMGameSubsystem (Global ViewModel collection)
```

---

## Public API 파일 구조

### Root Public Directory

| 파일 | 역할 |
|------|------|
| `MVVMViewModelBase.h` | ViewModel 기본 클래스, 핵심 매크로 정의 |
| `MVVMSubsystem.h` | Engine 서브시스템 (유틸리티 함수) |
| `MVVMGameSubsystem.h` | GameInstance 서브시스템 (Global Collection) |
| `MVVMBlueprintLibrary.h` | Blueprint 노출 함수 라이브러리 |
| `MVVMMessageLog.h` | 로깅 유틸리티 |

### View/ 디렉토리

| 파일 | 역할 |
|------|------|
| `MVVMView.h` | View 인스턴스 (UserWidget Extension) |
| `MVVMViewClass.h` | View 공유 메타데이터 (컴파일 결과) |
| `MVVMViewTypes.h` | View 관련 Key 타입 정의 |
| `MVVMViewModelContextResolver.h` | ViewModel 생성 커스터마이징 |

### ViewModel/ 디렉토리

| 파일 | 역할 |
|------|------|
| `MVVMFieldNotificationDelegates.h` | Field Notify 델리게이트 구현 |
| `MVVMInstancedViewModelGeneratedClass.h` | Instanced ViewModel 지원 |

### Bindings/ 디렉토리

| 파일 | 역할 |
|------|------|
| `MVVMCompiledBindingLibrary.h` | 컴파일된 바인딩 실행 라이브러리 |
| `MVVMBindingHelper.h` | 바인딩 헬퍼 함수 |
| `MVVMFieldPathHelper.h` | Field 경로 평가 헬퍼 |
| `MVVMConversionLibrary.h` | Conversion Function 기본 클래스 |

### Types/ 디렉토리

| 파일 | 역할 |
|------|------|
| `MVVMFieldVariant.h` | FProperty 또는 UFunction Variant |
| `MVVMBindingMode.h` | OneTime/OneWay/TwoWay/ToSource |
| `MVVMExecutionMode.h` | Immediate/Delayed/Tick/Auto |
| `MVVMAvailableBinding.h` | 바인딩 가능 필드 정보 |
| `MVVMViewModelContext.h` | ViewModel 컨텍스트 |
| `MVVMViewModelCollection.h` | ViewModel 컬렉션 |
| `MVVMObjectVariant.h` | UObject/UClass/UStruct Variant |
| `MVVMFieldContext.h` | Field 평가 컨텍스트 |
| `MVVMFunctionContext.h` | Function 실행 컨텍스트 |

### Extensions/ 디렉토리

| 파일 | 역할 |
|------|------|
| `MVVMViewClassExtension.h` | View 확장 기본 클래스 |
| `MVVMViewListViewBaseExtension.h` | ListView 특화 확장 |
| `MVVMViewPanelWidgetExtension.h` | Panel Widget 특화 확장 |

---

## Private API 구조

### Private 디렉토리

| 파일 | 역할 |
|------|------|
| `ModelViewViewModelModule.h` | 모듈 정의 및 초기화 |
| `View/MVVMBindingSubsystem.h` | Delayed/Tick 바인딩 관리 Subsystem |

---

## 핵심 구조체

### FMVVMViewClass_Source
Source(ViewModel 또는 Widget) 정의:
```cpp
struct FMVVMViewClass_Source
{
    FName Name;                                // Source 이름
    EMVVMBlueprintFieldPath FieldPath;         // 생성 경로
    TArray<FMVVMViewClass_FieldId> FieldIds;   // Field ID 목록
    TArray<FMVVMViewClass_SourceBinding> Bindings;  // 바인딩 목록
    uint32 Flags;                              // Optional, CanBeSet, HasTickBindings 등
};
```

### FMVVMViewClass_Binding
바인딩 정의:
```cpp
struct FMVVMViewClass_Binding
{
    FMVVMVCompiledBinding Binding;             // 컴파일된 바인딩
    EMVVMExecutionMode ExecuteMode;            // Immediate/Delayed/Tick
    uint64 Sources;                            // 필요한 Source 비트마스크
    bool bOneTime;                             // 한 번만 실행
    bool bTwoWay;                              // 양방향 바인딩
};
```

### FMVVMVCompiledBinding
컴파일된 바인딩 실행 정보:
```cpp
struct FMVVMVCompiledBinding
{
    FMVVMVCompiledFieldPath SourceFieldPath;        // Source 경로
    FMVVMVCompiledFieldPath DestinationFieldPath;   // Destination 경로
    FMVVMVCompiledFieldPath ConversionFunctionPath; // Conversion 함수
    EMVVMBindingType BindingType;                   // Simple/Complex/Runtime
};
```

### FMVVMVCompiledFieldPath
컴파일된 필드 경로:
```cpp
struct FMVVMVCompiledFieldPath
{
    int16 StartingPropertyIndex;               // 시작 Property 인덱스
    TArray<int16> SegmentIndices;              // 경로 세그먼트 인덱스
    bool bIsProperty;                          // Property vs Function
};
```

---

## 주요 열거형

### EMVVMBindingMode
```cpp
enum class EMVVMBindingMode : uint8
{
    OneTimeToDestination,  // 초기화 시 한 번만
    OneWayToDestination,   // Source → Destination
    OneWayToSource,        // Destination → Source
    TwoWay,                // 양방향
    OneTime,               // Deprecated
};
```

### EMVVMExecutionMode
```cpp
enum class EMVVMExecutionMode : uint8
{
    Immediate,  // 즉시 실행
    Delayed,    // Queue에 추가 후 실행
    Tick,       // 매 프레임 실행
    Auto,       // 자동 선택
};
```

---

## Extension 시스템

### UMVVMViewClassExtension Lifecycle Hooks
```cpp
class UMVVMViewClassExtension
{
    virtual void ViewConstructed(UMVVMView* View);
    virtual void OnSourcesInitialized(UMVVMView* View);
    virtual void OnBindingsInitialized(UMVVMView* View);
    virtual void OnEventsInitialized(UMVVMView* View);
    virtual void OnSourcesUninitialized(UMVMView* View);
    virtual void OnBindingsUninitialized(UMVVMView* View);
    virtual void OnEventsUninitialized(UMVVMView* View);
    virtual void OnViewDestructed(UMVVMView* View);
};
```

**사용 예시**:
- `UMVVMViewListViewBaseExtension`: ListView의 아이템별 ViewModel 관리
- `UMVVMViewPanelWidgetExtension`: Panel Widget의 동적 자식 Widget 바인딩

---

## 아키텍처 원칙

### 1. Compiled vs Runtime 분리
- **ViewClass** (컴파일 타임, 모든 인스턴스가 공유)
  - 바인딩 메타데이터
  - 컴파일된 경로
  - Source 정의

- **View** (런타임, 인스턴스별)
  - Source 인스턴스
  - 등록된 델리게이트
  - 런타임 상태

### 2. Field Path 시스템
- **컴파일 타임**: Property/Function 경로를 인덱스로 변환
- **런타임**: 인덱스를 통해 캐시된 FProperty/UFunction에 O(1) 접근

### 3. Variant 타입 활용
- **FMVVMFieldVariant**: Property 또는 Function 저장
- **FMVVMObjectVariant**: Object, Class, Struct 저장
- 타입 안전성과 유연성 동시 확보

### 4. Subsystem 분리
- **UMVVMSubsystem** (Engine): 유틸리티 함수, 바인딩 유효성 검증
- **UMVVMGameSubsystem** (GameInstance): Global ViewModel Collection
- **UMVVMBindingSubsystem** (Engine, Private): Delayed/Tick 바인딩 실행 관리

---

## 성능 최적화 설계

### 1. 비트마스크 Source 검증
```cpp
uint64 ValidSources = 0b00001111;  // 4개 Source 모두 유효
uint64 BindingSources = 0b00000011; // Binding이 Source 0, 1 필요

if ((BindingSources & ValidSources) == BindingSources)
    // O(1) 시간에 검증 완료
```

### 2. Property 캐싱
```cpp
// 컴파일 타임: 경로 인덱싱
"ViewModel.Health" → { StartingIndex: 5, SegmentIndices: [12, 34] }

// 런타임: O(1) 접근
FProperty* HealthProp = CachedProperties[34];
```

### 3. Delayed Batching
여러 Field 변경을 Queue에 모아서 한 번에 처리 → SetTimer 오버헤드 감소

### 4. Tick 바인딩 최소화
Tick 바인딩이 있는 View만 별도 추적 → 불필요한 순회 방지

---

## 다음: [Field Notify 메커니즘](02-field-notify-mechanism.md)
