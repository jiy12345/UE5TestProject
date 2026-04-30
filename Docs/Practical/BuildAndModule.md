# Build.cs와 모듈 의존성

UE의 `Build.cs`(ModuleRules)에서 모듈 의존성을 선언하는 실무 가이드. 큰 그림(왜 모듈인가)은 [UEModuleSystem.md](UEModuleSystem.md) 참고.

## TL;DR

- **Private 우선, 헤더 노출 필요해지면 Public** (최소 권한 원칙).
- `Public/Private`는 헤더 전이뿐 아니라 **다운스트림 Build.cs 명시 강제, PCH 후보 풀, Restricted Folder 검증, 의존성 의도 명시**까지 결정한다. `#include` 위치(.h vs .cpp)와는 다른 추상화 레벨.
- Build.cs 한 줄 추가 = 컴파일러 `-I`/`/I` 자동 노출 (강제 `#include` X).
- Unity Build = 여러 cpp를 `#include`로 묶은 하나의 번역 단위. 어느 cpp가 헤더 인클루드 → **같은 unity 파일 전체**가 그 헤더에 의존.
- PCH 내용은 **PCH 헤더 파일(.h) 1개의 `#include` 트리만으로** 결정. cpp의 `#include`는 PCH에 영향 0. Build.cs는 "PCH 헤더가 인클루드한 헤더가 적법한가"의 권한 체크 + 후보 풀 정의.
- 런타임 영향: RAM 사용량 디맨드 페이징으로 무영향. **i-cache 히트율은 이론적 영향 (inline 사본 흩뿌려짐)**, 실측은 거대 코드베이스에서만 의미.

## Recipes

### `#include` 위치와 Build.cs Public/Private 짝짓기

| 헤더 노출 상태 | `.h`/`.cpp` 어디에 `#include` | `Build.cs` |
|---|---|---|
| 우리 .h가 `OtherType` 멤버/시그니처 직접 노출 | 우리 .h | **Public** |
| 우리 .cpp에서만 `OtherType` 사용 | 우리 .cpp | **Private** |
| 우리 .h에 forward declare(`class OtherType;`)만, 실제 호출은 .cpp | 우리 .cpp | **Private** |

강제로 묶이진 않지만 어긋나면 빌드 실패 (헤더에 노출했는데 Build.cs Private이면 다운스트림이 우리 .h 인클루드 시 OtherType을 못 찾음).

### 새 모듈 의존성 추가 절차

1. 우리 모듈 코드에서 새 모듈의 타입을 `.h`/`.cpp` 어디에서 쓰는지 확인
2. 헤더 노출이면 → `PublicDependencyModuleNames` / cpp 한정이면 → `PrivateDependencyModuleNames`
3. 모듈명은 `Engine/Source/Runtime/<ModuleName>/` 의 `<ModuleName>` 그대로 (PascalCase)
4. UBT 재실행 (.uproject 우클릭 "Generate Visual Studio project files" 또는 다음 빌드 자동 처리)

### `Public` 의존성이 다운스트림에 자동 전이되는 메커니즘

Build.cs에서 모듈 X를 `Public`으로 추가하면, X의 `Public/` 디렉토리가 `CppCompileEnvironment.UserIncludePaths`에 들어감. 툴체인이 컴파일러 호출 시 자동으로 `/I"<path>"`(MSVC) 또는 `-I"<path>"`(Clang) 인자 생성 → 다운스트림 cpp에서 `#include "X_Header.h"`를 자기 Build.cs에 X 명시 안 해도 가능.

```csharp
// 우리 모듈
PublicDependencyModuleNames.AddRange(new[] { "X" });

// 다운스트림 모듈 Y (자동 효과)
// → Y의 cpp에서 #include "X_Header.h" 가능 (Y의 Build.cs에 X 안 적어도 됨)
```

## 핵심 개념 — `#include` 위치 ≠ Build.cs Public/Private

이 둘은 **다른 추상화 레벨**이다.

| 제어 대상 | 결정자 |
|---|---|
| 헤더 → 헤더 전이 (소스 레벨) | `#include` 위치 (.h vs .cpp) |
| 모듈 → 모듈 의존성 인지 (빌드 그래프 노드) | Build.cs `PublicDependencyModuleNames` vs `PrivateDependencyModuleNames` |
| 다운스트림이 같은 모듈 또 명시할 필요 | Build.cs Public/Private |
| PCH 후보 풀 / 권한 체크 | Build.cs Public/Private |
| Restricted Folder / 라이선스 검증 범위 | Build.cs Public/Private |
| 코드 리뷰에서 의존성 의도 가시성 | Build.cs Public/Private |

**"include 위치만 잘 두면 전이는 제어된다, 그런데 왜 Build.cs까지?"**의 답:
- `#include` 위치는 *헤더 차원*의 전이만 다룸
- Build.cs Public/Private은 *모듈 그래프 차원*에서 5가지를 동시에 결정 (위 표)
- 둘이 자연스럽게 짝지어지지만, 강제는 아님 → 어긋나면 다운스트림 빌드 실패

### A → B(Private) → C 전이 룰

- B의 공개 시그니처(헤더의 클래스/함수 선언)에 A 타입 노출 X → C는 A 몰라도 됨 → A→B는 **Private**
- B 헤더에 A 타입 노출 → C가 B 헤더 인클루드 시 A_Header.h 필요 → A→B는 **Public** 필요

## Build.cs ↔ Unity Build 관계

UE의 Unity Build는 빌드 시간 단축을 위해 모듈 내 여러 cpp를 하나의 번역 단위로 묶는다.

```cpp
// (자동 생성) Module.UE5TestProject.cpp
#include "C:/.../UE5TestProject.cpp"
#include "C:/.../UE5TestProjectGameMode.cpp"
#include "C:/.../NavDebugVisualizerComponent.cpp"
// ...
```

**Build.cs에 모듈 추가는 Unity 파일에 그 모듈 헤더를 강제 삽입하지 않는다.** 각 cpp의 `#include`만 처리됨.

→ 헤더 변경 시 재컴파일 범위:
- 그 헤더를 인클루드한 cpp가 들어있는 unity 파일 **전체** 재컴파일
- 같은 unity의 무관한 cpp들도 같이 휘말림 (Unity Build의 본질적 trade-off)

## Public 의존성의 비용

다운스트림 모듈에 미치는 영향:
- **다운스트림 컴파일 시간 ↑** (transitive include path 추가)
- **헤더 변경 시 재컴파일 범위 ↑** (다운스트림 unity 파일까지)
- **다운스트림 cpp의 전처리 후 코드 크기 ↑** (다운스트림이 우리 모듈 헤더를 통해 transitive로 큰 헤더 받게 되는 경우)

런타임 영향:
- RAM 사용량 — OS 디맨드 페이징으로 무영향 (호출 안 되는 함수는 안 올라옴)
- 바이너리 크기 — inline 함수/template 인스턴스화 폭증 시 증가, 일반 선언만이면 링커가 약 디덕션
- **i-cache 히트율** — 이론적 영향 있음. inline 함수 사본이 흩뿌려져 핫패스 사이 거리 ↑ → 미스율 ↑. 실측은 AAA 거대 코드베이스에서만 의미

## PCH (Precompiled Header) 정확한 동작

### 영향 매트릭스

같은 모듈에 cpp1, cpp2가 있고 모듈 PCH 헤더가 `MyModulePCH.h`인 상황:

| 어디에 `#include "OtherModule.h"`? | PCH 내용 | cpp1 코드 크기 | cpp2 코드 크기 |
|---|---|---|---|
| 어디에도 X | OtherModule 모름 | 0 | 0 |
| **cpp1에만** | OtherModule 모름 | OtherModule만큼 ↑ | **0** |
| cpp1 + cpp2 둘 다 | OtherModule 모름 | 각자 ↑ (개별) | 각자 ↑ (개별) |
| **`MyModulePCH.h`에 작성** | OtherModule 포함 | 모든 cpp ↑ | 모든 cpp ↑ |

### 메커니즘

1. **PCH 컴파일 입력 = PrivatePCHHeaderFile 1개**(Build.cs 지정)와 그 인클루드 트리만. 모듈의 다른 cpp는 PCH 생성에 입력 X
2. **PCH는 `/FI`(MSVC) / `-include`(Clang)로 모듈 모든 cpp에 자동 강제 인클루드** — cpp가 `#include` 안 적어도 들어감
3. cpp는 PCH의 **소비자**, 생성자가 아님 — cpp의 `#include`는 PCH에 영향 0
4. PCH가 큰 헤더를 미리 파싱해 캐시 → cpp 컴파일 시 재파싱 X → 컴파일 시간 ↓ (PCH 자체 컴파일은 처음 한 번 비쌈)

### Build.cs Public/Private과 PCH 관계

- Build.cs `Public` 의존성 = PCH 헤더가 인클루드할 수 있는 **권한 부여 + 후보 풀 정의**
- Build.cs는 **PCH에 자동으로 헤더를 박지 않는다** — 사람이 PCH 헤더 파일에 `#include`를 작성해야 PCH에 들어감
- 따라서 "Build.cs 한 줄 추가 = 자동 코드 크기 영향"의 직접 인과는 **없음**
- 예외: SharedPCH 자동 채택 케이스 (UE 엔진 모듈에서만 의미, 일반 게임 모듈 무관)

## Pitfalls

### 증상: 다른 모듈의 헤더를 `#include`했는데 "cannot open include file" 빌드 에러

- 원인: Build.cs에 그 모듈을 의존성으로 추가 안 함
- 확인: `Engine/Source/Runtime/<모듈>/Public/<헤더>.h` 위치 찾고, 그 모듈명을 우리 Build.cs `PublicDependencyModuleNames` 또는 `PrivateDependencyModuleNames`에 추가

### 증상: 우리 모듈은 빌드되는데 다운스트림 모듈이 우리 .h 인클루드 시 빌드 실패

- 원인: 우리 .h에서 다른 모듈 타입(예: `ANavigationData*`)을 노출했는데, Build.cs에서 그 모듈을 `Private` 의존으로 둠 → 다운스트림이 그 모듈 헤더를 못 찾음
- 확인: 우리 .h의 시그니처에 다른 모듈 타입이 보이는지 → 보이면 그 모듈을 `Public`으로 옮기거나, 우리 .h에 `class ANavigationData;` forward declare로 바꾸고 .cpp에서만 인클루드

### 증상: 우리 모듈의 cpp 한 줄 고쳤는데 컴파일이 길게 걸림

- 원인: 그 cpp가 큰 헤더(예: `NavigationSystem.h`)를 인클루드 → 같은 unity 파일의 다른 cpp들도 함께 재컴파일 (Unity Build)
- 확인: `Game/Intermediate/Build/.../UnityBuild` 디렉토리에서 우리 cpp가 어느 unity 파일에 묶였는지 확인. 자주 변경되는 cpp는 unity에서 분리하기(`bUseUnityBuild = false` 또는 adaptive unity build) 검토

### 증상: PCH 헤더 한 줄 고쳤는데 모듈 전체가 재컴파일

- 원인: PCH는 모듈 모든 cpp의 prerequisite → PCH 헤더 변경 시 PCH 재빌드 → 모든 cpp 재컴파일
- 확인: PCH 헤더에 자주 변경되는 헤더는 절대 넣지 말 것. 안정적인 인프라 헤더만 (Core, CoreUObject 등)

## References

- 출처: #19 RecastNavMesh 디버그 시각화 컴포넌트 1단계
- 엔진 소스:
  - `Engine/Source/Programs/UnrealBuildTool/Configuration/UEBuildModule.cs:680` — Public include path 추가
  - `Engine/Source/Programs/UnrealBuildTool/Configuration/UEBuildModule.cs:564-605` — Public/Private 의존성 propagation 차이
  - `Engine/Source/Programs/UnrealBuildTool/Configuration/UEBuildModule.cs:502-503` — Restricted folder 검증
  - `Engine/Source/Programs/UnrealBuildTool/Configuration/UEBuildModule.cs:1122-1156` — PCH 의존성 후보 (Public/Private 분기)
  - `Engine/Source/Programs/UnrealBuildTool/Configuration/UEBuildModuleCPP.cs:1418-1488` — `GetSharedPCHModuleDependencies` (PCH 헤더 실제 파싱)
  - `Engine/Source/Programs/UnrealBuildTool/Configuration/UEBuildModuleCPP.cs:1543-1570` — `CreatePrivatePCH` (PCH 컴파일 입력 = wrapper 1개)
  - `Engine/Source/Programs/UnrealBuildTool/Configuration/UEBuildModuleCPP.cs:804` — PCH를 `ForceIncludeFiles`에 추가
  - `Engine/Source/Programs/UnrealBuildTool/System/Unity.cs:282-293` — Unity 파일 생성 (cpp들을 `#include`로 묶음)
  - `Engine/Source/Programs/UnrealBuildTool/ToolChain/ClangToolChain.cs:567` — `-I` 인자 생성
  - `Engine/Source/Programs/UnrealBuildTool/ToolChain/ClangToolChain.cs:593` — `-include` (Clang force include)
  - `Engine/Source/Programs/UnrealBuildTool/ToolChain/IWYUToolChain.cs:234` — `/FI` (MSVC force include)
- 큰 그림: [UEModuleSystem.md](UEModuleSystem.md)
