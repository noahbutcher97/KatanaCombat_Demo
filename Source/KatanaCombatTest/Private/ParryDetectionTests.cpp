// Copyright Epic Games, Inc. All Rights Reserved.

#include "CombatTestHelpers.h"
#include "Core/CombatComponent.h"
#include "Data/AttackData.h"

/** The attacker owns canonical parry-window identity; the defender only queries it. */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FParryDetectionAttackerOwnsCanonicalWindow,
	"KatanaCombat.Defense.ParryDetection.AttackerOwnsCanonicalWindow",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FParryDetectionAttackerOwnsCanonicalWindow::RunTest(const FString& Parameters)
{
	(void)Parameters;
	UWorld* World = FCombatTestHelpers::CreateTestWorld();
	UCombatComponent* AttackerCombat = nullptr;
	UCombatComponent* DefenderCombat = nullptr;
	APlayerCharacter* Attacker =
		FCombatTestHelpers::CreateTestCharacterWithCombat(World, AttackerCombat);
	APlayerCharacter* Defender =
		FCombatTestHelpers::CreateTestCharacterWithCombat(World, DefenderCombat);
	if (!TestNotNull(TEXT("Attacker combat component"), AttackerCombat)
		|| !TestNotNull(TEXT("Defender combat component"), DefenderCombat))
	{
		FCombatTestHelpers::DestroyTestWorld(World);
		return false;
	}

	TestFalse(TEXT("Attacker starts without a canonical parry window"),
		AttackerCombat->GetActiveAttackWindow(EAttackWindowKind::Parry).IsValid());
	TestFalse(TEXT("Defender starts without a canonical parry window"),
		DefenderCombat->GetActiveAttackWindow(EAttackWindowKind::Parry).IsValid());

	UAttackData* Attack = FCombatTestHelpers::CreateTestAttack(EAttackType::Light);
	AttackerCombat->SeedAttackWindowStateForTesting(Attack, EAttackPhase::Windup, 17);
	FAnimNotifyRuntimeSourceId Source;
	Source.SourceAnimation = FSoftObjectPath(
		TEXT("/Game/Test/Defense/AM_AttackerParryWindow"));
	Source.NotifyEventIndex = 2;
	const FAttackWindowInstanceId Window = AttackerCombat->OpenAttackWindow(
		EAttackWindowKind::Parry,
		Source,
		71,
		0.30f);
	TestTrue(TEXT("Attacker opens an identity-bearing parry window"), Window.IsValid());
	TestEqual(TEXT("Published window is the exact opened instance"),
		AttackerCombat->GetActiveAttackWindow(EAttackWindowKind::Parry), Window);
	TestFalse(TEXT("Attacker window is never copied onto the defender"),
		DefenderCombat->GetActiveAttackWindow(EAttackWindowKind::Parry).IsValid());

	TestFalse(TEXT("A stale montage instance cannot close the active window"),
		AttackerCombat->CloseAttackWindow(EAttackWindowKind::Parry, Source, 72));
	TestEqual(TEXT("Rejected stale close preserves the active window"),
		AttackerCombat->GetActiveAttackWindow(EAttackWindowKind::Parry), Window);
	TestTrue(TEXT("The matching runtime source and montage instance close the window"),
		AttackerCombat->CloseAttackWindow(EAttackWindowKind::Parry, Source, 71));
	TestFalse(TEXT("Closed parry window is no longer published"),
		AttackerCombat->GetActiveAttackWindow(EAttackWindowKind::Parry).IsValid());

	World->DestroyActor(Attacker);
	World->DestroyActor(Defender);
	FCombatTestHelpers::DestroyTestWorld(World);
	return true;
}
