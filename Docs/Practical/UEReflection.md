# UE Reflection — 실무 노트 (짧은 요약)

UE5의 reflection 시스템은 `UCLASS` / `USTRUCT` / `UENUM` / `UPROPERTY` / `UFUNCTION` 매크로 + UnrealHeaderTool(UHT)로 구성된 자체 reflection 메커니즘이다. 이 문서는 **실무 시 알아야 할 핵심 한 페이지**만 다룬다. 깊은 분석은 별도 시리즈 [#31](https://github.com/jiy12345/UE5TestProject/issues/31) 참조.

> **이 문서는 실무 진입점**. UHT 동작, generated 코드 구조, UClass/FProperty 런타임 구조, GC/BP/Replication이 어떻게 reflection을 활용하는지의 깊은 분석은 `Docs/Reflection/` 시리즈(예정 — #31)로 분리되어 있다.

## TL;DR

- C++ 표준엔 reflection 없음. UE는 자체 reflection으로 GC, BP 통합, Replication, 직렬화, 에디터 디테일 패널, Hot Reload 등을 구현.
- 마크업 매크로(`UCLASS`, `UPROPERTY` 등) → UHT가 빌드 직전에 파싱 → `*.generated.h` + `Module.<Name>.gen.cpp` 생성.
- 런타임에 `UObject::GetClass()` → `UClass*` → `FindPropertyByName/FindFunctionByName` 등으로 메타 접근.
- 마크업 매크로 = 컴파일러용 매크로 + UHT용 마커 (UHT는 매크로 인자 텍스트만 파싱).

## 매크로 → 무엇이 reflection 대상인가

| 매크로 | 대상 | 핵심 효과 |
|---|---|---|
| `UCLASS()` | UObject 자식 클래스 | reflection 활성, BP 통합, GC 후보 |
| `USTRUCT()` | 구조체 | 가벼운 reflection (GC X) |
| `UENUM()` | enum class | BP 노출, 직렬화 |
| `UPROPERTY()` | 멤버 변수 | GC 추적, 에디터/BP 노출, 직렬화, replication |
| `UFUNCTION()` | 멤버 함수 | BP 호출, RPC, 함수 이름→호출 |
| `UDELEGATE_*` | 델리게이트 | 동적 델리게이트 (BP 바인딩) |
| `UINTERFACE()` | 인터페이스 | BP 구현 가능 인터페이스 |

## 빌드 파이프라인 (간단)

```
[헤더 작성: UCLASS, UPROPERTY 등]
        │
        ▼ (빌드 직전)
[UnrealHeaderTool: 매크로 파싱]
        │
        ▼ (생성)
[*.generated.h] + [Module.<Name>.gen.cpp]
        │
        ▼ (UBT 컴파일/링크)
[DLL]
        │
        ▼ (런타임 모듈 로드)
[정적 초기화로 UClass/UFunction/FProperty 등록]
        │
        ▼ (런타임)
[GetClass(), FindPropertyByName(), 직렬화, GC 등]
```

## 매크로/UHT vs 런타임 Reflection — 본질 분리

> "매크로로 가능한 건 그냥 전처리기로도 가능한 거 아냐?"
>
> **반은 맞고 반은 틀림.** 매크로/UHT는 **컴파일 시점에 reflection 인프라를 박제**하는 것이고, reflection의 본질은 **그 인프라를 런타임에 동적으로 쿼리·조작**하는 것이다. 두 단계가 합쳐져 한 시스템.

### 한 문장 정리

- **매크로/UHT** = 타입 정보를 **런타임에 접근 가능한 형태로 박제**하는 컴파일 시점 작업 (코드 생성)
- **런타임 Reflection** = 박제된 타입 정보를 **런타임에 동적으로 쿼리·조작**하는 메커니즘

### 매크로/UHT만으로 가능한 것 (코드 생성 영역)

이런 건 사실 reflection이라기보다 **컴파일 시점 코드 생성**:

| 메커니즘 | 매크로/UHT가 하는 일 |
|---|---|
| `MODULE_API` (DLL export) | 단순 `__declspec` attribute 부착 — C++ attribute 기능 |
| `UCLASS()` → `Z_Construct_UClass_X()` 함수 | UClass 등록 함수 코드를 **생성**. 이 자체는 컴파일 시점 작업 (BOOST_FUSION_ADAPT_STRUCT 등과 같은 패턴) |
| `UFUNCTION()` → 함수 시그니처 정적 테이블 | 함수 메타를 컴파일 시점에 박제 |
| vtable hook (`GENERATED_BODY` 안의 override 매크로) | C++ 가상함수 메커니즘 그대로 — 매크로는 boilerplate만 줄임 |
| `BlueprintCallable` 함수의 thunk 생성 | UHT가 만드는 wrapper 함수 — 컴파일 시점 코드 생성 |

→ 다른 게임 엔진들은 이 정도까지만 매크로/codegen으로 처리하기도 함.

### 런타임 Reflection이 진짜로 가능하게 하는 것

**컴파일 시점에 알 수 없는 정보**를 다루는 능력. 4가지 카테고리:

#### 카테고리 1 — 이름 → 코드 매핑 (string-based lookup)

문자열로 들어오는 입력을 런타임에 코드에 매핑. 컴파일 시점엔 어떤 문자열이 들어올지 모름.

| 기능 | 매크로로 안 되는 이유 |
|---|---|
| **콘솔 명령** (`MyDebugCommand` 입력 → 함수 호출) | 입력 시점에 사용자가 어떤 명령을 칠지 컴파일러가 모름. UFunction 테이블에서 이름으로 lookup 필요 |
| **BP → C++ 함수 호출** (`Set Health` 노드 클릭 → 우리 C++ 함수 실행) | BP 그래프는 우리 C++ 코드를 컴파일 시점에 안 봤음. BP 실행기가 런타임에 UFunction 이름으로 dispatch |
| **SaveGame 로드** (파일에 저장된 `"AMyEnemy"` 클래스명 → 인스턴스 생성) | 저장 시점과 로드 시점 사이에 코드가 바뀔 수 있음. 클래스명으로 UClass lookup → `NewObject` |
| **DataTable, .uasset 로드** | 디자이너가 만든 자산이 어떤 클래스 인스턴스인지 자산 안에 클래스명으로 저장됨 → 런타임 lookup |

이게 reflection의 가장 핵심 가치 — **컴파일 시점에 존재하지 않는 정보**를 다룬다.

#### 카테고리 2 — 객체 그래프 동적 순회

인스턴스 N개가 어떤 그래프를 이루고 있는지 런타임에만 알 수 있음.

| 기능 | 매크로로 안 되는 이유 |
|---|---|
| **GC** | 어느 인스턴스가 어느 인스턴스를 참조하는지 컴파일 시점엔 모름. 런타임에 모든 UObject 순회 + UPROPERTY 포인터 따라 mark-sweep |
| **직렬화 (저장)** | 어떤 객체들이 저장될지 런타임에 결정. 객체 그래프 BFS/DFS — 각 객체의 UPROPERTY를 reflection으로 순회 |
| **에디터 디테일 패널** | 사용자가 어떤 Actor를 클릭할지 모름. 클릭 시 그 인스턴스의 UClass에서 모든 UPROPERTY를 reflection으로 가져와 위젯 자동 생성 |
| **Replication 변경 감지** | 매 프레임 모든 Replicated UPROPERTY를 이전 값과 비교. 어느 멤버가 바뀌었는지는 런타임 비교 |

매크로는 멤버 목록을 정적 테이블로 만들어주지만, **그 테이블을 런타임에 순회하며 동적 작업**하는 게 reflection의 본질.

#### 카테고리 3 — 타입 트리 동적 비교

```cpp
USceneComponent* Comp = GetSomeComponent();
if (UStaticMeshComponent* Mesh = Cast<UStaticMeshComponent>(Comp)) { ... }
```

`Cast<T>`는 런타임에 `Comp`의 실제 클래스 → UClass → 부모 트리 따라가며 `UStaticMeshComponent`와의 관계 검사.
- `Comp`의 실제 타입이 컴파일 시점엔 결정 안 됨 (다형성)
- C++ `dynamic_cast`로도 가능하지만 RTTI를 켜야 하고 더 무거움 — UE는 자체 reflection으로 더 가볍게 구현
- 추가 기능: `Comp->IsA(UStaticMeshComponent::StaticClass())`, `Comp->GetClass()->IsChildOf(...)` 등

#### 카테고리 4 — 클래스 정보 → 인스턴스/CDO

인스턴스 없이 **클래스 정보(UClass\*)만으로** 작업.

| 기능 | 메커니즘 |
|---|---|
| **`NewObject<T>(Name)` / `NewObject(UClass*)`** | UClass 메타로 메모리 할당 + 디폴트값 채움. 동적 클래스 인자 버전은 reflection 필수 |
| **CDO (Class Default Object)** | 모든 UClass마다 자동 생성되는 1개 디폴트 인스턴스. UPROPERTY 디폴트값 보관소. 새 인스턴스는 CDO 복사 후 변경값 적용 |
| **Hot Reload / Live Coding** | 새 DLL의 UClass와 기존 인스턴스를 reflection으로 매칭. 새 멤버 추가/타입 변경을 런타임에 마이그레이션 |
| **에디터에서 새 BP 클래스 생성** | 사용자가 부모 클래스 선택 → UClass 정보만으로 새 BP UClass 파생 (인스턴스 없음) |

### C++26 정적 reflection과의 비교

C++26에 컴파일 시점 reflection 도입 예정 — 정적 멤버 순회 등 매크로 boilerplate 제거 가치는 큼.
하지만 UE의 reflection이 가진 **런타임 동적 부분**(GC, BP↔C++, SaveGame, Cast<T>, NewObject(UClass*) 등)은 C++26으로도 대체 불가. 별도 메커니즘 필수.

→ 이게 "왜 UE는 자체 reflection 시스템을 만들었나"의 답: C++ 컴파일 시점 reflection만으론 게임 엔진 핵심 기능의 절반 이상이 불가능하기 때문.

## Reflection이 가능하게 하는 것 (요약 표)

| 기능 | 메커니즘 | 매크로만으로? |
|---|---|---|
| **GC** | UPROPERTY 포인터를 mark-sweep으로 자동 추적 | X (런타임 그래프 순회) |
| **BP 통합** | UClass/UFunction/UProperty 메타 → BP가 C++ 호출 | X (런타임 string lookup) |
| **Replication** | `UPROPERTY(Replicated)` 메타 + 런타임 비교/전송 | 부분 (메타 등록은 매크로, 비교는 런타임) |
| **직렬화** | `FArchive` + UPROPERTY 자동 순회 | X (런타임 그래프 순회) |
| **Hot Reload / Live Coding** | UClass 메타로 새 정의 적용, 인스턴스 마이그레이션 | X (런타임 매칭) |
| **CDO** | UClass마다 1개 디폴트 인스턴스 자동 생성 | 부분 (생성은 매크로, 활용은 런타임) |
| **`Cast<T>`** | UClass 트리 런타임 비교 | X (런타임 트리 탐색) |
| **에디터 디테일 패널** | UPROPERTY 메타 → 위젯 자동 매핑 | X (런타임 introspection) |
| **콘솔 명령** | `UFUNCTION(Exec)` → 콘솔에서 직접 호출 | X (런타임 string lookup) |
| **RPC** | `UFUNCTION(Server/Client/NetMulticast)` | 부분 (메타는 매크로, dispatch는 런타임) |

## Recipes

### 클래스/멤버에 reflection 활성화

```cpp
UCLASS()
class UE5TESTPROJECT_API UMyClass : public UObject
{
    GENERATED_BODY()

    UPROPERTY()
    int32 SomeValue = 0;

    UFUNCTION()
    void DoSomething();
};
```

- `GENERATED_BODY()`는 UHT가 채울 자리. 빠뜨리면 빌드 에러
- `UE5TESTPROJECT_API` 빠뜨리면 다른 모듈에서 사용 시 링크 에러 ([UClassMetadata.md](UClassMetadata.md) 참고)

### 런타임에 멤버 이름으로 접근

```cpp
UClass* Class = MyObj->GetClass();
FProperty* Prop = Class->FindPropertyByName(TEXT("SomeValue"));
int32* ValuePtr = Prop->ContainerPtrToValuePtr<int32>(MyObj);
*ValuePtr = 42;
```

- 직렬화/에디터/BP 모두 이 패턴을 내부적으로 사용
- 실무에서 직접 쓸 일은 적지만 디버그 시 유용

### 함수를 이름으로 호출 (BP에서 C++ 호출하는 메커니즘)

```cpp
UFunction* Func = MyObj->FindFunction(TEXT("DoSomething"));
MyObj->ProcessEvent(Func, nullptr);
```

## Pitfalls

### 증상: `GENERATED_BODY()` 누락 빌드 에러

```
error: 'StaticClass' is not a member of 'UMyClass'
```

- 원인: `GENERATED_BODY()` 빠뜨림 → UHT가 채울 코드가 없음
- 확인: 클래스 본체 첫 줄에 `GENERATED_BODY()` 부착

### 증상: UCLASS 안 붙은 클래스를 BP에서 사용하려 함

- 원인: UCLASS/USTRUCT 없는 일반 C++ 클래스는 reflection X → BP에서 못 봄
- 확인: BP 노출 필요하면 `UCLASS(BlueprintType)` 또는 `USTRUCT(BlueprintType)`로 변경

### 증상: 헤더 수정 후 generated 코드가 갱신 안 됨

- 원인: UHT가 캐시된 결과 재사용 (incremental). 매크로 인자만 바꾸고 timestamp 변화 없으면 가끔 발생
- 확인: `Game/Intermediate/Build/.../UHT/` 폴더 비우고 재빌드. 또는 헤더 timestamp touch.

### 증상: UPROPERTY 없는 UObject 포인터가 dangling

- [UProperty.md](UProperty.md) Pitfalls 섹션 참조 (이 함정은 워낙 흔해서 별도 문서에 깊이 다룸)

## References

- 출처: #19 NavDebugVisualizer 단계 2
- **깊은 분석 (예정)**: [#31 UE Reflection 시스템 분석 시리즈](https://github.com/jiy12345/UE5TestProject/issues/31) — 시리즈 완성 후 `Docs/Reflection/`로 cross-link
- 관련 실무 문서: [UClassMetadata.md](UClassMetadata.md), [UProperty.md](UProperty.md), [UEModuleSystem.md](UEModuleSystem.md) §5
- 엔진 소스 (시리즈에서 깊이 다룰 위치):
  - `Engine/Source/Runtime/CoreUObject/Public/UObject/Class.h` — UClass
  - `Engine/Source/Runtime/CoreUObject/Public/UObject/UnrealType.h` — FProperty
  - `Engine/Source/Runtime/CoreUObject/Public/UObject/ObjectMacros.h` — 매크로 정의
  - `Engine/Source/Programs/UnrealHeaderTool/` — UHT (UE5.5+에서 다른 위치로 이동했을 수 있음)
  - `<Module>/Intermediate/Build/.../Inc/<Module>/UHT/<Header>.generated.h` — 생성된 헤더 실물
