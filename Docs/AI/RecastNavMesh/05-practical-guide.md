# 05. 실전 활용 가이드

> **작성일**: 2026-03-23
> **엔진 버전**: UE 5.7

## 1. 기본 설정

### RecastNavMesh 배치 및 설정

레벨에 `RecastNavMesh-Default` 액터가 자동 배치됩니다. 주요 설정:

| 항목 | 프로퍼티 | 권장값 | 설명 |
|------|----------|--------|------|
| 타일 크기 | `TileSizeUU` | 1000 ~ 3000 UU | 크게 할수록 빌드 속도 ↓, 타일 수 ↓ |
| Cell Size | `NavMeshResolutionParams[Default].CellSize` | 10 ~ 20 UU | 정밀도 vs 성능 |
| Cell Height | `NavMeshResolutionParams[Default].CellHeight` | 10 UU | 수직 해상도 |
| Agent Radius | `AgentRadius` | 캐릭터 캡슐 반경과 일치 | |
| Agent Height | `AgentHeight` | 캐릭터 키와 일치 | |
| Runtime Generation | `RuntimeGeneration` | 목적에 맞게 | 아래 참조 |

### RuntimeGeneration 선택 기준

```
Static           → 정적 월드. 런타임 NavMesh 변경 없음 (최고 성능)
DynamicModifiersOnly → Nav Modifier(NavArea, 비용 변경)만 런타임에 추가/제거
Dynamic          → 지오메트리 자체가 변하는 경우 (문, 파괴 가능 벽 등)
```

---

## 2. C++에서 NavMesh 쿼리

### 기본 경로 쿼리

```cpp
#include "NavigationSystem.h"
#include "NavigationPath.h"

// 경로 찾기 (동기)
UNavigationSystemV1* NavSys = UNavigationSystemV1::GetNavigationSystem(GetWorld());
if (!NavSys) return;

FNavLocation StartLoc, EndLoc;
NavSys->ProjectPointToNavigation(StartPos, StartLoc, FVector(100.f, 100.f, 200.f));
NavSys->ProjectPointToNavigation(EndPos,   EndLoc,   FVector(100.f, 100.f, 200.f));

FPathFindingQuery Query(this, *NavSys->GetDefaultNavDataInstance(),
                         StartLoc.Location, EndLoc.Location);
FPathFindingResult Result = NavSys->FindPathSync(Query);

if (Result.IsSuccessful() && Result.Path.IsValid())
{
    const TArray<FNavPathPoint>& Points = Result.Path->GetPathPoints();
    // Points[0] = 시작, Points.Last() = 도착
}
```

#### FindPathSync 내부 로직과 시간 복잡도

**호출 체인**:
```
UNavigationSystemV1::FindPathSync()
  └── ANavigationData::FindPath() (virtual)
       └── ARecastNavMesh::FindPath()
            └── FPImplRecastNavMesh::FindPath()
                 ├── dtNavMeshQuery::findNearestPoly()   ← Start/End 포인트를 폴리곤으로 투사
                 ├── dtNavMeshQuery::findPath()          ← A* 폴리곤 경로 탐색
                 └── dtNavMeshQuery::findStraightPath()  ← 폴리곤 경로 → 직선 경로 (Funnel)
```

**각 단계의 시간 복잡도** (V = 검색 구역 내 폴리곤 수, E = 폴리곤 간 링크 수):

| 단계 | 파일 (엔진) | 복잡도 | 설명 |
|------|--------|--------|------|
| `findNearestPoly()` | `DetourNavMeshQuery.cpp` | **O(log N + k)** | BV 트리 가속 검색. N=타일당 폴리곤, k=반환된 후보 수 |
| `findPath()` (A\*) | `DetourNavMeshQuery.cpp:1578` | **O((V+E) log V)** | 바이너리 힙 우선순위 큐 기반 A\*. `RECAST_MAX_SEARCH_NODES=2048` 제한 |
| `findStraightPath()` (Funnel) | `DetourNavMeshQuery.cpp` | **O(P)** | P=폴리곤 경로 길이. Simple Stupid Funnel Algorithm |

**A\* findPath() 세부 로직** (`DetourNavMeshQuery.cpp:1578~1700`):

```cpp
dtStatus dtNavMeshQuery::findPath(dtPolyRef startRef, dtPolyRef endRef,
    const dtReal* startPos, const dtReal* endPos, const dtReal costLimit,
    const dtQueryFilter* filter, dtQueryResult& result, dtReal* totalCost)
{
    // 1. 시작 노드 초기화
    dtNode* startNode = m_nodePool->getNode(startRef);
    startNode->cost = 0;
    startNode->total = heuristicCost(startPos, endPos);  // H_SCALE 가중 휴리스틱
    m_openList->push(startNode);

    // 2. A* 메인 루프
    while (!m_openList->empty())
    {
        dtNode* bestNode = m_openList->pop();   // f-cost 최소 노드
        if (bestNode->id == endRef) break;      // 목표 도달

        // 3. 이웃 폴리곤 확장 (dtLink를 통해)
        for (링크 in bestTile->links[bestPoly])
        {
            if (!filter->passFilter(neighbour)) continue;  // Null area 등 제외

            float cost = bestNode->cost + edgeCost;        // g-cost
            float total = cost + heuristic(neighbour, end);// f = g + h
            if (total < neighbourNode->total)
                m_openList->update(neighbourNode);         // 더 나은 경로 발견
        }
    }

    // 4. 목표에서 역추적하여 폴리곤 경로 구성
    // 5. 결과 반환
}
```

**성능 특징:**
- **노드 풀 제한**: `RECAST_MAX_SEARCH_NODES=2048` — 매우 긴 경로(예: 수백 m 거리의 복잡한 미로)에서 노드 풀 고갈 시 `DT_OUT_OF_NODES` 반환
- **휴리스틱 가중치 `H_SCALE`**: 기본 약 1.0. 높이면 탐색이 목표 방향으로 편향되어 빨라지지만 최적 경로 보장 안 됨
- **타일 기반 pruning**: A\*는 타일 경계를 넘어 이웃 타일의 폴리곤으로 이어짐 — 로드되지 않은 타일은 자동으로 탐색에서 배제됨 (WP/레벨 스트리밍)

**실무적 복잡도**:
- 단거리 (동일 타일 내): 수십 ~ 수백 μs
- 장거리 (수십 타일 횡단): 수 ms, 최악 수십 ms
- 경로 블로킹(완전 막힘) 감지: 전체 `MAX_SEARCH_NODES`를 다 쓰고 나서 실패 — 가장 비싼 경우

비동기 경로 요청(`FindPathAsync`)은 이 계산을 워커 스레드에서 수행합니다 (`dtNavMeshQuery`는 스레드별 인스턴스 필요, 3-3절 비동기 패턴 참조).

### NavQueryFilter 커스터마이징

```cpp
// 특정 NavArea 비용을 높여서 우회하게 만들기
UNavigationQueryFilter* Filter = NewObject<URecastFilter_UseDefaultArea>(this);
// 또는 C++에서 직접:
FSharedNavQueryFilter QueryFilter = NavData->GetDefaultQueryFilter()->GetCopy();
QueryFilter->SetAreaCost(RECAST_NULL_AREA, FLT_MAX);   // Null 영역 통과 불가
QueryFilter->SetAreaCost(RECAST_DEFAULT_AREA, 1.0f);   // 기본 영역 비용

FPathFindingQuery Query(this, *NavData, Start, End, QueryFilter);
```

### 특정 위치가 NavMesh 위에 있는지 확인

```cpp
FNavLocation NavLocation;
bool bOnNavMesh = NavSys->ProjectPointToNavigation(
    ActorLocation,
    NavLocation,
    FVector(50.f, 50.f, 100.f)  // 검색 반경
);
```

---

## 3. 커스텀 NavArea 만들기

### C++ NavArea 정의

```cpp
// NavArea_Water.h
#pragma once
#include "NavAreas/NavArea.h"
#include "NavArea_Water.generated.h"

UCLASS()
class UNavArea_Water : public UNavArea
{
    GENERATED_BODY()
public:
    UNavArea_Water()
    {
        DefaultCost = 3.0f;           // 물 위는 3배 비용
        DrawColor = FColor(0, 100, 255);
    }
};
```

### Nav Modifier Volume에 적용

에디터에서 `NavModifierVolume` 액터를 배치하고 `Area Class`를 `NavArea_Water`로 설정합니다.
런타임에서 C++로:

```cpp
#include "NavModifierVolume.h"

ANavModifierVolume* ModVol = GetWorld()->SpawnActor<ANavModifierVolume>(
    ANavModifierVolume::StaticClass(), SpawnParams);
ModVol->SetAreaClass(UNavArea_Water::StaticClass());
// 위치/크기 설정...
```

---

## 4. 이해도 확인 실습 — NavMesh 디버그 시각화

분석한 내용을 직접 확인하는 C++ GameMode/Actor를 구현하여 `DynamicModifiersOnly` 동작을 이해합니다.

### 실습 목표

- NavMesh 타일 경계 시각화
- 특정 위치의 NavArea ID / 이동 비용 출력
- 경로 쿼리 결과 실시간 렌더링

### NavDebugVisualizerComponent

```cpp
// NavDebugVisualizerComponent.h
#pragma once
#include "Components/ActorComponent.h"
#include "NavMesh/RecastNavMesh.h"
#include "NavDebugVisualizerComponent.generated.h"

UCLASS(ClassGroup=(Navigation), meta=(BlueprintSpawnableComponent))
class UNavDebugVisualizerComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    virtual void TickComponent(float DeltaTime, ELevelTick TickType,
                               FActorComponentTickFunction* ThisTickFunction) override;

    /** 경로 쿼리 시작점 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="NavDebug")
    AActor* PathStart = nullptr;

    /** 경로 쿼리 끝점 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="NavDebug")
    AActor* PathEnd = nullptr;

    /** 활성화 여부 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="NavDebug")
    bool bEnabled = true;

private:
    void DrawNavPath();
    void DrawNavMeshInfo(const FVector& Location);
};
```

```cpp
// NavDebugVisualizerComponent.cpp
#include "NavDebugVisualizerComponent.h"
#include "NavigationSystem.h"
#include "DrawDebugHelpers.h"
#include "NavMesh/RecastNavMesh.h"

void UNavDebugVisualizerComponent::TickComponent(
    float DeltaTime, ELevelTick TickType,
    FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
    if (!bEnabled) return;

    DrawNavPath();
    if (PathStart)
        DrawNavMeshInfo(PathStart->GetActorLocation());
}

void UNavDebugVisualizerComponent::DrawNavPath()
{
    if (!PathStart || !PathEnd) return;

    UNavigationSystemV1* NavSys =
        UNavigationSystemV1::GetNavigationSystem(GetWorld());
    if (!NavSys) return;

    FPathFindingQuery Query(
        this,
        *NavSys->GetDefaultNavDataInstance(),
        PathStart->GetActorLocation(),
        PathEnd->GetActorLocation()
    );

    FPathFindingResult Result = NavSys->FindPathSync(Query);
    if (!Result.IsSuccessful() || !Result.Path.IsValid()) return;

    const TArray<FNavPathPoint>& Points = Result.Path->GetPathPoints();
    for (int32 i = 1; i < Points.Num(); ++i)
    {
        DrawDebugLine(GetWorld(),
            Points[i - 1].Location,
            Points[i].Location,
            FColor::Green, false, -1.f, 0, 5.f);
    }

    // 경로 포인트 수 화면 출력
    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(
            1, 0.f, FColor::Yellow,
            FString::Printf(TEXT("PathPoints: %d  PathLen: %.0f cm"),
                Points.Num(),
                Result.Path->GetLength())
        );
    }
}

void UNavDebugVisualizerComponent::DrawNavMeshInfo(const FVector& Location)
{
    ARecastNavMesh* NavMesh = Cast<ARecastNavMesh>(
        UNavigationSystemV1::GetNavigationSystem(GetWorld())
            ->GetDefaultNavDataInstance());
    if (!NavMesh) return;

    // 위치 투사
    FNavLocation NavLoc;
    bool bOnNav = UNavigationSystemV1::GetNavigationSystem(GetWorld())
        ->ProjectPointToNavigation(Location, NavLoc, FVector(50, 50, 100));

    DrawDebugSphere(GetWorld(), NavLoc.Location, 20.f, 8,
        bOnNav ? FColor::Cyan : FColor::Red, false, -1.f);

    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(
            2, 0.f, FColor::Cyan,
            FString::Printf(TEXT("OnNavMesh: %s  NodeRef: %llu"),
                bOnNav ? TEXT("Yes") : TEXT("No"),
                (uint64)NavLoc.NodeRef)
        );
    }
}
```

### 실습 검증 체크리스트

- [ ] GameMode에서 컴포넌트 부착 또는 에디터에서 Actor에 추가
- [ ] `PathStart`, `PathEnd` 설정 후 PIE 실행
- [ ] 경로가 녹색 선으로 표시되는지 확인
- [ ] Nav Modifier Volume 배치 후 경로가 우회하는지 확인
- [ ] `DynamicModifiersOnly` 모드에서 런타임 Nav Modifier 추가/제거 시 경로 변화 확인

---

## 5. DynamicModifiersOnly 동작 검증

이슈 #9에서 분석한 쿠킹 빌드 버그를 이해하기 위한 로그 확인 방법:

### 로그 활성화 (DefaultEngine.ini)

```ini
[Core.Log]
LogNavigation=VeryVerbose
LogNavigationDirtyArea=VeryVerbose
LogNavOctree=Verbose
```

### 확인 포인트

```
[NavTest][...][INIT] Build Type: COOKED BUILD         ← 쿠킹 빌드 확인
[NavTest][...][VERIFY] PathPoints: 2                  ← 직선 경로 = Modifier 미적용
[NavTest][...][VERIFY] PathPoints: 5                  ← 우회 경로 = Modifier 정상 적용
```

### CompressedTileCacheLayers 확인

엔진 소스 `RecastNavMeshGenerator.cpp` 약 라인 1808에 로그 추가:

```cpp
// 엔진 커스터마이징 예시 (개인 fork에서)
if (CompressedLayers.Num() == 0)
{
    UE_LOG(LogNavigation, Warning,
        TEXT("[NavDebug] Tile(%d,%d): CompressedLayers EMPTY → "
             "geometry rebuild will be attempted"),
        TileX, TileY);
}
```

---

## 6. 게임 활용 패턴

### 패턴 1: 런타임 위험 구역 설정

AI가 특정 영역을 피하도록 런타임에 NavArea_Obstacle을 동적으로 적용합니다.

```cpp
// 폭발 지점 주변을 일시적으로 위험 구역으로 설정
void AGameManager::MarkExplosionZone(FVector Center, float Radius, float Duration)
{
    // NavModifierVolume을 스폰하거나
    // 또는 Data Layer를 활성화하여 미리 배치된 Nav Modifier를 적용
}
```

### 패턴 2: 에이전트별 경로 커스터마이징

서로 다른 `AgentRadius`를 가진 NavMesh를 여러 개 설정하여 대형/소형 AI를 구분합니다.

```cpp
// 특정 에이전트 속성으로 NavData 조회
FNavAgentProperties LargeAgentProps;
LargeAgentProps.AgentRadius = 100.f;
LargeAgentProps.AgentHeight = 200.f;

ANavigationData* NavData = NavSys->GetNavDataForProps(LargeAgentProps);
FPathFindingQuery Query(this, *NavData, Start, End);
```

#### 에이전트가 여러 개면 NavMesh도 여러 개

프로젝트 설정의 `Project Settings > Navigation System > Supported Agents`에 **N개의 에이전트를 등록하면, 레벨에는 N개의 `ARecastNavMesh` 액터가 생성됩니다**.

**소스**: `UNavigationSystemV1::PostInitProperties()` (`NavigationSystem.cpp`)
- `SupportedAgents` 배열을 순회하면서 각 에이전트에 대해 `CreateNavigationDataInstanceInLevel()` 호출
- 각 호출이 해당 에이전트 전용 `ARecastNavMesh` 인스턴스를 스폰하고 `NavDataSet`에 등록

**구조:**

```
UWorld
 └── UNavigationSystemV1
      ├── SupportedAgents: [Human(r=34,h=144), Mob(r=100,h=200), Drone(r=50,h=80)]
      └── NavDataSet:
           ├── ARecastNavMesh-Human   (Human용 메시: 좁은 통로 통과 가능)
           ├── ARecastNavMesh-Mob     (Mob용 메시: 좁은 통로 배제된 별도 지오메트리)
           └── ARecastNavMesh-Drone   (Drone용 메시)
```

**중요한 비용 특성:**
- 각 `ARecastNavMesh`는 **독립적으로 전체 레벨을 빌드**합니다 (지오메트리는 공유하지만 복셀화/폴리곤 생성은 별도).
- 따라서 에이전트 수가 늘어나면 **빌드 시간/메모리/디스크 용량이 거의 선형적으로 증가**.
- 런타임 경로 쿼리도 에이전트별로 적절한 NavData를 선택해야 함 — `GetNavDataForProps()` 또는 `UNavigationSystemV1::GetNavDataForAgentName()` 사용.

**실무 권장:**
- 에이전트는 꼭 필요한 만큼만. 캐릭터의 `AgentRadius`가 비슷하면 하나로 통합하고 카테고리별 NavAreaCost로 구분 가능.
- 모든 에이전트가 같은 `TileSizeUU`를 사용하면 타일 그리드가 정렬되어 WP 스트리밍 효율성이 높아짐.

### 패턴 3: 비동기 경로 요청

메인 스레드 부하를 줄이기 위해 비동기 경로 요청을 사용합니다.

```cpp
void AMyAIController::RequestAsyncPath(FVector Destination)
{
    UNavigationSystemV1* NavSys = UNavigationSystemV1::GetNavigationSystem(GetWorld());

    FPathFindingQuery Query(this, *NavSys->GetDefaultNavDataInstance(),
                             GetPawn()->GetActorLocation(), Destination);

    FNavPathQueryDelegate Delegate;
    Delegate.BindUObject(this, &AMyAIController::OnPathFound);

    NavSys->FindPathAsync(NavSys->GetDefaultNavDataInstance()->GetNavAgentPropertiesRef(),
                           Query, Delegate);
}

void AMyAIController::OnPathFound(uint32 QueryID, ENavigationQueryResult::Type Result,
                                   FNavPathSharedPtr Path)
{
    if (Result == ENavigationQueryResult::Success && Path.IsValid())
    {
        // 경로 사용
    }
}
```
