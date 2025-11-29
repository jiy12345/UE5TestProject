#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "PlayerStatsModel.generated.h"

/**
 * Player stats data model
 * Manages player health, level, and name data with business logic
 */
UCLASS(BlueprintType)
class UE5TESTPROJECT_API UPlayerStatsModel : public UObject
{
	GENERATED_BODY()

public:
	UPlayerStatsModel();

	// Properties
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats")
	float CurrentHealth;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats")
	float MaximumHealth;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats")
	int32 PlayerLevel;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats")
	FString Name;

	// Delegate for stats changes
	DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnStatsChanged);

	UPROPERTY(BlueprintAssignable, Category = "Stats")
	FOnStatsChanged OnStatsChanged;

	// Business logic methods
	UFUNCTION(BlueprintCallable, Category = "Stats")
	void TakeDamage(float Damage);

	UFUNCTION(BlueprintCallable, Category = "Stats")
	void Heal(float Amount);

	UFUNCTION(BlueprintCallable, Category = "Stats")
	void LevelUp();

	UFUNCTION(BlueprintPure, Category = "Stats")
	bool IsAlive() const;

	UFUNCTION(BlueprintPure, Category = "Stats")
	float GetHealthPercentage() const;
};
