# UCLASS 메타 옵션

`UCLASS(...)` 매크로 안에 들어가는 키워드/메타로 클래스의 에디터/BP/직렬화 동작을 결정한다. 매크로 자체의 동작 원리(reflection 일반)는 [UEReflection.md](UEReflection.md) 참조.

## TL;DR

- 첫 인자(키워드)는 **클래스 specifier** — `Blueprintable`, `Abstract`, `MinimalAPI` 등
- `meta=(...)` 안의 키는 **에디터 UX/검증 메타** — `BlueprintSpawnableComponent`, `DisplayName`, `ToolTip` 등
- `ClassGroup=(<Group>)` — 에디터 컴포넌트 메뉴/팔레트의 그룹 분류
- 잘못 빠뜨리면 보통 **빌드 에러는 안 남, 에디터에서만 안 보임** — 발견하기 어려운 함정

## Recipes

### BP에서 Add Component로 추가 가능한 컴포넌트

```cpp
UCLASS(ClassGroup=(Navigation), meta=(BlueprintSpawnableComponent))
class UE5TESTPROJECT_API UNavDebugVisualizerComponent : public UActorComponent
```

- `ClassGroup=(Navigation)`: 컴포넌트 추가 메뉴에서 "Navigation" 그룹으로 묶임 (그룹명 자유)
- `meta=(BlueprintSpawnableComponent)`: BP의 Add Component 드롭다운에 노출

둘 다 빠지면 컴포넌트 자체는 만들어지지만 **에디터/BP에서 발견 불가**.

### BP에서 상속 가능한 클래스

```cpp
UCLASS(Blueprintable)
class UE5TESTPROJECT_API UMyService : public UObject
```

`Blueprintable` 없으면 BP 클래스를 만들려고 할 때 부모 후보로 안 보임.

### 추상 베이스

```cpp
UCLASS(Abstract)
class UE5TESTPROJECT_API UMyBase : public UObject
```

직접 인스턴스화 금지 (BP/에디터에서도 강제). 베이스 클래스 의도 명시.

### 자동 생성 안 되는 가벼운 reflection (빌드 시간 절약)

```cpp
UCLASS(MinimalAPI)
class UMyClass : public UObject
```

- `UCLASS()` 만 쓰면 모든 멤버가 DLL export 후보
- `MinimalAPI`는 클래스 자체 메타만 export, 멤버는 명시적으로 `UE5TESTPROJECT_API` 붙인 것만
- 큰 모듈에서 빌드 시간/링크 시간 단축 효과

## 자주 쓰는 메타 키워드

| 키워드 | 효과 |
|---|---|
| `Blueprintable` | BP에서 이 클래스를 부모로 BP 클래스 생성 가능 |
| `BlueprintType` | BP 변수/함수 인자로 이 타입 사용 가능 |
| `Abstract` | 직접 인스턴스화 금지 |
| `MinimalAPI` | 클래스 메타만 export (DLL 표면적 축소) |
| `Config=Game` | INI 파일에 멤버 자동 저장/로드 |
| `Transient` | 모든 멤버를 직렬화 대상에서 제외 |
| `NotPlaceable` | 에디터에서 레벨에 배치 불가 (UClass 뷰포트 메뉴 숨김) |
| `Within=AActor` | 이 UObject는 AActor 안에서만 만들어질 수 있음 |

## 자주 쓰는 `meta=(...)` 키

| 키 | 효과 |
|---|---|
| `BlueprintSpawnableComponent` | Add Component 드롭다운 노출 |
| `DisplayName="..."` | 에디터 UI 표시명 |
| `ToolTip="..."` | 마우스 호버 툴팁 |
| `ShortToolTip="..."` | 짧은 툴팁 |
| `IsBlueprintBase=true` | BP 베이스로 우선 노출 |
| `Keywords="search keywords"` | 에디터 검색 매치 |
| `DontUseGenericSpawnObject="true"` | `SpawnObject` BP 노드에서 제외 |

## Pitfalls

### 증상: 컴포넌트가 에디터의 Add Component 메뉴에 안 보임

- 원인: `meta=(BlueprintSpawnableComponent)` 누락
- 확인: `UCLASS(...)` 매크로 메타 부분 점검

### 증상: BP에서 부모 클래스 후보로 우리 클래스가 안 나옴

- 원인: `Blueprintable` 키워드 누락
- 확인: `UCLASS(Blueprintable)` 또는 `UCLASS(BlueprintType, Blueprintable)` 추가

### 증상: 다른 모듈에서 우리 클래스 사용 시 unresolved external

- 원인: `UE5TESTPROJECT_API`(또는 `MODULE_API`) 매크로 미부착
- 확인: 클래스 선언 앞에 모듈 API 매크로 부착 (`class UE5TESTPROJECT_API UMyClass`)

### 증상: 클래스 변경했는데 에디터의 BP 자식 클래스가 컴파일 에러

- 원인: 변경한 클래스가 `MinimalAPI`인데, BP가 자식 클래스 컴파일 시 추가 멤버 필요
- 확인: 멤버 함수에 `UE5TESTPROJECT_API` 명시적 부착하거나 `MinimalAPI` 제거

## References

- 출처: #19 NavDebugVisualizer 단계 2
- 관련: [UProperty.md](UProperty.md), [UEReflection.md](UEReflection.md), [UEModuleSystem.md](UEModuleSystem.md) §5
- 엔진 소스: `Engine/Source/Runtime/CoreUObject/Public/UObject/Class.h` (UClass), `Engine/Source/Runtime/CoreUObject/Public/UObject/ObjectMacros.h` (UCLASS 매크로)
