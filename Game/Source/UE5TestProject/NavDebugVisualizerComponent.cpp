#include "NavDebugVisualizerComponent.h"

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

	// 다음 단계에서 DrawNavPath() / DrawNavMeshInfo() 호출 추가
}

void UNavDebugVisualizerComponent::DrawNavPath()
{
	// 다음 단계(3단계)에서 구현 — FindPathSync 결과를 DrawDebug로 시각화
}

void UNavDebugVisualizerComponent::DrawNavMeshInfo(const FVector& Location)
{
	// 다음 단계(4단계)에서 구현 — ProjectPointToNavigation 결과를 시각화
}
