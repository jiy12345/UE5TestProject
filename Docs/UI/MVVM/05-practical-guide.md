# 실전 가이드

## ViewModel 작성 방법

### 기본 템플릿

```cpp
// PlayerStatsViewModel.h
#pragma once

#include "CoreMinimal.h"
#include "MVVMViewModelBase.h"
#include "PlayerStatsViewModel.generated.h"

UCLASS()
class UPlayerStatsViewModel : public UMVVMViewModelBase
{
    GENERATED_BODY()

public:
    //-------------------------------------------------------------------
    // Field Notify Properties
    //-------------------------------------------------------------------

    UPROPERTY(BlueprintReadWrite, FieldNotify, Setter, Getter)
    float Health;

    UPROPERTY(BlueprintReadWrite, FieldNotify, Setter, Getter)
    float MaxHealth;

    //-------------------------------------------------------------------
    // Computed Properties (Read-only)
    //-------------------------------------------------------------------

    UFUNCTION(BlueprintPure, FieldNotify)
    float GetHealthPercentage() const;

    UFUNCTION(BlueprintPure, FieldNotify)
    FText GetFormattedHealth() const;

    //-------------------------------------------------------------------
    // Commands
    //-------------------------------------------------------------------

    UFUNCTION(BlueprintCallable)
    void TakeDamage(float Damage);

private:
    // Setters
    void SetHealth(float NewHealth);
    void SetMaxHealth(float NewMaxHealth);

    // Getters
    float GetHealth() const { return Health; }
    float GetMaxHealth() const { return MaxHealth; }
};
```

```cpp
// PlayerStatsViewModel.cpp
#include "PlayerStatsViewModel.h"

void UPlayerStatsViewModel::SetHealth(float NewHealth)
{
    // ★ 핵심: 이 매크로가 모든 마법을 일으킴
    UE_MVVM_SET_PROPERTY_VALUE(Health, NewHealth);

    // ★ Computed Property도 알림
    UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(GetHealthPercentage);
    UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(GetFormattedHealth);
}

void UPlayerStatsViewModel::SetMaxHealth(float NewMaxHealth)
{
    UE_MVVM_SET_PROPERTY_VALUE(MaxHealth, NewMaxHealth);
    UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(GetHealthPercentage);
}

float UPlayerStatsViewModel::GetHealthPercentage() const
{
    return MaxHealth > 0 ? Health / MaxHealth : 0.0f;
}

FText UPlayerStatsViewModel::GetFormattedHealth() const
{
    return FText::Format(INVTEXT("{0}/{1}"), FText::AsNumber(Health), FText::AsNumber(MaxHealth));
}

void UPlayerStatsViewModel::TakeDamage(float Damage)
{
    SetHealth(FMath::Max(0.0f, Health - Damage));
}
```

---

## Best Practices

### ✅ DO: 권장 사항

#### 1. 항상 Setter를 통해 값 변경
```cpp
// ✅ 좋은 예
void ApplyBuff(float Amount)
{
    SetHealth(Health + Amount);
}

// ❌ 나쁜 예
void ApplyBuff(float Amount)
{
    Health += Amount;  // Field Notify 발생 안 함!
}
```

#### 2. Computed Property 의존성 명시
```cpp
void SetHealth(float NewHealth)
{
    UE_MVVM_SET_PROPERTY_VALUE(Health, NewHealth);

    // ★ Health에 의존하는 Computed Property 알림
    UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(GetHealthPercentage);
    UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(GetIsAlive);
}
```

#### 3. Getter는 const 함수로
```cpp
// ✅ 좋은 예
float GetHealth() const { return Health; }

// ❌ 나쁜 예
float GetHealth() { return Health; }  // const 없음!
```

#### 4. UPROPERTY 필수
```cpp
// ✅ 좋은 예
UPROPERTY(BlueprintReadWrite, FieldNotify, Setter, Getter)
float Health;

// ❌ 나쁜 예
float Health;  // UPROPERTY 없음 - Field Notify 작동 안 함!
```

#### 5. 값 검증은 Setter에서
```cpp
void SetHealth(float NewHealth)
{
    // ★ 범위 검증
    float ClampedHealth = FMath::Clamp(NewHealth, 0.0f, MaxHealth);
    UE_MVVM_SET_PROPERTY_VALUE(Health, ClampedHealth);
}
```

---

### ❌ DON'T: 피해야 할 패턴

#### 1. Getter에서 값 변경
```cpp
// ❌ 나쁜 예
float GetHealth()  // const가 아님!
{
    Health = RecalculateHealth();  // 부작용!
    return Health;
}

// ✅ 좋은 예
float GetHealth() const
{
    return Health;
}

void RecalculateAndSetHealth()
{
    SetHealth(RecalculateHealth());
}
```

#### 2. 불필요한 중복 알림
```cpp
// ❌ 나쁜 예
void SetHealth(float NewHealth)
{
    UE_MVVM_SET_PROPERTY_VALUE(Health, NewHealth);
    UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(Health);  // 중복!
}

// ✅ 좋은 예
void SetHealth(float NewHealth)
{
    UE_MVVM_SET_PROPERTY_VALUE(Health, NewHealth);  // 알림 자동 포함
}
```

#### 3. Setter에서 다른 Setter 호출
```cpp
// ⚠️ 주의 필요
void SetHealth(float NewHealth)
{
    UE_MVVM_SET_PROPERTY_VALUE(Health, NewHealth);

    if (Health <= 0)
    {
        SetIsAlive(false);  // 재귀 가능성!
    }
}

void SetIsAlive(bool bNewIsAlive)
{
    UE_MVVM_SET_PROPERTY_VALUE(bIsAlive, bNewIsAlive);

    if (!bIsAlive)
    {
        SetHealth(0);  // 재귀!
    }
}

// ✅ 더 나은 방법
void SetHealth(float NewHealth)
{
    UE_MVVM_SET_PROPERTY_VALUE(Health, NewHealth);

    // bIsAlive는 Computed Property로 처리
    UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(GetIsAlive);
}

UFUNCTION(BlueprintPure, FieldNotify)
bool GetIsAlive() const { return Health > 0; }
```

---

## UMG 바인딩 설정

### 1. Widget Blueprint에서 ViewModel 설정

**ViewModelClass 지정**:
1. Widget Blueprint 열기
2. Graph 탭 → Class Settings
3. View Models 섹션 → Add ViewModel
4. Class: UPlayerStatsViewModel
5. Name: PlayerStats
6. Creation Type: CreateInstance (또는 Manual)

---

### 2. Property 바인딩

**Designer에서**:
1. Text Block 선택
2. Details Panel → Content → Text
3. Bind 드롭다운 → ViewModel → PlayerStats
4. Property 선택 (예: FormattedHealth)

**또는 Binding Panel에서**:
1. Binding Panel 열기 (Window → Bindings)
2. Add Binding
3. Source: PlayerStats (ViewModel)
4. Source Property: Health
5. Destination: TextBlock_Health
6. Destination Property: Text
7. Conversion Function: (필요시) Float to Text

---

### 3. ExecutionMode 선택

**Immediate** (즉시 실행):
- 대부분의 경우 권장
- 즉각적인 UI 반응
- 예: 버튼 클릭 시 상태 변경

**Delayed** (지연 실행):
- 여러 Property가 동시에 변경될 때
- 불필요한 중복 업데이트 방지
- 예: 초기화 로직, 복잡한 계산

**Tick** (매 프레임):
- 항상 최신 값 필요
- Computed Property
- 예: Health / MaxHealth 비율

---

## 디버깅 팁

### 1. Binding 실행 로그

**C++ 코드에서**:
```cpp
// ViewModel
void SetHealth(float NewHealth)
{
    UE_LOG(LogTemp, Log, TEXT("SetHealth: %f → %f"), Health, NewHealth);
    UE_MVVM_SET_PROPERTY_VALUE(Health, NewHealth);
}

// View
#if UE_WITH_MVVM_DEBUGGING
    bLogBinding = true;  // Binding 실행 시 로그 출력
#endif
```

---

### 2. Binding 실패 원인

**Source가 null**:
```
Error: The binding 'PlayerStats.Health → TextBlock.Text' was not executed. Invalid sources.
```
→ ViewModel 인스턴스가 생성되지 않았거나 null

**해결책**:
1. ViewModelClass 설정 확인
2. Creation Type 확인 (CreateInstance vs Manual)
3. Manual인 경우 C++에서 SetViewModel() 호출

**재귀 감지**:
```
Error: Recursive binding detected (A→B→C→A)
```
→ Setter에서 다른 Setter를 호출하는 순환 참조

**해결책**:
1. Computed Property 사용
2. Setter 체인 제거
3. 플래그를 사용한 재귀 방지

---

### 3. Breakpoint 설정 위치

**ViewModel**:
- `SetHealth()` - Setter 호출 확인
- `BroadcastFieldValueChanged()` - Field Notify 발생 확인

**View**:
- `HandledLibraryBindingValueChanged()` - 델리게이트 호출 확인
- `ExecuteBindingImmediately()` - 바인딩 실행 확인

**BindingLibrary**:
- `Execute()` - 실제 값 전달 확인

---

## 주의사항 및 제한사항

### 1. Source 최대 개수

**제한**: 64개 (uint64 비트 수)

ValidSources가 uint64 비트마스크이므로 Source는 최대 64개까지만 지원됩니다.

---

### 2. Field Notify와 Replication

**주의**: Field Notify는 Replication과 독립적입니다.

```cpp
UPROPERTY(Replicated, BlueprintReadWrite, FieldNotify, Setter, Getter)
float Health;

// ★ Setter에서 명시적으로 Replicate 호출 필요
void SetHealth(float NewHealth)
{
    UE_MVVM_SET_PROPERTY_VALUE(Health, NewHealth);

    // Multiplayer인 경우:
    if (GetOwner()->HasAuthority())
    {
        MARK_PROPERTY_DIRTY_FROM_NAME(ThisClass, Health, this);
    }
}
```

---

### 3. Blueprint에서의 제한

**C++에서만 가능**:
- Computed Property (UFUNCTION FieldNotify)
- 복잡한 Conversion Function
- Template 사용

**Blueprint에서도 가능**:
- Simple Property (UPROPERTY FieldNotify)
- Basic Setter/Getter
- Event 바인딩

---

## 성능 최적화 팁

### 1. Delayed 모드 활용

여러 Property를 동시에 변경하는 경우:
```cpp
void InitializeStats(float InHealth, float InMana, float InStamina)
{
    // ★ Delayed 모드 바인딩이면:
    // → 3개 모두 Queue에 추가
    // → 프레임 끝에 한 번에 처리
    SetHealth(InHealth);
    SetMana(InMana);
    SetStamina(InStamina);
}
```

---

### 2. Tick 바인딩 최소화

```cpp
// ❌ 피하기
// Tick 모드: Health → TextBlock.Text

// ✅ 권장
// Immediate 모드: Health → TextBlock.Text
// (Health 변경 시에만 업데이트)
```

Tick 바인딩은 매 프레임 실행되므로 성능 비용이 큽니다. 꼭 필요한 경우 (Computed Property 등)에만 사용하세요.

---

### 3. Conversion Function 최적화

```cpp
// ❌ 느린 Conversion
UFUNCTION()
FText ConvertHealthToText(float Health)
{
    // 복잡한 로직...
    return FText::Format(INVTEXT("HP: {0}%"), ...);
}

// ✅ 더 빠른 방법: ViewModel에 Computed Property
UFUNCTION(BlueprintPure, FieldNotify)
FText GetFormattedHealth() const
{
    return CachedFormattedHealth;  // 캐시된 값
}

void SetHealth(float NewHealth)
{
    UE_MVVM_SET_PROPERTY_VALUE(Health, NewHealth);

    // 캐시 업데이트
    CachedFormattedHealth = FText::Format(...);
    UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(GetFormattedHealth);
}
```

---

## 마무리

이제 UE5 MVVM 플러그인의 내부 동작 원리를 완벽히 이해했습니다!

**핵심 요약**:
1. `UE_MVVM_SET_PROPERTY_VALUE` 매크로가 자동으로 Field Notify 발생
2. Widget 생성 시 Field별로 델리게이트 등록
3. Immediate/Delayed/Tick 세 가지 실행 모드
4. 비트마스크와 컴파일 타임 인덱싱으로 성능 최적화

**다음 단계**:
- [Issue #3](https://github.com/jiy12345/UE5TestProject/issues/3): UMG Widget 생성 및 바인딩 테스트

---

[← 이전: 라이프사이클](04-lifecycle.md) | [↑ 목차](README.md)
