#include "Misc/AutomationTest.h"

#include "Data/AttackData.h"
#include "Data/DefenseConfiguration.h"
#include "Defense/DefenseResolver.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Character.h"
#include "Utilities/CombatGameplayTags.h"

#include <limits>

namespace DefenseResolverTests
{
FDefenseQuery MakeInputQuery()
{
	FDefenseQuery Query;
	Query.Stage = EDefenseQueryStage::InputIntent;
	Query.bDefenderAlive = true;
	Query.bDefenderCanGuard = true;
	Query.bHasSelectedThreat = true;
	Query.RelativeYawDegrees = 5.0f;
	Query.TimeToAlignmentDeadline = 0.20f;
	Query.HardGuardConeHalfAngle = 70.0f;
	Query.MaximumAutomaticTurn = 70.0f;
	Query.RemainingAutomaticTurn = 70.0f;
	Query.DefenseTurnRate = 180.0f;
	Query.NormalBlockFinalTolerance = 35.0f;
	Query.PerfectParryFinalTolerance = 10.0f;
	Query.Defender = GetMutableDefault<ACharacter>();

	Query.Attack.AttackData = GetMutableDefault<UAttackData>();
	Query.Attack.AttackInstance.Attacker = GetMutableDefault<AActor>();
	Query.Attack.AttackInstance.AttackGeneration = 1;
	Query.Attack.bAttackerAlive = true;
	Query.Attack.bAttackActive = true;
	Query.Attack.bAttackIdentityCurrent = true;
	Query.Attack.bIsHostileToDefender = true;
	Query.Attack.bHasCredibleIntent = true;
	Query.Attack.IntendedTarget = Query.Defender;
	Query.Attack.ActiveParryWindow.AttackInstance = Query.Attack.AttackInstance;
	Query.Attack.ActiveParryWindow.Kind = EAttackWindowKind::Parry;
	Query.Attack.ActiveParryWindow.WindowGeneration = 1;
	Query.Attack.ActiveParryWindow.NotifySource.SourceAnimation =
		FSoftObjectPath(TEXT("/Game/Test/AM_Defense.AM_Defense"));
	Query.Attack.ActiveParryWindow.NotifySource.NotifyEventIndex = 0;
	Query.Attack.ActiveParryWindow.MontageInstanceId = 0;
	Query.Attack.ActiveParryWindow.SimulationStartTime = 0.0;
	Query.Attack.ActiveParryWindow.SimulationEndTime = 0.30;
	Query.Attack.AttackTags.AddTag(KatanaCombatGameplayTags::AttackDefenseParryable());
	Query.Attack.PredictedContact.bIsValid = true;
	Query.Attack.PredictedContact.IntendedTarget = Query.Defender;
	Query.Attack.PredictedContact.Confidence = EDefensePredictionConfidence::High;
	Query.Attack.PredictedContact.bPathIntersectsThreatVolume = true;
	return Query;
}

FDefenseQuery MakeContactQuery()
{
	FDefenseQuery Query;
	Query.Stage = EDefenseQueryStage::Contact;
	Query.bDefenderAlive = true;
	Query.bDefenderCanGuard = true;
	Query.bDefenderGuarding = true;
	Query.bDefenderCanBeDamaged = true;
	Query.bContactIdentityValid = true;
	Query.bHasActualContact = true;
	Query.RelativeYawDegrees = 5.0f;
	Query.NormalBlockFinalTolerance = 35.0f;
	Query.Attack.AttackData = GetMutableDefault<UAttackData>();
	Query.Attack.AttackInstance.Attacker = GetMutableDefault<AActor>();
	Query.Attack.AttackInstance.AttackGeneration = 1;
	Query.Attack.bAttackerAlive = true;
	Query.Attack.bAttackIdentityCurrent = true;
	Query.Attack.bAttackActive = true;
	Query.Attack.bIsHostileToDefender = true;
	Query.ActualContact.bIsValid = true;
	return Query;
}

struct FResolverCase
{
	const TCHAR* Name;
	FDefenseQuery Query;
	EDefenseOutcome ExpectedOutcome;
	EDefenseDamageDisposition ExpectedDamage;
	EAttackerResponse ExpectedResponse;
};

FAttackExecutionSnapshot MakeThreat(
	uint64 StableId,
	float Deadline,
	float Yaw,
	float Distance,
	EDefensePredictionConfidence Confidence = EDefensePredictionConfidence::High)
{
	FAttackExecutionSnapshot Threat;
	Threat.StableId.Value = StableId;
	Threat.AttackInstance.Attacker = GetMutableDefault<AActor>();
	Threat.AttackInstance.AttackGeneration = static_cast<int32>(StableId);
	Threat.bAttackerAlive = true;
	Threat.bAttackActive = true;
	Threat.bAttackIdentityCurrent = true;
	Threat.bIsHostileToDefender = true;
	Threat.bHasCredibleIntent = true;
	Threat.TimeToAlignmentDeadline = Deadline;
	Threat.RelativeYawDegrees = Yaw;
	Threat.DistanceToDefender = Distance;
	Threat.PredictedContact.bIsValid = true;
	Threat.PredictedContact.Confidence = Confidence;
	Threat.PredictedContact.bPathIntersectsThreatVolume = true;
	return Threat;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDefenseResolverMatrixTest,
	"KatanaCombat.Defense.Resolver.Matrix",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FDefenseResolverMatrixTest::RunTest(const FString& Parameters)
{
	using namespace DefenseResolverTests;
	(void)Parameters;

	TArray<FResolverCase> Cases;

	FDefenseQuery InvalidDefender = MakeInputQuery();
	InvalidDefender.bDefenderCanGuard = false;
	Cases.Add({TEXT("Input invalid defender"), InvalidDefender, EDefenseOutcome::Rejected,
		EDefenseDamageDisposition::NoContactSideEffects, EAttackerResponse::None});

	FDefenseQuery NoCandidate = MakeInputQuery();
	NoCandidate.bHasSelectedThreat = false;
	Cases.Add({TEXT("Input no candidate"), NoCandidate, EDefenseOutcome::GuardEntered,
		EDefenseDamageDisposition::NoContactSideEffects, EAttackerResponse::None});

	FDefenseQuery NoWindow = MakeInputQuery();
	NoWindow.Attack.ActiveParryWindow = {};
	Cases.Add({TEXT("Input no parry window"), NoWindow, EDefenseOutcome::GuardEntered,
		EDefenseDamageDisposition::NoContactSideEffects, EAttackerResponse::None});

	FDefenseQuery WrongWindowKind = MakeInputQuery();
	WrongWindowKind.Attack.ActiveParryWindow.Kind = EAttackWindowKind::Hit;
	Cases.Add({TEXT("Input wrong window kind"), WrongWindowKind, EDefenseOutcome::GuardEntered,
		EDefenseDamageDisposition::NoContactSideEffects, EAttackerResponse::None});

	FDefenseQuery MismatchedWindowAttack = MakeInputQuery();
	MismatchedWindowAttack.Attack.ActiveParryWindow.AttackInstance.AttackGeneration = 2;
	Cases.Add({TEXT("Input window belongs to another attack generation"), MismatchedWindowAttack,
		EDefenseOutcome::GuardEntered, EDefenseDamageDisposition::NoContactSideEffects,
		EAttackerResponse::None});

	FDefenseQuery NoCapability = MakeInputQuery();
	NoCapability.Attack.AttackTags.RemoveTag(KatanaCombatGameplayTags::AttackDefenseParryable());
	Cases.Add({TEXT("Input no parry capability"), NoCapability, EDefenseOutcome::GuardEntered,
		EDefenseDamageDisposition::NoContactSideEffects, EAttackerResponse::None});

	FDefenseQuery NullAttackData = MakeInputQuery();
	NullAttackData.Attack.AttackData = nullptr;
	Cases.Add({TEXT("Input null AttackData cannot inherit malformed parry tags"), NullAttackData,
		EDefenseOutcome::GuardEntered, EDefenseDamageDisposition::NoContactSideEffects,
		EAttackerResponse::None});

	FDefenseQuery ConsumedInput = MakeInputQuery();
	ConsumedInput.Attack.bAttackConsumed = true;
	Cases.Add({TEXT("Input consumed attack"), ConsumedInput, EDefenseOutcome::GuardEntered,
		EDefenseDamageDisposition::NoContactSideEffects, EAttackerResponse::None});

	FDefenseQuery PairedAttackerInput = MakeInputQuery();
	PairedAttackerInput.Attack.bAttackerPaired = true;
	Cases.Add({TEXT("Input paired attacker"), PairedAttackerInput, EDefenseOutcome::GuardEntered,
		EDefenseDamageDisposition::NoContactSideEffects, EAttackerResponse::None});

	FDefenseQuery ContradictoryRelationship = MakeInputQuery();
	ContradictoryRelationship.Attack.bIsFriendlyToDefender = true;
	Cases.Add({TEXT("Input contradictory friendly-hostile relationship"), ContradictoryRelationship,
		EDefenseOutcome::GuardEntered, EDefenseDamageDisposition::NoContactSideEffects,
		EAttackerResponse::None});

	FDefenseQuery NonCredibleInput = MakeInputQuery();
	NonCredibleInput.Attack.bHasCredibleIntent = false;
	Cases.Add({TEXT("Input non-credible intent"), NonCredibleInput, EDefenseOutcome::GuardEntered,
		EDefenseDamageDisposition::NoContactSideEffects, EAttackerResponse::None});

	FDefenseQuery MissingIntendedTarget = MakeInputQuery();
	MissingIntendedTarget.Attack.IntendedTarget = nullptr;
	MissingIntendedTarget.Attack.PredictedContact.IntendedTarget = nullptr;
	Cases.Add({TEXT("Input missing intended defender"), MissingIntendedTarget,
		EDefenseOutcome::GuardEntered, EDefenseDamageDisposition::NoContactSideEffects,
		EAttackerResponse::None});

	FDefenseQuery MismatchedPredictedTarget = MakeInputQuery();
	MismatchedPredictedTarget.Attack.PredictedContact.IntendedTarget =
		MismatchedPredictedTarget.Attack.AttackInstance.Attacker;
	Cases.Add({TEXT("Input prediction targets a different actor"), MismatchedPredictedTarget,
		EDefenseOutcome::GuardEntered, EDefenseDamageDisposition::NoContactSideEffects,
		EAttackerResponse::None});

	FDefenseQuery PredictionMiss = MakeInputQuery();
	PredictionMiss.Attack.PredictedContact.bPathIntersectsThreatVolume = false;
	Cases.Add({TEXT("Input prediction misses threat volume"), PredictionMiss, EDefenseOutcome::GuardEntered,
		EDefenseDamageDisposition::NoContactSideEffects, EAttackerResponse::None});

	FDefenseQuery Stale = MakeInputQuery();
	Stale.Attack.bAttackIdentityCurrent = false;
	Cases.Add({TEXT("Input stale attack"), Stale, EDefenseOutcome::GuardEntered,
		EDefenseDamageDisposition::NoContactSideEffects, EAttackerResponse::None});

	FDefenseQuery InvalidIdentity = MakeInputQuery();
	InvalidIdentity.Attack.AttackInstance = {};
	InvalidIdentity.Attack.ActiveParryWindow.AttackInstance = {};
	Cases.Add({TEXT("Input structurally invalid attack identity"), InvalidIdentity,
		EDefenseOutcome::GuardEntered, EDefenseDamageDisposition::NoContactSideEffects,
		EAttackerResponse::None});

	FDefenseQuery LowConfidence = MakeInputQuery();
	LowConfidence.Attack.PredictedContact.Confidence = EDefensePredictionConfidence::Low;
	Cases.Add({TEXT("Input low confidence"), LowConfidence, EDefenseOutcome::GuardEntered,
		EDefenseDamageDisposition::NoContactSideEffects, EAttackerResponse::None});

	FDefenseQuery StaleHighConfidence = MakeInputQuery();
	StaleHighConfidence.CurrentSimulationTime = 1.0;
	StaleHighConfidence.MaximumHighConfidencePredictionAge = 0.10f;
	StaleHighConfidence.Attack.PredictedContact.PredictionSimulationTimestamp = 0.50;
	Cases.Add({TEXT("Input stale high-confidence prediction"), StaleHighConfidence, EDefenseOutcome::GuardEntered,
		EDefenseDamageDisposition::NoContactSideEffects, EAttackerResponse::None});

	FDefenseQuery OutsideCone = MakeInputQuery();
	OutsideCone.RelativeYawDegrees = 71.0f;
	Cases.Add({TEXT("Input outside hard cone"), OutsideCone, EDefenseOutcome::GuardEntered,
		EDefenseDamageDisposition::NoContactSideEffects, EAttackerResponse::None});

	FDefenseQuery NormalOnly = MakeInputQuery();
	NormalOnly.RelativeYawDegrees = 25.0f;
	NormalOnly.TimeToAlignmentDeadline = 0.05f;
	Cases.Add({TEXT("Input only normal alignment reachable"), NormalOnly, EDefenseOutcome::GuardEntered,
		EDefenseDamageDisposition::NoContactSideEffects, EAttackerResponse::None});

	FDefenseQuery Perfect = MakeInputQuery();
	Cases.Add({TEXT("Input perfect parry"), Perfect, EDefenseOutcome::PerfectParry,
		EDefenseDamageDisposition::NoContactSideEffects, EAttackerResponse::ParryStagger});

	FDefenseQuery UnblockableParry = MakeInputQuery();
	UnblockableParry.Attack.AttackTags.AddTag(KatanaCombatGameplayTags::AttackPropertyUnblockable());
	Cases.Add({TEXT("Input unblockable but parryable"), UnblockableParry, EDefenseOutcome::PerfectParry,
		EDefenseDamageDisposition::NoContactSideEffects, EAttackerResponse::ParryStagger});

	FDefenseQuery Friendly = MakeContactQuery();
	Friendly.Attack.bIsHostileToDefender = false;
	Friendly.Attack.bIsFriendlyToDefender = true;
	Cases.Add({TEXT("Contact friendly"), Friendly, EDefenseOutcome::IgnoredFriendly,
		EDefenseDamageDisposition::NoContactSideEffects, EAttackerResponse::None});

	FDefenseQuery InvalidContact = MakeContactQuery();
	InvalidContact.Attack.bAttackerAlive = false;
	Cases.Add({TEXT("Contact invalid participant"), InvalidContact, EDefenseOutcome::IgnoredInvalid,
		EDefenseDamageDisposition::NoContactSideEffects, EAttackerResponse::None});

	FDefenseQuery Consumed = MakeContactQuery();
	Consumed.Attack.bAttackConsumed = true;
	Cases.Add({TEXT("Contact consumed attack"), Consumed, EDefenseOutcome::IgnoredConsumed,
		EDefenseDamageDisposition::NoContactSideEffects, EAttackerResponse::None});

	FDefenseQuery ConsumedDuringPairedTakeover = MakeContactQuery();
	ConsumedDuringPairedTakeover.Attack.bAttackConsumed = true;
	ConsumedDuringPairedTakeover.Attack.bAttackerPaired = true;
	ConsumedDuringPairedTakeover.bDefenderPaired = true;
	Cases.Add({TEXT("Consumed attack wins over paired takeover state"),
		ConsumedDuringPairedTakeover, EDefenseOutcome::IgnoredConsumed,
		EDefenseDamageDisposition::NoContactSideEffects, EAttackerResponse::None});

	FDefenseQuery Invulnerable = MakeContactQuery();
	Invulnerable.bDefenderCanBeDamaged = false;
	Cases.Add({TEXT("Contact invulnerable"), Invulnerable, EDefenseOutcome::IgnoredInvulnerable,
		EDefenseDamageDisposition::NoContactSideEffects, EAttackerResponse::None});

	FDefenseQuery IFrames = MakeContactQuery();
	IFrames.bDefenderInIFrames = true;
	Cases.Add({TEXT("Contact i-frames"), IFrames, EDefenseOutcome::IgnoredInvulnerable,
		EDefenseDamageDisposition::NoContactSideEffects, EAttackerResponse::None});

	FDefenseQuery PairedDefender = MakeContactQuery();
	PairedDefender.bDefenderPaired = true;
	Cases.Add({TEXT("Contact paired defender"), PairedDefender, EDefenseOutcome::IgnoredInvalid,
		EDefenseDamageDisposition::NoContactSideEffects, EAttackerResponse::None});

	FDefenseQuery PairedAttacker = MakeContactQuery();
	PairedAttacker.Attack.bAttackerPaired = true;
	Cases.Add({TEXT("Contact paired attacker"), PairedAttacker, EDefenseOutcome::IgnoredInvalid,
		EDefenseDamageDisposition::NoContactSideEffects, EAttackerResponse::None});

	FDefenseQuery StaleActiveAttack = MakeContactQuery();
	StaleActiveAttack.Attack.bAttackActive = false;
	Cases.Add({TEXT("Contact inactive authored attack"), StaleActiveAttack, EDefenseOutcome::IgnoredConsumed,
		EDefenseDamageDisposition::NoContactSideEffects, EAttackerResponse::None});

	FDefenseQuery MissingActualContact = MakeContactQuery();
	MissingActualContact.bHasActualContact = false;
	Cases.Add({TEXT("Contact missing actual payload"), MissingActualContact, EDefenseOutcome::IgnoredInvalid,
		EDefenseDamageDisposition::NoContactSideEffects, EAttackerResponse::None});

	FDefenseQuery InvalidContactIdentity = MakeContactQuery();
	InvalidContactIdentity.bContactIdentityValid = false;
	Cases.Add({TEXT("Contact missing identity"), InvalidContactIdentity, EDefenseOutcome::IgnoredInvalid,
		EDefenseDamageDisposition::NoContactSideEffects, EAttackerResponse::None});

	FDefenseQuery Unblockable = MakeContactQuery();
	Unblockable.Attack.AttackTags.AddTag(KatanaCombatGameplayTags::AttackPropertyUnblockable());
	Cases.Add({TEXT("Contact unblockable"), Unblockable, EDefenseOutcome::UnblockableHit,
		EDefenseDamageDisposition::ApplyRequestedDamage, EAttackerResponse::Continue});

	FDefenseQuery NotGuarding = MakeContactQuery();
	NotGuarding.bDefenderGuarding = false;
	Cases.Add({TEXT("Contact not guarding"), NotGuarding, EDefenseOutcome::Hit,
		EDefenseDamageDisposition::ApplyRequestedDamage, EAttackerResponse::Continue});

	FDefenseQuery InterruptibleHit = MakeContactQuery();
	InterruptibleHit.bDefenderGuarding = false;
	InterruptibleHit.Attack.AttackTags.AddTag(
		KatanaCombatGameplayTags::AttackDefenseBlockInterruptible());
	Cases.Add({TEXT("Block interruptible tag does not affect a hit"), InterruptibleHit,
		EDefenseOutcome::Hit, EDefenseDamageDisposition::ApplyRequestedDamage,
		EAttackerResponse::Continue});

	FDefenseQuery InterruptibleUnblockable = MakeContactQuery();
	InterruptibleUnblockable.Attack.AttackTags.AddTag(
		KatanaCombatGameplayTags::AttackPropertyUnblockable());
	InterruptibleUnblockable.Attack.AttackTags.AddTag(
		KatanaCombatGameplayTags::AttackDefenseBlockInterruptible());
	Cases.Add({TEXT("Block interruptible tag does not affect an unblockable hit"),
		InterruptibleUnblockable, EDefenseOutcome::UnblockableHit,
		EDefenseDamageDisposition::ApplyRequestedDamage, EAttackerResponse::Continue});

	FDefenseQuery OutsideBlockTolerance = MakeContactQuery();
	OutsideBlockTolerance.RelativeYawDegrees = 36.0f;
	Cases.Add({TEXT("Contact outside block tolerance"), OutsideBlockTolerance, EDefenseOutcome::Hit,
		EDefenseDamageDisposition::ApplyRequestedDamage, EAttackerResponse::Continue});

	FDefenseQuery NonFiniteContactYaw = MakeContactQuery();
	NonFiniteContactYaw.RelativeYawDegrees = std::numeric_limits<float>::quiet_NaN();
	Cases.Add({TEXT("Contact non-finite yaw fails closed"), NonFiniteContactYaw, EDefenseOutcome::Hit,
		EDefenseDamageDisposition::ApplyRequestedDamage, EAttackerResponse::Continue});

	FDefenseQuery NormalBlock = MakeContactQuery();
	Cases.Add({TEXT("Contact normal block"), NormalBlock, EDefenseOutcome::NormalBlock,
		EDefenseDamageDisposition::SuppressDamage, EAttackerResponse::Continue});

	FDefenseQuery LowRightBlock = MakeContactQuery();
	LowRightBlock.ActualContact.Height = EAttackHeight::Low;
	LowRightBlock.ActualContact.Lane = EIncomingAttackLane::Right;
	Cases.Add({TEXT("Height and lane do not alter normal-block eligibility"), LowRightBlock,
		EDefenseOutcome::NormalBlock, EDefenseDamageDisposition::SuppressDamage,
		EAttackerResponse::Continue});

	FDefenseQuery RecoilBlock = MakeContactQuery();
	RecoilBlock.Attack.AttackTags.AddTag(KatanaCombatGameplayTags::AttackDefenseBlockInterruptible());
	Cases.Add({TEXT("Contact interruptible block"), RecoilBlock, EDefenseOutcome::NormalBlock,
		EDefenseDamageDisposition::SuppressDamage, EAttackerResponse::Recoil});

	FDefenseQuery CompatibilityBlock = MakeContactQuery();
	CompatibilityBlock.Attack.AttackData = nullptr;
	CompatibilityBlock.Attack.AttackInstance = {};
	CompatibilityBlock.Attack.bAttackIdentityCurrent = false;
	CompatibilityBlock.Attack.AttackTags.AddTag(
		KatanaCombatGameplayTags::AttackPropertyUnblockable());
	CompatibilityBlock.Attack.AttackTags.AddTag(
		KatanaCombatGameplayTags::AttackDefenseBlockInterruptible());
	Cases.Add({TEXT("Contact null AttackData ignores fabricated authored tags"), CompatibilityBlock,
		EDefenseOutcome::NormalBlock,
		EDefenseDamageDisposition::SuppressDamage, EAttackerResponse::Continue});

	FDefenseQuery FriendlyFireEnabled = MakeContactQuery();
	FriendlyFireEnabled.Attack.bIsHostileToDefender = false;
	FriendlyFireEnabled.Attack.bIsFriendlyToDefender = true;
	FriendlyFireEnabled.bFriendlyFireEnabled = true;
	Cases.Add({TEXT("Contact explicit friendly fire"), FriendlyFireEnabled, EDefenseOutcome::NormalBlock,
		EDefenseDamageDisposition::SuppressDamage, EAttackerResponse::Continue});

	for (const FResolverCase& Case : Cases)
	{
		const FDefenseDecision Decision = FDefenseResolver::Resolve(Case.Query);
		TestEqual(FString::Printf(TEXT("%s outcome"), Case.Name), Decision.Outcome, Case.ExpectedOutcome);
		TestEqual(FString::Printf(TEXT("%s damage"), Case.Name), Decision.DamageDisposition, Case.ExpectedDamage);
		TestEqual(FString::Printf(TEXT("%s response"), Case.Name), Decision.AttackerResponse, Case.ExpectedResponse);
	}

	const FDefenseDecision PerfectDecision = FDefenseResolver::Resolve(Perfect);
	TestEqual(TEXT("Perfect parry has no failure reason"), PerfectDecision.Reason, EDefenseReason::None);
	TestEqual(TEXT("Perfect parry owns bridge alignment"), PerfectDecision.AlignmentPolicy,
		EDefenseAlignmentPolicy::PerfectParryBridge);
	TestTrue(TEXT("Perfect parry is chain eligible"), PerfectDecision.bChainEligible);

	const FDefenseDecision GuardDecision = FDefenseResolver::Resolve(OutsideCone);
	TestEqual(TEXT("Outside-cone guard records its reason"), GuardDecision.Reason,
		EDefenseReason::OutsideHardCone);
	TestEqual(TEXT("Outside-cone guard does not auto-align"), GuardDecision.AlignmentPolicy,
		EDefenseAlignmentPolicy::None);

	TestEqual(TEXT("Null AttackData reports missing parry capability"),
		FDefenseResolver::Resolve(NullAttackData).Reason,
		EDefenseReason::MissingParryCapability);
	TestEqual(TEXT("Consumed input reports consumed identity"),
		FDefenseResolver::Resolve(ConsumedInput).Reason, EDefenseReason::Consumed);
	TestEqual(TEXT("Paired attacker is not a hostile input candidate"),
		FDefenseResolver::Resolve(PairedAttackerInput).Reason,
		EDefenseReason::NoHostileCandidate);
	TestEqual(TEXT("Non-credible intent reports insufficient prediction"),
		FDefenseResolver::Resolve(NonCredibleInput).Reason,
		EDefenseReason::PredictionInsufficient);

	const FDefenseDecision BlockDecision = FDefenseResolver::Resolve(NormalBlock);
	TestEqual(TEXT("Normal block uses contact alignment"), BlockDecision.AlignmentPolicy,
		EDefenseAlignmentPolicy::BlockContact);
	TestFalse(TEXT("Normal block is not chain eligible"), BlockDecision.bChainEligible);

	FDefenseQuery ActualSocketQuery = MakeContactQuery();
	ActualSocketQuery.Attack.PredictedContact.SourceSocket = TEXT("PredictedSocket");
	ActualSocketQuery.Attack.SourceSocket = TEXT("AuthoredSocket");
	ActualSocketQuery.ActualContact.SourceSocket = TEXT("ActualSocket");
	const FDefenseDecision ActualSocketDecision = FDefenseResolver::Resolve(ActualSocketQuery);
	TestEqual(TEXT("Actual contact source socket is authoritative"), ActualSocketDecision.SourceSocket,
		FName(TEXT("ActualSocket")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDefenseReachabilityBoundaryTest,
	"KatanaCombat.Defense.Resolver.ReachabilityBoundaries",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FDefenseReachabilityBoundaryTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	const FDefenseReachability AtHardCone = FDefenseResolver::CalculateReachability(
		70.0f, 1.0f, 180.0f, 10.0f, 70.0f, 70.0f);
	TestTrue(TEXT("Yaw exactly at hard cone is eligible"), AtHardCone.bWithinHardCone);
	TestTrue(TEXT("Yaw exactly at hard cone is reachable with enough budget"), AtHardCone.bReachable);

	const FDefenseReachability AtReachableBoundary = FDefenseResolver::CalculateReachability(
		28.0f, 0.10f, 180.0f, 10.0f, 70.0f, 70.0f);
	TestEqual(TEXT("Available turn uses deadline and rate"), AtReachableBoundary.AvailableTurn, 18.0f);
	TestTrue(TEXT("Yaw exactly at tolerance plus available turn is reachable"), AtReachableBoundary.bReachable);

	TestTrue(TEXT("Zero deadline still accepts existing final tolerance"),
		FDefenseResolver::CalculateReachability(10.0f, 0.0f, 180.0f, 10.0f, 70.0f, 70.0f).bReachable);
	TestFalse(TEXT("Zero deadline cannot invent turn"),
		FDefenseResolver::CalculateReachability(10.1f, 0.0f, 180.0f, 10.0f, 70.0f, 70.0f).bReachable);
	TestFalse(TEXT("Negative deadline cannot invent turn"),
		FDefenseResolver::CalculateReachability(10.1f, -1.0f, 180.0f, 10.0f, 70.0f, 70.0f).bReachable);
	TestFalse(TEXT("Exhausted cumulative budget prevents additional correction"),
		FDefenseResolver::CalculateReachability(16.0f, 1.0f, 180.0f, 10.0f, 70.0f, 5.0f).bReachable);

	const FDefenseReachability ExtremeFiniteYaw = FDefenseResolver::CalculateReachability(
		TNumericLimits<float>::Max(), 0.10f, 180.0f, 10.0f, 70.0f, 70.0f);
	TestTrue(TEXT("Extreme finite yaw is normalized without unbounded work"),
		FMath::IsFinite(ExtremeFiniteYaw.AbsoluteYawError));
	TestTrue(TEXT("Normalized yaw remains in the signed-angle domain"),
		ExtremeFiniteYaw.AbsoluteYawError <= 180.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDefenseThreatSelectionDeterminismTest,
	"KatanaCombat.Defense.Resolver.ThreatSelectionDeterministic",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FDefenseThreatSelectionDeterminismTest::RunTest(const FString& Parameters)
{
	using namespace DefenseResolverTests;
	(void)Parameters;

	FDefenseThreatSelectionContext Context;
	Context.HardGuardConeHalfAngle = 70.0f;
	Context.MaximumAutomaticTurn = 70.0f;
	Context.RemainingAutomaticTurn = 70.0f;
	Context.DefenseTurnRate = 180.0f;
	Context.PerfectParryFinalTolerance = 10.0f;

	const FAttackExecutionSnapshot HigherId = MakeThreat(42, 0.25f, 15.0f, 300.0f);
	const FAttackExecutionSnapshot LowerId = MakeThreat(7, 0.25f, -15.0f, 300.0f);
	const TArray<FAttackExecutionSnapshot> ForwardOrder{HigherId, LowerId};
	const TArray<FAttackExecutionSnapshot> ReverseOrder{LowerId, HigherId};

	const FDefenseThreatSelectionResult Forward = FDefenseResolver::SelectThreat(ForwardOrder, Context);
	const FDefenseThreatSelectionResult Reverse = FDefenseResolver::SelectThreat(ReverseOrder, Context);
	TestTrue(TEXT("Forward selection finds a threat"), Forward.bFound);
	TestTrue(TEXT("Reverse selection finds a threat"), Reverse.bFound);
	TestEqual(TEXT("Stable ID breaks a complete tie"), Forward.SelectedThreat.StableId.Value, uint64{7});
	TestEqual(TEXT("Candidate order cannot change selection"), Reverse.SelectedThreat.StableId.Value, uint64{7});
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDefenseThreatRankingOrderTest,
	"KatanaCombat.Defense.Resolver.ThreatRankingOrder",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FDefenseThreatRankingOrderTest::RunTest(const FString& Parameters)
{
	using namespace DefenseResolverTests;
	(void)Parameters;

	FDefenseThreatSelectionContext Context;
	Context.HardGuardConeHalfAngle = 70.0f;
	Context.MaximumAutomaticTurn = 70.0f;
	Context.RemainingAutomaticTurn = 70.0f;
	Context.DefenseTurnRate = 180.0f;
	Context.PerfectParryFinalTolerance = 10.0f;

	auto SelectId = [&Context](const FAttackExecutionSnapshot& Left,
		const FAttackExecutionSnapshot& Right)
	{
		return FDefenseResolver::SelectThreat({Left, Right}, Context).SelectedThreat.StableId.Value;
	};

	FAttackExecutionSnapshot Credible = MakeThreat(50, 0.50f, 20.0f, 500.0f);
	FAttackExecutionSnapshot GuardOnly = MakeThreat(1, 0.01f, 0.0f, 1.0f);
	GuardOnly.bHasCredibleIntent = false;
	TestEqual(TEXT("Credible intent is the first ranking criterion"),
		SelectId(Credible, GuardOnly), uint64{50});

	const FAttackExecutionSnapshot EarlierUnreachable = MakeThreat(50, 0.05f, 70.0f, 500.0f);
	const FAttackExecutionSnapshot LaterReachable = MakeThreat(1, 0.25f, 0.0f, 1.0f);
	TestEqual(TEXT("Deadline outranks reachability and later criteria"),
		SelectId(EarlierUnreachable, LaterReachable), uint64{50});

	const FAttackExecutionSnapshot Reachable = MakeThreat(50, 0.10f, 20.0f, 500.0f);
	const FAttackExecutionSnapshot Unreachable = MakeThreat(1, 0.10f, 40.0f, 1.0f);
	TestEqual(TEXT("Reachability outranks later criteria"),
		SelectId(Reachable, Unreachable), uint64{50});

	const FAttackExecutionSnapshot HighConfidence = MakeThreat(
		50, 0.25f, 10.0f, 500.0f, EDefensePredictionConfidence::High);
	const FAttackExecutionSnapshot LowConfidence = MakeThreat(
		1, 0.25f, 10.0f, 1.0f, EDefensePredictionConfidence::Low);
	TestEqual(TEXT("Confidence outranks yaw, distance, and stable ID"),
		SelectId(HighConfidence, LowConfidence), uint64{50});

	const FAttackExecutionSnapshot SmallerYaw = MakeThreat(50, 0.25f, 5.0f, 500.0f);
	const FAttackExecutionSnapshot LargerYaw = MakeThreat(1, 0.25f, 15.0f, 1.0f);
	TestEqual(TEXT("Absolute yaw outranks distance and stable ID"),
		SelectId(SmallerYaw, LargerYaw), uint64{50});

	const FAttackExecutionSnapshot Nearer = MakeThreat(50, 0.25f, 10.0f, 100.0f);
	const FAttackExecutionSnapshot Farther = MakeThreat(1, 0.25f, 10.0f, 500.0f);
	TestEqual(TEXT("Distance outranks stable ID"), SelectId(Nearer, Farther), uint64{50});

	Context.CurrentSimulationTime = 1.0;
	Context.MaximumHighConfidencePredictionAge = 0.10f;
	FAttackExecutionSnapshot StaleHigh = MakeThreat(1, 0.25f, 10.0f, 100.0f);
	StaleHigh.PredictedContact.PredictionSimulationTimestamp = 0.0;
	FAttackExecutionSnapshot FreshHigh = MakeThreat(50, 0.25f, 10.0f, 100.0f);
	FreshHigh.PredictedContact.PredictionSimulationTimestamp = 0.95;
	TestEqual(TEXT("Stale high confidence is downgraded before ranking"),
		SelectId(StaleHigh, FreshHigh), uint64{50});

	Context.CurrentSimulationTime = 0.0;
	FAttackExecutionSnapshot Consumed = MakeThreat(1, 0.01f, 0.0f, 1.0f);
	Consumed.bAttackConsumed = true;
	FAttackExecutionSnapshot Paired = MakeThreat(2, 0.02f, 0.0f, 1.0f);
	Paired.bAttackerPaired = true;
	FAttackExecutionSnapshot InvalidIdentity = MakeThreat(3, 0.005f, 0.0f, 1.0f);
	InvalidIdentity.AttackInstance = {};
	const FAttackExecutionSnapshot Valid = MakeThreat(50, 0.50f, 20.0f, 500.0f);
	const FDefenseThreatSelectionResult Filtered = FDefenseResolver::SelectThreat(
		{Consumed, Paired, InvalidIdentity, Valid}, Context);
	TestTrue(TEXT("A valid threat remains after filtering"), Filtered.bFound);
	TestEqual(TEXT("Consumed, paired, and structurally invalid threats are not selectable"),
		Filtered.SelectedThreat.StableId.Value, uint64{50});
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDefenseThreatLockPolicyTest,
	"KatanaCombat.Defense.Resolver.ThreatLockPolicy",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FDefenseThreatLockPolicyTest::RunTest(const FString& Parameters)
{
	using namespace DefenseResolverTests;
	(void)Parameters;

	FDefenseThreatSelectionContext Context;
	Context.LockedThreatId.Value = 20;
	Context.LockAgeSeconds = 0.20f;
	Context.ThreatLockMinSeconds = 0.15f;
	Context.ThreatSwitchLeadSeconds = 0.10f;

	FAttackExecutionSnapshot LockedNoDeadline = MakeThreat(20, -1.0f, 15.0f, 300.0f);
	FAttackExecutionSnapshot LowerIdNoDeadline = MakeThreat(5, -1.0f, -15.0f, 300.0f);
	FDefenseThreatSelectionResult Result = FDefenseResolver::SelectThreat(
		{LockedNoDeadline, LowerIdNoDeadline}, Context);
	TestEqual(TEXT("Threats without reliable deadlines cannot replace a valid lock"),
		Result.SelectedThreat.StableId.Value, uint64{20});

	FAttackExecutionSnapshot Locked = MakeThreat(20, 0.50f, 15.0f, 300.0f);
	FAttackExecutionSnapshot Earlier = MakeThreat(5, 0.20f, -15.0f, 300.0f);
	Result = FDefenseResolver::SelectThreat({Locked, Earlier}, Context);
	TestEqual(TEXT("A threat beyond the switch lead replaces an expired lock"),
		Result.SelectedThreat.StableId.Value, uint64{5});

	Context.LockAgeSeconds = 0.05f;
	Result = FDefenseResolver::SelectThreat({Locked, Earlier}, Context);
	TestEqual(TEXT("Lock minimum suppresses an otherwise valid switch"),
		Result.SelectedThreat.StableId.Value, uint64{20});
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDefenseIdentityContractTest,
	"KatanaCombat.Defense.Resolver.IdentityContracts",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FDefenseIdentityContractTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	FAnimNotifyRuntimeSourceId NotifyA;
	NotifyA.SourceAnimation = FSoftObjectPath(TEXT("/Game/Test/AM_Defense.AM_Defense"));
	NotifyA.NotifyEventIndex = 3;
	FAnimNotifyRuntimeSourceId NotifyB = NotifyA;
	TestTrue(TEXT("Runtime notify source is structurally valid"), NotifyA.IsValid());
	TestTrue(TEXT("Equal notify source IDs compare equal"), NotifyA == NotifyB);
	TestEqual(TEXT("Equal notify source IDs hash equally"), GetTypeHash(NotifyA), GetTypeHash(NotifyB));
	NotifyB.NotifyEventIndex = 4;
	TestFalse(TEXT("Notify event index participates in identity"), NotifyA == NotifyB);
	NotifyB.NotifyEventIndex = -2;
	TestFalse(TEXT("Negative notify indices are invalid"), NotifyB.IsValid());

	FAttackWindowInstanceId Window;
	Window.AttackInstance.Attacker = GetMutableDefault<AActor>();
	Window.AttackInstance.AttackGeneration = 1;
	Window.Kind = EAttackWindowKind::Parry;
	Window.WindowGeneration = 1;
	Window.NotifySource = NotifyA;
	Window.MontageInstanceId = 0;
	Window.SimulationStartTime = 1.0;
	Window.SimulationEndTime = 2.0;
	TestTrue(TEXT("Complete finite window identity is valid"), Window.IsValid());
	Window.MontageInstanceId = -2;
	TestFalse(TEXT("Negative montage instance IDs are invalid"), Window.IsValid());
	Window.MontageInstanceId = 0;
	Window.SimulationEndTime = std::numeric_limits<double>::infinity();
	TestFalse(TEXT("Non-finite window timing is invalid"), Window.IsValid());

	FDefenseInteractionKey InputA;
	InputA.Stage = EDefenseQueryStage::InputIntent;
	InputA.AttackInstance.AttackGeneration = 10;
	InputA.ContactInstance.CompatibilityTrace.TraceGeneration = 1;
	FDefenseInteractionKey InputB = InputA;
	InputB.ContactInstance.CompatibilityTrace.TraceGeneration = 99;
	TestTrue(TEXT("Input interaction equality ignores irrelevant contact identity"), InputA == InputB);
	TestEqual(TEXT("Input interaction hash ignores irrelevant contact identity"),
		GetTypeHash(InputA), GetTypeHash(InputB));

	FDefenseInteractionKey ContactA;
	ContactA.Stage = EDefenseQueryStage::Contact;
	ContactA.AttackInstance.AttackGeneration = 10;
	ContactA.ContactInstance.CompatibilityTrace.TraceGeneration = 7;
	FDefenseInteractionKey ContactB = ContactA;
	ContactB.AttackInstance.AttackGeneration = 11;
	TestTrue(TEXT("Contact interaction equality ignores redundant attack identity"), ContactA == ContactB);
	TestEqual(TEXT("Contact interaction hash ignores redundant attack identity"),
		GetTypeHash(ContactA), GetTypeHash(ContactB));

	FPredictedDefenseContact Predicted;
	Predicted.IntendedTarget = nullptr;
	Predicted.bPathIntersectsThreatVolume = true;
	TestTrue(TEXT("Predicted contact retains threat-volume intersection"),
		Predicted.bPathIntersectsThreatVolume);

	FAttackExecutionSnapshot Snapshot;
	Snapshot.TimeToPredictedContact = 0.20f;
	Snapshot.TimeToParryWindowEnd = 0.10f;
	Snapshot.TimeToAlignmentDeadline = 0.10f;
	TestEqual(TEXT("Snapshot retains predicted-contact deadline"),
		Snapshot.TimeToPredictedContact, 0.20f);
	TestEqual(TEXT("Snapshot retains parry-window deadline"),
		Snapshot.TimeToParryWindowEnd, 0.10f);

	FDefenseQuery Query;
	Query.LockedThreatId.Value = 17;
	Query.ThreatLockAgeSeconds = 0.05f;
	TestEqual(TEXT("Query retains locked threat identity"), Query.LockedThreatId.Value, uint64{17});
	TestEqual(TEXT("Query retains threat-lock age"), Query.ThreatLockAgeSeconds, 0.05f);

	FActualDefenseContact Actual;
	Actual.SourceSocket = TEXT("WeaponContact");
	TestEqual(TEXT("Actual contact retains the source socket"), Actual.SourceSocket,
		FName(TEXT("WeaponContact")));

	FDefenseAlignmentRequestSpec Alignment;
	Alignment.OwnerInteraction.Epoch = 9;
	TestEqual(TEXT("Alignment request retains its interaction owner"),
		Alignment.OwnerInteraction.Epoch, uint64{9});
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDefenseContactDerivationTest,
	"KatanaCombat.Defense.Resolver.ContactDerivation",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FDefenseContactDerivationTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	const FTransform DefenderTransform = FTransform::Identity;

	const FDefenseLaneResolution VelocityWins = FDefenseResolver::ResolveIncomingLane(
		FVector(100.0f, 100.0f, 0.0f),
		FVector::ZeroVector,
		FVector(100.0f, -100.0f, 0.0f),
		EIncomingAttackLane::Center,
		DefenderTransform,
		12.0f);
	TestEqual(TEXT("Weapon velocity has first lane precedence"), VelocityWins.Provenance,
		EDefenseLaneProvenance::WeaponVelocity);
	TestEqual(TEXT("Positive defender-local trajectory resolves right"), VelocityWins.Lane,
		EIncomingAttackLane::Right);

	const FDefenseLaneResolution TraceWins = FDefenseResolver::ResolveIncomingLane(
		FVector::ZeroVector,
		FVector::ZeroVector,
		FVector(100.0f, -100.0f, 0.0f),
		EIncomingAttackLane::Center,
		DefenderTransform,
		12.0f);
	TestEqual(TEXT("Trace segment is the second lane source"), TraceWins.Provenance,
		EDefenseLaneProvenance::TraceSegment);
	TestEqual(TEXT("Negative defender-local trajectory resolves left"), TraceWins.Lane,
		EIncomingAttackLane::Left);

	const FDefenseLaneResolution AuthoredFallback = FDefenseResolver::ResolveIncomingLane(
		FVector::ZeroVector,
		FVector::ZeroVector,
		FVector::ZeroVector,
		EIncomingAttackLane::Right,
		DefenderTransform,
		12.0f);
	TestEqual(TEXT("Authored lane is explicitly low-confidence fallback"), AuthoredFallback.Provenance,
		EDefenseLaneProvenance::AuthoredFallback);

	const FDefenseLaneResolution StraightIncoming = FDefenseResolver::ResolveIncomingLane(
		FVector(-100.0f, 0.0f, 0.0f),
		FVector::ZeroVector,
		FVector::ZeroVector,
		EIncomingAttackLane::Right,
		DefenderTransform,
		12.0f);
	TestEqual(TEXT("A longitudinal trajectory toward the defender is center lane"),
		StraightIncoming.Lane, EIncomingAttackLane::Center);

	const float NonFinite = std::numeric_limits<float>::quiet_NaN();
	const FDefenseLaneResolution InvalidVelocityFallsThrough = FDefenseResolver::ResolveIncomingLane(
		FVector(NonFinite, 100.0f, 0.0f),
		FVector::ZeroVector,
		FVector(100.0f, -100.0f, 0.0f),
		EIncomingAttackLane::Right,
		DefenderTransform,
		12.0f);
	TestEqual(TEXT("Non-finite velocity falls through to a valid trace"),
		InvalidVelocityFallsThrough.Provenance, EDefenseLaneProvenance::TraceSegment);

	const FDefenseLaneResolution InvalidTrajectoryFallsBack = FDefenseResolver::ResolveIncomingLane(
		FVector(NonFinite, 100.0f, 0.0f),
		FVector::ZeroVector,
		FVector(NonFinite, 0.0f, 0.0f),
		EIncomingAttackLane::Right,
		DefenderTransform,
		12.0f);
	TestEqual(TEXT("Non-finite trajectory uses authored fallback"),
		InvalidTrajectoryFallsBack.Provenance, EDefenseLaneProvenance::AuthoredFallback);

	FTransform InvalidDefenderTransform = FTransform::Identity;
	InvalidDefenderTransform.SetLocation(FVector(NonFinite, 0.0f, 0.0f));
	const FDefenseLaneResolution InvalidTransformFallsBack = FDefenseResolver::ResolveIncomingLane(
		FVector(100.0f, 100.0f, 0.0f),
		FVector::ZeroVector,
		FVector::ZeroVector,
		EIncomingAttackLane::Left,
		InvalidDefenderTransform,
		12.0f);
	TestEqual(TEXT("Non-finite defender transform uses authored fallback"),
		InvalidTransformFallsBack.Provenance, EDefenseLaneProvenance::AuthoredFallback);

	const float SourceYaw = FDefenseResolver::CalculateDefenderRelativeYaw(
		DefenderTransform, FVector(1.0f, 1.0f, 0.0f));
	TestEqual(TEXT("Source bearing remains separate from trajectory lane"), SourceYaw, 45.0f);
	TestEqual(TEXT("Non-finite source bearing fails closed"),
		FDefenseResolver::CalculateDefenderRelativeYaw(
			DefenderTransform, FVector(NonFinite, 0.0f, 0.0f)),
		TNumericLimits<float>::Max());
	TestEqual(TEXT("Missing source bearing fails closed"),
		FDefenseResolver::CalculateDefenderRelativeYaw(DefenderTransform, FVector::ZeroVector),
		TNumericLimits<float>::Max());
	TestEqual(TEXT("Non-finite defender transform fails closed"),
		FDefenseResolver::CalculateDefenderRelativeYaw(
			InvalidDefenderTransform, FVector(1.0f, 0.0f, 0.0f)),
		TNumericLimits<float>::Max());

	UDefenseConfiguration* Configuration = NewObject<UDefenseConfiguration>();
	Configuration->BoneHeightRows = {
		{TEXT("head"), EAttackHeight::High},
		{TEXT("spine_03"), EAttackHeight::Middle}
	};

	const FDefenseHeightResolution Exact = Configuration->ResolveHeight(
		TEXT("head"), {TEXT("neck_01"), TEXT("spine_03")}, EAttackHeight::Low);
	TestEqual(TEXT("Exact bone mapping wins"), Exact.Provenance, EDefenseHeightProvenance::ExactBone);
	TestEqual(TEXT("Exact bone height"), Exact.Height, EAttackHeight::High);

	const FDefenseHeightResolution Parent = Configuration->ResolveHeight(
		TEXT("neck_01"), {TEXT("spine_03")}, EAttackHeight::Low);
	TestEqual(TEXT("Mapped parent is second"), Parent.Provenance, EDefenseHeightProvenance::MappedParent);
	TestEqual(TEXT("Mapped parent height"), Parent.Height, EAttackHeight::Middle);

	const FDefenseHeightResolution Authored = Configuration->ResolveHeight(
		TEXT("calf_l"), {TEXT("thigh_l")}, EAttackHeight::Low);
	TestEqual(TEXT("Authored height is final fallback"), Authored.Provenance, EDefenseHeightProvenance::Authored);
	TestEqual(TEXT("Authored fallback height"), Authored.Height, EAttackHeight::Low);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDefenseAttackProfileContractTest,
	"KatanaCombat.Defense.Resolver.AttackProfileContract",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FDefenseAttackProfileContractTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	UAttackData* AttackData = NewObject<UAttackData>();
	TestEqual(TEXT("Defense profile defaults to middle height"), AttackData->DefenseProfile.Height,
		EAttackHeight::Middle);
	TestEqual(TEXT("Defense profile defaults to center lane"), AttackData->DefenseProfile.NominalLane,
		EIncomingAttackLane::Center);
	TestEqual(TEXT("Existing contact bone is the default defense fallback"),
		AttackData->GetDefenseTargetBoneFallback(), AttackData->DefaultContactBone);

	AttackData->DefaultContactBone = TEXT("pelvis");
	TestEqual(TEXT("Updated legacy contact bone remains the fallback"),
		AttackData->GetDefenseTargetBoneFallback(), FName(TEXT("pelvis")));
	AttackData->DefenseProfile.DefenderTargetBoneFallback = TEXT("head");
	TestEqual(TEXT("Explicit defense target bone wins"),
		AttackData->GetDefenseTargetBoneFallback(), FName(TEXT("head")));
	return true;
}
