#pragma once

#include "CoreMinimal.h"
#include "MVVMViewModelBase.h"
#include "PlayerStatsModel.h"
#include "PlayerStatsViewModel.generated.h"

/**
 * ViewModel for player stats
 * Mediates between View (UMG) and Model (PlayerStatsModel)
 * Uses Field Notify system for automatic UI updates
 */
UCLASS(BlueprintType)
class UE5TESTPROJECT_API UPlayerStatsViewModel : public UMVVMViewModelBase
{
	GENERATED_BODY()

public:
	UPlayerStatsViewModel();

	// Initialize with a Model
	UFUNCTION(BlueprintCallable, Category = "ViewModel")
	void Initialize(UPlayerStatsModel* InModel);

	//----------------------------------------------------------------------
	// Field Notify Properties
	//----------------------------------------------------------------------

	// Health property with Field Notify
	UPROPERTY(BlueprintReadWrite, FieldNotify, Setter, Getter, Category = "Stats")
	float Health;

	UPROPERTY(BlueprintReadWrite, FieldNotify, Setter, Getter, Category = "Stats")
	float MaxHealth;

	UPROPERTY(BlueprintReadWrite, FieldNotify, Setter, Getter, Category = "Stats")
	int32 Level;

	UPROPERTY(BlueprintReadWrite, FieldNotify, Setter, Getter, Category = "Stats")
	FString PlayerName;

	//----------------------------------------------------------------------
	// Computed Properties (Read-only)
	//----------------------------------------------------------------------

	// Returns health as a percentage (0.0 to 1.0)
	UFUNCTION(BlueprintPure, FieldNotify, Category = "Stats")
	float GetHealthPercentage() const;

	// Returns formatted health text (e.g., "75/100")
	UFUNCTION(BlueprintPure, FieldNotify, Category = "Stats")
	FText GetFormattedHealthText() const;

	// Returns whether player is alive
	UFUNCTION(BlueprintPure, FieldNotify, Category = "Stats")
	bool GetIsAlive() const;

	//----------------------------------------------------------------------
	// Commands (Actions)
	//----------------------------------------------------------------------

	UFUNCTION(BlueprintCallable, Category = "Stats")
	void ExecuteTakeDamage(float Damage);

	UFUNCTION(BlueprintCallable, Category = "Stats")
	void ExecuteHeal(float Amount);

	UFUNCTION(BlueprintCallable, Category = "Stats")
	void ExecuteLevelUp();

private:
	// Setters with Field Notify
	void SetHealth(float NewHealth);
	void SetMaxHealth(float NewMaxHealth);
	void SetLevel(int32 NewLevel);
	void SetPlayerName(const FString& NewName);

	// Getters
	float GetHealth() const { return Health; }
	float GetMaxHealth() const { return MaxHealth; }
	int32 GetLevel() const { return Level; }
	FString GetPlayerName() const { return PlayerName; }

	// Reference to the Model
	UPROPERTY()
	TObjectPtr<UPlayerStatsModel> Model;

	// Callback for Model changes
	UFUNCTION()
	void OnModelStatsChanged();

	// Sync ViewModel from Model
	void SyncFromModel();
};
