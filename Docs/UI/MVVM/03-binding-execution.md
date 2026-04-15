# Binding 실행 흐름

## 개요

MVVM Binding은 ViewModel의 Field 변경 시 자동으로 UI를 업데이트하는 핵심 메커니즘입니다. 이 문서는 Binding이 초기화되고 실행되는 전체 과정을 상세히 설명합니다.

---

## 초기화 흐름

### 1. InitializeBindings() - 바인딩 초기화

```cpp
// MVVMView.cpp (344-373줄)
void UMVVMView::InitializeBindings()
{
    if (!bSourcesInitialized)
    {
        InitializeSources();  // Source부터 초기화
    }

    if (bBindingsInitialized)
    {
        return;  // 이미 초기화됨
    }

    // ★ 각 Source에 대해 바인딩 초기화
    for (int32 SourceIndex = 0; SourceIndex < Sources.Num(); ++SourceIndex)
    {
        InitializeSourceBindings(FMVVMView_SourceKey(SourceIndex), false);
    }

    InitializeSourceBindingsCommon();

    // Extension들에게 통지
    for (int32 Index = 0; Index < Extensions.Num(); ++Index)
    {
        ClassExtensions[Index]->OnBindingsInitialized(GetUserWidget(), this, Extensions[Index]);
    }
}
```

---

### 2. InitializeSourceBindings() - Field Notify 델리게이트 등록

```cpp
// MVVMView.cpp (436-526줄)
void UMVVMView::InitializeSourceBindings(FMVVMView_SourceKey SourceKey, bool bRunAllBindings)
{
    FMVVMView_Source& ViewSource = Sources[SourceKey.GetIndex()];
    const FMVVMViewClass_Source& ClassSource = GeneratedViewClass->GetSource(ViewSource.ClassKey);

    const bool bIsPointerValid = ViewSource.Source != nullptr;
    if (bIsPointerValid)
    {
        // ★★★ 핵심: Field Notify 델리게이트 등록 ★★★
        if (ClassSource.GetFieldIds().Num() > 0)
        {
            TScriptInterface<INotifyFieldValueChanged> NotifyInterface =
                ViewSource.Source->Implements<UNotifyFieldValueChanged>() ?
                TScriptInterface<INotifyFieldValueChanged>(ViewSource.Source) :
                TScriptInterface<INotifyFieldValueChanged>();

            if (NotifyInterface.GetInterface() != nullptr)
            {
                // 각 Field에 대해 델리게이트 등록
                for (const FMVVMViewClass_FieldId& FieldId : ClassSource.GetFieldIds())
                {
                    if (ensureMsgf(FieldId.IsValid(), TEXT("Invalid field...")))
                    {
                        // ★ HandledLibraryBindingValueChanged를 콜백으로 등록
                        UE::FieldNotification::FFieldMulticastDelegate::FDelegate Delegate =
                            UE::FieldNotification::FFieldMulticastDelegate::FDelegate::CreateUObject(
                                this,                                        // View
                                &UMVVMView::HandledLibraryBindingValueChanged,  // 콜백 함수
                                SourceKey                                    // 파라미터
                            );

                        // ★ ViewModel에 델리게이트 등록
                        NotifyInterface->AddFieldValueChangedDelegate(
                            FieldId.GetFieldId(),
                            MoveTemp(Delegate)
                        );

                        ++ViewSource.RegisteredCount;
                    }
                }
            }
        }

        // ★ 초기 바인딩 실행
        if (bRunAllBindings)
        {
            // 모든 바인딩을 한 번씩 실행
            for (const FMVVMViewClass_SourceBinding& SourceBinding : ClassSource.GetBindings())
            {
                ExecuteBindingImmediately(ClassBinding, SourceBinding.GetBindingKey());
            }
        }
        else
        {
            // ExecuteAtInitialization==true인 바인딩만 실행
            for (const FMVVMViewClass_SourceBinding& SourceBinding : ClassSource.GetBindings())
            {
                if (SourceBinding.ExecuteAtInitialization())
                {
                    ExecuteBindingImmediately(ClassBinding, SourceBinding.GetBindingKey());
                }
            }
        }

        // ★ Tick 바인딩 관리
        if (ClassSource.HasTickBindings())
        {
            ++NumberOfSourceWithTickBinding;

            // 첫 Tick 바인딩이면 Subsystem에 등록
            if (NumberOfSourceWithTickBinding == 1 && !bHasDefaultTickBinding)
            {
                GEngine->GetEngineSubsystem<UMVVMBindingSubsystem>()->AddViewWithTickBinding(this);
            }
        }
    }

    ViewSource.bBindingsInitialized = true;
}
```

**핵심 포인트**:
1. `INotifyFieldValueChanged` 인터페이스 확인
2. 각 Field에 `HandledLibraryBindingValueChanged` 델리게이트 등록
3. 초기 실행 플래그에 따라 바인딩 즉시 실행
4. Tick 바인딩이 있으면 Subsystem에 View 등록

---

## Field 변경 시 실행 흐름

### 3. HandledLibraryBindingValueChanged() - 핵심 콜백

```cpp
// MVVMView.cpp (666-718줄)
void UMVVMView::HandledLibraryBindingValueChanged(
    UObject* InSource,
    UE::FieldNotification::FFieldId InFieldId,
    FMVVMView_SourceKey ViewSourceKey)
{
    check(InSource);
    check(InFieldId.IsValid());
    check(ViewSourceKey.IsValid());

    if (ensure(GeneratedViewClass) && ensure(Sources.IsValidIndex(ViewSourceKey.GetIndex())))
    {
        const FMVVMView_Source& ViewSource = Sources[ViewSourceKey.GetIndex()];

        // ★ 초기화 중에는 실행하지 않음
        if (!ViewSource.bBindingsInitialized || !ViewSource.bSourceInitialized)
        {
            return;
        }

        const FMVVMViewClass_Source& ClassSource = GeneratedViewClass->GetSource(ViewSource.ClassKey);

        // ★★★ 1. Evaluate 바인딩 실행 (동적 Source 재평가) ★★★
        if (ClassSource.HasEvaluateBindings())
        {
            for (const FMVVMViewClass_EvaluateSource& ClassEvaluate : GeneratedViewClass->GetEvaluateSources())
            {
                if (ClassEvaluate.GetParentSource() == ClassSource.GetKey() &&
                    ClassEvaluate.GetFieldId().GetFieldName() == InFieldId.GetName())
                {
                    EvaluateSource(ClassEvaluate.GetSource());
                }
            }
        }

        // ★★★ 2. 일반 바인딩 실행 ★★★
        for (const FMVVMViewClass_SourceBinding& SourceBinding : ClassSource.GetBindings())
        {
            // 이 Field를 사용하는 바인딩만 실행
            if (SourceBinding.GetFieldId().GetFieldName() == InFieldId.GetName())
            {
                ExecuteBindingInternal(SourceBinding);  // ← ExecutionMode 분기
            }
        }

        // ★★★ 3. Condition 실행 ★★★
        for (const FMVVMViewClass_SourceCondition& SourceCondition : ClassSource.GetConditions())
        {
            if (SourceCondition.GetFieldId().GetFieldName() == InFieldId.GetName())
            {
                ExecuteConditionInternal(SourceCondition);
            }
        }
    }
}
```

**실행 순서**:
1. Evaluate Binding (동적 Source)
2. 일반 Binding (UI 업데이트)
3. Condition (조건부 로직)

---

### 4. ExecuteBindingInternal() - ExecutionMode 분기

```cpp
// MVVMView.cpp (731-764줄)
void UMVVMView::ExecuteBindingInternal(const FMVVMViewClass_SourceBinding& SourceBinding) const
{
    const FMVVMViewClass_Binding& ClassBinding = GeneratedViewClass->GetBinding(SourceBinding.GetBindingKey());

    if (ensure(ClassBinding.IsOneWay()))
    {
        const EMVVMExecutionMode ExecutionMode = ClassBinding.GetExecuteMode();

        // ★★★ Immediate 모드 ★★★
        if (ExecutionMode == EMVVMExecutionMode::Immediate)
        {
            // 재귀 감지
            if (RecursiveDetector.Contains({this, SourceBinding.GetBindingKey()}))
            {
                ensureAlwaysMsgf(false, TEXT("Recursive binding detected"));
                return;
            }

            {
                // 스택에 추가 (재귀 감지용)
                RecursiveDetector.Emplace(this, SourceBinding.GetBindingKey());

                // ★ 즉시 실행
                ExecuteBindingImmediately(ClassBinding, SourceBinding.GetBindingKey());

                RecursiveDetector.Pop();
            }
        }
        // ★★★ Delayed 모드 ★★★
        else if (ExecutionMode == EMVVMExecutionMode::Delayed)
        {
            // ★ Subsystem Queue에 추가
            GEngine->GetEngineSubsystem<UMVVMBindingSubsystem>()->AddDelayedBinding(
                this,
                SourceBinding.GetBindingKey()
            );
        }
        // ★★★ Tick 모드는 여기서 처리하지 않음 ★★★
        // (TickBindings()에서 매 프레임 실행)
    }
}
```

---

### 5. ExecuteBindingImmediately() - 실제 바인딩 실행

```cpp
// MVVMView.cpp (569-639줄)
void UMVVMView::ExecuteBindingImmediately(
    const FMVVMViewClass_Binding& ClassBinding,
    FMVVMViewClass_BindingKey KeyForLog) const
{
    check(GeneratedViewClass);
    check(GetUserWidget());

    // ★ Source 유효성 검증 (비트마스크)
    if ((ClassBinding.GetSources() & ValidSources) == ClassBinding.GetSources())
    {
        // 모든 필요한 Source가 유효함 - 실행 가능

        // Conversion Function 타입 결정
        FMVVMCompiledBindingLibrary::EConversionFunctionType FunctionType =
            ClassBinding.GetBinding().HasComplexConversionFunction() ?
            FMVVMCompiledBindingLibrary::EConversionFunctionType::Complex :
            FMVVMCompiledBindingLibrary::EConversionFunctionType::Simple;

        // ★★★ BindingLibrary를 통해 실제 실행 ★★★
        TValueOrError<void, FMVVMCompiledBindingLibrary::EExecutionFailingReason> ExecutionResult =
            GeneratedViewClass->GetBindingLibrary().Execute(
                GetUserWidget(),           // Execution Source (UserWidget)
                ClassBinding.GetBinding(), // 컴파일된 바인딩 정보
                FunctionType               // Conversion 타입
            );

#if UE_WITH_MVVM_DEBUGGING
        if (ExecutionResult.HasError())
        {
            UE::MVVM::FMessageLog Log(GetUserWidget());
            Log.Error(FText::Format(
                LOCTEXT("ExecuteBindingFailGeneric", "The binding '{0}' was not executed. {1}."),
                FText::FromString(ClassBinding.ToString(...)),
                FMVVMCompiledBindingLibrary::LexToText(ExecutionResult.GetError())
            ));
        }
        else
        {
            if (bLogBinding)
            {
                Log.Info(FText::Format(LOCTEXT("ExecuteBindingGeneric", "Execute binding '{0}'."), ...));
            }
        }
#endif
    }
    else
    {
        // Source가 유효하지 않음
        const uint64 MissingSources = ClassBinding.GetSources() & (~ValidSources);

        // Optional Source인지 확인
        if ((MissingSources & GeneratedViewClass->GetOptionalSources()) != MissingSources)
        {
            // 필수 Source가 없음 - 에러
            UE::MVVM::FMessageLog Log(GetUserWidget());
            Log.Error(FText::Format(
                LOCTEXT("ExecuteBindingFailInvalidSource", "The binding '{0}' was not executed. Invalid sources."),
                ...
            ));
        }
    }
}
```

**핵심 로직**:
1. 비트마스크로 Source 유효성 검증 (O(1))
2. Conversion Function 타입 결정
3. `BindingLibrary.Execute()` 호출
4. 에러 처리 및 디버깅 로그

---

## ExecutionMode별 실행 흐름

### Immediate 모드

```
ViewModel.Health = 100
    ↓
BroadcastFieldValueChanged("Health")
    ↓
HandledLibraryBindingValueChanged(ViewModel, "Health", SourceKey)
    ↓
ExecuteBindingInternal(Binding)
    ↓
[ExecutionMode == Immediate]
    ↓
RecursiveDetector.Push({View, BindingKey})
    ↓
ExecuteBindingImmediately(Binding)
    ↓
GeneratedViewClass->BindingLibrary.Execute(...)
    ↓
RecursiveDetector.Pop()
    ↓
UI 업데이트 (예: TextBlock.Text = "100")
```

**특징**:
- ✅ 즉시 실행
- ✅ 재귀 감지
- ❌ 여러 Field 변경 시 각각 실행 (성능 고려 필요)

---

### Delayed 모드

```
ViewModel.Health = 100
    ↓
HandledLibraryBindingValueChanged()
    ↓
ExecuteBindingInternal()
    ↓
[ExecutionMode == Delayed]
    ↓
UMVVMBindingSubsystem::AddDelayedBinding(View, BindingKey)
    // ★ Queue에 추가만 하고 즉시 리턴
    ↓
... 다른 로직 계속 실행 ...
    ↓
[프레임 끝 또는 Subsystem Tick]
    ↓
UMVVMBindingSubsystem::ProcessDelayedBindings()
    ↓
for each (View, BindingKey) in Queue:
    View->ExecuteDelayedBinding(BindingKey)
        ↓
        ExecuteBindingImmediately(Binding)
```

**특징**:
- ✅ 여러 Field 변경을 Batch 처리 가능
- ✅ 불필요한 중복 실행 방지
- ❌ 약간의 지연 (보통 1프레임 이내)

---

### Tick 모드

```
[매 프레임 Tick]
    ↓
UMVVMBindingSubsystem::Tick(DeltaTime)
    ↓
for each View in ViewsWithTickBinding:
    View->ExecuteTickBindings()
        ↓
        for each Binding in View:
            if Binding.ExecutionMode == Tick:
                ExecuteBindingImmediately(Binding)
                    ↓
                    UI 업데이트
```

**코드 구현**:
```cpp
// MVVMView.cpp (801-825줄)
void UMVVMView::ExecuteTickBindings() const
{
    if (ensure(GeneratedViewClass))
    {
        const TArrayView<const FMVVMViewClass_Binding> ClassBindings = GeneratedViewClass->GetBindings();

        // ★ 모든 바인딩 순회
        for (int32 BindingIndex = 0; BindingIndex < ClassBindings.Num(); ++BindingIndex)
        {
            const FMVVMViewClass_Binding& ClassBinding = ClassBindings[BindingIndex];

            // ★ Tick 모드만 실행
            if (ClassBinding.GetExecuteMode() == EMVVMExecutionMode::Tick)
            {
                ExecuteBindingImmediately(ClassBinding, FMVVMViewClass_BindingKey(BindingIndex));
            }
        }
    }
}
```

**특징**:
- ✅ 항상 최신 값으로 업데이트 보장
- ❌ 매 프레임 실행 (성능 비용)
- 사용 예: Progress Bar (Health / MaxHealth 같은 Computed 값)

---

## 다음: [Binding Library 실행 세부](03-binding-execution-part2.md)
