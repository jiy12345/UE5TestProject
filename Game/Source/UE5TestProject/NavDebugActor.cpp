#include "NavDebugActor.h"

ANavDebugActor::ANavDebugActor()
{
	// Actor 자체 Tick은 비활성 — 컴포넌트가 Tick함
	PrimaryActorTick.bCanEverTick = false;

	NavDebugComp = CreateDefaultSubobject<UNavDebugVisualizerComponent>(TEXT("NavDebugComp"));
}

void ANavDebugActor::SetPathTargets(AActor* InStart, AActor* InEnd)
{
	if (NavDebugComp == nullptr)
	{
		return;
	}
	NavDebugComp->PathStart = InStart;
	NavDebugComp->PathEnd = InEnd;
}
