#include "UE5TestProjectGameMode.h"

#include "UE5TestProjectCharacter.h"
#include "UE5TestProjectPlayerController.h"

AUE5TestProjectGameMode::AUE5TestProjectGameMode()
{
	PlayerControllerClass = AUE5TestProjectPlayerController::StaticClass();
	DefaultPawnClass = AUE5TestProjectCharacter::StaticClass();
}
