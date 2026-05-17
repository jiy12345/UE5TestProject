#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "NavDebugVisualizerComponent.h"
#include "NavDebugActor.generated.h"

/**
 * UNavDebugVisualizerComponent를 자동 부착하는 wrapper Actor.
 * 검증/테스트 맵에 spawn 후 컴포넌트의 PathStart/PathEnd만 할당하면 즉시 동작.
 *
 * 빈 AActor에 컴포넌트를 런타임 부착하는 방식보다 C++ 생성자 부착이
 * 컴파일 시점에 보장되어 reflection 의존성 회피.
 */
UCLASS()
class UE5TESTPROJECT_API ANavDebugActor : public AActor
{
	GENERATED_BODY()

public:
	ANavDebugActor();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="NavDebug")
	UNavDebugVisualizerComponent* NavDebugComp = nullptr;

	/** PathStart/PathEnd 일괄 할당 (스크립트 또는 BP에서 호출). */
	UFUNCTION(BlueprintCallable, Category="NavDebug")
	void SetPathTargets(AActor* InStart, AActor* InEnd);
};
