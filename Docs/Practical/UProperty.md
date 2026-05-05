# UPROPERTY 키워드와 메타

`UPROPERTY(...)` 매크로로 멤버 변수에 reflection 메타를 붙인다. 가시성/편집/직렬화/GC/Replication 동작을 한 줄에서 결정. 매크로 일반 동작은 [UEReflection.md](UEReflection.md) 참조.

## TL;DR

- `UPROPERTY` 안 붙은 raw pointer는 **GC 미추적 → dangling 위험**. UObject 포인터엔 항상 부착.
- 편집 가능 범위는 3개: `EditAnywhere`(둘 다) / `EditDefaultsOnly`(CDO) / `EditInstanceOnly`(인스턴스).
- BP 접근은 2개: `BlueprintReadWrite`(Get+Set) / `BlueprintReadOnly`(Get만).
- `Category="..."`는 에디터 디테일 패널 그룹화.
- 직렬화 제외는 `Transient`, `DuplicateTransient`, `SkipSerialization` 등 — 디폴트 직렬화는 켜져 있음.

## Recipes

### 우리 컴포넌트의 표준 패턴

```cpp
UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="NavDebug")
AActor* PathStart = nullptr;

UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="NavDebug")
bool bEnabled = true;

UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="NavDebug")
float PathLineThickness = 5.f;
```

- `EditAnywhere` — CDO(Class Default Object)와 인스턴스 둘 다 디테일 패널에서 편집
- `BlueprintReadWrite` — BP 변수처럼 Get/Set 노드 사용 가능
- `Category="NavDebug"` — 디테일 패널에서 "NavDebug" 그룹으로 묶임
- 디폴트 값(`= nullptr`, `= true`, `= 5.f`)은 **인라인 멤버 초기화** (UE5 표준; 이전엔 생성자에서 설정)

### 에디터에서만 편집, BP에서 접근 불가

```cpp
UPROPERTY(EditDefaultsOnly, Category="Tuning")
float TuningParameter = 1.f;
```

- 디자이너가 클래스 디폴트에서 튜닝
- BP에서 코드로 못 바꿈 (BP 노출 키워드 없음)

### 읽기만 가능 (Set 못 함)

```cpp
UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="State")
int32 CurrentScore = 0;
```

- `VisibleAnywhere` — 디테일 패널에 보이지만 편집 불가 (디버깅용)
- BP에서 Get만, Set 안 됨

### 직렬화에서 제외

```cpp
UPROPERTY(Transient)
UTexture2D* RuntimeCache = nullptr;
```

- `.uasset` 저장 시 제외, 매번 0/nullptr로 초기화
- 런타임 캐시, 핸들 등에 사용

### 네트워크 동기화

```cpp
UPROPERTY(Replicated)
int32 Health = 100;
```

- 서버 → 클라 자동 동기화 (프레임워크가 변경 감지)
- `GetLifetimeReplicatedProps` override 필요

## UPROPERTY의 5가지 효과 (한 줄에 다 들어감)

UPROPERTY 한 줄로 다음 5가지가 동시에 결정:

| 효과 | 메커니즘 |
|---|---|
| **GC 추적** | UObject 포인터를 GC 마크-스윕에 등록. 안 붙으면 raw pointer는 GC 무시 → 객체 삭제돼도 우리 포인터 남음 (dangling) |
| **에디터 노출** | `EditAnywhere`/`VisibleAnywhere` 키워드로 디테일 패널 위젯 자동 생성 |
| **BP 노출** | `BlueprintReadWrite`/`BlueprintReadOnly`로 BP 변수 노드 생성 |
| **직렬화** | 디폴트로 `.uasset`/SaveGame에 자동 저장. `Transient`로 제외 가능 |
| **Replication** | `Replicated` 키워드로 네트워크 동기화 등록 |

## 핵심 함정 — UObject 포인터엔 반드시 UPROPERTY

### 증상: Actor가 destroy됐는데 우리 포인터로 접근 시 크래시

```cpp
// 함정: UPROPERTY 없음
AActor* TargetActor = SomeWorld->SpawnActor(...);
// ... 어딘가에서 TargetActor->Destroy() ...
TargetActor->GetActorLocation();   // 크래시 — TargetActor는 dangling
```

- 원인: GC가 `TargetActor`를 추적 안 함. 다른 reference도 없으면 GC가 destroy. 우리 포인터는 그대로 남아 dangling.
- 확인: 모든 UObject* 멤버에 UPROPERTY 부착했는지 점검. 로컬 변수도 위험할 수 있음 (긴 생명주기면 `TStrongObjectPtr` 또는 `TWeakObjectPtr` 사용).
- 해결:
  ```cpp
  UPROPERTY()
  AActor* TargetActor = nullptr;
  ```
  편집/BP 노출 필요 없으면 키워드 없는 `UPROPERTY()` 만으로 GC 추적 활성화.

## EditAnywhere vs EditDefaultsOnly vs EditInstanceOnly

| 키워드 | CDO 편집 | 인스턴스 편집 | 사용처 |
|---|---|---|---|
| `EditAnywhere` | O | O | 디자이너가 클래스 기본값 + 레벨 인스턴스 둘 다 튜닝 |
| `EditDefaultsOnly` | O | X | 클래스 정의에 속한 값. 인스턴스마다 다르면 안 됨 (예: AI 패턴 데이터) |
| `EditInstanceOnly` | X | O | 인스턴스 고유 값. 클래스 디폴트에 적힐 의미 X (예: 레벨 배치 시 결정되는 ID) |
| `VisibleAnywhere` | 보기만 | 보기만 | 디버깅 — 런타임 상태 관찰용 |
| `VisibleDefaultsOnly` | 보기만 | X | 클래스 메타 표시 |
| `VisibleInstanceOnly` | X | 보기만 | 인스턴스 상태 관찰 |

## Pitfalls

### 증상: UObject* 멤버가 갑자기 nullptr이 됨 (또는 댕글링)

- 원인: UPROPERTY 미부착 → GC 미추적
- 확인: 모든 UObject 자식 타입 포인터에 UPROPERTY 부착

### 증상: 디테일 패널에 우리 변수가 안 보임

- 원인: `EditAnywhere`/`VisibleAnywhere` 키워드 없음, 또는 `Category` 누락 후 다른 카테고리에 묻힘
- 확인: `EditAnywhere, BlueprintReadWrite, Category="..."` 풀 셋

### 증상: BP에서 우리 변수 Set 못 함

- 원인: `BlueprintReadOnly` 또는 BP 키워드 없음
- 확인: `BlueprintReadWrite`로 변경 (단 외부에서 막 바꾸면 안 되는 값이면 의도된 readonly일 수 있음)

### 증상: SaveGame에서 변수 값이 저장 안 됨

- 원인: `Transient` 또는 클래스 단위 `Transient`
- 확인: 클래스 메타와 변수 메타 둘 다 점검

### 증상: 네트워크에서 변수가 동기화 안 됨

- 원인: `Replicated` 키워드 누락 또는 `GetLifetimeReplicatedProps` 미구현
- 확인: `UPROPERTY(Replicated)` + `DOREPLIFETIME(MyClass, MyVar)` 함께

## References

- 출처: #19 NavDebugVisualizer 단계 2
- 관련: [UClassMetadata.md](UClassMetadata.md), [UEReflection.md](UEReflection.md)
- 엔진 소스: `Engine/Source/Runtime/CoreUObject/Public/UObject/UnrealType.h` (FProperty), `Engine/Source/Runtime/CoreUObject/Public/UObject/ObjectMacros.h` (UPROPERTY 매크로 정의)
