# NavigationSystem — Path Query (FindPathSync / FPathFindingQuery)

UE의 `UNavigationSystemV1`을 통해 경로를 쿼리하는 실무 가이드. NavMesh 빌드/구조 자체는 [`Docs/AI/RecastNavMesh/`](../../AI/RecastNavMesh/) 참고.

## TL;DR

- 동기 vs 비동기: 디버그/즉시성 → `FindPathSync`, 게임 로직/긴 경로 → `FindPathAsync`.
- `FPathFindingQuery` Owner 인자(`this`)는 NavAgentProperties 추론용. `INavAgentInterface` 구현 시 자기 에이전트 속성 제공.
- 결과 체크는 `IsSuccessful()` + `Path.IsValid()` **둘 다** — 결과 코드 OK여도 Path null 엣지 케이스 존재.
- `Result.Path`는 `FNavPathSharedPtr` = `TSharedPtr<FNavigationPath, ESPMode::ThreadSafe>`. **UObject 아니라 UPROPERTY 부착 불가** — TSharedPtr이 lifetime 관리.
- 부분 경로(`IsPartial()`)는 별도 처리 가능 — 목적지 도달 못 했지만 중간까지 경로 있음.
- NavData nullptr 가능 (NavMesh 빌드 전, PIE 시작 직후) → early-return 방어 필수.

## Recipes

### 동기 경로 쿼리 (디버그/즉시성)

```cpp
UNavigationSystemV1* NavSys = UNavigationSystemV1::GetNavigationSystem(GetWorld());
if (!NavSys) return;

const ANavigationData* NavData = NavSys->GetDefaultNavDataInstance();
if (!NavData) return;   // NavMesh 빌드 전이면 nullptr

FPathFindingQuery Query(this, *NavData, StartLocation, EndLocation);
FPathFindingResult Result = NavSys->FindPathSync(Query);

if (Result.IsSuccessful() && Result.Path.IsValid())
{
    const TArray<FNavPathPoint>& Points = Result.Path->GetPathPoints();
    // 사용
}
```

비용:
- 짧은 경로 (수십 polygon): ~10μs
- 긴 경로 + 큰 NavMesh: ~수 ms
- **게임 스레드 블로킹** — 매 프레임 부담스러우면 캐싱 또는 Async

### 비동기 경로 쿼리 (게임 로직)

```cpp
// Delegate 바인딩
FNavPathQueryDelegate Delegate;
Delegate.BindLambda([](uint32 QueryID, ENavigationQueryResult::Type Result, FNavPathSharedPtr Path) {
    // 콜백 (게임 스레드)
});

NavSys->FindPathAsync(NavAgentProperties, Query, Delegate);
```

- 별도 스레드에서 계산 → 게임 스레드 블로킹 X
- 콜백은 다음 프레임 게임 스레드에서 호출
- AI 등 매 프레임 인터랙티브 갱신 필요 없는 케이스에 적합

### 부분 경로 활용 (장애물 막힌 경우)

```cpp
if (Result.IsPartial())
{
    // 목적지 도달 못 했지만 가능한 한 가까이 경로 있음
    // AI: 그래도 이동 시작 (가까이 가다가 막히면 재탐색)
}
```

## FPathFindingQuery 인자 매트릭스

```cpp
FPathFindingQuery(const UObject* InOwner,
                  const ANavigationData& InNavData,
                  const FVector& Start,
                  const FVector& End,
                  FSharedConstNavQueryFilter SourceQueryFilter = NULL,
                  FNavPathSharedPtr InPathInstanceToFill = NULL,
                  const FVector::FReal CostLimit = TNumericLimits<FVector::FReal>::Max(),
                  const bool bInRequireNavigableEndLocation = true);
```

| 인자 | 의미 |
|---|---|
| `InOwner` | NavAgentProperties 추론 + 로깅/추적. `INavAgentInterface` 구현 시 자기 에이전트 속성 제공. 미구현이면 기본 NavAgent |
| `InNavData` | 어느 NavMesh 위에서 탐색할지. 보통 `GetDefaultNavDataInstance()`. 에이전트별 NavMesh가 여러 개면 `GetNavDataForProps(AgentProps)` |
| `Start`, `End` | 월드 좌표 — NavMesh 위가 아니어도 됨 (자동 투사) |
| `SourceQueryFilter` | NavArea별 비용 필터 (예: 물 = 비용 2배). null이면 NavAgent 기본 필터 |
| `InPathInstanceToFill` | 기존 path 객체 재사용 (allocation 절감) |
| `CostLimit` | 비용 상한 — 너무 멀리 가는 경로 차단 |
| `bInRequireNavigableEndLocation` | true면 End가 NavMesh 위 아니면 실패. false면 가장 가까운 NavMesh 위치로 자동 투사 |

## FPathFindingResult 체크 패턴

```cpp
struct FPathFindingResult
{
    FNavPathSharedPtr Path;
    ENavigationQueryResult::Type Result;  // Success, Fail, Error 등
    
    bool IsSuccessful() const;
    bool IsPartial() const;
};
```

체크 순서:
1. `IsSuccessful()` — 결과 코드 검사
2. `Path.IsValid()` — TSharedPtr 유효성 (IsSuccessful 통과해도 Path null 엣지 케이스)
3. `IsPartial()` — 부분 경로면 별도 처리 (선택)

```cpp
if (!Result.IsSuccessful() || !Result.Path.IsValid())
{
    // 실패 처리
    return;
}

if (Result.IsPartial())
{
    // 부분 경로 — 가능한 한 이동 또는 재탐색 결정
}

const TArray<FNavPathPoint>& Points = Result.Path->GetPathPoints();
```

## FNavPathSharedPtr — GC 시스템 밖

```cpp
typedef TSharedPtr<struct FNavigationPath, ESPMode::ThreadSafe> FNavPathSharedPtr;
```

- `FNavigationPath`는 **UObject 아닌 struct** → UPROPERTY 부착 불가
- `TSharedPtr<..., ThreadSafe>`로 lifetime 관리 — async path 결과를 게임 스레드에서 받을 때 race 방지
- 멤버 변수로 보관하려면:

```cpp
class UMyAIComp : public UActorComponent
{
    FNavPathSharedPtr CachedPath;   // OK — UPROPERTY 없이 TSharedPtr이 lifetime 관리
};
```

- TSharedPtr 참조 카운트가 0이 되면 path destroy (GC 안 거침)
- 즉 우리가 들고 있는 동안은 path가 살아있음 (raw pointer 댕글링 위험 X — TSharedPtr이 막아줌)

## FNavPathPoint 구조

```cpp
struct FNavPathPoint : public FNavLocation
{
    FVector Location;          // 월드 좌표
    NavNodeRef NodeRef;        // 이 점이 속한 NavMesh polygon ID
    uint32 Flags;              // 특수 플래그 (NavLink 등)
    uint16 CustomLinkId;       // CustomNavLink 식별자
};
```

- 단순 좌표 점이 아님 — **NavMesh polygon + 메타** 함께
- `NodeRef`로 어느 polygon인지 추적 가능 (디버그 시 NavMesh 구조 검증)
- `Flags` — `FNavPathPoint::NavLinkBit` 등으로 점프/워프 링크 표시

## Pitfalls

### 증상: 경로가 항상 실패 (직선 또는 PATH NOT FOUND)

- 원인 1: NavMesh 빌드 안 됨 (`NavData` nullptr)
  - 확인: `GetDefaultNavDataInstance()` 결과 검사
  - 해결: 에디터 Build → Build Paths, 또는 ARecastNavMesh를 레벨에 배치
- 원인 2: Start/End가 NavMesh 영역 밖 (자동 투사 거리 초과)
  - 확인: 두 점이 NavMesh 위 또는 근처에 있는지
  - 해결: `bInRequireNavigableEndLocation = false`로 자동 투사 활성화, 또는 `ProjectPointToNavigation`으로 먼저 투사
- 원인 3: 두 점이 분리된 NavMesh 영역 (섬, 막힌 방 등)
  - 확인: NavMesh 표시(`P` 키 PIE 콘솔 또는 `show navigation`)로 연결성 확인

### 증상: PIE 시작 직후 몇 프레임 경로 안 그려짐

- 원인: NavMesh 빌드가 비동기 — PIE 시작 시 아직 빌드 중일 수 있음
- 확인: `NavData` nullptr → early-return 동작 중 (정상)
- 해결: 빌드 끝나기 기다리거나, `Project Settings > Navigation Mesh > Force Rebuild on Load` 활성화

### 증상: 매 프레임 FindPathSync 호출이 무거움 (큰 경로 + 큰 NavMesh)

- 원인: Sync 호출은 게임 스레드 블로킹
- 해결: 캐싱 (목적지 변경 시만 재쿼리) 또는 `FindPathAsync` 전환

### 증상: 부분 경로인데 게임 로직이 인지 못 함

- 원인: `IsSuccessful()`은 부분 경로도 true 반환
- 확인: `IsPartial()` 별도 체크
- 해결: 부분이면 AI가 가능한 한 이동 + 일정 간격 재탐색

### 증상: `FNavPathSharedPtr` 멤버에 UPROPERTY 붙였는데 빌드 에러

```
error: 'TSharedPtr' is not supported by UnrealHeaderTool
```

- 원인: UPROPERTY는 UObject 메타용. TSharedPtr<struct> 같은 비 UObject 타입엔 부착 불가
- 확인: 멤버 선언에서 UPROPERTY 제거
- 해결: 그냥 `FNavPathSharedPtr CachedPath;` — TSharedPtr이 lifetime 관리하므로 UPROPERTY 불필요

## References

- 출처: #19 NavDebugVisualizer 단계 3
- 관련 실무 문서: [`../DebugVisualization.md`](../DebugVisualization.md), [`../UProperty.md`](../UProperty.md) (UPROPERTY와 비-UObject 차이)
- 분석 문서: [`../../AI/RecastNavMesh/`](../../AI/RecastNavMesh/) (NavMesh 빌드, 폴리곤 구조 등)
- 엔진 소스:
  - `Engine/Source/Runtime/NavigationSystem/Public/NavigationSystem.h:636-744` — `UNavigationSystemV1::FindPathSync`, `GetNavigationSystem`, `GetDefaultNavDataInstance`, `ProjectPointToNavigation`
  - `Engine/Source/Runtime/NavigationSystem/Public/NavigationSystemTypes.h:36-75` — `FPathFindingQuery`, `FPathFindingResult`
  - `Engine/Source/Runtime/NavigationSystem/Public/NavigationData.h:307-329` — `FNavigationPath::GetLength`, `GetPathPoints`
