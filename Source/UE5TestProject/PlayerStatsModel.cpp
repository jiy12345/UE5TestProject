#include "PlayerStatsModel.h"

UPlayerStatsModel::UPlayerStatsModel()
	: CurrentHealth(100.0f)
	, MaximumHealth(100.0f)
	, PlayerLevel(1)
	, Name(TEXT("Player"))
{
}

void UPlayerStatsModel::TakeDamage(float Damage)
{
	if (Damage < 0.0f)
	{
		return;
	}

	CurrentHealth = FMath::Max(0.0f, CurrentHealth - Damage);
	OnStatsChanged.Broadcast();
}

void UPlayerStatsModel::Heal(float Amount)
{
	if (Amount < 0.0f)
	{
		return;
	}

	CurrentHealth = FMath::Min(MaximumHealth, CurrentHealth + Amount);
	OnStatsChanged.Broadcast();
}

void UPlayerStatsModel::LevelUp()
{
	PlayerLevel++;

	// Increase max health by 10% per level
	MaximumHealth *= 1.1f;

	// Fully heal on level up
	CurrentHealth = MaximumHealth;

	OnStatsChanged.Broadcast();
}

bool UPlayerStatsModel::IsAlive() const
{
	return CurrentHealth > 0.0f;
}

float UPlayerStatsModel::GetHealthPercentage() const
{
	if (MaximumHealth <= 0.0f)
	{
		return 0.0f;
	}

	return CurrentHealth / MaximumHealth;
}
