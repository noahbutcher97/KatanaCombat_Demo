// Copyright Epic Games, Inc. All Rights Reserved.

#include "Data/CombatSettings.h"
#include "Data/WeaponData.h"
#include "Data/AttackConfiguration.h"

UCombatSettings::UCombatSettings()
{
	// Default values are set in header file
}

UAttackConfiguration* UCombatSettings::GetAttackConfiguration() const
{
	// Priority 1: New pattern - DefaultWeaponData->AttackConfiguration
	if (DefaultWeaponData && DefaultWeaponData->AttackConfiguration)
	{
		return DefaultWeaponData->AttackConfiguration;
	}

	// Priority 2: Deprecated fallback (for unmigrated assets)
	if (AttackConfiguration)
	{
		return AttackConfiguration;
	}

	return nullptr;
}

void UCombatSettings::PostLoad()
{
	Super::PostLoad();

	// ============================================================================
	// DEPRECATED FIELD MIGRATION
	// ============================================================================
	// Migrate AttackConfiguration → DefaultWeaponData->AttackConfiguration
	// This supports existing CombatSettings assets that used the old direct field.

	if (AttackConfiguration && !DefaultWeaponData)
	{
		UE_LOG(LogTemp, Warning, TEXT("[CombatSettings] Migrating deprecated AttackConfiguration to DefaultWeaponData on %s. "
			"Please re-save this asset to complete migration."), *GetName());

		// Create a new WeaponData to hold the attack configuration
		DefaultWeaponData = NewObject<UWeaponData>(this);
		DefaultWeaponData->AttackConfiguration = AttackConfiguration;

		// Mark package dirty so user can re-save to complete migration
		MarkPackageDirty();
	}
	else if (AttackConfiguration && DefaultWeaponData)
	{
		// Both old and new fields set - prefer new, clear old
		UE_LOG(LogTemp, Warning, TEXT("[CombatSettings] %s has both deprecated AttackConfiguration and DefaultWeaponData set. "
			"Using DefaultWeaponData. Clear AttackConfiguration and re-save."), *GetName());
		AttackConfiguration = nullptr;
		MarkPackageDirty();
	}
}

#if WITH_EDITOR
void UCombatSettings::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	// Warn designers if they set the deprecated field
	if (PropertyChangedEvent.GetPropertyName() == GET_MEMBER_NAME_CHECKED(UCombatSettings, AttackConfiguration))
	{
		if (AttackConfiguration)
		{
			UE_LOG(LogTemp, Warning, TEXT("[CombatSettings] AttackConfiguration is deprecated. "
				"Set DefaultWeaponData with AttackConfiguration instead."));
		}
	}
}
#endif