// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "CombatTypes.h"

class KATANACOMBAT_API FDefenseResolver
{
public:
	/** Canonical pure predicate used by selection and threat telemetry. */
	static bool IsSelectableThreat(const FAttackExecutionSnapshot& Candidate);

	static FDefenseThreatSelectionResult SelectThreat(
		const TArray<FAttackExecutionSnapshot>& Candidates,
		const FDefenseThreatSelectionContext& Context);

	static FDefenseDecision Resolve(const FDefenseQuery& Query);

	static FDefenseReachability CalculateReachability(
		float YawError,
		float TimeToDeadline,
		float TurnRate,
		float FinalTolerance,
		float HardCone,
		float RemainingTurnBudget);

	static FDefenseLaneResolution ResolveIncomingLane(
		const FVector& WeaponVelocity,
		const FVector& TraceStart,
		const FVector& TraceEnd,
		EIncomingAttackLane AuthoredLane,
		const FTransform& DefenderTransform,
		float CenterLaneHalfAngle);

	static float CalculateDefenderRelativeYaw(
		const FTransform& DefenderTransform,
		const FVector& SourceBearing);
};
