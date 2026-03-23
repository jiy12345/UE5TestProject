#include "NavDebugVisualizerComponent.h"
#include "NavigationSystem.h"
#include "NavMesh/RecastNavMesh.h"
#include "DrawDebugHelpers.h"
#include "Engine/Engine.h"

UNavDebugVisualizerComponent::UNavDebugVisualizerComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
}

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

    UNavigationSystemV1* NavSys = UNavigationSystemV1::GetNavigationSystem(GetWorld());
    if (!NavSys) return;

    const ANavigationData* NavData = NavSys->GetDefaultNavDataInstance();
    if (!NavData) return;

    FPathFindingQuery Query(this, *NavData,
                             PathStart->GetActorLocation(),
                             PathEnd->GetActorLocation());

    FPathFindingResult Result = NavSys->FindPathSync(Query);

    if (!Result.IsSuccessful() || !Result.Path.IsValid())
    {
        // 경로 없음 — 빨간 X 표시
        DrawDebugLine(GetWorld(),
            PathStart->GetActorLocation(),
            PathEnd->GetActorLocation(),
            FColor::Red, false, -1.f, 0, PathLineThickness);

        if (GEngine)
            GEngine->AddOnScreenDebugMessage(1, 0.f, FColor::Red, TEXT("PATH NOT FOUND"));
        return;
    }

    // 경로 선 렌더링
    const TArray<FNavPathPoint>& Points = Result.Path->GetPathPoints();
    for (int32 i = 1; i < Points.Num(); ++i)
    {
        DrawDebugLine(GetWorld(),
            Points[i - 1].Location + FVector(0, 0, 10),
            Points[i].Location + FVector(0, 0, 10),
            FColor::Green, false, -1.f, 0, PathLineThickness);
    }

    // 경유점 구체 렌더링
    for (const FNavPathPoint& Pt : Points)
        DrawDebugSphere(GetWorld(), Pt.Location, 15.f, 6, FColor::Yellow, false, -1.f);

    // HUD 정보
    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(1, 0.f, FColor::Green,
            FString::Printf(TEXT("PathPoints: %d  |  Length: %.0f cm"),
                Points.Num(), Result.Path->GetLength()));
    }
}

void UNavDebugVisualizerComponent::DrawNavMeshInfo(const FVector& Location)
{
    UNavigationSystemV1* NavSys = UNavigationSystemV1::GetNavigationSystem(GetWorld());
    if (!NavSys) return;

    FNavLocation NavLoc;
    const bool bOnNav = NavSys->ProjectPointToNavigation(
        Location, NavLoc, FVector(50.f, 50.f, 100.f));

    DrawDebugSphere(GetWorld(), NavLoc.Location, 20.f, 8,
        bOnNav ? FColor::Cyan : FColor::Red, false, -1.f);

    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(2, 0.f, FColor::Cyan,
            FString::Printf(TEXT("OnNavMesh: %s  |  NodeRef: %llu"),
                bOnNav ? TEXT("YES") : TEXT("NO"),
                (uint64)NavLoc.NodeRef));
    }
}
