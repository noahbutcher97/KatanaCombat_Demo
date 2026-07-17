// Copyright Epic Games, Inc. All Rights Reserved.

#include "Defense/DefenseResolver.h"

#include "Utilities/CombatGameplayTags.h"

namespace
{
float SanitizedDeadline(const float Deadline)
{
	return FMath::IsFinite(Deadline) && Deadline >= 0.0f ? Deadline : TNumericLimits<float>::Max();
}

float SanitizedSignedYaw(const float Value)
{
	if (!FMath::IsFinite(Value))
	{
		return TNumericLimits<float>::Max();
	}

	float Wrapped = FMath::Fmod(Value, 360.0f);
	if (!FMath::IsFinite(Wrapped))
	{
		return TNumericLimits<float>::Max();
	}
	if (Wrapped > 180.0f)
	{
		Wrapped -= 360.0f;
	}
	else if (Wrapped < -180.0f)
	{
		Wrapped += 360.0f;
	}
	return Wrapped;
}

float SanitizedYawMagnitude(const float Value)
{
	return FMath::Abs(SanitizedSignedYaw(Value));
}

EDefensePredictionConfidence GetEffectivePredictionConfidence(
	const FPredictedDefenseContact& Prediction,
	const double CurrentSimulationTime,
	const float MaximumHighConfidencePredictionAge)
{
	if (!Prediction.bIsValid || Prediction.Confidence != EDefensePredictionConfidence::High)
	{
		return Prediction.bIsValid ? Prediction.Confidence : EDefensePredictionConfidence::None;
	}

	if (!FMath::IsFinite(CurrentSimulationTime)
		|| !FMath::IsFinite(Prediction.PredictionSimulationTimestamp)
		|| !FMath::IsFinite(MaximumHighConfidencePredictionAge)
		|| MaximumHighConfidencePredictionAge < 0.0f)
	{
		return EDefensePredictionConfidence::Low;
	}

	const double PredictionAge = CurrentSimulationTime - Prediction.PredictionSimulationTimestamp;
	return PredictionAge >= 0.0 && PredictionAge <= MaximumHighConfidencePredictionAge
		? EDefensePredictionConfidence::High
		: EDefensePredictionConfidence::Low;
}

bool IsSelectableThreat(const FAttackExecutionSnapshot& Candidate)
{
	return Candidate.StableId.IsValid()
		&& Candidate.AttackInstance.IsValid()
		&& Candidate.bAttackerAlive
		&& Candidate.bAttackActive
		&& Candidate.bAttackIdentityCurrent
		&& !Candidate.bAttackConsumed
		&& !Candidate.bAttackerPaired
		&& Candidate.bIsHostileToDefender
		&& !Candidate.bIsFriendlyToDefender;
}

struct FThreatRank
{
	bool bCredibleIntent = false;
	float Deadline = TNumericLimits<float>::Max();
	bool bReachable = false;
	EDefensePredictionConfidence Confidence = EDefensePredictionConfidence::None;
	float AbsoluteYaw = TNumericLimits<float>::Max();
	float Distance = TNumericLimits<float>::Max();
	FCombatantStableId StableId;
};

FThreatRank BuildThreatRank(
	const FAttackExecutionSnapshot& Candidate,
	const FDefenseThreatSelectionContext& Context)
{
	FThreatRank Rank;
	Rank.Confidence = GetEffectivePredictionConfidence(
		Candidate.PredictedContact,
		Context.CurrentSimulationTime,
		Context.MaximumHighConfidencePredictionAge);
	Rank.bCredibleIntent = Candidate.bHasCredibleIntent
		&& Rank.Confidence == EDefensePredictionConfidence::High;
	Rank.Deadline = SanitizedDeadline(Candidate.TimeToAlignmentDeadline);
	Rank.bReachable = Candidate.TimeToAlignmentDeadline >= 0.0f
		&& FMath::IsFinite(Candidate.TimeToAlignmentDeadline)
		&& FDefenseResolver::CalculateReachability(
			Candidate.RelativeYawDegrees,
			Candidate.TimeToAlignmentDeadline,
			Context.DefenseTurnRate,
			Context.PerfectParryFinalTolerance,
			Context.HardGuardConeHalfAngle,
			FMath::Min(Context.MaximumAutomaticTurn, Context.RemainingAutomaticTurn)).bReachable;
	Rank.AbsoluteYaw = SanitizedYawMagnitude(Candidate.RelativeYawDegrees);
	Rank.Distance = FMath::IsFinite(Candidate.DistanceToDefender)
		? FMath::Max(0.0f, Candidate.DistanceToDefender)
		: TNumericLimits<float>::Max();
	Rank.StableId = Candidate.StableId;
	return Rank;
}

bool IsBetterThreat(const FThreatRank& Left, const FThreatRank& Right)
{
	if (Left.bCredibleIntent != Right.bCredibleIntent)
	{
		return Left.bCredibleIntent;
	}
	if (Left.Deadline != Right.Deadline)
	{
		return Left.Deadline < Right.Deadline;
	}
	if (Left.bReachable != Right.bReachable)
	{
		return Left.bReachable;
	}
	if (Left.Confidence != Right.Confidence)
	{
		return static_cast<uint8>(Left.Confidence) > static_cast<uint8>(Right.Confidence);
	}
	if (Left.AbsoluteYaw != Right.AbsoluteYaw)
	{
		return Left.AbsoluteYaw < Right.AbsoluteYaw;
	}
	if (Left.Distance != Right.Distance)
	{
		return Left.Distance < Right.Distance;
	}
	return Left.StableId < Right.StableId;
}

FDefenseDecision MakeBaseDecision(const FDefenseQuery& Query)
{
	FDefenseDecision Decision;
	Decision.SelectedAttack = Query.Attack.AttackData;
	Decision.AttackInstance = Query.Attack.AttackInstance;
	Decision.LockedThreatId = Query.Attack.StableId;
	Decision.Height = Query.Stage == EDefenseQueryStage::Contact && Query.bHasActualContact
		? Query.ActualContact.Height
		: Query.Attack.PredictedContact.bIsValid
			? Query.Attack.PredictedContact.Height
			: Query.Attack.AuthoredHeight;
	Decision.Lane = Query.Stage == EDefenseQueryStage::Contact && Query.bHasActualContact
		? Query.ActualContact.Lane
		: Query.Attack.PredictedContact.bIsValid
			? Query.Attack.PredictedContact.Lane
			: Query.Attack.NominalLane;
	Decision.SwingShape = Query.Attack.SwingShape;
	Decision.ContactPoint = Query.Stage == EDefenseQueryStage::Contact && Query.bHasActualContact
		? Query.ActualContact.HitInfo.ImpactPoint
		: Query.Attack.PredictedContact.ContactPoint;
	Decision.SourceSocket = Query.Stage == EDefenseQueryStage::Contact
			&& Query.bHasActualContact
			&& !Query.ActualContact.SourceSocket.IsNone()
		? Query.ActualContact.SourceSocket
		: Query.Attack.PredictedContact.SourceSocket.IsNone()
			? Query.Attack.SourceSocket
			: Query.Attack.PredictedContact.SourceSocket;
	Decision.TargetBone = Query.Stage == EDefenseQueryStage::Contact && Query.bHasActualContact
		? Query.ActualContact.ResolvedTargetBone
		: Query.Attack.PredictedContact.DefenderTargetBone.IsNone()
			? Query.Attack.DefenderTargetBone
			: Query.Attack.PredictedContact.DefenderTargetBone;
	Decision.MeasuredYawDegrees = SanitizedSignedYaw(Query.RelativeYawDegrees);
	Decision.PredictionConfidence = GetEffectivePredictionConfidence(
		Query.Attack.PredictedContact,
		Query.CurrentSimulationTime,
		Query.MaximumHighConfidencePredictionAge);
	return Decision;
}

FDefenseDecision ResolveInputIntent(const FDefenseQuery& Query)
{
	FDefenseDecision Decision = MakeBaseDecision(Query);
	Decision.RequiredFinalTolerance = Query.PerfectParryFinalTolerance;

	if (!Query.bDefenderAlive || !Query.bDefenderCanGuard || Query.bDefenderPaired)
	{
		Decision.Outcome = EDefenseOutcome::Rejected;
		Decision.Reason = EDefenseReason::InvalidDefenderState;
		return Decision;
	}

	Decision.Outcome = EDefenseOutcome::GuardEntered;
	if (!Query.bHasSelectedThreat
		|| !Query.Attack.bAttackerAlive
		|| !Query.Attack.bAttackActive
		|| Query.Attack.bAttackerPaired
		|| !Query.Attack.bIsHostileToDefender
		|| Query.Attack.bIsFriendlyToDefender)
	{
		Decision.Reason = EDefenseReason::NoHostileCandidate;
		return Decision;
	}

	if (Query.Attack.bAttackConsumed)
	{
		Decision.Reason = EDefenseReason::Consumed;
		return Decision;
	}

	if (!Query.Attack.AttackInstance.IsValid() || !Query.Attack.bAttackIdentityCurrent)
	{
		Decision.Reason = EDefenseReason::StaleAttack;
		return Decision;
	}

	Decision.AlignmentPolicy = EDefenseAlignmentPolicy::GuardFacing;
	if (Query.Attack.AttackData == nullptr)
	{
		Decision.Reason = EDefenseReason::MissingParryCapability;
		return Decision;
	}

	if (!Query.Attack.ActiveParryWindow.IsValid()
		|| Query.Attack.ActiveParryWindow.Kind != EAttackWindowKind::Parry
		|| !(Query.Attack.ActiveParryWindow.AttackInstance == Query.Attack.AttackInstance))
	{
		Decision.Reason = EDefenseReason::NoParryWindow;
		return Decision;
	}

	if (!Query.Attack.AttackTags.HasTagExact(KatanaCombatGameplayTags::AttackDefenseParryable()))
	{
		Decision.Reason = EDefenseReason::MissingParryCapability;
		return Decision;
	}

	if (!Query.Attack.PredictedContact.bIsValid
		|| !Query.Attack.bHasCredibleIntent
		|| !Query.Defender.IsValid()
		|| Query.Attack.IntendedTarget != Query.Defender
		|| Query.Attack.PredictedContact.IntendedTarget != Query.Defender
		|| !Query.Attack.PredictedContact.bPathIntersectsThreatVolume
		|| Decision.PredictionConfidence != EDefensePredictionConfidence::High
		|| !FMath::IsFinite(Query.TimeToAlignmentDeadline)
		|| Query.TimeToAlignmentDeadline < 0.0f)
	{
		Decision.Reason = EDefenseReason::PredictionInsufficient;
		return Decision;
	}

	const FDefenseReachability PerfectReachability = FDefenseResolver::CalculateReachability(
		Query.RelativeYawDegrees,
		Query.TimeToAlignmentDeadline,
		Query.DefenseTurnRate,
		Query.PerfectParryFinalTolerance,
		Query.HardGuardConeHalfAngle,
		FMath::Min(Query.MaximumAutomaticTurn, Query.RemainingAutomaticTurn));
	Decision.AvailableTurnDegrees = PerfectReachability.AvailableTurn;

	if (!PerfectReachability.bWithinHardCone)
	{
		Decision.AlignmentPolicy = EDefenseAlignmentPolicy::None;
		Decision.Reason = EDefenseReason::OutsideHardCone;
		return Decision;
	}

	if (!PerfectReachability.bReachable)
	{
		Decision.Reason = EDefenseReason::PerfectAlignmentUnreachable;
		return Decision;
	}

	Decision.Outcome = EDefenseOutcome::PerfectParry;
	Decision.Reason = EDefenseReason::None;
	Decision.AttackerResponse = EAttackerResponse::ParryStagger;
	Decision.AlignmentPolicy = EDefenseAlignmentPolicy::PerfectParryBridge;
	Decision.bChainEligible = true;
	return Decision;
}

FDefenseDecision ResolveContact(const FDefenseQuery& Query)
{
	FDefenseDecision Decision = MakeBaseDecision(Query);
	Decision.RequiredFinalTolerance = Query.NormalBlockFinalTolerance;

	if (!Query.bContactIdentityValid
		|| !Query.bHasActualContact
		|| !Query.ActualContact.bIsValid
		|| !Query.bDefenderAlive
		|| !Query.Attack.bAttackerAlive)
	{
		Decision.Outcome = EDefenseOutcome::IgnoredInvalid;
		Decision.Reason = EDefenseReason::InvalidParticipant;
		return Decision;
	}

	if (Query.Attack.bIsFriendlyToDefender && !Query.bFriendlyFireEnabled)
	{
		Decision.Outcome = EDefenseOutcome::IgnoredFriendly;
		Decision.Reason = EDefenseReason::FriendlyFireDisabled;
		return Decision;
	}

	if (Query.Attack.bAttackConsumed
		|| (Query.Attack.AttackInstance.IsValid()
			&& (!Query.Attack.bAttackIdentityCurrent || !Query.Attack.bAttackActive)))
	{
		Decision.Outcome = EDefenseOutcome::IgnoredConsumed;
		Decision.Reason = EDefenseReason::Consumed;
		return Decision;
	}

	if (Query.bDefenderPaired || Query.Attack.bAttackerPaired)
	{
		Decision.Outcome = EDefenseOutcome::IgnoredInvalid;
		Decision.Reason = EDefenseReason::InvalidParticipant;
		return Decision;
	}

	if (!Query.bDefenderCanBeDamaged || Query.bDefenderInIFrames)
	{
		Decision.Outcome = EDefenseOutcome::IgnoredInvulnerable;
		Decision.Reason = EDefenseReason::Invulnerable;
		return Decision;
	}

	const bool bHasAuthoredAttack = Query.Attack.AttackData != nullptr;
	Decision.AttackerResponse = EAttackerResponse::Continue;
	if (bHasAuthoredAttack
		&& Query.Attack.AttackTags.HasTagExact(KatanaCombatGameplayTags::AttackPropertyUnblockable()))
	{
		Decision.Outcome = EDefenseOutcome::UnblockableHit;
		Decision.Reason = EDefenseReason::Unblockable;
		Decision.DamageDisposition = EDefenseDamageDisposition::ApplyRequestedDamage;
		return Decision;
	}

	if (!Query.bDefenderGuarding)
	{
		Decision.Outcome = EDefenseOutcome::Hit;
		Decision.Reason = EDefenseReason::NotGuarding;
		Decision.DamageDisposition = EDefenseDamageDisposition::ApplyRequestedDamage;
		return Decision;
	}

	const float ContactYaw = SanitizedYawMagnitude(Query.RelativeYawDegrees);
	const float BlockTolerance = FMath::IsFinite(Query.NormalBlockFinalTolerance)
		? FMath::Max(0.0f, Query.NormalBlockFinalTolerance)
		: 0.0f;
	if (ContactYaw > BlockTolerance)
	{
		Decision.Outcome = EDefenseOutcome::Hit;
		Decision.Reason = EDefenseReason::OutsideBlockTolerance;
		Decision.DamageDisposition = EDefenseDamageDisposition::ApplyRequestedDamage;
		return Decision;
	}

	Decision.Outcome = EDefenseOutcome::NormalBlock;
	Decision.DamageDisposition = EDefenseDamageDisposition::SuppressDamage;
	Decision.AlignmentPolicy = EDefenseAlignmentPolicy::BlockContact;
	if (bHasAuthoredAttack
		&& Query.Attack.AttackTags.HasTagExact(KatanaCombatGameplayTags::AttackDefenseBlockInterruptible()))
	{
		Decision.AttackerResponse = EAttackerResponse::Recoil;
	}
	return Decision;
}
}

FDefenseThreatSelectionResult FDefenseResolver::SelectThreat(
	const TArray<FAttackExecutionSnapshot>& Candidates,
	const FDefenseThreatSelectionContext& Context)
{
	int32 BestIndex = INDEX_NONE;
	int32 LockedIndex = INDEX_NONE;
	FThreatRank BestRank;

	for (int32 Index = 0; Index < Candidates.Num(); ++Index)
	{
		const FAttackExecutionSnapshot& Candidate = Candidates[Index];
		if (!IsSelectableThreat(Candidate))
		{
			continue;
		}

		if (Candidate.StableId == Context.LockedThreatId)
		{
			LockedIndex = Index;
		}

		const FThreatRank Rank = BuildThreatRank(Candidate, Context);
		if (BestIndex == INDEX_NONE || IsBetterThreat(Rank, BestRank))
		{
			BestIndex = Index;
			BestRank = Rank;
		}
	}

	if (BestIndex == INDEX_NONE)
	{
		return {};
	}

	int32 SelectedIndex = BestIndex;
	if (LockedIndex != INDEX_NONE && LockedIndex != BestIndex)
	{
		const float LockedDeadline = Candidates[LockedIndex].TimeToAlignmentDeadline;
		const float BestDeadline = Candidates[BestIndex].TimeToAlignmentDeadline;
		const bool bLockMinimumActive = Context.LockAgeSeconds < Context.ThreatLockMinSeconds;
		const bool bDeadlinesReliable = FMath::IsFinite(LockedDeadline) && LockedDeadline >= 0.0f
			&& FMath::IsFinite(BestDeadline) && BestDeadline >= 0.0f;
		const bool bNewThreatLeads = bDeadlinesReliable
			&& BestDeadline + FMath::Max(0.0f, Context.ThreatSwitchLeadSeconds) <= LockedDeadline;
		if (bLockMinimumActive || !bNewThreatLeads)
		{
			SelectedIndex = LockedIndex;
		}
	}

	FDefenseThreatSelectionResult Result;
	Result.bFound = true;
	Result.SourceCandidateIndex = SelectedIndex;
	Result.SelectedThreat = Candidates[SelectedIndex];
	return Result;
}

FDefenseDecision FDefenseResolver::Resolve(const FDefenseQuery& Query)
{
	return Query.Stage == EDefenseQueryStage::InputIntent
		? ResolveInputIntent(Query)
		: ResolveContact(Query);
}

FDefenseReachability FDefenseResolver::CalculateReachability(
	const float YawError,
	const float TimeToDeadline,
	const float TurnRate,
	const float FinalTolerance,
	const float HardCone,
	const float RemainingTurnBudget)
{
	FDefenseReachability Result;
	Result.AbsoluteYawError = SanitizedYawMagnitude(YawError);
	const float SafeDeadline = FMath::IsFinite(TimeToDeadline) ? FMath::Max(TimeToDeadline, 0.0f) : 0.0f;
	const float SafeTurnRate = FMath::IsFinite(TurnRate) ? FMath::Max(TurnRate, 0.0f) : 0.0f;
	const float SafeTolerance = FMath::IsFinite(FinalTolerance) ? FMath::Max(FinalTolerance, 0.0f) : 0.0f;
	const float SafeHardCone = FMath::IsFinite(HardCone) ? FMath::Clamp(HardCone, 0.0f, 180.0f) : 0.0f;
	const float SafeBudget = FMath::IsFinite(RemainingTurnBudget) ? FMath::Max(RemainingTurnBudget, 0.0f) : 0.0f;
	Result.AvailableTurn = FMath::Min(SafeBudget, SafeDeadline * SafeTurnRate);
	Result.bWithinHardCone = Result.AbsoluteYawError <= SafeHardCone + KINDA_SMALL_NUMBER;
	Result.bReachable = Result.bWithinHardCone
		&& Result.AbsoluteYawError <= SafeTolerance + Result.AvailableTurn + KINDA_SMALL_NUMBER;
	return Result;
}

FDefenseLaneResolution FDefenseResolver::ResolveIncomingLane(
	const FVector& WeaponVelocity,
	const FVector& TraceStart,
	const FVector& TraceEnd,
	const EIncomingAttackLane AuthoredLane,
	const FTransform& DefenderTransform,
	const float CenterLaneHalfAngle)
{
	if (DefenderTransform.ContainsNaN())
	{
		return {AuthoredLane, EDefenseLaneProvenance::AuthoredFallback, FVector::ZeroVector};
	}

	auto IsUsableTrajectory = [](const FVector& Value)
	{
		return !Value.ContainsNaN() && Value.SizeSquared2D() > KINDA_SMALL_NUMBER;
	};

	FVector Trajectory = FVector::ZeroVector;
	EDefenseLaneProvenance Provenance = EDefenseLaneProvenance::AuthoredFallback;
	if (IsUsableTrajectory(WeaponVelocity))
	{
		Trajectory = WeaponVelocity;
		Provenance = EDefenseLaneProvenance::WeaponVelocity;
	}
	else if (!TraceStart.ContainsNaN() && !TraceEnd.ContainsNaN())
	{
		const FVector TraceTrajectory = TraceEnd - TraceStart;
		if (IsUsableTrajectory(TraceTrajectory))
		{
			Trajectory = TraceTrajectory;
			Provenance = EDefenseLaneProvenance::TraceSegment;
		}
	}

	if (Provenance == EDefenseLaneProvenance::AuthoredFallback)
	{
		return {AuthoredLane, Provenance, FVector::ZeroVector};
	}

	Trajectory.Z = 0.0f;
	if (!Trajectory.Normalize())
	{
		return {AuthoredLane, EDefenseLaneProvenance::AuthoredFallback, FVector::ZeroVector};
	}

	const FVector LocalTrajectory = DefenderTransform.InverseTransformVectorNoScale(Trajectory);
	if (LocalTrajectory.ContainsNaN() || LocalTrajectory.SizeSquared2D() <= KINDA_SMALL_NUMBER)
	{
		return {AuthoredLane, EDefenseLaneProvenance::AuthoredFallback, FVector::ZeroVector};
	}

	const float SignedAngle = FMath::RadiansToDegrees(FMath::Atan2(LocalTrajectory.Y, FMath::Abs(LocalTrajectory.X)));
	if (!FMath::IsFinite(SignedAngle))
	{
		return {AuthoredLane, EDefenseLaneProvenance::AuthoredFallback, FVector::ZeroVector};
	}

	const float SafeCenterHalfAngle = FMath::IsFinite(CenterLaneHalfAngle)
		? FMath::Clamp(FMath::Abs(CenterLaneHalfAngle), 0.0f, 90.0f)
		: 0.0f;
	const EIncomingAttackLane Lane = FMath::Abs(SignedAngle) <= SafeCenterHalfAngle
		? EIncomingAttackLane::Center
		: SignedAngle > 0.0f
			? EIncomingAttackLane::Right
			: EIncomingAttackLane::Left;
	return {Lane, Provenance, Trajectory};
}

float FDefenseResolver::CalculateDefenderRelativeYaw(
	const FTransform& DefenderTransform,
	const FVector& SourceBearing)
{
	if (DefenderTransform.ContainsNaN()
		|| SourceBearing.ContainsNaN()
		|| SourceBearing.SizeSquared2D() <= KINDA_SMALL_NUMBER)
	{
		return TNumericLimits<float>::Max();
	}

	const FVector NormalizedBearing = SourceBearing.GetSafeNormal2D();
	const FVector LocalBearing = DefenderTransform.InverseTransformVectorNoScale(NormalizedBearing);
	if (NormalizedBearing.ContainsNaN()
		|| LocalBearing.ContainsNaN()
		|| LocalBearing.SizeSquared2D() <= KINDA_SMALL_NUMBER)
	{
		return TNumericLimits<float>::Max();
	}

	return SanitizedSignedYaw(
		FMath::RadiansToDegrees(FMath::Atan2(LocalBearing.Y, LocalBearing.X)));
}
