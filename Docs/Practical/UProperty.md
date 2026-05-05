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

## UObject 포인터엔 반드시 UPROPERTY — GC 메커니즘과 함께

> **이 섹션은 실무 진입점**. UE GC 시스템의 깊은 분석은 별도 시리즈 [#32](https://github.com/jiy12345/UE5TestProject/issues/32) (예정 — `Docs/GC/`)로 분리. 여기서는 UPROPERTY와 직접 관련된 부분만 정리.

### 핵심 명제

**UE GC = Mark-Sweep (Tracing GC), NOT Reference Counting.**

| 방식 | UE | C++ shared_ptr/Rc |
|---|---|---|
| 메커니즘 | 주기적으로 루트에서 그래프 traversal, 도달 안 되는 객체 destroy | 참조 추가/제거 시 카운트 ±1, 0이면 즉시 destroy |
| 순환 참조 | **자동 처리** (둘 다 unreachable이면 같이 정리) | **누수** (카운트 영원히 ≥1) |
| 즉시성 | 지연 (다음 GC pass) | 즉시 |
| 참조 변경 오버헤드 | 0 (평소엔) | atomic increment/decrement |

→ **raw pointer가 위험한 이유는 "ref count에 안 잡혀서 null 됨"이 아니라 "GC traversal에 안 잡혀 객체가 destroy되는데 우리 포인터는 그대로 dangling"**. null이 되는 게 아니라 메모리는 해제되고 포인터는 남는 것.

### UPROPERTY의 두 가지 GC 효과

#### 효과 1 — Reachability (keep-alive)

GC는 mark-sweep:
1. 루트 셋에서 시작 (월드, GameInstance, RF_Standalone, AddToRoot한 객체)
2. **각 객체의 UPROPERTY 포인터를 따라가며 reachable 마크**
3. 한 번 마크된 객체는 **재방문 X** — `MarkAsReachableInterlocked` atomic CAS (`GarbageCollection.cpp:1078-1090`)
4. 마크 안 된 객체는 destroy

→ **우리가 UPROPERTY로 들고 있으면 GC가 그 객체를 reachable로 본다 = 살려둠**.
→ raw pointer는 traversal 엣지가 아니라 GC가 못 봄 → 다른 reference 없으면 destroy됨.

#### 효과 2 — 자동 nullptr 클리어

`GarbageCollection.cpp:5390`:
```cpp
*ReferenceInfo.Reference = nullptr;
```

객체가 명시적 destroy되면 다음 GC pass에서 **모든 UPROPERTY 포인터를 자동 nullptr로 클리어**. raw pointer는 그대로.

→ **UPROPERTY 부착 시에만 `if (Target)` null 체크가 신뢰성 있음**. raw pointer는 destroy 후에도 null 안 됨 → if문 통과 후 use-after-free.

### 두 시나리오로 비교

#### 시나리오 1 — 다른 reference 없을 때 GC 동작

```cpp
// UPROPERTY 없음 (raw)
AActor* Target = nullptr;

void Init() { Target = GetWorld()->SpawnActor<AActor>(); }
// 다른 reference 없는 상태로 GC pass...
// → Target이 가리킨 Actor는 unreachable로 판정 → destroy
Target->GetActorLocation();  // 💥 use-after-free
```

```cpp
// UPROPERTY 있음
UPROPERTY()
AActor* Target = nullptr;

void Init() { Target = GetWorld()->SpawnActor<AActor>(); }
// GC pass... Target이 reachable이라 살아있음
Target->GetActorLocation();  // ✅
```

#### 시나리오 2 — 명시적 `Destroy()`

```cpp
// UPROPERTY 없음
AActor* Target = ...;
Target->Destroy();
if (Target) Target->GetActorLocation();  // 💥 if문 통과 후 dangling 접근
```

```cpp
// UPROPERTY 있음
UPROPERTY()
AActor* Target = ...;
Target->Destroy();
// GC pass에서 Target = nullptr 자동 클리어
if (Target) Target->GetActorLocation();  // ✅ if문이 정확히 막아줌
```

### GC 주기와 트리거

- **기본 60s** (`gc.TimeBetweenPurgingPendingKillObjects`, `UnrealEngine.cpp:1682`)
- 메모리 압박 시 30s (`gc.LowMemoryTimeBetweenPurgingPendingKillObjects`)
- 명시적 `GEngine->ForceGarbageCollection()` 가능
- 레벨 전환 시 자동
- UE 5.x **Incremental GC** — hitch 방지 위해 traversal을 여러 프레임에 분산

### 루트 셋

GC traversal 시작점:
| 종류 | 설정 |
|---|---|
| 명시적 루트 | `Object->AddToRoot()` |
| `RF_Standalone` 플래그 | `NewObject<>(... RF_Standalone)` (에셋 등) |
| 내부 매니저 추적 | `GameInstance`, `GEngine`, 활성 `World` |
| 클러스터 루트 | UE 내부 최적화 (UClass, BP) |

**일반 UObject는 자동 루트 X** — `NewObject<>()`로 만든 객체는 보통 **Outer 체인**(컴포넌트 → Actor → Level → World → 루트)으로 살아남.

`AddToRoot()`는 드물게 사용 (싱글톤 매니저, 임시 보존 등). 대신 `TStrongObjectPtr<T>`가 더 안전 (소멸 시 자동 RemoveFromRoot).

### 함정: STL 컨테이너에 UObject* 금지

```cpp
// 함정
std::vector<AActor*> EnemyList;   // GC 추적 X — 컨테이너 안 raw pointer
EnemyList.push_back(SpawnedActor);
// GC 후...
EnemyList[0]->...   // 💥 dangling
```

해결:
```cpp
UPROPERTY()
TArray<AActor*> EnemyList;   // GC 추적 — TArray 안 모든 포인터를 GC가 따라감
```

`TArray`/`TMap`/`TSet`은 reflection 시스템과 통합되어 컨테이너 안 UObject 포인터를 GC가 추적. STL `std::vector`는 reflection 모름.

### 긴 생명주기 / 멀티스레드 — Smart Pointer 대안

```cpp
TWeakObjectPtr<AActor> WeakTarget;     // weak: destroy 시 IsValid() = false
TStrongObjectPtr<AActor> StrongTarget; // strong: 자동 AddToRoot/RemoveFromRoot
```

UPROPERTY 없이도 GC와 협력. 일반 멤버는 UPROPERTY로 충분, 특수 케이스에서만.

### Raw Pointer가 사실상 안전한 패턴

**raw `UObject*` = 항상 위험은 아니다**. 위험한 건 **다음 GC pass를 넘어가는 경우**. GC는 60초 간격(기본)으로 동작하므로 짧은 스코프에서는 raw도 실용적으로 안전.

핵심 룰: **누군가 다른 곳에서 keep-alive를 보장하면 raw pointer로 가리켜도 OK.** 다음 4가지 중 하나면 안전:

| keep-alive 메커니즘 | 예시 |
|---|---|
| 다른 객체의 UPROPERTY가 들고 있음 (가장 흔함) | 부모 컴포넌트가 우리를 UPROPERTY로 들고 있음 |
| `AddToRoot()`로 명시적 루트 | 싱글톤 매니저, 임시 보존 |
| `RF_Standalone` 플래그 | 에셋 (Texture, Material 등) |
| Outer 체인 | Component의 Outer = Actor → Level → World → 루트 |

#### 안전 패턴 1 — 함수 인자

```cpp
void DoSomething(AActor* Target)
{
    Target->GetActorLocation();
    // 함수 안에서만 쓰고 끝
    // → caller가 Target을 살려두고 있다고 가정 (보통 caller의 UPROPERTY)
}
```

함수 호출 동안은 안전. 단 **저장 X** — 멤버 변수에 넣으면 위험.

#### 안전 패턴 2 — 같은 함수 내 로컬 변수, 짧은 사용

```cpp
void Tick(float Delta)
{
    AActor* Target = Cast<AActor>(SomeComponent->GetOwner());
    if (Target) { Target->...; }
    // 같은 Tick 안에서만 사용
    // → GC는 게임 스레드에서 동기적 — 같은 함수 안에선 안 끼어듦
}
```

#### 안전 패턴 3 — Outer가 살아있다고 보장된 자식

```cpp
void UMyComp::DoWork()
{
    AActor* Owner = GetOwner();   // 우리 컴포넌트의 Outer
    Owner->...;
    // 우리가 살아있는 동안 Owner도 살아있음 (Outer 체인 보장)
}
```

#### 위험 패턴 — 멤버 변수

```cpp
class UMyComp
{
    AActor* SavedTarget = nullptr;   // 💥 raw 멤버

    void OnSomething(AActor* T) { SavedTarget = T; }
    void Tick(...) { SavedTarget->...; }   // T가 살아있다는 보장 X — 다음 GC pass에서 dangling 가능
};
```

멤버 변수는 **다음 GC pass를 넘어 살아남아야** 하므로 UPROPERTY 필수.

### 사용 결정 표 — raw vs UPROPERTY vs Smart Pointer

| 상황 | 권장 |
|---|---|
| **클래스 멤버 변수** (UObject*) | 항상 `UPROPERTY()` |
| **함수 인자** (단순 전달) | raw `UObject*` OK (caller 보장) |
| **로컬 변수, 같은 함수 내 짧은 사용** | raw OK |
| **컨테이너 멤버** (배열/맵) | `UPROPERTY() TArray<UObject*>` (절대 `std::vector` X) |
| **글로벌/싱글톤** | `TStrongObjectPtr<T>` (자동 AddToRoot) |
| **Optional / 약한 참조 / 멀티스레드** | `TWeakObjectPtr<T>` (destroy 시 IsValid() = false) |
| **에셋 핸들 / 디스크 로드 대상** | `TSoftObjectPtr<T>` (지연 로드) |

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
- **깊은 분석 (예정)**:
  - [#32 UE Garbage Collector 분석 시리즈](https://github.com/jiy12345/UE5TestProject/issues/32) — Mark-Sweep 메커니즘, 루트 셋, Incremental GC 등 깊이 다룸. 시리즈 완성 후 `Docs/GC/`로 cross-link
  - [#31 UE Reflection 시스템 분석 시리즈](https://github.com/jiy12345/UE5TestProject/issues/31) — UPROPERTY 메타 등록 메커니즘 (GC와 직접 연결)
- 관련 실무 문서: [UClassMetadata.md](UClassMetadata.md), [UEReflection.md](UEReflection.md)
- 엔진 소스:
  - `Engine/Source/Runtime/CoreUObject/Public/UObject/UnrealType.h` — FProperty
  - `Engine/Source/Runtime/CoreUObject/Public/UObject/ObjectMacros.h` — UPROPERTY 매크로 정의
  - `Engine/Source/Runtime/CoreUObject/Public/UObject/UObjectBaseUtility.h:206` — `AddToRoot` / `RemoveFromRoot`
  - `Engine/Source/Runtime/CoreUObject/Private/UObject/GarbageCollection.cpp:1078-1090` — Reachability 마크 (재방문 방지)
  - `Engine/Source/Runtime/CoreUObject/Private/UObject/GarbageCollection.cpp:5390` — UPROPERTY 자동 nullptr 클리어
  - `Engine/Source/Runtime/Engine/Private/UnrealEngine.cpp:1682` — GC 주기 (`gc.TimeBetweenPurgingPendingKillObjects`)
