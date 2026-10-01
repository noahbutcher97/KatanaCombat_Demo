#include "CombatTestHelpers.h"
#include "Core/HitReactionComponent.h"
#include "Core/TargetingComponent.h"
#include "Data/AttackData.h"
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
	F.Victim->HitReactionComponent->ReleaseKnockback();
	TestEqual(TEXT("ReleaseKnockback clears it"), F.VictimTargeting()->GetAlignmentRequestCountForTesting(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FKnockbackDeathTest, "KatanaCombat.Knockback.Start.DeathReleasesPush",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FKnockbackDeathTest::RunTest(const FString&)
{
	FKnockbackFixture F;
	if (!TestTrue(TEXT("Push started"), F.Victim->HitReactionComponent->StartKnockback(F.Hit()))) { return false; }
	// Death releases every alignment request (ABaseCombatCharacter and UCombatComponent death paths),
	// and releasing removes the push channel (KatanaCombat.Displacement.Executor.ReleaseRemovesChannel).
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
	IConsoleVariable* Variable = IConsoleManager::Get().FindConsoleVariable(TEXT("Combat.ActionReaction.Debug"));
	if (!TestNotNull(TEXT("Telemetry CVar exists"), Variable)) { return false; }
	const int32 Previous = Variable->GetInt();
	Variable->Set(1, ECVF_SetByCode);

	{
		FKnockbackFixture F;
		UCombatComponent* Combat = F.Victim->GetCombatComponent();
		if (TestNotNull(TEXT("Victim combat component"), Combat))
		{
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
			const TArray<FActionReactionTelemetryRecord>& Rejected = Combat->GetActionReactionTelemetry();
			if (TestEqual(TEXT("A rejected push writes one row"), Rejected.Num(), 1))
			{
				TestEqual(TEXT("Owner"), Rejected[0].AlignmentOwner, FName(TEXT("HitKnockback")));
				TestEqual(TEXT("Disposition"), Rejected[0].AlignmentDisposition, FName(TEXT("Rejected")));
				TestEqual(TEXT("Zero resolved distance"), Rejected[0].MovementMagnitude, 0.0f);
			}
		}
	}

	Variable->Set(Previous, ECVF_SetByCode);
	return true;
}
