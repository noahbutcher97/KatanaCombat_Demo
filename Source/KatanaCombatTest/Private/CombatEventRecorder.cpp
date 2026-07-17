// Copyright Epic Games, Inc. All Rights Reserved.

#include "CombatEventRecorder.h"

#include "Characters/BaseCombatCharacter.h"
#include "Core/CombatComponent.h"
#include "Core/WeaponComponent.h"

bool ADefenseDestructiveTeamQueryCharacter::IsHostileTo_Implementation(AActor* Other) const
{
	if (AActor* Actor = ActorToDestroyOnHostilityQuery.Get())
	{
		Actor->Destroy();
	}
	return Super::IsHostileTo_Implementation(Other);
}

void UCombatEventRecorder::HandleDamageReceived(const FHitReactionInfo& HitInfo)
{
	++DamageReceivedCount;
	if (const UWeaponComponent* Weapon = ObservedWeapon.Get())
	{
		AcceptedHitCountObservedDuringDamage = Weapon->GetAcceptedHitCountForTesting();
	}

	if (bReenterOnDamage)
	{
		if (ABaseCombatCharacter* Target = ReentryTarget.Get())
		{
			DamageReentryReceipt = Target->ResolveAndCommitCombatContact(ReentryRequest);
		}
	}
	if (bDestroyOnDamage)
	{
		if (AActor* Actor = ActorToDestroy.Get())
		{
			Actor->Destroy();
		}
	}
}

void UCombatEventRecorder::HandleHealthChanged(const float NewHealth, const float MaxHealth)
{
	++HealthChangedCount;
	LastHealth = NewHealth;
	LastMaxHealth = MaxHealth;
	if (bReenterOnHealth)
	{
		if (ABaseCombatCharacter* Target = ReentryTarget.Get())
		{
			HealthReentryReceipt = Target->ResolveAndCommitCombatContact(ReentryRequest);
		}
	}
	if (bBeginBlockOnHealth)
	{
		if (ABaseCombatCharacter* Target = ReentryTarget.Get())
		{
			bBeginBlockResult = Target->CombatComponent
				&& Target->CombatComponent->BeginBlock(ReentryRequest.HitInfo.Attacker);
		}
	}
	if (bDestroyOnHealth)
	{
		if (AActor* Actor = ActorToDestroy.Get())
		{
			Actor->Destroy();
		}
	}
}

void UCombatEventRecorder::HandleCharacterDying(AActor* Killer)
{
	++CharacterDyingCount;
	if (bReenterOnDying)
	{
		if (ABaseCombatCharacter* Target = ReentryTarget.Get())
		{
			DyingReentryReceipt = Target->ResolveAndCommitCombatContact(ReentryRequest);
		}
	}
	if (bDestroyOnDying)
	{
		if (AActor* Actor = ActorToDestroy.Get())
		{
			Actor->Destroy();
		}
	}
}

void UCombatEventRecorder::HandleAttackHit(AActor* HitActor, const FHitReactionInfo& HitInfo)
{
	++AttackHitCount;
}

void UCombatEventRecorder::HandlePairedAnimationEnded(const EPairedReactionType Type)
{
	++PairedAnimationEndedCount;
	LastPairedReaction = Type;
	if (bBeginBlockOnPairedEnded)
	{
		if (ABaseCombatCharacter* Target = PairedActionTarget.Get())
		{
			bBeginBlockOnPairedEndedResult = Target->CombatComponent
				&& Target->CombatComponent->BeginBlock();
		}
	}
	if (bDestroyOnPairedEnded)
	{
		if (AActor* Actor = ActorToDestroy.Get())
		{
			Actor->Destroy();
		}
	}
}
