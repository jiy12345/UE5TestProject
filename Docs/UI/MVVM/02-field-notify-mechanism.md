# Field Notify 메커니즘

## 개요

Field Notify는 UE5의 **FieldNotification 시스템**을 기반으로 구축된 Property 변경 알림 메커니즘입니다. ViewModel의 Property가 변경되면 자동으로 UI를 업데이트할 수 있게 해줍니다.

---

## 핵심 인터페이스: INotifyFieldValueChanged

### 정의
```cpp
// Engine/Source/Runtime/CoreUObject/Public/UObject/FieldNotification.h
class INotifyFieldValueChanged
{
public:
    using FFieldValueChangedDelegate = TDelegate<void(UObject*, FFieldId)>;

    // Field 변경 델리게이트 등록
    virtual FDelegateHandle AddFieldValueChangedDelegate(
        FFieldId InFieldId,
        FFieldValueChangedDelegate InNewDelegate
    ) = 0;

    // 델리게이트 제거
    virtual bool RemoveFieldValueChangedDelegate(
        FFieldId InFieldId,
        FDelegateHandle InHandle
    ) = 0;

    // Field 변경 알림 브로드캐스트
    virtual void BroadcastFieldValueChanged(FFieldId InFieldId) = 0;

    // Field Descriptor 조회
    virtual const IClassDescriptor& GetFieldNotificationDescriptor() const = 0;
};
```

---

## UMVVMViewModelBase 구현

### 클래스 선언
```cpp
// MVVMViewModelBase.h
UCLASS(Blueprintable, Abstract)
class UMVVMViewModelBase : public UObject, public INotifyFieldValueChanged
{
    GENERATED_BODY()

public:
    // INotifyFieldValueChanged 구현
    virtual FDelegateHandle AddFieldValueChangedDelegate(...) override final;
    virtual bool RemoveFieldValueChangedDelegate(...) override final;
    virtual void BroadcastFieldValueChanged(FFieldId InFieldId) override;
    virtual const IClassDescriptor& GetFieldNotificationDescriptor() const override;

private:
    // ★ 실제 델리게이트 저장소
    UE::MVVM::FMVVMFieldNotificationDelegates NotificationDelegates;
};
```

### 구현 세부사항

#### AddFieldValueChangedDelegate
```cpp
// MVVMViewModelBase.cpp
FDelegateHandle UMVVMViewModelBase::AddFieldValueChangedDelegate(
    UE::FieldNotification::FFieldId InFieldId,
    FFieldValueChangedDelegate InNewDelegate)
{
    // ★ NotificationDelegates에 위임
    return NotificationDelegates.AddFieldValueChangedDelegate(
        this,
        InFieldId,
        MoveTemp(InNewDelegate)
    );
}
```

#### BroadcastFieldValueChanged
```cpp
void UMVVMViewModelBase::BroadcastFieldValueChanged(
    UE::FieldNotification::FFieldId InFieldId)
{
    // ★ 해당 Field에 등록된 모든 델리게이트 실행
    NotificationDelegates.BroadcastFieldValueChanged(this, InFieldId);
}
```

---

## 핵심 매크로

### UE_MVVM_SET_PROPERTY_VALUE

#### 정의
```cpp
// MVVMViewModelBase.h (16-21줄)
#define UE_MVVM_SET_PROPERTY_VALUE(MemberName, NewValue) \
    SetPropertyValue(MemberName, NewValue, ThisClass::FFieldNotificationClassDescriptor::MemberName)
```

#### 사용 예시
```cpp
class UPlayerStatsViewModel : public UMVVMViewModelBase
{
private:
    UPROPERTY()
    float Health = 100.0f;

public:
    void SetHealth(float NewHealth)
    {
        // ★ 매크로 사용
        UE_MVVM_SET_PROPERTY_VALUE(Health, NewHealth);
    }
};
```

#### 매크로 확장 결과
```cpp
void SetHealth(float NewHealth)
{
    SetPropertyValue(
        Health,  // 멤버 변수
        NewHealth,  // 새 값
        ThisClass::FFieldNotificationClassDescriptor::Health  // Field ID
    );
}
```

---

## SetPropertyValue 템플릿 함수

### 일반 타입
```cpp
// MVVMViewModelBase.h (80-91줄)
template<typename T, typename U = T>
bool SetPropertyValue(T& Value, const U& NewValue, UE::FieldNotification::FFieldId FieldId)
{
    // ★ Step 1: 값 비교 (변경되지 않았으면 early return)
    if (Value == NewValue)
    {
        return false;
    }

    // ★ Step 2: 값 업데이트
    Value = NewValue;

    // ★ Step 3: Field 변경 알림 브로드캐스트
    BroadcastFieldValueChanged(FieldId);

    return true;
}
```

### FText 특수화
```cpp
// MVVMViewModelBase.h (93-103줄)
bool SetPropertyValue(FText& Value, const FText& NewValue, UE::FieldNotification::FFieldId FieldId)
{
    // ★ FText는 == 연산자 대신 IdenticalTo() 사용
    if (Value.IdenticalTo(NewValue))
    {
        return false;
    }

    Value = NewValue;
    BroadcastFieldValueChanged(FieldId);
    return true;
}
```

### TArray<FText> 특수화
```cpp
// MVVMViewModelBase.h (105-118줄)
bool SetPropertyValue(TArray<FText>& Value, const TArray<FText>& NewValue, UE::FieldNotification::FFieldId FieldId)
{
    // ★ 배열 비교 (Algo::Compare 사용)
    if (Algo::Compare(Value, NewValue, [](const FText& LHS, const FText& RHS)
        {
            return LHS.IdenticalTo(RHS);
        }))
    {
        return false;
    }

    Value = NewValue;
    BroadcastFieldValueChanged(FieldId);
    return true;
}
```

---

## UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED

### 정의
```cpp
// MVVMViewModelBase.h (15-17줄)
#define UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(MemberName) \
    BroadcastFieldValueChanged(ThisClass::FFieldNotificationClassDescriptor::MemberName)
```

### 사용 예시
직접 Field 변경 알림을 발생시킬 때 사용:
```cpp
void ApplyDamage(float Damage)
{
    Health -= Damage;

    // ★ 수동으로 알림 발생
    UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(Health);
}
```

**주의**: `UE_MVVM_SET_PROPERTY_VALUE`를 사용하면 자동으로 알림이 발생하므로, 대부분의 경우 이 매크로는 불필요합니다.

---

## FMVVMFieldNotificationDelegates 구현

### 클래스 구조
```cpp
// ViewModel/MVVMFieldNotificationDelegates.h
namespace UE::MVVM
{
    class FMVVMFieldNotificationDelegates
    {
    private:
        // ★ Field별 델리게이트 저장소
        UE::FieldNotification::FFieldMulticastDelegate Delegates;

        // ★ Field 활성화 비트마스크
        TBitArray<> EnabledFieldNotifications;

    public:
        // 델리게이트 추가
        FDelegateHandle AddFieldValueChangedDelegate(
            UObject* InObject,
            FFieldId InFieldId,
            FFieldValueChangedDelegate InDelegate
        );

        // 델리게이트 제거
        bool RemoveFieldValueChangedDelegate(
            UObject* InObject,
            FFieldId InFieldId,
            FDelegateHandle InHandle
        );

        // ★ 브로드캐스트
        void BroadcastFieldValueChanged(
            UObject* InObject,
            FFieldId InFieldId
        );

        // View 조회
        TArray<FFieldMulticastDelegate::FDelegateView> GetView() const;
    };
}
```

### BroadcastFieldValueChanged 구현
```cpp
void FMVVMFieldNotificationDelegates::BroadcastFieldValueChanged(
    UObject* InObject,
    UE::FieldNotification::FFieldId InFieldId)
{
    // ★ Field가 활성화되어 있는지 확인
    if (EnabledFieldNotifications.IsValidIndex(InFieldId.GetIndex()) &&
        EnabledFieldNotifications[InFieldId.GetIndex()])
    {
        // ★ 해당 Field의 모든 델리게이트 실행
        Delegates.Broadcast(InObject, InFieldId);
    }
}
```

---

## Field ID 시스템

### FFieldId 구조
```cpp
namespace UE::FieldNotification
{
    struct FFieldId
    {
    private:
        FName FieldName;     // Field 이름
        int32 Index;         // 인덱스 (최적화용)

    public:
        bool IsValid() const { return !FieldName.IsNone(); }
        FName GetName() const { return FieldName; }
        int32 GetIndex() const { return Index; }
    };
}
```

### IClassDescriptor
각 ViewModel 클래스의 Field 메타데이터를 제공:
```cpp
class IClassDescriptor
{
public:
    // 클래스의 모든 Field를 순회
    virtual void ForEachField(
        const UClass* Class,
        TFunctionRef<bool(FFieldId)> Callback
    ) const = 0;

    // 이름으로 Field 조회
    virtual FFieldId GetField(const UClass* Class, FName FieldName) const = 0;
};
```

### UMVVMViewModelBase::FFieldNotificationClassDescriptor
```cpp
// MVVMViewModelBase.h (47-50줄)
struct FFieldNotificationClassDescriptor : public IClassDescriptor
{
    virtual void ForEachField(
        const UClass* Class,
        TFunctionRef<bool(FFieldId)> Callback
    ) const override;
};

// MVVMViewModelBase.cpp (70-76줄)
void UMVVMViewModelBase::FFieldNotificationClassDescriptor::ForEachField(
    const UClass* Class,
    TFunctionRef<bool(FFieldId)> Callback) const
{
    if (const UBlueprintGeneratedClass* ViewModelBPClass = Cast<UBlueprintGeneratedClass>(Class))
    {
        // ★ Blueprint에서 생성된 Field Notify 정보 순회
        ViewModelBPClass->ForEachFieldNotify(Callback, true);
    }
}
```

---

## 실행 흐름 상세

### 1. Setter 호출
```cpp
ViewModel->SetHealth(75.0f);
```

### 2. 매크로 확장
```cpp
SetPropertyValue(Health, 75.0f, FFieldId("Health"));
```

### 3. SetPropertyValue 실행
```cpp
template<typename T, typename U>
bool SetPropertyValue(T& Value, const U& NewValue, FFieldId FieldId)
{
    if (Value == NewValue)  // 100.0f != 75.0f → false
        return false;

    Value = NewValue;  // ★ Health = 75.0f

    BroadcastFieldValueChanged(FieldId);  // ★ 알림 발생
    return true;
}
```

### 4. BroadcastFieldValueChanged
```cpp
void UMVVMViewModelBase::BroadcastFieldValueChanged(FFieldId InFieldId)
{
    NotificationDelegates.BroadcastFieldValueChanged(this, InFieldId);
}
```

### 5. Delegates.Broadcast
```cpp
void FMVVMFieldNotificationDelegates::BroadcastFieldValueChanged(UObject* InObject, FFieldId InFieldId)
{
    if (EnabledFieldNotifications[InFieldId.GetIndex()])
    {
        // ★ 등록된 모든 델리게이트 호출
        for (const FDelegate& Delegate : Delegates.GetInvocationList(InFieldId))
        {
            Delegate.Execute(InObject, InFieldId);
            // ↓ 실제로는:
            // UMVVMView::HandledLibraryBindingValueChanged(ViewModel, "Health", SourceKey)
        }
    }
}
```

### 6. UMVVMView::HandledLibraryBindingValueChanged
```cpp
// 이후 Binding 실행 흐름으로 연결됨 (03-binding-execution.md 참조)
```

---

## Blueprint 지원

### K2_AddFieldValueChangedDelegate
Blueprint에서 Field 변경 델리게이트를 등록할 수 있도록 노출:
```cpp
UFUNCTION(BlueprintCallable, Category = "FieldNotify")
void K2_AddFieldValueChangedDelegate(
    FFieldNotificationId FieldId,
    FFieldValueChangedDynamicDelegate Delegate
);
```

### K2_SetPropertyValue (Deprecated)
```cpp
// ★ UE 5.3부터 Deprecated
UFUNCTION(BlueprintCallable, CustomThunk, Category="Viewmodel")
bool K2_SetPropertyValue(
    UPARAM(ref) const int32& OldValue,
    UPARAM(ref) const int32& NewValue
);
```

**대체 방법**: Blueprint에서는 일반 Setter 노드 사용 (자동으로 Field Notify 발생)

---

## 성능 고려사항

### 1. 값 비교 최적화
```cpp
if (Value == NewValue)  // ★ 변경되지 않으면 즉시 리턴
    return false;
```
- 불필요한 알림 방지
- 델리게이트 호출 오버헤드 제거

### 2. 타입별 특수화
- `FText`: `IdenticalTo()` 사용 (문화권 고려 비교)
- `TArray<FText>`: `Algo::Compare` 사용

### 3. 비트마스크 활성화 검사
```cpp
if (EnabledFieldNotifications[FieldId.GetIndex()])
```
- O(1) 시간 복잡도
- 비활성화된 Field는 즉시 스킵

---

## 주의사항 및 Best Practices

### ✅ 권장 사항

1. **항상 UE_MVVM_SET_PROPERTY_VALUE 사용**
   ```cpp
   void SetHealth(float NewHealth)
   {
       UE_MVVM_SET_PROPERTY_VALUE(Health, NewHealth);
   }
   ```

2. **Getter는 const 함수로**
   ```cpp
   float GetHealth() const { return Health; }
   ```

3. **UPROPERTY 필수**
   ```cpp
   UPROPERTY()  // ★ 반드시 필요
   float Health;
   ```

### ❌ 피해야 할 패턴

1. **직접 멤버 변수 수정**
   ```cpp
   // ❌ 나쁜 예
   void ApplyDamage(float Damage)
   {
       Health -= Damage;  // Field Notify 발생 안 함!
   }

   // ✅ 좋은 예
   void ApplyDamage(float Damage)
   {
       SetHealth(Health - Damage);
   }
   ```

2. **불필요한 BROADCAST 호출**
   ```cpp
   // ❌ 나쁜 예 (중복 알림)
   void SetHealth(float NewHealth)
   {
       UE_MVVM_SET_PROPERTY_VALUE(Health, NewHealth);
       UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(Health);  // 불필요!
   }
   ```

3. **Getter에서 값 변경**
   ```cpp
   // ❌ 나쁜 예
   float GetHealth()  // const가 아님!
   {
       Health = CalculateHealth();  // 부작용!
       return Health;
   }
   ```

---

## 다음: [Binding 실행 흐름](03-binding-execution.md)
