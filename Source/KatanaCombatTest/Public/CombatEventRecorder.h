// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "CombatTypes.h"
#include "Characters/PlayerCharacter.h"
#include "UObject/Object.h"
#include "CombatEventRecorder.generated.h"

class ABaseCombatCharacter;
class UWeaponComponent;

/** Test combatant that invalidates a participant from the team-query callback. */
UCLASS()
class KATANACOMBATTEST_API ADefenseDestructiveTeamQueryCharacter : public APlayerCharacter
{
	GENERATED_BODY()

public:
	TWeakObjectPtr<AActor> ActorToDestroyOnHostilityQuery;
	virtual bool IsHostileTo_Implementation(AActor* Other) const override;
};

/** Records dynamic combat callbacks for ordering and idempotence automation tests. */
UCLASS()
class KATANACOMBATTEST_API UCombatEventRecorder : public UObject
{
	GENERATED_BODY()

public:
	int32 DamageReceivedCount = 0;
	int32 HealthChangedCount = 0;
	int32 CharacterDyingCount = 0;
	int32 AttackHitCount = 0;
	int32 AcceptedHitCountObservedDuringDamage = INDEX_NONE;
	float LastHealth = 0.0f;
	float LastMaxHealth = 0.0f;

	bool bReenterOnDamage = false;
	bool bReenterOnHealth = false;
	bool bReenterOnDying = false;
	bool bBeginBlockOnHealth = false;
	bool bBeginBlockResult = false;
	bool bDestroyOnDamage = false;
	bool bDestroyOnHealth = false;
	bool bDestroyOnDying = false;
	TWeakObjectPtr<ABaseCombatCharacter> ReentryTarget;
	TWeakObjectPtr<AActor> ActorToDestroy;
	TWeakObjectPtr<UWeaponComponent> ObservedWeapon;
	FDefenseContactRequest ReentryRequest;
	FDefenseContactReceipt DamageReentryReceipt;
	FDefenseContactReceipt HealthReentryReceipt;
	FDefenseContactReceipt DyingReentryReceipt;

	UFUNCTION()
	void HandleDamageReceived(const FHitReactionInfo& HitInfo);

	UFUNCTION()
	void HandleHealthChanged(float NewHealth, float MaxHealth);

	UFUNCTION()
	void HandleCharacterDying(AActor* Killer);

	UFUNCTION()
	void HandleAttackHit(AActor* HitActor, const FHitReactionInfo& HitInfo);
};
