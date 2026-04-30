# NavigationSystem 모듈 실무 노트

UE 모듈 `NavigationSystem` (`Engine/Source/Runtime/NavigationSystem/`) 사용 시 노하우.

개념 분석은 `Docs/AI/RecastNavMesh/`. Build.cs 의존성 추가 일반론은 [`../BuildAndModule.md`](../BuildAndModule.md).

## 모듈 개요

`NavigationSystem` 모듈은 UE의 내비게이션 메시(NavMesh) 시스템을 제공한다. AI 경로 탐색, NavMesh 동적 수정, 경로 쿼리, 위치 → NavMesh 투사 등.

### 주요 헤더 (`Public/`)

| 헤더 | 들어있는 것 |
|---|---|
| `NavigationSystem.h` | `UNavigationSystemV1` (메인 진입점, `FindPathSync` / `ProjectPointToNavigation` / `GetDefaultNavDataInstance` 등) |
| `NavigationData.h` | `ANavigationData`, `FNavigationPath`, `FNavPathPoint`, `FNavPathSharedPtr` |
| `NavigationSystemTypes.h` | `FPathFindingQuery`, `FPathFindingResult`, `FNavLocation` |
| `NavMesh/RecastNavMesh.h` | `ARecastNavMesh` (Recast 기반 구체 NavMesh 구현체) |
| `NavModifierVolume.h` | `ANavModifierVolume` (런타임 NavMesh 수정 영역) |
| `NavAreas/NavArea.h` | `UNavArea` 베이스 (지역별 통과 비용 정의) |

### Build.cs 의존성 추가

게임 모듈에서 사용 시:

```csharp
// MyGameModule.Build.cs
PrivateDependencyModuleNames.AddRange(new[] { "NavigationSystem" });
```

`Public` vs `Private` 판단:
- 우리 모듈의 헤더에 NavSystem 타입(예: `UNavigationSystemV1*`)이 노출되면 → `Public`
- 우리 모듈의 cpp에서만 사용 → `Private` (권장)

자세한 룰은 [`../BuildAndModule.md`](../BuildAndModule.md) `#include` 위치와 Build.cs Public/Private 짝짓기 참고.

## 주제별 문서 (예정)

- (TBD) `PathQuery.md` — 경로 쿼리 (`FindPathSync`, `FPathFindingQuery`)
- (TBD) `NavMeshProjection.md` — 위치 → NavMesh 투사 (`ProjectPointToNavigation`)
- (TBD) `NavModifier.md` — `NavModifierVolume`, `NavArea`, `DynamicModifiersOnly`

각 주제는 첫 인사이트 추가 시점에 lazy 생성 (빈 자리잡기 X).

## References

- 출처: #19 RecastNavMesh 디버그 시각화 컴포넌트 1단계
- 모듈 위치: `Engine/Source/Runtime/NavigationSystem/`
- 개념 분석: `../../AI/RecastNavMesh/` (01-overview ~ 05-practical-guide)
- 관련 실무 문서: [`../BuildAndModule.md`](../BuildAndModule.md)
