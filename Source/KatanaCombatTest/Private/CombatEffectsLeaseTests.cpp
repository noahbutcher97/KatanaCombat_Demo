// Copyright Epic Games, Inc. All Rights Reserved.

#include "CombatTestHelpers.h"

#include "Containers/Ticker.h"
#include "Core/PairedAnimationComponent.h"
#include "Data/PairedAnimationData.h"
#include "GameFramework/WorldSettings.h"
#include "Subsystems/CombatEffectsWorldSubsystem.h"
#include "Utilities/CinematicEffectsUtilityLibrary.h"

#include <limits>

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCombatEffectsOverlappingWorldLeasesTest,
	"KatanaCombat.Defense.CombatEffects.OverlappingWorldLeases",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FCombatEffectsOverlappingWorldLeasesTest::RunTest(const FString& Parameters)
{
	UWorld* World = FCombatTestHelpers::CreateTestWorld();
	UCombatEffectsWorldSubsystem* Effects = World
		? World->GetSubsystem<UCombatEffectsWorldSubsystem>()
		: nullptr;
	if (!TestNotNull(TEXT("World effects subsystem exists"), Effects))
	{
		FCombatTestHelpers::DestroyTestWorld(World);
		return false;
	}
	TestFalse(TEXT("Compatibility slow motion rejects a nonfinite scale"),
		UCinematicEffectsUtilityLibrary::ApplySlowMotion(
			World,
			std::numeric_limits<float>::quiet_NaN()));
	TestEqual(TEXT("Rejected compatibility input acquires no lease"),
		Effects->GetActiveLeaseCount(), 0);

	World->GetWorldSettings()->SetTimeDilation(0.8f);
	const FTimeDilationLeaseHandle First = Effects->AcquireWorldLease(TEXT("First"), 0.5f, 10.0);
	const FTimeDilationLeaseHandle Second = Effects->AcquireWorldLease(TEXT("Second"), 0.2f, 10.0);
	TestTrue(TEXT("Both world handles are valid"), First.IsValid() && Second.IsValid());
	TestEqual(TEXT("Minimum active request wins"), World->GetWorldSettings()->TimeDilation, 0.2f);

	TestTrue(TEXT("Second owner can release itself"), Effects->ReleaseLease(Second));
	TestEqual(TEXT("Remaining request resumes"), World->GetWorldSettings()->TimeDilation, 0.5f);
	TestFalse(TEXT("Duplicate release is rejected"), Effects->ReleaseLease(Second));
	TestTrue(TEXT("First owner can release itself"), Effects->ReleaseLease(First));
	TestEqual(TEXT("Last release restores captured baseline"), World->GetWorldSettings()->TimeDilation, 0.8f);

	FCombatTestHelpers::DestroyTestWorld(World);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCombatEffectsOverlappingActorLeasesTest,
	"KatanaCombat.Defense.CombatEffects.OverlappingActorLeases",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FCombatEffectsOverlappingActorLeasesTest::RunTest(const FString& Parameters)
{
	UWorld* World = FCombatTestHelpers::CreateTestWorld();
	APlayerCharacter* Actor = FCombatTestHelpers::CreateTestPlayerCharacter(World);
	UCombatEffectsWorldSubsystem* Effects = World
		? World->GetSubsystem<UCombatEffectsWorldSubsystem>()
		: nullptr;
	if (!TestNotNull(TEXT("World effects subsystem exists"), Effects)
		|| !TestNotNull(TEXT("Actor exists"), Actor))
	{
		FCombatTestHelpers::DestroyTestWorld(World);
		return false;
	}

	Actor->CustomTimeDilation = 0.75f;
	const FTimeDilationLeaseHandle First = Effects->AcquireActorLease(Actor, TEXT("First"), 0.4f, 10.0);
	const FTimeDilationLeaseHandle Second = Effects->AcquireActorLease(Actor, TEXT("Second"), 0.0001f, 10.0);
	TestEqual(TEXT("Minimum actor request wins"), Actor->CustomTimeDilation, 0.0001f);
	TestTrue(TEXT("Freeze owner releases itself"), Effects->ReleaseLease(Second));
	TestEqual(TEXT("Remaining actor request resumes"), Actor->CustomTimeDilation, 0.4f);
	TestTrue(TEXT("Final actor owner releases itself"), Effects->ReleaseLease(First));
	TestEqual(TEXT("Actor baseline is restored"), Actor->CustomTimeDilation, 0.75f);

	TestFalse(TEXT("Null actor fails closed"),
		Effects->AcquireActorLease(nullptr, TEXT("Invalid"), 0.5f, 10.0).IsValid());
	TestFalse(TEXT("Nonfinite request fails closed"),
		Effects->AcquireActorLease(
			Actor,
			TEXT("Invalid"),
			std::numeric_limits<float>::quiet_NaN(),
			10.0).IsValid());
	TestFalse(TEXT("Watchdogs that cannot be represented by the ticker fail closed"),
		Effects->AcquireActorLease(
			Actor,
			TEXT("InvalidWatchdog"),
			0.5f,
			std::numeric_limits<double>::max()).IsValid());

	FCombatTestHelpers::DestroyTestWorld(World);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCombatEffectsWatchdogRestorationTest,
	"KatanaCombat.Defense.CombatEffects.WatchdogRestoration",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FCombatEffectsWatchdogRestorationTest::RunTest(const FString& Parameters)
{
	UWorld* World = FCombatTestHelpers::CreateTestWorld();
	APlayerCharacter* Actor = FCombatTestHelpers::CreateTestPlayerCharacter(World);
	UCombatEffectsWorldSubsystem* Effects = World
		? World->GetSubsystem<UCombatEffectsWorldSubsystem>()
		: nullptr;
	if (!Effects || !Actor)
	{
		AddError(TEXT("Failed to create watchdog fixture"));
		FCombatTestHelpers::DestroyTestWorld(World);
		return false;
	}

	World->GetWorldSettings()->SetTimeDilation(0.85f);
	Actor->CustomTimeDilation = 0.65f;
	const FTimeDilationLeaseHandle WorldLease =
		Effects->AcquireWorldLease(TEXT("WatchdogWorld"), 0.25f, 0.01);
	const FTimeDilationLeaseHandle ActorLease =
		Effects->AcquireActorLease(Actor, TEXT("WatchdogActor"), 0.15f, 0.01);
	TestTrue(TEXT("Watchdog fixture acquires both leases"),
		WorldLease.IsValid() && ActorLease.IsValid());
	FTSTicker::GetCoreTicker().Tick(0.02f);

	TestFalse(TEXT("World watchdog retires its exact handle"), Effects->IsLeaseActive(WorldLease));
	TestFalse(TEXT("Actor watchdog retires its exact handle"), Effects->IsLeaseActive(ActorLease));
	TestEqual(TEXT("World watchdog restores captured baseline"),
		World->GetWorldSettings()->TimeDilation, 0.85f);
	TestEqual(TEXT("Actor watchdog restores captured baseline"),
		Actor->CustomTimeDilation, 0.65f);
	TestEqual(TEXT("All watchdog records are removed"), Effects->GetActiveLeaseCount(), 0);

	FCombatTestHelpers::DestroyTestWorld(World);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCombatEffectsDestroyedActorReleaseTest,
	"KatanaCombat.Defense.CombatEffects.DestroyedActorRelease",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FCombatEffectsDestroyedActorReleaseTest::RunTest(const FString& Parameters)
{
	UWorld* World = FCombatTestHelpers::CreateTestWorld();
	APlayerCharacter* Actor = FCombatTestHelpers::CreateTestPlayerCharacter(World);
	UCombatEffectsWorldSubsystem* Effects = World
		? World->GetSubsystem<UCombatEffectsWorldSubsystem>()
		: nullptr;
	if (!Effects || !Actor)
	{
		AddError(TEXT("Failed to create destroyed-actor fixture"));
		FCombatTestHelpers::DestroyTestWorld(World);
		return false;
	}
	const FTimeDilationLeaseHandle Handle =
		Effects->AcquireActorLease(Actor, TEXT("DestroyedActor"), 0.2f, 10.0);
	TestTrue(TEXT("Destroyed actor fixture acquires a lease"), Handle.IsValid());
	Actor->Destroy();
	TestTrue(TEXT("Destroyed actor lease remains safely releasable"), Effects->ReleaseLease(Handle));
	TestEqual(TEXT("Destroyed actor release removes the record"), Effects->GetActiveLeaseCount(), 0);
	TestFalse(TEXT("Destroyed actor duplicate release is harmless"), Effects->ReleaseLease(Handle));

	FCombatTestHelpers::DestroyTestWorld(World);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCombatEffectsLegacyCompatibilityOverlapTest,
	"KatanaCombat.Defense.CombatEffects.LegacyCompatibilityOverlap",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FCombatEffectsLegacyCompatibilityOverlapTest::RunTest(const FString& Parameters)
{
	UWorld* World = FCombatTestHelpers::CreateTestWorld();
	APlayerCharacter* Actor = FCombatTestHelpers::CreateTestPlayerCharacter(World);
	UCombatEffectsWorldSubsystem* Effects = World
		? World->GetSubsystem<UCombatEffectsWorldSubsystem>()
		: nullptr;
	if (!Effects || !Actor)
	{
		AddError(TEXT("Failed to create compatibility-overlap fixture"));
		FCombatTestHelpers::DestroyTestWorld(World);
		return false;
	}

	World->GetWorldSettings()->SetTimeDilation(0.8f);
	const FTimeDilationLeaseHandle IndependentWorld =
		Effects->AcquireWorldLease(TEXT("IndependentWorld"), 0.4f, 10.0);
	TestTrue(TEXT("Legacy slow motion acquires alongside independent ownership"),
		UCinematicEffectsUtilityLibrary::ApplySlowMotion(World, 0.2f));
	TestEqual(TEXT("Slower compatibility request wins"),
		World->GetWorldSettings()->TimeDilation, 0.2f);
	UCinematicEffectsUtilityLibrary::RestoreTimeDilation(World);
	TestEqual(TEXT("Legacy restore cannot clobber independent world ownership"),
		World->GetWorldSettings()->TimeDilation, 0.4f);
	Effects->ReleaseLease(IndependentWorld);
	TestEqual(TEXT("Independent world release restores baseline"),
		World->GetWorldSettings()->TimeDilation, 0.8f);

	Actor->CustomTimeDilation = 0.75f;
	const FTimeDilationLeaseHandle IndependentActor =
		Effects->AcquireActorLease(Actor, TEXT("IndependentActor"), 0.35f, 10.0);
	UCinematicEffectsUtilityLibrary::SetActorTimeDilation(Actor, 0.1f);
	TestEqual(TEXT("Legacy actor request composes with independent ownership"),
		Actor->CustomTimeDilation, 0.1f);
	UCinematicEffectsUtilityLibrary::RestoreActorTimeDilation(Actor);
	TestEqual(TEXT("Legacy actor restore cannot clobber independent ownership"),
		Actor->CustomTimeDilation, 0.35f);
	Effects->ReleaseLease(IndependentActor);
	TestEqual(TEXT("Independent actor release restores baseline"),
		Actor->CustomTimeDilation, 0.75f);

	FCombatTestHelpers::DestroyTestWorld(World);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCombatEffectsOverlappingHitstopTest,
	"KatanaCombat.Defense.CombatEffects.OverlappingHitstop",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FCombatEffectsOverlappingHitstopTest::RunTest(const FString& Parameters)
{
	UWorld* World = FCombatTestHelpers::CreateTestWorld();
	APlayerCharacter* Actor = FCombatTestHelpers::CreateTestPlayerCharacter(World);
	UCombatEffectsWorldSubsystem* Effects = World
		? World->GetSubsystem<UCombatEffectsWorldSubsystem>()
		: nullptr;
	if (!Effects || !Actor)
	{
		AddError(TEXT("Failed to create hitstop fixture"));
		FCombatTestHelpers::DestroyTestWorld(World);
		return false;
	}
	Actor->CustomTimeDilation = 0.6f;
	TArray<AActor*> Actors{Actor};
	TestTrue(TEXT("First hitstop acquires"),
		UCinematicEffectsUtilityLibrary::ApplyHitstopToActors(Actors, 0.01f));
	TestTrue(TEXT("Overlapping hitstop acquires independently"),
		UCinematicEffectsUtilityLibrary::ApplyHitstopToActors(Actors, 0.03f));
	TestEqual(TEXT("Two hitstops own two leases"), Effects->GetActiveLeaseCount(), 2);
	FTSTicker::GetCoreTicker().Tick(0.02f);
	TestEqual(TEXT("First watchdog leaves the overlapping freeze active"),
		Effects->GetActiveLeaseCount(), 1);
	TestEqual(TEXT("Overlapping freeze remains effective"), Actor->CustomTimeDilation, 0.0001f);
	FTSTicker::GetCoreTicker().Tick(0.02f);
	TestEqual(TEXT("Last hitstop watchdog releases all ownership"), Effects->GetActiveLeaseCount(), 0);
	TestEqual(TEXT("Last hitstop restores pre-freeze actor dilation"),
		Actor->CustomTimeDilation, 0.6f);

	FCombatTestHelpers::DestroyTestWorld(World);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCombatEffectsSavedFreezeDuplicateInputTest,
	"KatanaCombat.Defense.CombatEffects.SavedFreezeDuplicateInput",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FCombatEffectsSavedFreezeDuplicateInputTest::RunTest(const FString& Parameters)
{
	UWorld* World = FCombatTestHelpers::CreateTestWorld();
	APlayerCharacter* Actor = FCombatTestHelpers::CreateTestPlayerCharacter(World);
	UCombatEffectsWorldSubsystem* Effects = World
		? World->GetSubsystem<UCombatEffectsWorldSubsystem>()
		: nullptr;
	if (!Effects || !Actor)
	{
		AddError(TEXT("Failed to create duplicate saved-freeze fixture"));
		FCombatTestHelpers::DestroyTestWorld(World);
		return false;
	}

	Actor->CustomTimeDilation = 0.65f;
	TArray<AActor*> DuplicateActors{Actor, Actor};
	const TMap<TWeakObjectPtr<AActor>, float> Saved =
		UCinematicEffectsUtilityLibrary::FreezeActorsWithSave(DuplicateActors);
	TestEqual(TEXT("Duplicate input produces one saved actor record"), Saved.Num(), 1);
	TestEqual(TEXT("Duplicate input acquires exactly one saved-freeze lease"),
		Effects->GetActiveLeaseCount(), 1);
	TestEqual(TEXT("The actor is frozen"), Actor->CustomTimeDilation, 0.0001f);

	UCinematicEffectsUtilityLibrary::RestoreActorsFromSaved(Saved);
	TestEqual(TEXT("One restore releases all ownership created by the call"),
		Effects->GetActiveLeaseCount(), 0);
	TestEqual(TEXT("One restore returns the actor to its captured baseline"),
		Actor->CustomTimeDilation, 0.65f);

	FCombatTestHelpers::DestroyTestWorld(World);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCombatEffectsWorldTeardownTest,
	"KatanaCombat.Defense.CombatEffects.WorldTeardown",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FCombatEffectsWorldTeardownTest::RunTest(const FString& Parameters)
{
	UWorld* World = FCombatTestHelpers::CreateTestWorld();
	APlayerCharacter* Actor = FCombatTestHelpers::CreateTestPlayerCharacter(World);
	UCombatEffectsWorldSubsystem* Effects = World
		? World->GetSubsystem<UCombatEffectsWorldSubsystem>()
		: nullptr;
	if (!Effects || !Actor)
	{
		AddError(TEXT("Failed to create world-teardown fixture"));
		FCombatTestHelpers::DestroyTestWorld(World);
		return false;
	}
	Effects->AcquireWorldLease(TEXT("TeardownWorld"), 0.3f, 0.01);
	Effects->AcquireActorLease(Actor, TEXT("TeardownActor"), 0.2f, 0.01);
	FCombatTestHelpers::DestroyTestWorld(World);
	FTSTicker::GetCoreTicker().Tick(0.02f);
	TestTrue(TEXT("World teardown removes watchdogs without stale dereference"), true);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCombatEffectsPairedOwnerIsolationTest,
	"KatanaCombat.Defense.CombatEffects.PairedOwnerIsolation",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FCombatEffectsPairedOwnerIsolationTest::RunTest(const FString& Parameters)
{
	UWorld* World = FCombatTestHelpers::CreateTestWorld();
	APlayerCharacter* First = FCombatTestHelpers::CreateTestPlayerCharacter(World);
	APlayerCharacter* Second = FCombatTestHelpers::CreateTestPlayerCharacter(
		World,
		FVector(0.0f, 300.0f, 0.0f));
	UCombatEffectsWorldSubsystem* Effects = World
		? World->GetSubsystem<UCombatEffectsWorldSubsystem>()
		: nullptr;
	UPairedAnimationComponent* FirstPaired = First ? First->PairedAnimationComponent.Get() : nullptr;
	UPairedAnimationComponent* SecondPaired = Second ? Second->PairedAnimationComponent.Get() : nullptr;
	if (!Effects || !FirstPaired || !SecondPaired)
	{
		AddError(TEXT("Failed to create paired-owner isolation fixture"));
		FCombatTestHelpers::DestroyTestWorld(World);
		return false;
	}

	UPairedAnimationData* FirstData = NewObject<UPairedAnimationData>();
	FirstData->bApplySlowMotion = true;
	FirstData->SlowMotionScale = 0.5f;
	FirstData->SlowMotionDuration = 5.0f;
	UPairedAnimationData* SecondData = NewObject<UPairedAnimationData>();
	SecondData->bApplySlowMotion = true;
	SecondData->SlowMotionScale = 0.2f;
	SecondData->SlowMotionDuration = 5.0f;

	FirstPaired->BeginPairedAnimation(FirstData, EPairedReactionType::Counter, true);
	SecondPaired->BeginPairedAnimation(SecondData, EPairedReactionType::Finisher, true);
	TestEqual(TEXT("Each paired owner acquires an independent world lease"),
		Effects->GetActiveLeaseCount(), 2);
	TestEqual(TEXT("The slowest paired request wins"),
		World->GetWorldSettings()->TimeDilation, 0.2f);

	FirstPaired->EndPairedAnimation();
	TestEqual(TEXT("Ending one paired owner preserves the other lease"),
		Effects->GetActiveLeaseCount(), 1);
	TestEqual(TEXT("The surviving paired request remains effective"),
		World->GetWorldSettings()->TimeDilation, 0.2f);
	SecondPaired->EndPairedAnimation();
	TestEqual(TEXT("Ending the final paired owner releases all leases"),
		Effects->GetActiveLeaseCount(), 0);
	TestEqual(TEXT("The final release restores the world baseline"),
		World->GetWorldSettings()->TimeDilation, 1.0f);

	FCombatTestHelpers::DestroyTestWorld(World);
	return true;
}
