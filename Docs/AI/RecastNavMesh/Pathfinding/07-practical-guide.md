# 07. 이해도 확인 실습 — 길찾기 디버그 시각화

> **작성일**: 2026-04-16
> **엔진 버전**: UE 5.5

## 1. 실습 목표

분석한 길찾기 파이프라인을 게임 내에서 직접 눈으로 확인합니다:

- A* 탐색 결과인 **폴리곤 코리더**를 시각화
- 퍼널 알고리즘 결과인 **직선 경로(웨이포인트)**를 시각화
- **경로 추적 상태** (현재 세그먼트, 목표 웨이포인트)를 실시간 표시
- **경로 무효화/재탐색**이 발생하는 순간을 감지하여 로그 출력

## 2. 디버그 시각화 컴포넌트

### 2.1 설계

`UPathfindingDebugComponent`를 AI 폰에 부착하여, 매 프레임 경로 정보를 `DrawDebug`로 렌더링합니다.

```cpp
// PathfindingDebugComponent.h
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Navigation/PathFollowingComponent.h"
#include "PathfindingDebugComponent.generated.h"

/**
 * AI의 길찾기 파이프라인을 실시간으로 시각화하는 디버그 컴포넌트.
 * AI 폰에 부착하면 폴리곤 코리더, 직선 경로, 경로 추적 상태를 DrawDebug로 표시합니다.
 */
UCLASS(ClassGroup=(AI), meta=(BlueprintSpawnableComponent))
class UPathfindingDebugComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UPathfindingDebugComponent();

    virtual void TickComponent(float DeltaTime, ELevelTick TickType,
        FActorComponentTickFunction* ThisTickFunction) override;

    // 폴리곤 코리더 시각화 (A* 결과)
    UPROPERTY(EditAnywhere, Category="Debug|Pathfinding")
    bool bDrawCorridor = true;

    // 직선 경로 시각화 (퍼널 알고리즘 결과)
    UPROPERTY(EditAnywhere, Category="Debug|Pathfinding")
    bool bDrawStraightPath = true;

    // 현재 경로 추적 상태 시각화
    UPROPERTY(EditAnywhere, Category="Debug|Pathfinding")
    bool bDrawFollowingState = true;

    // 경로 무효화 이벤트 로그
    UPROPERTY(EditAnywhere, Category="Debug|Pathfinding")
    bool bLogInvalidation = true;

    // 시각화 색상
    UPROPERTY(EditAnywhere, Category="Debug|Pathfinding")
    FColor CorridorColor = FColor::Blue;

    UPROPERTY(EditAnywhere, Category="Debug|Pathfinding")
    FColor StraightPathColor = FColor::Green;

    UPROPERTY(EditAnywhere, Category="Debug|Pathfinding")
    FColor CurrentSegmentColor = FColor::Yellow;

private:
    void DrawPolygonCorridor(const class FNavMeshPath* NavPath);
    void DrawStraightPath(const FNavigationPath* Path);
    void DrawFollowingState(const UPathFollowingComponent* PFC);

    // 경로 변경 감지를 위한 이전 경로 ID
    uint32 LastPathId = 0;
};
```

### 2.2 구현

```cpp
// PathfindingDebugComponent.cpp
#include "PathfindingDebugComponent.h"
#include "AIController.h"
#include "NavigationSystem.h"
#include "NavMesh/NavMeshPath.h"
#include "Navigation/PathFollowingComponent.h"
#include "DrawDebugHelpers.h"

UPathfindingDebugComponent::UPathfindingDebugComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.bStartWithTickEnabled = true;
}

void UPathfindingDebugComponent::TickComponent(
    float DeltaTime, ELevelTick TickType,
    FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

#if ENABLE_DRAW_DEBUG
    APawn* OwnerPawn = Cast<APawn>(GetOwner());
    if (!OwnerPawn) return;

    AAIController* AIC = Cast<AAIController>(OwnerPawn->GetController());
    if (!AIC) return;

    UPathFollowingComponent* PFC = AIC->GetPathFollowingComponent();
    if (!PFC) return;

    FNavPathSharedPtr CurrentPath = PFC->GetPath();
    if (!CurrentPath.IsValid()) return;

    // 경로 무효화/변경 감지
    if (bLogInvalidation)
    {
        uint32 CurrentPathId = CurrentPath->GetUniqueID();
        if (LastPathId != 0 && CurrentPathId != LastPathId)
        {
            UE_LOG(LogTemp, Warning,
                TEXT("[PathfindingDebug] Path changed! Old=%u New=%u UpToDate=%s Partial=%s"),
                LastPathId, CurrentPathId,
                CurrentPath->IsUpToDate() ? TEXT("Yes") : TEXT("No"),
                CurrentPath->IsPartial() ? TEXT("Yes") : TEXT("No"));
        }
        LastPathId = CurrentPathId;
    }

    // 폴리곤 코리더 시각화
    if (bDrawCorridor)
    {
        if (const FNavMeshPath* NavPath = CurrentPath->CastPath<FNavMeshPath>())
        {
            DrawPolygonCorridor(NavPath);
        }
    }

    // 직선 경로 시각화
    if (bDrawStraightPath)
    {
        DrawStraightPath(CurrentPath.Get());
    }

    // 경로 추적 상태 시각화
    if (bDrawFollowingState)
    {
        DrawFollowingState(PFC);
    }
#endif
}

void UPathfindingDebugComponent::DrawPolygonCorridor(const FNavMeshPath* NavPath)
{
#if ENABLE_DRAW_DEBUG
    const TArray<FNavigationPortalEdge>& PortalEdges = NavPath->GetPathCorridorEdges();

    for (const FNavigationPortalEdge& Edge : PortalEdges)
    {
        // 포탈 에지 (인접 폴리곤 경계) 시각화
        DrawDebugLine(GetWorld(), Edge.Left, Edge.Right,
            CorridorColor, false, -1.f, 0, 2.f);

        // 포탈 중점 표시
        FVector Mid = (Edge.Left + Edge.Right) * 0.5f;
        DrawDebugPoint(GetWorld(), Mid, 8.f, CorridorColor, false, -1.f);
    }
#endif
}

void UPathfindingDebugComponent::DrawStraightPath(const FNavigationPath* Path)
{
#if ENABLE_DRAW_DEBUG
    const TArray<FNavPathPoint>& Points = Path->GetPathPoints();

    for (int32 i = 0; i < Points.Num(); ++i)
    {
        const FVector& Pt = Points[i].Location;

        // 웨이포인트 구체 표시
        DrawDebugSphere(GetWorld(), Pt, 15.f, 8,
            StraightPathColor, false, -1.f, 0, 1.5f);

        // 인덱스 텍스트
        DrawDebugString(GetWorld(), Pt + FVector(0, 0, 30),
            FString::Printf(TEXT("[%d]"), i), nullptr,
            StraightPathColor, -1.f, true);

        // 이전 웨이포인트와 연결선
        if (i > 0)
        {
            DrawDebugLine(GetWorld(), Points[i-1].Location, Pt,
                StraightPathColor, false, -1.f, 0, 2.f);
        }
    }
#endif
}

void UPathfindingDebugComponent::DrawFollowingState(const UPathFollowingComponent* PFC)
{
#if ENABLE_DRAW_DEBUG
    FNavPathSharedPtr Path = PFC->GetPath();
    if (!Path.IsValid()) return;

    const TArray<FNavPathPoint>& Points = Path->GetPathPoints();
    int32 CurrentIdx = PFC->GetCurrentPathIndex();

    if (!Points.IsValidIndex(CurrentIdx)) return;

    const FVector& CurrentTarget = Points[CurrentIdx].Location;
    const FVector& PawnLocation = GetOwner()->GetActorLocation();

    // 현재 목표 웨이포인트까지 선
    DrawDebugLine(GetWorld(), PawnLocation, CurrentTarget,
        CurrentSegmentColor, false, -1.f, 0, 3.f);

    // 현재 목표에 큰 구체
    DrawDebugSphere(GetWorld(), CurrentTarget, 25.f, 12,
        CurrentSegmentColor, false, -1.f, 0, 2.f);

    // 상태 텍스트
    FString StatusText = FString::Printf(
        TEXT("Segment: %d/%d\nStatus: %s\nPartial: %s"),
        CurrentIdx, Points.Num() - 1,
        *UEnum::GetValueAsString(PFC->GetStatus()),
        Path->IsPartial() ? TEXT("Yes") : TEXT("No"));

    DrawDebugString(GetWorld(), PawnLocation + FVector(0, 0, 100),
        StatusText, nullptr, FColor::White, -1.f, true);
#endif
}
```

---

## 3. 동작 검증 체크리스트

### 3.1 A* 탐색 + 퍼널 알고리즘 검증

- [ ] AI 폰에 `UPathfindingDebugComponent` 부착
- [ ] PIE에서 AI에게 이동 명령 부여
- [ ] **파란색 포탈 에지**(폴리곤 코리더)가 시작→목표 사이에 표시되는지 확인
- [ ] **초록색 직선 경로**(웨이포인트)가 포탈 코리더 위에 겹쳐 표시되는지 확인
- [ ] 직선 경로의 웨이포인트 수가 코리더 폴리곤 수보다 적은지 확인 (퍼널 알고리즘이 불필요한 꼭짓점을 제거했으므로)

### 3.2 경로 추적 상태 검증

- [ ] **노란색 선**이 AI 폰에서 현재 목표 웨이포인트까지 그려지는지 확인
- [ ] AI가 웨이포인트에 도달하면 Segment 인덱스가 증가하는지 확인
- [ ] 최종 목표 도달 시 시각화가 사라지는지 확인

### 3.3 경로 무효화 검증

- [ ] AI 이동 중에 경로 상에 `NavModifierVolume`을 배치
- [ ] 로그에 `[PathfindingDebug] Path changed!` 메시지가 출력되는지 확인
- [ ] AI가 장애물을 우회하는 새 경로로 자동 전환하는지 확인

### 3.4 부분 경로 검증

- [ ] NavMesh가 끊긴 지역(섬)으로 이동 명령 부여
- [ ] `Partial: Yes`가 표시되는지 확인
- [ ] AI가 도달 가능한 가장 가까운 지점까지만 이동하는지 확인

### 3.5 영역 비용 검증

- [ ] `Nav Area` 클래스를 만들어 비용을 높게 설정 (예: Cost = 5.0)
- [ ] 높은 비용 영역을 가로지르는 짧은 경로 vs 우회하는 긴 경로 중 어떤 것을 선택하는지 확인
- [ ] 비용이 충분히 높으면 우회 경로를 선택하는지 확인

---

## 4. 엔진 내장 디버그 도구 활용

### 4.1 콘솔 명령

| 명령 | 효과 |
|------|------|
| `ai.nav.DisplayNavLinks 1` | NavLink 시각화 |
| `ai.nav.DrawNavMesh 1` | NavMesh 폴리곤 시각화 |
| `ai.nav.ShowPath 1` | AI 경로 시각화 |

### 4.2 GameplayDebugger (')

PIE에서 `'` (작은따옴표) 키를 누르면 GameplayDebugger가 활성화됩니다.
**NavMesh** 카테고리에서:
- 현재 경로와 코리더를 시각화
- PathFollowing 상태 표시
- NavMesh 타일 경계 표시
