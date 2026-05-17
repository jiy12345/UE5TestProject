#include "UE5TestProjectCharacter.h"

#include "Animation/AnimBlueprint.h"
#include "Animation/AnimInstance.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "UObject/ConstructorHelpers.h"

AUE5TestProjectCharacter::AUE5TestProjectCharacter()
{
	// Capsule 크기 (기본값 유지 가능, 명시적 설정)
	GetCapsuleComponent()->InitCapsuleSize(42.f, 96.f);

	// 컨트롤러가 Yaw만 캐릭터에 영향, Pitch/Roll은 카메라에만
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	// 이동 방향으로 캐릭터가 회전
	UCharacterMovementComponent* MoveComp = GetCharacterMovement();
	MoveComp->bOrientRotationToMovement = true;
	MoveComp->RotationRate = FRotator(0.f, 540.f, 0.f);
	MoveComp->JumpZVelocity = 600.f;
	MoveComp->AirControl = 0.2f;
	MoveComp->MaxWalkSpeed = 500.f;

	// 3인칭 SpringArm
	SpringArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("SpringArm"));
	SpringArm->SetupAttachment(RootComponent);
	SpringArm->TargetArmLength = 350.f;
	SpringArm->bUsePawnControlRotation = true;   // 컨트롤러 회전을 SpringArm에 반영

	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(SpringArm, USpringArmComponent::SocketName);
	FollowCamera->bUsePawnControlRotation = false;

	// 엔진 기본 제공 TutorialTPP 메시 + AnimBlueprint 할당
	// 경로: /Engine/Tutorial/SubEditors/TutorialAssets/Character/TutorialTPP
	static ConstructorHelpers::FObjectFinder<USkeletalMesh> MeshAsset(
		TEXT("/Engine/Tutorial/SubEditors/TutorialAssets/Character/TutorialTPP.TutorialTPP"));
	if (MeshAsset.Succeeded())
	{
		GetMesh()->SetSkeletalMesh(MeshAsset.Object);
	}

	static ConstructorHelpers::FObjectFinder<UAnimBlueprint> AnimAsset(
		TEXT("/Engine/Tutorial/SubEditors/TutorialAssets/Character/TutorialTPP_AnimBlueprint.TutorialTPP_AnimBlueprint"));
	if (AnimAsset.Succeeded() && AnimAsset.Object->GeneratedClass != nullptr)
	{
		GetMesh()->SetAnimInstanceClass(AnimAsset.Object->GeneratedClass);
	}

	// 메시를 캡슐 발끝에 맞추고 Yaw -90도로 정면을 카메라 정면으로
	GetMesh()->SetRelativeLocation(FVector(0.f, 0.f, -96.f));
	GetMesh()->SetRelativeRotation(FRotator(0.f, -90.f, 0.f));
}

void AUE5TestProjectCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	// 점프 — ACharacter가 제공하는 기본 핸들러
	PlayerInputComponent->BindAction("Jump", IE_Pressed, this, &ACharacter::Jump);
	PlayerInputComponent->BindAction("Jump", IE_Released, this, &ACharacter::StopJumping);

	// 이동
	PlayerInputComponent->BindAxis("MoveForward", this, &AUE5TestProjectCharacter::MoveForward);
	PlayerInputComponent->BindAxis("MoveRight", this, &AUE5TestProjectCharacter::MoveRight);

	// 회전 — Turn/LookUp은 마우스, Pawn 기본 매핑 활용
	PlayerInputComponent->BindAxis("Turn", this, &AUE5TestProjectCharacter::Turn);
	PlayerInputComponent->BindAxis("LookUp", this, &AUE5TestProjectCharacter::LookUp);
}

void AUE5TestProjectCharacter::MoveForward(float Value)
{
	if (Controller == nullptr || Value == 0.f)
	{
		return;
	}

	// 컨트롤러 Yaw 기준 전방 벡터
	const FRotator Rotation = Controller->GetControlRotation();
	const FRotator YawRotation(0.f, Rotation.Yaw, 0.f);
	const FVector Direction = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
	AddMovementInput(Direction, Value);
}

void AUE5TestProjectCharacter::MoveRight(float Value)
{
	if (Controller == nullptr || Value == 0.f)
	{
		return;
	}

	const FRotator Rotation = Controller->GetControlRotation();
	const FRotator YawRotation(0.f, Rotation.Yaw, 0.f);
	const FVector Direction = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);
	AddMovementInput(Direction, Value);
}

void AUE5TestProjectCharacter::Turn(float Value)
{
	AddControllerYawInput(Value);
}

void AUE5TestProjectCharacter::LookUp(float Value)
{
	AddControllerPitchInput(Value);
}
