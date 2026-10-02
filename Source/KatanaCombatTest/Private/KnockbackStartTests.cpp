#include "CombatTestHelpers.h"
#include "Core/HitReactionComponent.h"
#include "Core/PairedAnimationComponent.h"
#include "Core/TargetingComponent.h"
#include "Data/AttackData.h"
#include "Data/CombatSettings.h"
#include "Data/PairedAnimationData.h"
#include "Core/CombatComponent.h"
#include "Data/HitReactionSettings.h"
#include "Debug/ActionReactionTelemetry.h"
#include "HAL/IConsoleManager.h"

namespace
{
struct FKnockbackFixture
{
	UWorld* World = FCombatTestHelpers::CreateTestWorld();
	APlayerCharacter* Attacker = FCombatTestHelpers::CreateTestPlayerCharacter(World, FVector(0, 0, 100));
	AEnemyCharacter* Victim = FCombatTestHelpers::CreateTestEnemyCharacter(World, FVector(100, 0, 100));
	UAttackData* Light = FCombatTestHelpers::CreateTestAttack(EAttackType::Light);
	~FKnockbackFixture() { FCombatTestHelpers::DestroyTestWorld(World); }

	FHitReactionInfo Hit() const
	{
		FHitReactionInfo Info = FCombatTestHelpers::CreateTestHitInfo(Attacker, 10.f, FVector(-1, 0, 0), Light);
		return Info;
	}
	UTargetingComponent* VictimTargeting() const { return Victim->GetTargetingComponent(); }
	FAlignmentRequestSpec ActiveSpec() const
	{
		FAlignmentRequestSpec Spec;
		VictimTargeting()->GetAlignmentRequestSpec(VictimTargeting()->GetActiveAlignmentRequest(), Spec);
		return Spec;
	}
};

/** Turns Combat.ActionReaction.Debug on for its scope. Declare it before the fixture, so it is restored on every exit after teardown. */
struct FKnockbackTelemetryOn
{
	IConsoleVariable* Variable = IConsoleManager::Get().FindConsoleVariable(TEXT("Combat.ActionReaction.Debug"));
	int32 Previous = Variable ? Variable->GetInt() : 0;
	FKnockbackTelemetryOn() { if (Variable) { Variable->Set(1, ECVF_SetByCode); } }
	~FKnockbackTelemetryOn() { if (Variable) { Variable->Set(Previous, ECVF_SetByCode); } }
};

/** The character's HitKnockback telemetry rows with one disposition, in order. */
TArray<FActionReactionTelemetryRecord> KnockbackRows(const ABaseCombatCharacter* Character, const TCHAR* Disposition)
{
	return Character->GetCombatComponent()->GetActionReactionTelemetry().FilterByPredicate(
		[Disposition](const FActionReactionTelemetryRecord& Row)
		{
			return Row.AlignmentOwner == FName(TEXT("HitKnockback")) && Row.AlignmentDisposition == FName(Disposition);
		});
}

/** True while the targeting component still holds the request, running or suspended. */
bool HoldsRequest(const UTargetingComponent* Targeting, const FAlignmentRequestHandle Handle)
{
	FAlignmentRequestSpec Spec;
	return Targeting->GetAlignmentRequestSpec(Handle, Spec);
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FKnockbackStartRequestTest, "KatanaCombat.Knockback.Start.AcquiresResolvedRequest",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FKnockbackStartRequestTest::RunTest(const FString&)
{
	FKnockbackFixture F;
	TestTrue(TEXT("Push started"), F.Victim->HitReactionComponent->StartKnockback(F.Hit()));
	const FAlignmentRequestSpec Spec = F.ActiveSpec();
	TestEqual(TEXT("Executor"), Spec.Executor, EAlignmentExecutor::ProceduralDisplacement);
	TestEqual(TEXT("Priority"), Spec.Priority, EDefenseAlignmentPriority::HitKnockback);
	TestTrue(TEXT("Self-releasing"), Spec.bReleaseWhenFinished);
	TestEqual(TEXT("Light default distance"), Spec.Displacement.Distance, 25.0f);
	TestEqual(TEXT("Light default duration"), Spec.Displacement.Duration, 0.2f);
	TestEqual(TEXT("Default profile"), Spec.Displacement.SpeedProfile, EDisplacementSpeedProfile::EaseOut);
	TestEqual(TEXT("Actor clock"), Spec.Displacement.Clock, EDisplacementClock::ActorTime);
	TestEqual(TEXT("Resolved blend"), Spec.Displacement.AnimationBlend, EDisplacementAnimationBlend::AddToAnimation);
	TestTrue(TEXT("Pushed away from the attacker"), Spec.Displacement.Direction.Equals(FVector(1, 0, 0), 1e-4));

	// The direction comes from the actors' positions, not from the hit's direction field.
	FHitReactionInfo Sideways = F.Hit();
	Sideways.DirectionToAttacker = FVector(0, 1, 0);
	TestTrue(TEXT("Sideways hit field: push started"), F.Victim->HitReactionComponent->StartKnockback(Sideways));
	TestTrue(FString::Printf(TEXT("Direction comes from positions, not DirectionToAttacker (%s)"), *F.ActiveSpec().Displacement.Direction.ToString()),
		F.ActiveSpec().Displacement.Direction.Equals(FVector(1, 0, 0), 1e-4));

	// The attacker's combat settings own the push: the victim's own defaults never change it.
	F.Victim->CombatSettings->DefaultKnockback.FindChecked(EAttackType::Light).Distance = 40.0f;
	TestTrue(TEXT("Different victim defaults: push started"), F.Victim->HitReactionComponent->StartKnockback(F.Hit()));
	TestEqual(TEXT("The attacker's settings win over the victim's"), F.ActiveSpec().Displacement.Distance, 25.0f);

	// A per-attack blend override reaches the request.
	F.Light->Knockback.bOverrideAnimationBlend = true;
	F.Light->Knockback.AnimationBlend = EDisplacementAnimationBlend::ReplaceAnimation;
	TestTrue(TEXT("Blend override: push started"), F.Victim->HitReactionComponent->StartKnockback(F.Hit()));
	TestEqual(TEXT("The per-attack blend override reaches the request"), F.ActiveSpec().Displacement.AnimationBlend,
		EDisplacementAnimationBlend::ReplaceAnimation);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FKnockbackStartChargeTest, "KatanaCombat.Knockback.Start.ChargeAndVictimScale",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FKnockbackStartChargeTest::RunTest(const FString&)
{
	FKnockbackFixture F;
	F.Light->MaxChargeKnockbackMultiplier = 2.0f;
	FHitReactionInfo Charged = F.Hit();
	Charged.ChargeLevel = 1.0f;
	F.Victim->HitReactionComponent->StartKnockback(Charged);
	TestEqual(TEXT("Fully charged doubles the push"), F.ActiveSpec().Displacement.Distance, 50.0f);

	UHitReactionSettings* Immune = NewObject<UHitReactionSettings>();
	Immune->KnockbackScale = 0.0f;
	F.Victim->HitReactionComponent->HitReactionSettingsOverride = Immune;
	TestFalse(TEXT("Immune victim gets no push"), F.Victim->HitReactionComponent->StartKnockback(F.Hit()));
	TestEqual(TEXT("No request remains"), F.VictimTargeting()->GetAlignmentRequestCountForTesting(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FKnockbackReplaceTest, "KatanaCombat.Knockback.Start.ReplacesRunningPush",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FKnockbackReplaceTest::RunTest(const FString&)
{
	FKnockbackFixture F;
	F.Victim->HitReactionComponent->StartKnockback(F.Hit());
	const FAlignmentRequestHandle First = F.VictimTargeting()->GetActiveAlignmentRequest();
	F.Victim->HitReactionComponent->StartKnockback(F.Hit());
	TestEqual(TEXT("A second hit replaces, never stacks"), F.VictimTargeting()->GetAlignmentRequestCountForTesting(), 1);
	TestTrue(TEXT("The running request is the new one"), F.VictimTargeting()->GetActiveAlignmentRequest() != First);
	F.Victim->HitReactionComponent->ReleaseKnockback(TEXT("Test"));
	TestEqual(TEXT("ReleaseKnockback clears it"), F.VictimTargeting()->GetAlignmentRequestCountForTesting(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FKnockbackDeathTest, "KatanaCombat.Knockback.Start.DeathReleasesPush",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FKnockbackDeathTest::RunTest(const FString&)
{
	FKnockbackFixture F;
	if (!TestTrue(TEXT("Push started"), F.Victim->HitReactionComponent->StartKnockback(F.Hit()))) { return false; }
	// Death releases every alignment request (ABaseCombatCharacter and UCombatComponent death paths), and that
	// broad release removes the push channel (KatanaCombat.Displacement.Executor.ReleaseAllRemovesChannel).
	FCombatTestHelpers::DealLethalDamage(F.Victim, F.Attacker);
	FCombatTestHelpers::FinalizeDeathIfDying(F.Victim);
	TestTrue(TEXT("Victim died"), FCombatTestHelpers::IsCharacterDead(F.Victim));
	TestEqual(TEXT("Death released the push"), F.VictimTargeting()->GetAlignmentRequestCountForTesting(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FKnockbackTelemetryTest, "KatanaCombat.Knockback.Start.WritesStartedAndRejectedRows",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FKnockbackTelemetryTest::RunTest(const FString&)
{
	const FKnockbackTelemetryOn Telemetry;
	if (!TestNotNull(TEXT("Telemetry CVar exists"), Telemetry.Variable)) { return false; }
	FKnockbackFixture F;
	UCombatComponent* Combat = F.Victim->GetCombatComponent();
	if (!TestNotNull(TEXT("Victim combat component"), Combat)) { return false; }

	Combat->ClearActionReactionTelemetry();
	F.Victim->HitReactionComponent->StartKnockback(F.Hit());
	const TArray<FActionReactionTelemetryRecord>& Started = Combat->GetActionReactionTelemetry();
	if (TestEqual(TEXT("A started push writes one row"), Started.Num(), 1))
	{
		TestEqual(TEXT("Event"), Started[0].Event, EActionReactionTelemetryEvent::AlignmentChanged);
		TestEqual(TEXT("Owner"), Started[0].AlignmentOwner, FName(TEXT("HitKnockback")));
		TestEqual(TEXT("Disposition"), Started[0].AlignmentDisposition, FName(TEXT("Started")));
		TestEqual(TEXT("Resolved distance"), Started[0].MovementMagnitude, 25.0f);
	}

	// An immune victim resolves a zero distance: the push is never acquired but the attempt is still recorded.
	UHitReactionSettings* Immune = NewObject<UHitReactionSettings>();
	Immune->KnockbackScale = 0.0f;
	F.Victim->HitReactionComponent->HitReactionSettingsOverride = Immune;
	Combat->ClearActionReactionTelemetry();
	TestFalse(TEXT("Immune victim gets no push"), F.Victim->HitReactionComponent->StartKnockback(F.Hit()));
	// The first push is still running, so the new attempt replaces it: it ends Cancelled, naming the replacement.
	const TArray<FActionReactionTelemetryRecord> Cancelled = KnockbackRows(F.Victim, TEXT("Cancelled"));
	if (TestEqual(TEXT("The replaced running push writes one Cancelled row"), Cancelled.Num(), 1))
	{
		TestEqual(TEXT("The Cancelled row names the replacement"), Cancelled[0].Detail, FString(TEXT("Replaced")));
	}
	const TArray<FActionReactionTelemetryRecord> Rejected = KnockbackRows(F.Victim, TEXT("Rejected"));
	if (TestEqual(TEXT("A rejected push writes one Rejected row"), Rejected.Num(), 1))
	{
		TestEqual(TEXT("Zero resolved distance"), Rejected[0].MovementMagnitude, 0.0f);
		TestEqual(TEXT("The Rejected row names its cause"), Rejected[0].Detail, FString(TEXT("no push distance: zero victim scale")));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FKnockbackRejectionCauseTest, "KatanaCombat.Knockback.Start.RejectedRowsNameTheirCause",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FKnockbackRejectionCauseTest::RunTest(const FString&)
{
	const FKnockbackTelemetryOn Telemetry;
	if (!TestNotNull(TEXT("Telemetry CVar exists"), Telemetry.Variable)) { return false; }
	FKnockbackFixture F;
	UCombatComponent* Combat = F.Victim->GetCombatComponent();
	if (!TestNotNull(TEXT("Victim combat component"), Combat)) { return false; }

	struct FCase
	{
		const TCHAR* Name;
		FHitReactionInfo Hit;
		const TCHAR* Detail;
		float Magnitude;
	};
	// An attack type with no default and no distance override resolves no push distance.
	UAttackData* Special = FCombatTestHelpers::CreateTestAttack(EAttackType::Special);
	// A zero duration is below the editor clamp, so only code can author it; the arbiter refuses the request.
	UAttackData* ZeroDuration = FCombatTestHelpers::CreateTestAttack(EAttackType::Light);
	ZeroDuration->Knockback.bOverrideDuration = true;
	ZeroDuration->Knockback.Duration = 0.0f;
	const FCase Cases[] = {
		{TEXT("Missing type default"), FCombatTestHelpers::CreateTestHitInfo(F.Attacker, 10.f, FVector(-1, 0, 0), Special),
			TEXT("no push distance: missing type default (Special)"), 0.0f},
		{TEXT("Acquire rejected"), FCombatTestHelpers::CreateTestHitInfo(F.Attacker, 10.f, FVector(-1, 0, 0), ZeroDuration),
			TEXT("acquire rejected"), 25.0f},
	};
	for (const FCase& Case : Cases)
	{
		Combat->ClearActionReactionTelemetry();
		TestFalse(FString::Printf(TEXT("%s: no push"), Case.Name), F.Victim->HitReactionComponent->StartKnockback(Case.Hit));
		const TArray<FActionReactionTelemetryRecord> Rejected = KnockbackRows(F.Victim, TEXT("Rejected"));
		if (TestEqual(FString::Printf(TEXT("%s: one Rejected row"), Case.Name), Rejected.Num(), 1))
		{
			TestEqual(FString::Printf(TEXT("%s: the row names its cause"), Case.Name), Rejected[0].Detail, FString(Case.Detail));
			TestEqual(FString::Printf(TEXT("%s: resolved distance"), Case.Name), Rejected[0].MovementMagnitude, Case.Magnitude);
		}
	}

	// An attacker stacked straight above the victim gives no horizontal direction.
	Combat->ClearActionReactionTelemetry();
	F.Attacker->SetActorLocation(F.Victim->GetActorLocation() + FVector(0, 0, 150));
	TestFalse(TEXT("Stacked attacker: no push"), F.Victim->HitReactionComponent->StartKnockback(F.Hit()));
	const TArray<FActionReactionTelemetryRecord> Rejected = KnockbackRows(F.Victim, TEXT("Rejected"));
	if (TestEqual(TEXT("Stacked attacker: one Rejected row"), Rejected.Num(), 1))
	{
		TestEqual(TEXT("Stacked attacker: the row names its cause"), Rejected[0].Detail, FString(TEXT("degenerate direction")));
		TestEqual(TEXT("Stacked attacker: the resolved distance is still recorded"), Rejected[0].MovementMagnitude, 25.0f);
	}
	TestEqual(TEXT("No request remains"), F.VictimTargeting()->GetAlignmentRequestCountForTesting(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FKnockbackInitiatorPairedReleaseTest, "KatanaCombat.Knockback.Start.InitiatorPairedStartReleasesPush",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FKnockbackInitiatorPairedReleaseTest::RunTest(const FString&)
{
	const FKnockbackTelemetryOn Telemetry;
	if (!TestNotNull(TEXT("Telemetry CVar exists"), Telemetry.Variable)) { return false; }
	FKnockbackFixture F;
	// The enemy hits the player, so the player is pushed; then the player starts a paired animation of its own.
	APlayerCharacter* Player = F.Attacker;
	UHitReactionComponent* Reaction = Player->HitReactionComponent;
	UTargetingComponent* Targeting = Player->GetTargetingComponent();
	UPairedAnimationComponent* Paired = Player->GetPairedAnimationComponent();
	UCombatComponent* Combat = Player->GetCombatComponent();
	if (!TestNotNull(TEXT("Hit reaction"), Reaction) || !TestNotNull(TEXT("Targeting"), Targeting)
		|| !TestNotNull(TEXT("Paired animation"), Paired) || !TestNotNull(TEXT("Combat"), Combat))
	{
		return false;
	}
	Combat->ClearActionReactionTelemetry();
	if (!TestTrue(TEXT("The player is pushed"), Reaction->StartKnockback(
		FCombatTestHelpers::CreateTestHitInfo(F.Victim, 10.f, FVector(1, 0, 0), F.Light))))
	{
		return false;
	}
	const FAlignmentRequestHandle Push = Reaction->KnockbackAlignmentHandle;
	TestTrue(TEXT("The player's targeting holds the push"), HoldsRequest(Targeting, Push));

	Paired->BeginPairedAnimation(NewObject<UPairedAnimationData>(), EPairedReactionType::Finisher, false);
	TestTrue(TEXT("The paired animation took over the player"), Paired->IsPairedAnimationActive());
	TestFalse(TEXT("Starting a paired animation releases the initiator's own push"), HoldsRequest(Targeting, Push));
	TestEqual(TEXT("No alignment request remains"), Targeting->GetAlignmentRequestCountForTesting(), 0);
	const TArray<FActionReactionTelemetryRecord> Cancelled = KnockbackRows(Player, TEXT("Cancelled"));
	if (TestEqual(TEXT("The released push writes one Cancelled row"), Cancelled.Num(), 1))
	{
		TestEqual(TEXT("Released by the paired takeover"), Cancelled[0].Detail, FString(TEXT("PairedTakeover")));
	}
	Paired->CancelPairedAnimation(0.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FKnockbackPairedEntryReleaseTest, "KatanaCombat.Knockback.Start.PairedEntryReleasesPush",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FKnockbackPairedEntryReleaseTest::RunTest(const FString&)
{
	const FKnockbackTelemetryOn Telemetry;
	if (!TestNotNull(TEXT("Telemetry CVar exists"), Telemetry.Variable)) { return false; }
	FKnockbackFixture F;
	UHitReactionComponent* Reaction = F.Victim->HitReactionComponent;
	F.Victim->GetCombatComponent()->ClearActionReactionTelemetry();
	if (!TestTrue(TEXT("Push started"), Reaction->StartKnockback(F.Hit()))) { return false; }
	const FAlignmentRequestHandle Push = Reaction->KnockbackAlignmentHandle;
	TestTrue(TEXT("The victim's targeting holds the push"), HoldsRequest(F.VictimTargeting(), Push));

	// A real paired flow always names the partner.
	Reaction->EnterPairedAnimationState(nullptr, EReactionOutcome::Ragdoll, 0.2f, false, F.Attacker);
	TestTrue(TEXT("The victim entered the paired state"), Reaction->IsInPairedAnimationState());
	TestFalse(TEXT("Paired entry releases the victim's push"), HoldsRequest(F.VictimTargeting(), Push));
	TestEqual(TEXT("No alignment request remains"), F.VictimTargeting()->GetAlignmentRequestCountForTesting(), 0);
	// The entry releases it itself, before the takeover it also runs could.
	const TArray<FActionReactionTelemetryRecord> Cancelled = KnockbackRows(F.Victim, TEXT("Cancelled"));
	if (TestEqual(TEXT("The released push writes one Cancelled row"), Cancelled.Num(), 1))
	{
		TestEqual(TEXT("Released by paired entry"), Cancelled[0].Detail, FString(TEXT("PairedEntry")));
	}
	Reaction->ExitPairedAnimationState();
	return true;
}
