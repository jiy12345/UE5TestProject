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

## Reflection이 가능하게 하는 것

| 기능 | 메커니즘 |
|---|---|
| **GC** | UPROPERTY 포인터를 마크-스윕으로 자동 추적 |
| **BP 통합** | UClass/UFunction/UProperty 메타 → BP가 C++ 호출 가능 |
| **Replication** | `UPROPERTY(Replicated)` 메타 + UNetDriver의 비교/전송 |
| **직렬화** | `FArchive` + UPROPERTY 자동 순회 → `.uasset` / SaveGame |
| **Hot Reload / Live Coding** | UClass 메타로 새 클래스 정의 적용, 인스턴스 마이그레이션 |
| **CDO (Class Default Object)** | UClass마다 1개 인스턴스 자동 생성 — UPROPERTY 디폴트 보관 |
| **Cast<T>** | UClass 트리 비교 (RTTI보다 가벼움) |
| **에디터 디테일 패널** | UPROPERTY 메타 → 위젯 자동 매핑 |
| **Console 명령** | `UFUNCTION(Exec)` → 콘솔에서 직접 호출 |
| **RPC** | `UFUNCTION(Server/Client/NetMulticast)` |

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
