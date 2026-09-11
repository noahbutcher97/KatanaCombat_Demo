// Copyright Epic Games, Inc. All Rights Reserved.
#include "CombatTestHelpers.h"
#include "Animation/AnimMontage.h"
#include "Characters/EnemyCharacter.h"
#include "Characters/PlayerCharacter.h"
#include "Components/CapsuleComponent.h"
#include "Core/HitReactionComponent.h"
#include "Core/PairedAnimationComponent.h"
#include "Data/PairedAnimationData.h"
#include "Interfaces/DamageableInterface.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPairedCollisionOpeningDeath,
	"KatanaCombat.PairedAnimation.Collision.OpeningDeathPreservesVictimPartner",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)
bool FPairedCollisionOpeningDeath::RunTest(const FString& Parameters)
{
	for (const bool bExpectedDeath : {true, false})
	{
		UWorld* World = FCombatTestHelpers::CreateTestWorld();
		auto* Attacker = FCombatTestHelpers::CreateTestPlayerCharacter(World);
		auto* Victim = FCombatTestHelpers::CreateTestEnemyCharacter(World, FVector(150, 0, 0));
		if (!Attacker || !Victim) { AddError(TEXT("Missing participants")); FCombatTestHelpers::DestroyTestWorld(World); return false; }
		for (ABaseCombatCharacter* Character : {static_cast<ABaseCombatCharacter*>(Attacker), static_cast<ABaseCombatCharacter*>(Victim)})
		{
			auto* Attacks = Character->CombatSettings->DefaultWeaponData->AttackConfiguration.Get();
			Attacks->DefaultLightAttack = FCombatTestHelpers::CreateTestAttack();
			Attacks->DefaultHeavyAttack = FCombatTestHelpers::CreateTestAttack(EAttackType::Heavy);
			if (!Character->HasActorBegunPlay()) { Character->DispatchBeginPlay(); }
		}
		auto* Owner = Attacker->PairedAnimationComponent.Get();
		auto* Participant = Victim->PairedAnimationComponent.Get();
		auto* Data = NewObject<UPairedAnimationData>();
		Data->bApplySlowMotion = false; Data->bIsLethal = true;
		Data->BaseDamage = 100; Data->DamageMultiplier = 1;
		Data->VictimMontage = NewObject<UAnimMontage>(Data);
		Victim->CurrentHealth = 15;
		Victim->HitReactionComponent->EnterPairedAnimationState(Data->VictimMontage, EReactionOutcome::Ragdoll, 0, true, Attacker);
		Owner->AddPairedPartner(Victim); Participant->AddPairedPartner(Attacker);
		Owner->BeginPairedAnimation(Data, EPairedReactionType::Finisher, false);
		if (bExpectedDeath) { Owner->HandlePairedSyncPoint(TEXT("OpeningDamage"), true); }
		else
		{
			FHitReactionInfo Hit; Hit.Attacker = Attacker; Hit.Damage = 100;
			IDamageableInterface::Execute_ApplyDamage(Victim, Hit);
		}
		TestTrue(TEXT("Actual damage delivers the dying event"), Victim->IsDeadOrDying());
		TestEqual(TEXT("Only expected finisher damage preserves the victim partner"), Participant->IsPairedPartner(Attacker), bExpectedDeath);
		TestEqual(TEXT("Unrelated death cancels the owning sequence"), Owner->IsPairedAnimationActive(), bExpectedDeath);
		if (bExpectedDeath)
		{
			// The opening sync can dispatch death before the victim's first notify tick.
			FAnimNotifyRuntimeSourceId Source; Source.SourceAnimation = FSoftObjectPath(TEXT("/Game/Test/Paired/Collision")); Source.NotifyEventIndex = 1;
			TestTrue(TEXT("Victim collision window begins after lethal sync"), Participant->BeginPairedCollisionNotify(Source, 5, true, true, false, false, false, 150));
			TestTrue(TEXT("Late collision window ignores its attacker"), Victim->GetCapsuleComponent()->GetMoveIgnoreActors().Contains(Attacker));
			Owner->CancelPairedAnimation();
			TestEqual(TEXT("Cancellation releases victim collision lease"), Participant->GetActivePairedStateLeaseCount(), 0);
			TestFalse(TEXT("Cancellation clears victim partner"), Participant->IsPairedPartner(Attacker));
			TestFalse(TEXT("Cancellation restores movement collision"), Victim->GetCapsuleComponent()->GetMoveIgnoreActors().Contains(Attacker));
		}
		FCombatTestHelpers::DestroyTestWorld(World);
	}
	return true;
}
