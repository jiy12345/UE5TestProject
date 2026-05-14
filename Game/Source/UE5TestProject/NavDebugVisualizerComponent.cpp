#include "NavDebugVisualizerComponent.h"
#include "NavigationSystem.h"
#include "NavigationData.h"
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
	// 다음 단계(4단계)에서 DrawNavMeshInfo() 호출 추가
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
		DrawDebugLine(GetWorld(),
			PathStart->GetActorLocation(),
			PathEnd->GetActorLocation(),
			FColor::Red, false, -1.f, 0, PathLineThickness);

		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(1, 0.f, FColor::Red, TEXT("PATH NOT FOUND"));
		}
		return;
	}

	const TArray<FNavPathPoint>& Points = Result.Path->GetPathPoints();

	// Z+10 오프셋: 경로 선이 지형 표면에 묻히는 z-fighting 회피
	for (int32 i = 1; i < Points.Num(); ++i)
	{
		DrawDebugLine(GetWorld(),
			Points[i - 1].Location + FVector(0, 0, 10),
			Points[i].Location + FVector(0, 0, 10),
			FColor::Green, false, -1.f, 0, PathLineThickness);
	}

	// 경유점 구체 (segments=6: 시각화용 충분한 해상도 / 성능 절충)
	for (const FNavPathPoint& Pt : Points)
	{
		DrawDebugSphere(GetWorld(), Pt.Location, 15.f, 6, FColor::Yellow, false, -1.f);
	}

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(1, 0.f, FColor::Green,
			FString::Printf(TEXT("PathPoints: %d  |  Length: %.0f cm"),
				Points.Num(), Result.Path->GetLength()));
	}
}

void UNavDebugVisualizerComponent::DrawNavMeshInfo(const FVector& Location)
{
	// 다음 단계(4단계)에서 구현 — ProjectPointToNavigation 결과를 시각화
}
