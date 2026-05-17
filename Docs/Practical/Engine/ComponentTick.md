# 컴포넌트 Tick — `TickComponent` 함수와 활성화

`UActorComponent::TickComponent`는 매 프레임 호출되는 **virtual 콜백**. 모든 ActorComponent 자식이 override 가능. 다만 호출되려면 **2단계 활성화 조건**(정적 가능 여부 + 등록)을 만족해야 함.

## TL;DR

- `TickComponent`는 `UActorComponent`의 `virtual` 함수. 모든 컴포넌트 자식이 override 가능 (`ActorComponent.h:962`).
- 호출 흐름: `UWorld::Tick` → Tick 그룹 순회 → `FActorComponentTickFunction::ExecuteTick` → `virtual TickComponent` (`ActorComponent.cpp:1657`). **우리가 직접 호출 X**.
- 베이스 `UActorComponent::TickComponent`는 **BP `Event Tick` 호출 + Latent Action 갱신** 처리 (`ActorComponent.cpp:1838-1859`). **`Super::TickComponent` 호출 누락 시 BP Tick 노드와 `Delay` 노드 동작 안 함**.
- `PrimaryComponentTick.bCanEverTick = true` — **생성자에서** 결정. 한 번 false면 그 클래스 인스턴스는 절대 Tick 등록 X.
- `SetComponentTickEnabled(bool)` — 런타임 동적 토글. 생성자 외에서 끄고 켜기.
- 생성자에서 `bCanEverTick = false`로 두면 **아무리 SetComponentTickEnabled(true) 호출해도 Tick 안 옴**. 가장 흔한 함정.
- BeginPlay에서 `bCanEverTick` 변경은 효과 X — 컴포넌트 등록(`RegisterTickFunctions`)이 이미 끝남.

## TickComponent 함수 자체

### 시그니처

```cpp
// ActorComponent.h:962
ENGINE_API virtual void TickComponent(
    float DeltaTime,
    enum ELevelTick TickType,
    FActorComponentTickFunction* ThisTickFunction);
```

| 인자 | 의미 |
|---|---|
| `float DeltaTime` | 직전 Tick 이후 경과 시간(초). 60fps면 ≈0.0166s. **프레임 독립적인 로직 작성에 사용** (`Velocity * DeltaTime`으로 1초당 일정 속도 보장) |
| `ELevelTick TickType` | Tick 종류. `LEVELTICK_TimeOnly`(시간만), `LEVELTICK_ViewportsOnly`(에디터 뷰포트), `LEVELTICK_All`(일반 게임). 에디터 시뮬레이션 vs PIE 구분 |
| `FActorComponentTickFunction* ThisTickFunction` | 이 Tick 호출을 일으킨 TickFunction 메타. 우선순위/조건부 실행 등 제어용. 거의 안 씀 |

### 호출 흐름 (소스 근거)

```
UWorld::Tick()                           (게임 루프)
  └─ Tick 그룹 순회 (PrePhysics → DuringPhysics → PostPhysics → PostUpdateWork)
      └─ FTickFunction 리스트 순회
          └─ FActorComponentTickFunction::ExecuteTick()    (ActorComponent.cpp:1651)
              └─ Target->TickComponent(DilatedTime, ...)   (라인 1657, virtual call)
                  └─ [override한 TickComponent 본문 실행]
```

`FActorComponentTickFunction`은 모든 컴포넌트가 멤버로 갖는 구조체 (생성자에서 본 `PrimaryComponentTick`이 그것). 게임 시작 시 World의 Tick 그룹 리스트에 등록되고, 매 프레임 순회됨.

### 베이스 `UActorComponent::TickComponent`가 하는 일

```cpp
// ActorComponent.cpp:1838-1859
void UActorComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    check(bRegistered);

    if (GetClass()->HasAnyClassFlags(CLASS_CompiledFromBlueprint) || !GetClass()->HasAnyClassFlags(CLASS_Native))
    {
        ReceiveTick(DeltaTime);   // ← BP의 Event Tick 노드 호출

        if (GTickComponentLatentActionsWithTheComponent)
        {
            // Latent action(Delay 노드 등 비동기 BP 노드) 진행 갱신
            if (UWorld* ComponentWorld = GetWorld())
            {
                ComponentWorld->GetLatentActionManager().ProcessLatentActions(this, ...);
            }
        }
    }
}
```

베이스가 처리하는 두 가지:
1. **`ReceiveTick(DeltaTime)`** — BP의 `Event Tick` 호출. BP에서 Tick 이벤트 동작 메커니즘
2. **Latent Action 갱신** — BP의 `Delay`, `MoveComponentTo` 같은 비동기 노드의 진행 갱신

→ **`Super::TickComponent` 호출 안 하면 BP Event Tick과 Delay 노드 동작 안 함**. Super 호출이 중요한 이유.

### 자식 클래스 override 패턴 (상속 사다리)

각 베이스가 자기 책임의 Tick 작업을 가짐:

```cpp
// USceneComponent::TickComponent (개념적)
void USceneComponent::TickComponent(...)
{
    Super::TickComponent(...);   // UActorComponent — BP Tick + Latent
    // Transform 갱신 관련 작업
}

// UPrimitiveComponent::TickComponent (개념적)
void UPrimitiveComponent::TickComponent(...)
{
    Super::TickComponent(...);   // USceneComponent → UActorComponent까지 전파
    // 충돌/렌더 데이터 갱신
}

// 우리 자체 컴포넌트
void UMyComponent::TickComponent(...)
{
    Super::TickComponent(...);   // 부모 책임 위임
    // 우리 로직
}
```

**Super 호출 위치는 보통 첫 줄** — 부모가 baseline 상태(예: Transform 갱신)를 만든 뒤 우리가 그 위에서 작업.

### 오버라이드만으로는 호출 안 됨 — 활성화 두 조건

| 단계 | 효과 |
|---|---|
| `TickComponent` virtual override만 | 함수 정의만 됨 — **호출 X** (Tick 시스템이 등록을 모름) |
| + 생성자 `bCanEverTick = true` | 컴포넌트 부착 시 `FActorComponentTickFunction` 등록됨 → 매 프레임 호출 |
| + 런타임 `SetComponentTickEnabled(false)` | 등록은 유지하되 호출만 잠시 멈춤 |

## Recipes

### 매 프레임 Tick하는 컴포넌트

```cpp
UMyComponent::UMyComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
}

void UMyComponent::TickComponent(float DeltaTime, ELevelTick TickType,
    FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
    // 매 프레임 로직
}
```

### 평소엔 끄고 필요할 때만 Tick

```cpp
UMyComponent::UMyComponent()
{
    PrimaryComponentTick.bCanEverTick = true;   // 가능성 열어둠
    PrimaryComponentTick.bStartWithTickEnabled = false;   // 시작은 비활성
}

// 런타임에 켜기
void UMyComponent::StartTracking()
{
    SetComponentTickEnabled(true);
}

void UMyComponent::StopTracking()
{
    SetComponentTickEnabled(false);
}
```

- `bCanEverTick = true`로 가능성을 미리 열어둬야 `SetComponentTickEnabled`가 동작
- `bStartWithTickEnabled = false`로 시작 시점만 비활성

### 특정 조건일 때만 Tick 본문 실행 (간단한 패턴)

```cpp
void UMyComponent::TickComponent(float DeltaTime, ...)
{
    Super::TickComponent(DeltaTime, ...);
    if (!bEnabled) return;   // 우리 컴포넌트의 NavDebugVisualizer 패턴
    // ...
}
```

- Tick 자체는 등록되어 매 프레임 호출됨 (오버헤드 발생)
- 본문 early-return으로 작업만 스킵
- 런타임에 자주 토글되는 경우 `SetComponentTickEnabled`보다 코드가 깔끔하지만 호출 오버헤드는 있음

## Tick 활성화의 2단계 구조

| 단계 | 변수 | 결정 시점 | 의미 |
|---|---|---|---|
| **정적 가능 여부** | `PrimaryComponentTick.bCanEverTick` | 생성자 | 이 클래스 인스턴스가 Tick 등록 가능한가 |
| **시작 시 활성** | `PrimaryComponentTick.bStartWithTickEnabled` | 생성자 | 첫 등록 시 활성 상태로 시작할지 |
| **런타임 토글** | `SetComponentTickEnabled(bool)` | 언제든지 | 동적으로 Tick on/off |

### 왜 두 단계인가?

UE 내부에서 컴포넌트 Tick은 별도 Tick 그룹/리스트에 등록되어 게임 루프에서 순회됨. `bCanEverTick`이 false면 처음부터 등록 자체가 안 됨 → 메모리/시간 절약. 등록 후엔 활성/비활성만 토글 가능.

## Pitfalls

### 증상: SetComponentTickEnabled(true) 호출했는데 Tick이 안 옴

- 원인: 생성자에서 `bCanEverTick = true` 안 함 → 등록 자체가 안 됐음
- 확인: 생성자에 `PrimaryComponentTick.bCanEverTick = true` 부착

### 증상: 부모 클래스에선 Tick하던데 자식 클래스는 안 함

- 원인: 자식 클래스 생성자가 `Super::Super(...)` 호출 안 하거나, 자식 생성자에서 `bCanEverTick = false` 덮어씀
- 확인: 자식 생성자에서 부모 호출 + bCanEverTick 재설정 점검

### 증상: BeginPlay에서 bCanEverTick = true로 바꿨는데 효과 없음

- 원인: BeginPlay 시점엔 컴포넌트 등록(`RegisterTickFunctions`)이 이미 끝남. `bCanEverTick`을 그 후 바꿔도 등록 안 됨
- 확인: 반드시 생성자에서 결정. 동적 토글이 필요하면 `SetComponentTickEnabled` 사용

### 증상: TickComponent override했는데 Super::TickComponent 호출 안 해서 부모 로직 누락

- 원인: `Super::TickComponent(DeltaTime, TickType, ThisTickFunction)` 첫 줄 호출 누락
- 확인: override 함수 첫 줄에 항상 Super 호출

### 증상: Tick 오버헤드가 의외로 크다

- 원인: bEnabled 체크 후 early-return하지만 Tick 등록 자체는 매 프레임 호출 — 호출 비용은 발생
- 확인: 진짜 오랫동안 안 쓰일 거면 `SetComponentTickEnabled(false)`로 등록 해제. 자주 토글이면 early-return 패턴이 코드 깔끔.

## References

- 출처: #19 NavDebugVisualizer 단계 2
- 관련: [ActorComponent.md](ActorComponent.md)
- 엔진 소스:
  - `Engine/Source/Runtime/Engine/Classes/Components/ActorComponent.h:962` — `virtual TickComponent` 선언
  - `Engine/Source/Runtime/Engine/Classes/Components/ActorComponent.h` — `PrimaryComponentTick`, `SetComponentTickEnabled`
  - `Engine/Source/Runtime/Engine/Private/Components/ActorComponent.cpp:1651-1667` — `FActorComponentTickFunction::ExecuteTick` (호출 진입점)
  - `Engine/Source/Runtime/Engine/Private/Components/ActorComponent.cpp:1838-1859` — `UActorComponent::TickComponent` 베이스 본문 (BP Event Tick + Latent Action)
  - `Engine/Source/Runtime/Engine/Public/Tickable.h` — Tick 그룹/리스트 메커니즘
