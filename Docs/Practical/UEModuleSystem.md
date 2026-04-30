# UE 모듈 시스템 — 왜 모듈인가

UE5 코드는 모놀리식이 아닌 **모듈** 단위로 구성된다. 한 모듈은 보통 `Engine/Source/Runtime/<ModuleName>/` 또는 플러그인의 `Source/<ModuleName>/`에 대응하며, `Build.cs`로 빌드 규칙을 선언한다.

이 문서는 "왜 그렇게 했는가"를 다룬다. "어떻게 쓰는가"(Build.cs 작성법, Public/Private 룰 등)는 [BuildAndModule.md](BuildAndModule.md) 참고.

## TL;DR

UE 모듈 시스템 존재 이유 5가지:

1. **컴파일 시간 절감** — 변경된 모듈만 재빌드, 의존성 그래프 따라 다운스트림만 영향
2. **의존성 명시·강제** — `Build.cs`에 의존성을 코드로 선언. 의도치 않은 의존성을 빌드 에러로 즉시 발견 (아키텍처 가드)
3. **모듈 단위 빌드 제외** — `*.uplugin`/`*.uproject`의 `Modules` 항목으로 모듈 통째 제외 가능. 서버 빌드에서 UI 모듈 빼기, 플랫폼별 활성화 등
4. **Hot Reload / Live Coding 기반** — 모듈을 DLL로 빌드 → 에디터 실행 중 동적 재로드 가능. 모놀리식이면 불가능
5. **API 캡슐화** — `MODULE_API` 매크로(`UE5TESTPROJECT_API` 등) = DLL export/import 경계. 안 붙이면 다른 모듈에서 사용 불가 (링크 에러로 강제)

## 5가지 이유 자세히

### 1. 컴파일 시간 절감

모놀리식 빌드는 한 줄 고치면 전체 재컴파일. 모듈 시스템은:
- 변경된 cpp가 속한 unity 파일만 재컴파일 (Unity Build 단위)
- 변경된 헤더가 속한 모듈을 의존하는 다운스트림 모듈들만 영향
- 그래프 노드 단위로 빌드 액션 결정 — UBT의 의존성 추적 알고리즘이 이걸 자동화

→ 결과: 큰 변경(헤더 수정)은 비싸지만 작은 변경(cpp 한 줄)은 모듈 1개만 재컴파일

### 2. 의존성 명시·강제 (아키텍처 가드)

`Build.cs`는 모듈의 의존성을 명시적으로 선언:

```csharp
PublicDependencyModuleNames.AddRange(new[] { "Core", "CoreUObject", "Engine" });
PrivateDependencyModuleNames.AddRange(new[] { "NavigationSystem" });
```

- 의도치 않은 의존성 생성 시 **빌드 에러로 즉시 발견** (예: NavMesh 모듈이 갑자기 UI 모듈 의존하면 컴파일 자체가 안 됨)
- 의존성 그래프 시각화/감사 가능 — 코드 리뷰 시 "이 모듈은 무엇에 의존하는가"가 30줄 이내에 명시됨
- 순환 의존성도 빌드 시점에 감지 (UBT가 그래프 검사)

### 3. 모듈 단위 빌드 제외

`UE5TestProject.uproject`의 `Modules` 항목 또는 `*.uplugin`의 모듈 정의에서:

```json
{
    "Modules": [
        {
            "Name": "UE5TestProject",
            "Type": "Runtime",
            "LoadingPhase": "Default",
            "WhitelistPlatforms": ["Win64", "Mac"],
            "BlacklistPlatforms": ["Server"]
        }
    ]
}
```

→ 서버 빌드에서 UI 관련 모듈 통째 제외 → 빌드 시간 + 바이너리 크기 동시 절감
→ 플랫폼별 모듈 활성화 (PS5 전용 모듈은 PS5 빌드에서만)

### 4. Hot Reload / Live Coding

모듈 = DLL 단위 → 에디터 실행 중에도 모듈 DLL을 동적 재로드 가능:
- Hot Reload: 한 모듈만 다시 빌드해 DLL 교체 (전체 재시작 X)
- Live Coding: 더 빠른 in-process 패치 (UE 5.x 표준)

모놀리식이면 매번 전체 재시작 → 개발 사이클 매우 느려짐

### 5. API 캡슐화 — `MODULE_API` 매크로

각 모듈은 자기 이름에서 파생된 API 매크로를 가짐:
- `UE5TestProject` 모듈 → `UE5TESTPROJECT_API`
- `Engine` 모듈 → `ENGINE_API`
- `NavigationSystem` 모듈 → `NAVIGATIONSYSTEM_API`

DLL export/import 경계:
- 모듈 빌드 시: `UE5TESTPROJECT_API` = `__declspec(dllexport)` (이 모듈이 외부에 공개)
- 다른 모듈이 사용 시: `UE5TESTPROJECT_API` = `__declspec(dllimport)` (이 모듈로부터 가져옴)

```cpp
// 다른 모듈에서 사용 가능 (export됨)
class UE5TESTPROJECT_API UPlayerStatsModel : public UObject { ... };

// 같은 모듈 내부에서만 사용 가능 (export 안 됨, 다른 모듈이 링크 시 unresolved external)
class UPlayerStatsModelInternal { ... };
```

매크로 안 붙이면 같은 모듈 내에서는 보이지만 **다른 모듈에서 사용 시 링크 에러**로 강제됨 → API 의도치 않은 노출 방지.

## 모듈 시스템의 단점 / Trade-off

- **Build.cs 관리 부담** — 새 의존성 추가 시 코드 + Build.cs 둘 다 수정 필요
- **DLL 로딩 오버헤드** — 에디터 시작 시 수백 개 DLL 로드 (런타임 게임 빌드는 모놀리식 옵션 있음)
- **순환 의존성 골치** — UBT가 막아주지만, 설계가 꼬이면 모듈 분리/병합 필요
- **헤더 노출 결정 비용** — Public/Private 결정에 사람의 판단 필요 (자동화 X)

## References

- 출처: #19 RecastNavMesh 디버그 시각화 컴포넌트 1단계
- 관련 실무 문서: [BuildAndModule.md](BuildAndModule.md) (Build.cs 작성법, Public/Private 룰)
- UE 공식: [Unreal Engine Modules](https://docs.unrealengine.com/5.0/en-US/unreal-engine-modules/) (외부 링크)
