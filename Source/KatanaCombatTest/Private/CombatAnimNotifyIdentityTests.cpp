// Copyright Epic Games, Inc. All Rights Reserved.

#include "Animation/CombatAnimNotifyIdentity.h"
#include "Animation/ActiveMontageInstanceScope.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimNotifyState_CounterWindow.h"
#include "CombatTestHelpers.h"
#include "Core/CombatComponent.h"
#include "Core/PairedAnimationComponent.h"
#include "Data/AttackData.h"

namespace
{
UAnimSequence* CreateNotifySource(const TCHAR* PackageName, const int32 NotifyCount)
{
	UPackage* Package = CreatePackage(PackageName);
	UAnimSequence* Sequence = NewObject<UAnimSequence>(Package, TEXT("Source"));
	Sequence->Notifies.AddDefaulted(NotifyCount);
	return Sequence;
}

FAnimNotifyRuntimeSourceId MakeSourceId(const TCHAR* Path, const int32 EventIndex)
{
	FAnimNotifyRuntimeSourceId Id;
	Id.SourceAnimation = FSoftObjectPath(Path);
	Id.NotifyEventIndex = EventIndex;
	return Id;
}

bool SeedActiveAttack(UCombatComponent* Combat, UAttackData* Attack, const int32 Generation)
{
	if (!Combat || !Attack)
	{
		return false;
	}

	Combat->SeedAttackWindowStateForTesting(Attack, EAttackPhase::Active, Generation);
	return true;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCombatAnimNotifyIdentity_ExactSourceAddress,
	"KatanaCombat.Defense.NotifyIdentity.ExactSourceAddress",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCombatAnimNotifyIdentity_ExactSourceAddress::RunTest(const FString& Parameters)
{
	(void)Parameters;
	UAnimSequence* Source = CreateNotifySource(TEXT("/Temp/KatanaCombatTests/NotifyIdentityExact"), 2);
	const FAnimNotifyEventReference FirstReference(&Source->Notifies[0], Source);
	const FAnimNotifyRuntimeSourceId FirstId = ResolveRuntimeNotifySourceId(FirstReference);

	TestTrue(TEXT("Exact source notify resolves"), FirstId.IsValid());
	TestEqual(TEXT("Source path is retained"), FirstId.SourceAnimation, FSoftObjectPath(Source));
	TestEqual(TEXT("Exact array address determines event index"), FirstId.NotifyEventIndex, 0);

	FAnimNotifyEvent ForeignNotify;
	const FAnimNotifyEventReference ForeignReference(&ForeignNotify, Source);
	TestFalse(
		TEXT("Value-equivalent notify outside the source array is rejected"),
		ResolveRuntimeNotifySourceId(ForeignReference).IsValid());
	TestFalse(
		TEXT("Missing source is rejected"),
		ResolveRuntimeNotifySourceId(FAnimNotifyEventReference()).IsValid());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCombatAnimNotifyIdentity_MontageContextRequired,
	"KatanaCombat.Defense.NotifyIdentity.MontageContextRequired",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCombatAnimNotifyIdentity_MontageContextRequired::RunTest(const FString& Parameters)
{
	(void)Parameters;
	UAnimSequence* Source = CreateNotifySource(TEXT("/Temp/KatanaCombatTests/NotifyIdentityContext"), 1);
	FAnimNotifyEventReference Reference(&Source->Notifies[0], Source);
	TestEqual(
		TEXT("Missing montage context fails closed"),
		ResolveRuntimeMontageInstanceId(Reference),
		INDEX_NONE);

	Reference.AddContextData<UE::Anim::FAnimNotifyMontageInstanceContext>(37);
	TestEqual(
		TEXT("Runtime montage instance is retained"),
		ResolveRuntimeMontageInstanceId(Reference),
		37);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAttackWindowIdentity_RejectsInvalidOpen,
	"KatanaCombat.Defense.NotifyIdentity.WindowRejectsInvalidOpen",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAttackWindowIdentity_RejectsInvalidOpen::RunTest(const FString& Parameters)
{
	(void)Parameters;
	UWorld* World = FCombatTestHelpers::CreateTestWorld();
	UCombatComponent* Combat = nullptr;
	APlayerCharacter* Player = FCombatTestHelpers::CreateTestCharacterWithCombat(World, Combat);
	UAttackData* Attack = FCombatTestHelpers::CreateTestAttack(EAttackType::Light);
	if (!Player || !SeedActiveAttack(Combat, Attack, 1))
	{
		AddError(TEXT("Failed to create attack-window fixture"));
		FCombatTestHelpers::DestroyTestWorld(World);
		return false;
	}

	TestFalse(
		TEXT("Invalid notify source cannot open a canonical window"),
		Combat->OpenAttackWindow(EAttackWindowKind::Parry, {}, 4, 0.2f).IsValid());
	TestFalse(
		TEXT("Missing montage context cannot open a canonical window"),
		Combat->OpenAttackWindow(
			EAttackWindowKind::Parry,
			MakeSourceId(TEXT("/Game/Test/Attack"), 0),
			INDEX_NONE,
			0.2f).IsValid());
	TestFalse(
		TEXT("Rejected opens are not published"),
		Combat->BuildAttackExecutionSnapshot().ActiveParryWindow.IsValid());

	FCombatTestHelpers::DestroyTestWorld(World);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAttackWindowIdentity_InvalidCounterNotifyCannotPublishLegacyContext,
	"KatanaCombat.Defense.NotifyIdentity.InvalidCounterNotifyCannotPublishLegacyContext",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAttackWindowIdentity_InvalidCounterNotifyCannotPublishLegacyContext::RunTest(
	const FString& Parameters)
{
	(void)Parameters;
	UWorld* World = FCombatTestHelpers::CreateTestWorld();
	UCombatComponent* Combat = nullptr;
	APlayerCharacter* Player = FCombatTestHelpers::CreateTestCharacterWithCombat(World, Combat);
	UAttackData* Attack = FCombatTestHelpers::CreateTestAttack(EAttackType::Light);
	UAnimSequence* Source = CreateNotifySource(
		TEXT("/Temp/KatanaCombatTests/CounterNotifyMissingMontage"), 1);
	FAnimNotifyEventReference Reference(&Source->Notifies[0], Source);
	if (!Player || !Player->PairedAnimationComponent || !SeedActiveAttack(Combat, Attack, 2))
	{
		AddError(TEXT("Failed to create invalid counter-notify fixture"));
		FCombatTestHelpers::DestroyTestWorld(World);
		return false;
	}

	UAnimNotifyState_CounterWindow* Notify = NewObject<UAnimNotifyState_CounterWindow>();
	static_cast<UAnimNotifyState*>(Notify)->NotifyBegin(
		Player->GetMesh(), Source, 0.25f, Reference);
	TestFalse(TEXT("Missing montage context cannot publish a canonical counter window"),
		Combat->GetActiveAttackWindow(EAttackWindowKind::Counter).IsValid());
	TestFalse(TEXT("Rejected canonical open cannot publish legacy counter context"),
		Player->PairedAnimationComponent->IsInCounterWindow());

	FCombatTestHelpers::DestroyTestWorld(World);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAttackWindowIdentity_StaleEndCannotCloseNewAttack,
	"KatanaCombat.Defense.NotifyIdentity.StaleEndCannotCloseNewAttack",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAttackWindowIdentity_StaleEndCannotCloseNewAttack::RunTest(const FString& Parameters)
{
	(void)Parameters;
	UWorld* World = FCombatTestHelpers::CreateTestWorld();
	UCombatComponent* Combat = nullptr;
	APlayerCharacter* Player = FCombatTestHelpers::CreateTestCharacterWithCombat(World, Combat);
	UAttackData* Attack = FCombatTestHelpers::CreateTestAttack(EAttackType::Light);
	const FAnimNotifyRuntimeSourceId Source = MakeSourceId(TEXT("/Game/Test/ParryWindow"), 2);
	if (!Player || !SeedActiveAttack(Combat, Attack, 11))
	{
		AddError(TEXT("Failed to create stale-window fixture"));
		FCombatTestHelpers::DestroyTestWorld(World);
		return false;
	}

	const FAttackWindowInstanceId OldWindow = Combat->OpenAttackWindow(
		EAttackWindowKind::Parry, Source, 101, 0.2f);
	Combat->SeedAttackWindowStateForTesting(Attack, EAttackPhase::Active, 12);
	const FAttackWindowInstanceId NewWindow = Combat->OpenAttackWindow(
		EAttackWindowKind::Parry, Source, 102, 0.2f);

	TestTrue(TEXT("Old window opens"), OldWindow.IsValid());
	TestTrue(TEXT("New attack window opens"), NewWindow.IsValid());
	TestFalse(
		TEXT("Old montage end does not close the new attack"),
		Combat->CloseAttackWindow(EAttackWindowKind::Parry, Source, 101));
	TestEqual(
		TEXT("New attack remains the published parry window"),
		Combat->BuildAttackExecutionSnapshot().ActiveParryWindow,
		NewWindow);

	TestTrue(
		TEXT("Matching new end closes its own window"),
		Combat->CloseAttackWindow(EAttackWindowKind::Parry, Source, 102));
	TestFalse(
		TEXT("Closed window is no longer published"),
		Combat->BuildAttackExecutionSnapshot().ActiveParryWindow.IsValid());

	FCombatTestHelpers::DestroyTestWorld(World);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAttackWindowIdentity_ConsumedAttackRetiresOpenRecords,
	"KatanaCombat.Defense.NotifyIdentity.ConsumedAttackRetiresOpenRecords",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAttackWindowIdentity_ConsumedAttackRetiresOpenRecords::RunTest(const FString& Parameters)
{
	(void)Parameters;
	UWorld* World = FCombatTestHelpers::CreateTestWorld();
	UCombatComponent* Combat = nullptr;
	APlayerCharacter* Player = FCombatTestHelpers::CreateTestCharacterWithCombat(World, Combat);
	UAttackData* Attack = FCombatTestHelpers::CreateTestAttack(EAttackType::Light);
	const FAnimNotifyRuntimeSourceId Source = MakeSourceId(TEXT("/Game/Test/ConsumedParryWindow"), 5);
	if (!Player || !SeedActiveAttack(Combat, Attack, 13))
	{
		AddError(TEXT("Failed to create consumed-window fixture"));
		FCombatTestHelpers::DestroyTestWorld(World);
		return false;
	}

	const FAttackWindowInstanceId ConsumedWindow = Combat->OpenAttackWindow(
		EAttackWindowKind::Parry, Source, 103, 0.2f);
	TestTrue(TEXT("Consumed attack window opens"), ConsumedWindow.IsValid());
	TestTrue(TEXT("Exact attack generation can be consumed"),
		Combat->ConsumeActiveAttack(ConsumedWindow.AttackInstance, EAttackConsumeReason::PerfectParry));
	TestFalse(TEXT("Delayed End from the consumed attack has no retained record"),
		Combat->CloseAttackWindow(EAttackWindowKind::Parry, Source, 103));
	TestFalse(TEXT("Delayed Begin cannot reopen the consumed generation"),
		Combat->OpenAttackWindow(EAttackWindowKind::Parry, Source, 103, 0.2f).IsValid());

	Combat->SeedAttackWindowStateForTesting(Attack, EAttackPhase::Active, 14);
	const FAttackWindowInstanceId Successor = Combat->OpenAttackWindow(
		EAttackWindowKind::Parry, Source, 103, 0.2f);
	TestTrue(TEXT("Successor reuses the authored notify and montage identity"), Successor.IsValid());
	TestTrue(TEXT("One matching successor End closes without consuming stale records"),
		Combat->CloseAttackWindow(EAttackWindowKind::Parry, Source, 103));
	TestFalse(TEXT("Successor window is no longer published"),
		Combat->BuildAttackExecutionSnapshot().ActiveParryWindow.IsValid());

	FCombatTestHelpers::DestroyTestWorld(World);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAttackWindowIdentity_DuplicateEndCannotCloseReopenedWindow,
	"KatanaCombat.Defense.NotifyIdentity.DuplicateEndCannotCloseReopenedWindow",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAttackWindowIdentity_DuplicateEndCannotCloseReopenedWindow::RunTest(const FString& Parameters)
{
	(void)Parameters;
	UWorld* World = FCombatTestHelpers::CreateTestWorld();
	UCombatComponent* Combat = nullptr;
	APlayerCharacter* Player = FCombatTestHelpers::CreateTestCharacterWithCombat(World, Combat);
	UAttackData* Attack = FCombatTestHelpers::CreateTestAttack(EAttackType::Light);
	const FAnimNotifyRuntimeSourceId Source = MakeSourceId(TEXT("/Game/Test/ReopenedParryWindow"), 4);
	if (!Player || !SeedActiveAttack(Combat, Attack, 21))
	{
		AddError(TEXT("Failed to create duplicate-window fixture"));
		FCombatTestHelpers::DestroyTestWorld(World);
		return false;
	}

	const FAttackWindowInstanceId First = Combat->OpenAttackWindow(
		EAttackWindowKind::Parry, Source, 201, 0.1f);
	const FAttackWindowInstanceId Reopened = Combat->OpenAttackWindow(
		EAttackWindowKind::Parry, Source, 201, 0.2f);
	TestTrue(TEXT("Reopen advances the window generation"), Reopened.WindowGeneration > First.WindowGeneration);

	TestFalse(
		TEXT("First matching End retires the stale open without closing the reopen"),
		Combat->CloseAttackWindow(EAttackWindowKind::Parry, Source, 201));
	TestEqual(
		TEXT("Reopened window remains current"),
		Combat->BuildAttackExecutionSnapshot().ActiveParryWindow,
		Reopened);
	TestTrue(
		TEXT("Second matching End closes the reopened window"),
		Combat->CloseAttackWindow(EAttackWindowKind::Parry, Source, 201));

	FCombatTestHelpers::DestroyTestWorld(World);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAttackWindowIdentity_StaleRecoveryCannotTransitionNewAttack,
	"KatanaCombat.Defense.NotifyIdentity.StaleRecoveryCannotTransitionNewAttack",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAttackWindowIdentity_StaleRecoveryCannotTransitionNewAttack::RunTest(const FString& Parameters)
{
	(void)Parameters;
	UWorld* World = FCombatTestHelpers::CreateTestWorld();
	UCombatComponent* Combat = nullptr;
	APlayerCharacter* Player = FCombatTestHelpers::CreateTestCharacterWithCombat(World, Combat);
	UAttackData* Attack = FCombatTestHelpers::CreateTestAttack(EAttackType::Light);
	const FAnimNotifyRuntimeSourceId ActiveSource = MakeSourceId(TEXT("/Game/Test/ActivePoint"), 1);
	const FAnimNotifyRuntimeSourceId RecoverySource = MakeSourceId(TEXT("/Game/Test/RecoveryPoint"), 2);
	if (!Player || !Combat || !Attack)
	{
		AddError(TEXT("Failed to create phase-transition fixture"));
		FCombatTestHelpers::DestroyTestWorld(World);
		return false;
	}
	Combat->SeedAttackWindowStateForTesting(Attack, EAttackPhase::Windup, 31);
	TestTrue(
		TEXT("First attack opens its contextual Active transition"),
		Combat->OnPhaseTransitionWithContext(EAttackPhase::Active, ActiveSource, 301, 0.5f));
	Combat->SeedAttackWindowStateForTesting(Attack, EAttackPhase::Windup, 32);
	TestTrue(
		TEXT("Second attack opens a new hit generation"),
		Combat->OnPhaseTransitionWithContext(EAttackPhase::Active, ActiveSource, 301, 0.5f));
	const FAttackWindowInstanceId NewHit = Combat->GetActiveAttackWindow(EAttackWindowKind::Hit);

	TestFalse(
		TEXT("Delayed Recovery retires the old Begin but cannot transition the new attack"),
		Combat->OnPhaseTransitionWithContext(EAttackPhase::Recovery, RecoverySource, 301, 0.0f));
	TestEqual(TEXT("New attack remains Active"), Combat->GetCurrentPhase(), EAttackPhase::Active);
	TestEqual(
		TEXT("New hit generation remains published"),
		Combat->GetActiveAttackWindow(EAttackWindowKind::Hit),
		NewHit);

	TestTrue(
		TEXT("Matching current Recovery closes and transitions the new attack"),
		Combat->OnPhaseTransitionWithContext(EAttackPhase::Recovery, RecoverySource, 301, 0.0f));
	TestEqual(TEXT("Current attack reaches Recovery"), Combat->GetCurrentPhase(), EAttackPhase::Recovery);
	TestFalse(
		TEXT("Recovery clears the current hit window"),
		Combat->GetActiveAttackWindow(EAttackWindowKind::Hit).IsValid());

	FCombatTestHelpers::DestroyTestWorld(World);
	return true;
}
