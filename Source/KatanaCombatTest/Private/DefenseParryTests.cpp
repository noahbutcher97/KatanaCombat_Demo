// Copyright Epic Games, Inc. All Rights Reserved.

#include "CombatTestHelpers.h"
#include "Core/CombatComponent.h"
#include "Core/HitReactionComponent.h"
#include "Core/PairedAnimationComponent.h"
#include "Core/TargetingComponent.h"
#include "Data/AttackConfiguration.h"
#include "Data/AttackData.h"
#include "Data/CombatSettings.h"
#include "Data/DefenseConfiguration.h"
#include "Data/PairedAnimationData.h"
#include "Data/TargetingSettings.h"
#include "Sound/SoundWave.h"
#include "Utilities/CinematicEffectsUtilityLibrary.h"
#include "Utilities/CombatGameplayTags.h"
#include "TimerManager.h"

namespace
{
struct FPerfectParryFixture
{
	UWorld* World = nullptr;
	APlayerCharacter* Defender = nullptr;
	UCombatComponent* DefenderCombat = nullptr;
	AEnemyCharacter* Attacker = nullptr;
	UCombatComponent* AttackerCombat = nullptr;
	UAttackData* Attack = nullptr;
	UDefenseConfiguration* DefenseConfig = nullptr;
	UDefenseConfiguration* AttackerDefenseConfig = nullptr;
	FAttackInstanceId AttackInstance;
	FAttackWindowInstanceId HitWindow;
	FAnimNotifyRuntimeSourceId ParrySource;

	bool Initialize(const bool bParryable = true)
	{
		World = FCombatTestHelpers::CreateTestWorld();
		Defender = FCombatTestHelpers::CreateTestCharacterWithCombat(World, DefenderCombat);
		Attacker = FCombatTestHelpers::CreateTestEnemyCharacter(
			World, FVector(250.0f, 0.0f, 0.0f));
		AttackerCombat = Attacker ? Attacker->CombatComponent.Get() : nullptr;
		if (!World || !Defender || !DefenderCombat || !Attacker || !AttackerCombat)
		{
			return false;
		}

		UCombatSettings* Settings = FCombatTestHelpers::CreateTestCombatSettings();
		Defender->CombatSettings = Settings;
		DefenderCombat->CombatSettings = Settings;

		UTargetingSettings* TargetingSettings = NewObject<UTargetingSettings>();
		TargetingSettings->MaxTargetDistance = 1500.0f;
		TargetingSettings->bRequireLineOfSight = false;
		Defender->TargetingComponent->TargetingSettingsOverride = TargetingSettings;

		DefenseConfig = NewObject<UDefenseConfiguration>();
		DefenseConfig->DefenseThreatRange = 1000.0f;
		DefenseConfig->MaximumHighConfidencePredictionAge = 1.0f;
		DefenseConfig->HardGuardConeHalfAngle = 70.0f;
		DefenseConfig->MaximumAutomaticTurn = 70.0f;
		DefenseConfig->DefenseTurnRate = 360.0f;
		DefenseConfig->PerfectParryFinalTolerance = 10.0f;
		DefenderCombat->DefenseConfigurationOverride = DefenseConfig;
		AttackerDefenseConfig = NewObject<UDefenseConfiguration>();
		AttackerDefenseConfig->ParryStaggerDuration = 2.75f;
		AttackerCombat->DefenseConfigurationOverride = AttackerDefenseConfig;

		Attack = FCombatTestHelpers::CreateTestAttack(EAttackType::Heavy);
		if (bParryable)
		{
			Attack->AttackTags.AddTag(KatanaCombatGameplayTags::AttackDefenseParryable());
		}
		AttackerCombat->SeedAttackWindowStateForTesting(Attack, EAttackPhase::Windup, 41);
		AttackerCombat->SetAttackIntentTarget(Defender);

		const double Now = World->GetTimeSeconds();
		FAttackThreatPrediction Prediction;
		Prediction.IntendedTarget = Defender;
		Prediction.PathOrigin = Attacker->GetActorLocation();
		Prediction.PathDirection =
			(Defender->GetActorLocation() - Attacker->GetActorLocation()).GetSafeNormal();
		Prediction.PredictedContactPoint = Defender->GetActorLocation();
		Prediction.SourceSocket = TEXT("weapon_tip");
		Prediction.DefenderTargetBone = TEXT("spine_03");
		Prediction.PredictionSimulationTimestamp = Now;
		Prediction.PredictedContactSimulationTime = Now + 0.20;
		Prediction.Lane = EIncomingAttackLane::Center;
		Prediction.Height = EAttackHeight::Middle;
		Prediction.Confidence = EDefensePredictionConfidence::High;
		Prediction.bPathIntersectsThreatVolume = true;
		AttackerCombat->PublishAttackThreatPrediction(Prediction);

		ParrySource.SourceAnimation = FSoftObjectPath(TEXT("/Game/Test/Defense/ParryWindow"));
		ParrySource.NotifyEventIndex = 3;
		FAnimNotifyRuntimeSourceId HitSource;
		HitSource.SourceAnimation = FSoftObjectPath(TEXT("/Game/Test/Defense/HitWindow"));
		HitSource.NotifyEventIndex = 2;
		HitWindow = AttackerCombat->OpenAttackWindow(
			EAttackWindowKind::Hit,
			HitSource,
			401,
			0.40f);
		const FAttackWindowInstanceId Window = AttackerCombat->OpenAttackWindow(
			EAttackWindowKind::Parry,
			ParrySource,
			401,
			0.40f);
		AttackInstance = Window.AttackInstance;
		return HitWindow.IsValid() && Window.IsValid();
	}

	void Destroy() const
	{
		FCombatTestHelpers::DestroyTestWorld(World);
	}
};

FDefenseContactRequest MakeCanonicalContactRequest(const FPerfectParryFixture& Fixture)
{
	FDefenseContactRequest Request;
	Request.ContactId = FContactInstanceId::FromAttackWindow(Fixture.HitWindow);
	Request.Query.Stage = EDefenseQueryStage::Contact;
	Request.HitInfo = FCombatTestHelpers::CreateTestHitInfo(
		Fixture.Attacker,
		Fixture.Attack ? Fixture.Attack->BaseDamage : 25.0f,
		(Fixture.Attacker->GetActorLocation() - Fixture.Defender->GetActorLocation()).GetSafeNormal(),
		Fixture.Attack);
	Request.HitInfo.ImpactPoint = Fixture.Defender->GetActorLocation();
	Request.HitInfo.ImpactNormal = FVector::BackwardVector;
	Request.HitInfo.BoneName = TEXT("spine_03");
	Request.HitInfo.WeaponVelocity =
		(Fixture.Defender->GetActorLocation() - Fixture.Attacker->GetActorLocation()).GetSafeNormal()
		* 1000.0f;
	Request.TraceStart = Fixture.Attacker->GetActorLocation();
	Request.TraceEnd = Fixture.Defender->GetActorLocation();
	Request.ActiveSourceSocket = TEXT("weapon_tip");
	return Request;
}

class FVerifyNoMontageDefenseBridge final : public IAutomationLatentCommand
{
public:
	FVerifyNoMontageDefenseBridge(
		const TSharedPtr<FPerfectParryFixture>& InFixture,
		FAutomationTestBase* InTest)
		: Fixture(InFixture)
		, Test(InTest)
	{
	}

	virtual bool Update() override
	{
		if (!Fixture.IsValid() || !Fixture->World || !Test)
		{
			return true;
		}

		++TickCount;
		Fixture->World->GetTimerManager().Tick(0.16f);
		if (Fixture->Defender->PairedAnimationComponent->GetChainState()
			!= EChainCounterState::CounterWindow
			&& TickCount < 3)
		{
			return false;
		}

		Test->TestEqual(TEXT("No-montage bridge opens the counter window after its delay"),
			Fixture->Defender->PairedAnimationComponent->GetChainState(),
			EChainCounterState::CounterWindow);
		const FDefenseResolution& Resolution =
			Fixture->DefenderCombat->GetLastInputDefenseResolutionForTesting();
		Test->TestEqual(TEXT("Bridge fallback does not rewrite the committed outcome"),
			Resolution.Decision.Outcome, EDefenseOutcome::PerfectParry);
		Test->TestTrue(TEXT("Bridge fallback does not reopen the consumed source attack"),
			Fixture->AttackerCombat->BuildAttackExecutionSnapshot().bAttackConsumed);
		Fixture->Destroy();
		Fixture.Reset();
		return true;
	}

private:
	TSharedPtr<FPerfectParryFixture> Fixture;
	FAutomationTestBase* Test = nullptr;
	int32 TickCount = 0;
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDefenseParry_BlockPressConsumesAttack,
	"KatanaCombat.Defense.Parry.BlockPressConsumesAttack",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDefenseParry_BlockPressConsumesAttack::RunTest(const FString& Parameters)
{
	(void)Parameters;
	const TSharedRef<FPerfectParryFixture> OwnedFixture =
		MakeShared<FPerfectParryFixture>();
	FPerfectParryFixture& Fixture = *OwnedFixture;
	if (!Fixture.Initialize())
	{
		AddError(TEXT("Failed to create perfect-parry fixture"));
		Fixture.Destroy();
		return false;
	}

	int32 ImmediateConsumeCount = 0;
	FAttackConsumedEvent ImmediateEvent;
	Fixture.AttackerCombat->OnAttackConsumedInternal.AddLambda(
		[&](const FAttackConsumedEvent& Event)
		{
			++ImmediateConsumeCount;
			ImmediateEvent = Event;
		});

	Fixture.DefenderCombat->OnInputEvent(EInputType::Block, EInputEventType::Press);
	const FDefenseResolution& Resolution =
		Fixture.DefenderCombat->GetLastInputDefenseResolutionForTesting();
	const FAttackExecutionSnapshot SourceAfter =
		Fixture.AttackerCombat->BuildAttackExecutionSnapshot();

	TestEqual(TEXT("Block Press commits perfect parry"),
		Resolution.Decision.Outcome, EDefenseOutcome::PerfectParry);
	TestEqual(TEXT("Committed resolution retains selected attack identity"),
		Resolution.Decision.AttackInstance, Fixture.AttackInstance);
	TestTrue(TEXT("Perfect parry retains held guard"), Fixture.DefenderCombat->IsBlocking());
	TestTrue(TEXT("Source generation is marked consumed"), SourceAfter.bAttackConsumed);
	TestFalse(TEXT("Consumed source is no longer selectable as active"), SourceAfter.bAttackActive);
	TestFalse(TEXT("Consumption closes the parry window"), SourceAfter.ActiveParryWindow.IsValid());
	TestEqual(TEXT("Internal termination is immediate and exact"), ImmediateConsumeCount, 1);
	TestEqual(TEXT("Termination carries the consumed generation"),
		ImmediateEvent.AttackInstance, Fixture.AttackInstance);
	TestEqual(TEXT("Termination reason is perfect parry"),
		ImmediateEvent.Reason, EAttackConsumeReason::PerfectParry);
	TestTrue(TEXT("Perfect parry applies the attacker's configured stagger response"),
		Fixture.Attacker->HitReactionComponent->IsStaggered());
	TestEqual(TEXT("Perfect parry uses the attacker's configured stagger duration"),
		Fixture.Attacker->HitReactionComponent->GetRemainingStaggerTime(),
		Fixture.AttackerDefenseConfig->ParryStaggerDuration,
		KINDA_SMALL_NUMBER);
	TestEqual(TEXT("One public event is queued for deferred dispatch"),
		Fixture.AttackerCombat->GetPendingAttackConsumedEventCountForTesting(), 1);
	TestFalse(TEXT("Duplicate consumption is side-effect free"),
		Fixture.AttackerCombat->ConsumeActiveAttack(
			Fixture.AttackInstance, EAttackConsumeReason::PerfectParry));
	TestEqual(TEXT("Duplicate does not emit a second internal event"), ImmediateConsumeCount, 1);
	TestEqual(TEXT("Committed parry starts the retained defense bridge stage"),
		Fixture.Defender->PairedAnimationComponent->GetChainState(),
		EChainCounterState::ParryActive);
	const FDefenseContactReceipt ConsumedContact =
		Fixture.Attacker->ResolveWeaponContactCandidate(
			Fixture.Defender,
			MakeCanonicalContactRequest(Fixture));
	Fixture.Attacker->FinalizeResolvedWeaponContact(Fixture.Defender, ConsumedContact);
	TestEqual(TEXT("Input-first contact observes whole-generation consumption"),
		ConsumedContact.Resolution.Decision.Outcome,
		EDefenseOutcome::IgnoredConsumed);
	TestFalse(TEXT("Consumed contact is not accepted by the weapon"),
		ConsumedContact.bAcceptsWeaponHit);
	TestEqual(TEXT("Consumed contact applies no damage"),
		ConsumedContact.AppliedDamage,
		0.0f);
	ADD_LATENT_AUTOMATION_COMMAND(FVerifyNoMontageDefenseBridge(OwnedFixture, this));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDefenseParry_ConsumeCallbackInvalidatesSource,
	"KatanaCombat.Defense.Parry.ConsumeCallbackInvalidatesSource",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDefenseParry_ConsumeCallbackInvalidatesSource::RunTest(const FString& Parameters)
{
	(void)Parameters;
	FPerfectParryFixture Fixture;
	if (!Fixture.Initialize())
	{
		AddError(TEXT("Failed to create consume-callback invalidation fixture"));
		Fixture.Destroy();
		return false;
	}

	FAttackConsumedEvent CapturedEvent;
	Fixture.AttackerCombat->OnAttackConsumedInternal.AddLambda(
		[&](const FAttackConsumedEvent& Event)
		{
			CapturedEvent = Event;
			Fixture.World->DestroyActor(Fixture.Attacker);
		});

	Fixture.DefenderCombat->OnInputEvent(EInputType::Block, EInputEventType::Press);
	TestFalse(TEXT("Synchronous consume callback destroys the source participant"),
		IsValid(Fixture.Attacker));
	const FDefenseResolution& Resolution =
		Fixture.DefenderCombat->GetLastInputDefenseResolutionForTesting();
	TestEqual(TEXT("Invalidated source terminates the commit as ignored"),
		Resolution.Decision.Outcome, EDefenseOutcome::IgnoredInvalid);
	TestEqual(TEXT("Invalidated source records the participant failure"),
		Resolution.Decision.Reason, EDefenseReason::InvalidParticipant);
	TestEqual(TEXT("Invalidated source cannot start retained Chain presentation"),
		Fixture.Defender->PairedAnimationComponent->GetChainState(),
		EChainCounterState::None);

	FDefenseInteractionId CachedId;
	FDefenseContactReceipt CachedReceipt;
	const EDefenseCommitStatus CachedStatus = Fixture.DefenderCombat->BeginDefenseInteraction(
		CapturedEvent.InteractionId.Key,
		CachedId,
		CachedReceipt,
		false);
	TestEqual(TEXT("Exact invalidated interaction is finalized for duplicate callers"),
		CachedStatus, EDefenseCommitStatus::Cached);
	TestEqual(TEXT("Cached invalidation preserves the terminal outcome"),
		CachedReceipt.Resolution.Decision.Outcome, EDefenseOutcome::IgnoredInvalid);
	TestEqual(TEXT("Cached invalidation preserves the terminal reason"),
		CachedReceipt.Resolution.Decision.Reason, EDefenseReason::InvalidParticipant);

	Fixture.Destroy();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDefenseParry_DefenderPresentationInvalidatesParticipants,
	"KatanaCombat.Defense.Parry.DefenderPresentationInvalidatesParticipants",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDefenseParry_DefenderPresentationInvalidatesParticipants::RunTest(
	const FString& Parameters)
{
	(void)Parameters;
	FPerfectParryFixture Fixture;
	if (!Fixture.Initialize())
	{
		AddError(TEXT("Failed to create presentation invalidation fixture"));
		Fixture.Destroy();
		return false;
	}

	Fixture.DefenseConfig->DefaultParryImpactAudio.ImpactSound =
		NewObject<USoundWave>(Fixture.DefenseConfig);
	const FDelegateHandle DestroyHandle =
		UCinematicEffectsUtilityLibrary::OnImpactSoundPlaybackInvokedForTesting.AddLambda(
			[&Fixture](UWorld*, USoundBase*, const FVector&, AActor*)
			{
				if (Fixture.World && IsValid(Fixture.Defender))
				{
					Fixture.World->DestroyActor(Fixture.Defender);
				}
			});

	Fixture.DefenderCombat->OnInputEvent(EInputType::Block, EInputEventType::Press);
	UCinematicEffectsUtilityLibrary::OnImpactSoundPlaybackInvokedForTesting.Remove(
		DestroyHandle);
	TestFalse(TEXT("Defender presentation callback destroys the committed defender"),
		IsValid(Fixture.Defender));
	TestEqual(TEXT("Invalid defender prevents stale attacker presentation"),
		Fixture.Attacker->HitReactionComponent->GetAttackerResponseAttemptCountForTesting(),
		0);
	TestFalse(TEXT("Invalid defender prevents stale stagger application"),
		Fixture.Attacker->HitReactionComponent->IsStaggered());

	Fixture.Destroy();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDefenseParry_MissingCapabilityDowngradesToSameGuardThreat,
	"KatanaCombat.Defense.Parry.MissingCapabilityDowngradesToSameGuardThreat",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDefenseParry_MissingCapabilityDowngradesToSameGuardThreat::RunTest(const FString& Parameters)
{
	(void)Parameters;
	FPerfectParryFixture Fixture;
	if (!Fixture.Initialize(false))
	{
		AddError(TEXT("Failed to create parry-downgrade fixture"));
		Fixture.Destroy();
		return false;
	}

	Fixture.DefenderCombat->OnInputEvent(EInputType::Block, EInputEventType::Press);
	const FDefenseResolution& Resolution =
		Fixture.DefenderCombat->GetLastInputDefenseResolutionForTesting();
	TestEqual(TEXT("Missing parry capability enters guard"),
		Resolution.Decision.Outcome, EDefenseOutcome::GuardEntered);
	TestEqual(TEXT("Downgrade keeps the originally selected attack"),
		Resolution.Decision.AttackInstance, Fixture.AttackInstance);
	TestTrue(TEXT("Downgrade retains held guard"), Fixture.DefenderCombat->IsBlocking());
	TestFalse(TEXT("Downgrade does not consume the attack"),
		Fixture.AttackerCombat->BuildAttackExecutionSnapshot().bAttackConsumed);
	TestEqual(TEXT("Downgrade emits no termination event"),
		Fixture.AttackerCombat->GetPendingAttackConsumedEventCountForTesting(), 0);

	Fixture.Destroy();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDefenseParry_ContactFirstPreservesCommittedHit,
	"KatanaCombat.Defense.Parry.ContactFirstPreservesCommittedHit",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDefenseParry_ContactFirstPreservesCommittedHit::RunTest(const FString& Parameters)
{
	(void)Parameters;
	FPerfectParryFixture Fixture;
	if (!Fixture.Initialize())
	{
		AddError(TEXT("Failed to create contact-first parry fixture"));
		Fixture.Destroy();
		return false;
	}

	const float HealthBefore = Fixture.Defender->CurrentHealth;
	const FDefenseContactReceipt Contact =
		Fixture.Attacker->ResolveWeaponContactCandidate(
			Fixture.Defender,
			MakeCanonicalContactRequest(Fixture));
	Fixture.Attacker->FinalizeResolvedWeaponContact(Fixture.Defender, Contact);
	TestEqual(TEXT("Contact-first ordering commits the unguarded hit"),
		Contact.Resolution.Decision.Outcome, EDefenseOutcome::Hit);
	TestTrue(TEXT("Committed contact consumes weapon hit budget"),
		Contact.bAcceptsWeaponHit && Contact.bConsumesHitBudget);
	TestTrue(TEXT("Committed contact applies damage"), Contact.AppliedDamage > 0.0f);
	TestTrue(TEXT("Committed contact changes defender health once"),
		Fixture.Defender->CurrentHealth < HealthBefore);

	Fixture.DefenderCombat->OnInputEvent(EInputType::Block, EInputEventType::Press);
	const FDefenseResolution& Resolution =
		Fixture.DefenderCombat->GetLastInputDefenseResolutionForTesting();
	TestEqual(TEXT("Later Block Press downgrades after same-generation contact"),
		Resolution.Decision.Outcome, EDefenseOutcome::GuardEntered);
	TestEqual(TEXT("Contact-first downgrade is reported as duplicate ordering"),
		Resolution.Decision.Reason, EDefenseReason::Duplicate);
	TestEqual(TEXT("Contact-first downgrade retains the same selected attack"),
		Resolution.Decision.AttackInstance, Fixture.AttackInstance);
	TestFalse(TEXT("Contact-first downgrade does not consume the source attack"),
		Fixture.AttackerCombat->BuildAttackExecutionSnapshot().bAttackConsumed);
	TestEqual(TEXT("Contact-first downgrade does not start a Chain sequence"),
		Fixture.Defender->PairedAnimationComponent->GetChainState(),
		EChainCounterState::None);

	Fixture.Destroy();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDefenseParry_UnusableBridgePreservesCommit,
	"KatanaCombat.Defense.Parry.UnusableBridgePreservesCommit",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDefenseParry_UnusableBridgePreservesCommit::RunTest(const FString& Parameters)
{
	(void)Parameters;
	FPerfectParryFixture Fixture;
	if (!Fixture.Initialize())
	{
		AddError(TEXT("Failed to create unusable-bridge fixture"));
		Fixture.Destroy();
		return false;
	}

	FDefensePresentationRow BrokenBridgeRow;
	BrokenBridgeRow.RowName = TEXT("BrokenExactBridge");
	BrokenBridgeRow.Outcome = EDefenseOutcome::PerfectParry;
	BrokenBridgeRow.Payload.PairedBridgeData = NewObject<UPairedAnimationData>();
	BrokenBridgeRow.Payload.ReviewedDeflectionMarker = TEXT("Deflect");
	BrokenBridgeRow.Payload.bRequiresBridgePreflight = true;
	Fixture.DefenseConfig->DefenderPresentationRows.Add(BrokenBridgeRow);

	Fixture.DefenderCombat->OnInputEvent(EInputType::Block, EInputEventType::Press);
	const FDefenseResolution& Resolution =
		Fixture.DefenderCombat->GetLastInputDefenseResolutionForTesting();
	const FDefenseSequenceContext& Sequence =
		Fixture.Defender->PairedAnimationComponent->GetActiveDefenseSequenceContext();
	TestEqual(TEXT("Malformed bridge cannot rewrite perfect parry"),
		Resolution.Decision.Outcome, EDefenseOutcome::PerfectParry);
	TestTrue(TEXT("Malformed bridge still consumes the exact attack"),
		Fixture.AttackerCombat->BuildAttackExecutionSnapshot().bAttackConsumed);
	TestEqual(TEXT("Malformed bridge uses the no-montage retained stage"),
		Fixture.Defender->PairedAnimationComponent->GetChainState(),
		EChainCounterState::ParryActive);
	TestNull(TEXT("Unusable paired data is removed from active sequence presentation"),
		Sequence.ActivePresentation.PairedBridgeData.Get());
	TestEqual(TEXT("Committed resolution retains selected-row telemetry"),
		Resolution.PresentationRow, BrokenBridgeRow.RowName);

	Fixture.Destroy();
	return true;
}
