# 전체 라이프사이클

## Widget 생성부터 파괴까지

### 생성 흐름

```
[UUserWidget 생성]
    ↓
UUserWidget::NativeConstruct()
    ↓
UMVVMView::Construct()  (UUserWidgetExtension)
    ↓
1. InitializeSources()
   - ViewModel 인스턴스 생성/설정
   - ValidSources 비트마스크 업데이트
    ↓
2. InitializeBindings()
   - Field Notify 델리게이트 등록
   - ExecuteAtInitialization 바인딩 실행
   - Tick 바인딩 Subsystem 등록
    ↓
3. InitializeEvents()
   - Event 바인딩
    ↓
4. Extension->OnBindingsInitialized()
    ↓
[Widget 사용 준비 완료]
```

---

### 파괴 흐름

```
[UUserWidget 파괴]
    ↓
UUserWidget::NativeDestruct()
    ↓
UMVVMView::Destruct()
    ↓
1. UninitializeEvents()
   - Event 바인딩 해제
    ↓
2. UninitializeBindings()
   - Field Notify 델리게이트 모두 제거
   - Tick 바인딩 Subsystem에서 제거
    ↓
3. UninitializeSources()
   - ViewModel 인스턴스 해제
   - ValidSources 비트마스크 클리어
    ↓
[정리 완료]
```

---

## 핵심 단계 상세

### InitializeSources()

```cpp
for each Source in GeneratedViewClass->GetSources():
    // 1. Resolver를 통해 ViewModel 인스턴스 얻기
    UObject* ViewModel = GetOrCreateViewModelInstance(Source)

    // 2. Source 저장
    Sources[SourceIndex].Source = ViewModel
    Sources[SourceIndex].bSourceInitialized = true

    // 3. ValidSources 비트마스크 업데이트
    ValidSources |= (1 << SourceIndex)

    // 4. Extension 통지
    Extension->OnSourceInitialized(ViewModel)
```

---

### InitializeBindings() - 상세

```cpp
for each Source:
    // 1. INotifyFieldValueChanged 확인
    INotifyFieldValueChanged* NotifyInterface = Cast<INotifyFieldValueChanged>(Source)

    if (NotifyInterface):
        // 2. 각 Field에 델리게이트 등록
        for each FieldId in Source.GetFieldIds():
            FDelegate Callback = CreateUObject(
                this,
                &UMVVMView::HandledLibraryBindingValueChanged,
                SourceKey
            )

            NotifyInterface->AddFieldValueChangedDelegate(FieldId, Callback)
            RegisteredCount++

        // 3. 초기 바인딩 실행
        for each Binding in Source.GetBindings():
            if Binding.ExecuteAtInitialization():
                ExecuteBindingImmediately(Binding)

    // 4. Tick 바인딩 관리
    if Source.HasTickBindings():
        NumberOfSourceWithTickBinding++

        if NumberOfSourceWithTickBinding == 1:
            UMVVMBindingSubsystem::Get()->AddViewWithTickBinding(this)
```

---

### UninitializeBindings() - 정리

```cpp
for each Source:
    if Source.RegisteredCount > 0:
        // 1. 모든 Field Notify 델리게이트 제거
        INotifyFieldValueChanged* NotifyInterface = Cast<INotifyFieldValueChanged>(Source)
        NotifyInterface->RemoveAllFieldValueChangedDelegates(this)

        Source.RegisteredCount = 0

    // 2. Tick 바인딩 해제
    if Source.HasTickBindings():
        NumberOfSourceWithTickBinding--

        if NumberOfSourceWithTickBinding == 0:
            UMVVMBindingSubsystem::Get()->RemoveViewWithTickBinding(this)

    Source.bBindingsInitialized = false
```

---

## Field 변경 시 완전한 흐름

```cpp
// 1. ViewModel Setter 호출
ViewModel->SetHealth(75.0f);

// 2. 매크로 확장
UE_MVVM_SET_PROPERTY_VALUE(Health, 75.0f)
    ↓
SetPropertyValue(Health, 75.0f, FFieldId("Health"))
    ↓
    if (Health == 75.0f) return;  // 값 비교
    Health = 75.0f;               // 값 업데이트
    BroadcastFieldValueChanged(FFieldId("Health"));

// 3. Field Notify 브로드캐스트
NotificationDelegates.BroadcastFieldValueChanged(this, FFieldId("Health"))
    ↓
    for each Delegate in Delegates.GetInvocationList(FFieldId("Health")):
        Delegate.Execute(ViewModel, FFieldId("Health"))

// 4. UMVVMView 콜백 호출
UMVVMView::HandledLibraryBindingValueChanged(ViewModel, FFieldId("Health"), SourceKey)
    ↓
    for each Binding using "Health":
        ExecuteBindingInternal(Binding)

// 5. ExecutionMode 분기
if (ExecutionMode == Immediate):
    RecursiveDetector.Push()
    ExecuteBindingImmediately()
    RecursiveDetector.Pop()

else if (ExecutionMode == Delayed):
    UMVVMBindingSubsystem::Get()->AddDelayedBinding(View, BindingKey)

// 6. 바인딩 실행
ExecuteBindingImmediately()
    ↓
    // Source 유효성 검증 (비트마스크)
    if ((Binding.Sources & ValidSources) == Binding.Sources):
        BindingLibrary.Execute(UserWidget, Binding, ConversionType)

// 7. BindingLibrary.Execute()
BindingLibrary.Execute()
    ↓
    // Source Field 읽기
    SourceValue = EvaluateFieldPath(UserWidget, Binding.SourceFieldPath)
    ↓
    // Conversion Function 실행 (있으면)
    if (Binding.HasConversionFunction()):
        ConvertedValue = InvokeFunction(ConversionFunction, SourceValue)
    else:
        ConvertedValue = SourceValue
    ↓
    // Destination에 쓰기
    SetFieldPath(UserWidget, Binding.DestinationFieldPath, ConvertedValue)

// 8. UI 업데이트 완료
TextBlock->SetText(FText::FromString("75"))
```

---

## 성능 최적화 포인트

### 1. 비트마스크 Source 검증

```cpp
// O(1) 시간 복잡도
uint64 ValidSources = 0b00001111;      // 4개 Source 모두 유효
uint64 BindingSources = 0b00000011;    // Binding이 Source 0, 1 필요

// AND 연산으로 즉시 검증
if ((BindingSources & ValidSources) == BindingSources)
{
    // 모든 필수 Source가 유효함 - 실행
}
```

**메모리**: Source 최대 64개 (uint64 비트 수)

---

### 2. 컴파일 타임 경로 인덱싱

```cpp
// 컴파일 타임:
"ViewModel.Health" → FMVVMVCompiledFieldPath {
    StartingPropertyIndex: 5,    // ViewModel Property의 인덱스
    SegmentIndices: [12, 34],    // Health까지의 경로 인덱스
}

// 런타임:
FProperty* CachedProperties[100];  // 미리 로드된 Property 배열
FProperty* HealthProp = CachedProperties[34];  // O(1) 접근
void* HealthValue = HealthProp->ContainerPtrToValuePtr(ViewModel);
```

**효과**: Reflection 비용 최소화

---

### 3. 재귀 감지 스택

```cpp
TArray<TTuple<UMVVMView*, FMVVMViewClass_BindingKey>> RecursiveDetector;

// A→B 실행 전:
RecursiveDetector.Push({ViewA, BindingAB});

// B→A 실행 시도 시:
if (RecursiveDetector.Contains({ViewA, BindingAB}))  // 발견!
{
    Error("Recursive binding: A→B→A");
    return;
}

// A→B 완료 후:
RecursiveDetector.Pop();
```

**방지되는 패턴**:
- A → B → C → A (순환 참조)
- A → A (직접 재귀)

---

### 4. Delayed Batching

```cpp
// 여러 Field 변경:
ViewModel->SetHealth(75);
ViewModel->SetMana(50);
ViewModel->SetStamina(80);

// Immediate 모드:
// → 3번의 UI 업데이트 (각각 즉시)

// Delayed 모드:
// → Queue에 추가
// → 프레임 끝에 한 번에 처리
// → 중복 제거 가능
```

---

### 5. Tick 바인딩 최소화

```cpp
// Subsystem이 View 목록 관리
TArray<TWeakObjectPtr<UMVVMView>> ViewsWithTickBinding;

// Tick 바인딩이 있는 View만 순회
void Tick(float DeltaTime)
{
    for (UMVVMView* View : ViewsWithTickBinding)
    {
        View->ExecuteTickBindings();  // Tick 모드 바인딩만 실행
    }
}
```

---

## 메모리 구조

### ViewClass (공유 데이터)

```
UMVVMViewClass [1개, 모든 인스턴스가 공유]
    ├─ FMVVMCompiledBindingLibrary BindingLibrary
    │   ├─ TArray<FProperty*> CachedProperties [컴파일 시 생성]
    │   ├─ TArray<UFunction*> CachedFunctions
    │   └─ TArray<FMVVMVCompiledBinding> Bindings
    ├─ TArray<FMVVMViewClass_Source> Sources
    ├─ TArray<FMVVMViewClass_Binding> Bindings
    └─ TArray<FMVVMViewClass_Event> Events
```

---

### View (인스턴스 데이터)

```
UMVVMView [Widget 인스턴스마다 1개]
    ├─ TObjectPtr<UMVVMViewClass> GeneratedViewClass [공유 데이터 참조]
    ├─ TArray<FMVVMView_Source> Sources [인스턴스별]
    │   ├─ TObjectPtr<UObject> Source [ViewModel 인스턴스]
    │   ├─ int32 RegisteredCount [등록된 델리게이트 수]
    │   ├─ bool bSourceInitialized
    │   └─ bool bBindingsInitialized
    ├─ TArray<FBoundEvent> BoundEvents
    ├─ uint64 ValidSources [비트마스크]
    └─ int32 NumberOfSourceWithTickBinding
```

---

## 다음: [실전 가이드](05-practical-guide.md)
