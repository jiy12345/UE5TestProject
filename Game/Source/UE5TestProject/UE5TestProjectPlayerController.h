#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "UE5TestProjectPlayerController.generated.h"

/**
 * Base Player Controller for UE5TestProject
 */
UCLASS()
class UE5TESTPROJECT_API AUE5TestProjectPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	AUE5TestProjectPlayerController();

protected:
	virtual void BeginPlay() override;
};
