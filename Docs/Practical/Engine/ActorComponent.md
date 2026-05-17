# UActorComponent / USceneComponent / UPrimitiveComponent 선택

Actor에 부착되는 컴포넌트는 3개 베이스 중 하나를 상속한다. **위치(Transform) 필요 여부 + 충돌/렌더 여부**로 선택.

## TL;DR

- `UActorComponent` — Transform **없음**. 로직/처리만. 가장 가볍다.
- `USceneComponent` — Transform **있음** (Location/Rotation/Scale). Actor 안에서 트리 노드 역할.
- `UPrimitiveComponent` — `USceneComponent` 자식. **충돌/렌더** 가능.
- 상속 사다리: `UObjectBase` ← `UObject` ← `UActorComponent` ← `USceneComponent` ← `UPrimitiveComponent` ← `UStaticMeshComponent` 등
- **Attach 가능 여부**: USceneComponent끼리만 가능. UActorComponent는 Attach 트리 X.

## 선택 가이드

```
컴포넌트가 World 위치를 가져야 하나?
├─ NO → UActorComponent  (HealthComponent, AIComponent, NavDebugVisualizerComponent 등)
└─ YES → 충돌/렌더 필요?
         ├─ NO → USceneComponent  (SpringArm, attach anchor 등)
         └─ YES → UPrimitiveComponent 자식
                  ├─ 메시 → UStaticMeshComponent / USkeletalMeshComponent
                  ├─ 충돌만 → UBoxComponent / USphereComponent / UCapsuleComponent
                  ├─ 라이트 → UPointLightComponent / USpotLightComponent / UDirectionalLightComponent
                  ├─ 카메라 → UCameraComponent
                  ├─ 오디오 → UAudioComponent (3D 사운드)
                  └─ 파티클 → UNiagaraComponent / UParticleSystemComponent
```

## Recipes

### UActorComponent — 로직만 (가장 흔한 자체 제작 패턴)

```cpp
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class UE5TESTPROJECT_API UMyLogicComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    UMyLogicComponent();
    virtual void TickComponent(float DeltaTime, ELevelTick TickType,
        FActorComponentTickFunction* ThisTickFunction) override;
};
```

- 자기 위치/Transform 없음 — 부착된 Actor의 위치는 `GetOwner()->GetActorLocation()`으로 얻음
- 메모리 가벼움 (Transform 부재 + Attach 트리 비용 없음)

### USceneComponent — 위치 의미 있는 추상 노드

```cpp
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class UE5TESTPROJECT_API UMyAnchorComponent : public USceneComponent
{
    GENERATED_BODY()
public:
    UMyAnchorComponent();
};
```

- 자기 Transform 있음 — `GetComponentLocation()`, `GetComponentRotation()` 등
- 부모/자식 attach 가능 (다른 SceneComponent를 자식으로, 또는 자기가 자식이 됨)
- 시각/물리 객체 아니지만 위치는 의미 있는 anchor 역할 (예: SpringArm, 부착 슬롯)

### UPrimitiveComponent 자식 — 시각/물리 객체

`UStaticMeshComponent`, `UCameraComponent` 등 — 직접 상속하기보단 기존 자식 클래스 사용.

## 상속 트리에서의 의미

| 베이스 | Transform | Attach 트리 | 충돌 | 렌더 | 메모리 |
|---|---|---|---|---|---|
| `UActorComponent` | X | X | X | X | 가장 가벼움 |
| `USceneComponent` | O | O (부모/자식) | X | X | 중간 |
| `UPrimitiveComponent` | O | O | O | O | 가장 무거움 (충돌 데이터 + 렌더 등록) |

## Actor의 Transform과 무엇이 다른가

핵심: **Actor는 Transform 1개, USceneComponent는 트리**.

```
ACharacter (Actor, World 위치 1개)
└─ Capsule (USceneComponent — RootComponent — Actor 원점 = 캡슐 중심)
   ├─ Mesh (USkeletalMeshComponent, 캡슐 발끝 기준 오프셋)
   │  └─ WeaponMesh (UStaticMeshComponent, 손 본에 attach)
   ├─ SpringArm (USpringArmComponent, 머리 뒤 어깨 위치)
   │  └─ Camera (UCameraComponent, SpringArm 끝)
   └─ AudioFootstep (UAudioComponent, 발 위치)
```

각 컴포넌트가 **부모 기준 상대 Transform**을 가지면서 Actor 원점부터 트리로 계산됨. UActorComponent는 이 트리에 못 들어감 — Transform이 없으니까.

### Attach 규칙 — USceneComponent끼리만 가능

```cpp
StaticMesh->AttachToComponent(SpringArm, FAttachmentTransformRules::KeepRelativeTransform);  // OK

// 컴파일 에러: UActorComponent는 attach 대상도 주체도 못 됨
// HealthComponent->AttachToComponent(...);  // 그런 함수 없음
```

## Pitfalls

### 증상: UActorComponent 자식인데 `GetComponentLocation()`이 없음

- 원인: `UActorComponent`는 Transform 없음
- 확인: 위치가 필요한 컴포넌트면 `USceneComponent` 자식으로 변경. 또는 `GetOwner()->GetActorLocation()`으로 부착된 Actor 위치 사용.

### 증상: 컴포넌트를 다른 컴포넌트에 attach하려는데 컴파일 에러

- 원인: `AttachToComponent`은 `USceneComponent`의 멤버. `UActorComponent`엔 없음
- 확인: 양쪽 다 `USceneComponent` 자식이어야 함

### 증상: 무겁다는 느낌 — Tick 부담, 메모리

- 원인: 위치 의미 없는데 `USceneComponent`를 골랐음. Transform 갱신, attach 트리 트래버스 비용
- 확인: 위치 진짜 안 쓰면 `UActorComponent`로 변경

## NavDebugVisualizer가 UActorComponent를 고른 이유

이 컴포넌트는:
- 시각화는 하지만 **자기 위치는 의미 없음** — DrawDebug가 World 좌표로 그림
- PathStart/PathEnd로 외부 Actor 좌표를 받아오기 때문에 컴포넌트 자기 Transform 불필요
- 어떤 컴포넌트의 자식이 될 일도, 자식을 가질 일도 없음 (Attach 트리 불필요)

→ `UActorComponent` 선택. 만약 `USceneComponent`를 골랐다면 컴포넌트 자체가 World에 위치를 가지면서 그 주변을 시각화하는 다른 디자인이 됐을 것 (그것도 가능한 디자인이지만 사용 패턴이 달라짐).

## References

- 출처: #19 NavDebugVisualizer 단계 2
- 관련: [ComponentTick.md](ComponentTick.md), [`../UClassMetadata.md`](../UClassMetadata.md)
- 엔진 소스:
  - `Engine/Source/Runtime/Engine/Classes/Components/ActorComponent.h`
  - `Engine/Source/Runtime/Engine/Classes/Components/SceneComponent.h`
  - `Engine/Source/Runtime/Engine/Classes/Components/PrimitiveComponent.h`
