#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "UE5TestProjectCharacter.generated.h"

class USpringArmComponent;
class UCameraComponent;

/**
 * 3인칭 테스트 캐릭터. TutorialTPP 메시 + AnimBlueprint 사용.
 * NavMesh 위 걷기 검증용 (#19 NavDebugVisualizer 시각 확인).
 *
 * 입력: WASD 이동 + 마우스 회전 + Space 점프 (레거시 InputComponent).
 * Enhanced Input 마이그레이션은 별도 이슈로 분리 예정.
 */
UCLASS()
class UE5TESTPROJECT_API AUE5TestProjectCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	AUE5TestProjectCharacter();

protected:
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

	// 입력 핸들러
	void MoveForward(float Value);
	void MoveRight(float Value);
	void Turn(float Value);
	void LookUp(float Value);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Camera")
	USpringArmComponent* SpringArm = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Camera")
	UCameraComponent* FollowCamera = nullptr;
};
