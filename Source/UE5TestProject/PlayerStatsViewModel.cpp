#include "PlayerStatsViewModel.h"

UPlayerStatsViewModel::UPlayerStatsViewModel()
	: Health(100.0f)
	, MaxHealth(100.0f)
	, Level(1)
	, PlayerName(TEXT("Player"))
	, Model(nullptr)
{
}

void UPlayerStatsViewModel::Initialize(UPlayerStatsModel* InModel)
{
	if (Model)
	{
		// Unbind from previous model
		Model->OnStatsChanged.RemoveDynamic(this, &UPlayerStatsViewModel::OnModelStatsChanged);
	}

	Model = InModel;

	if (Model)
	{
		// Bind to model changes
		Model->OnStatsChanged.AddDynamic(this, &UPlayerStatsViewModel::OnModelStatsChanged);

		// Initial sync
		SyncFromModel();
	}
}

//----------------------------------------------------------------------
// Setters with Field Notify
//----------------------------------------------------------------------

void UPlayerStatsViewModel::SetHealth(float NewHealth)
{
	if (UE_MVVM_SET_PROPERTY_VALUE(Health, NewHealth))
	{
		// When Health changes, also notify dependent computed properties
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(GetHealthPercentage);
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(GetFormattedHealthText);
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(GetIsAlive);
	}
}

void UPlayerStatsViewModel::SetMaxHealth(float NewMaxHealth)
{
	if (UE_MVVM_SET_PROPERTY_VALUE(MaxHealth, NewMaxHealth))
	{
		// MaxHealth affects percentage and formatted text
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(GetHealthPercentage);
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(GetFormattedHealthText);
	}
}

void UPlayerStatsViewModel::SetLevel(int32 NewLevel)
{
	UE_MVVM_SET_PROPERTY_VALUE(Level, NewLevel);
}

void UPlayerStatsViewModel::SetPlayerName(const FString& NewName)
{
	UE_MVVM_SET_PROPERTY_VALUE(PlayerName, NewName);
}

//----------------------------------------------------------------------
// Computed Properties
//----------------------------------------------------------------------

float UPlayerStatsViewModel::GetHealthPercentage() const
{
	if (MaxHealth <= 0.0f)
	{
		return 0.0f;
	}

	return Health / MaxHealth;
}

FText UPlayerStatsViewModel::GetFormattedHealthText() const
{
	return FText::FromString(FString::Printf(TEXT("%.0f / %.0f"), Health, MaxHealth));
}

bool UPlayerStatsViewModel::GetIsAlive() const
{
	return Health > 0.0f;
}

//----------------------------------------------------------------------
// Commands
//----------------------------------------------------------------------

void UPlayerStatsViewModel::ExecuteTakeDamage(float Damage)
{
	if (Model)
	{
		Model->TakeDamage(Damage);
	}
}

void UPlayerStatsViewModel::ExecuteHeal(float Amount)
{
	if (Model)
	{
		Model->Heal(Amount);
	}
}

void UPlayerStatsViewModel::ExecuteLevelUp()
{
	if (Model)
	{
		Model->LevelUp();
	}
}

//----------------------------------------------------------------------
// Model Synchronization
//----------------------------------------------------------------------

void UPlayerStatsViewModel::OnModelStatsChanged()
{
	SyncFromModel();
}

void UPlayerStatsViewModel::SyncFromModel()
{
	if (!Model)
	{
		return;
	}

	// Update ViewModel properties from Model
	// This will trigger Field Notify and update the View
	SetHealth(Model->CurrentHealth);
	SetMaxHealth(Model->MaximumHealth);
	SetLevel(Model->PlayerLevel);
	SetPlayerName(Model->Name);
}
