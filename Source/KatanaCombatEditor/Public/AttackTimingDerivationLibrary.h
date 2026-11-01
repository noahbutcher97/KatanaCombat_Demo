// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Data/AttackTimingDerivationTypes.h"
#include "AttackTimingDerivationLibrary.generated.h"

/**
 * Derives an attack's notify-generation timing from the notifies its montage section plays - PURE MATH ONLY.
 *
 * Takes timing markers (plain data read from the montage) and the section bounds; returns durations or the
 * reasons they cannot be derived. No UObjects, no side effects. FAttackTimingDerivationService reads the
 * montage and writes the asset.
 *
 * The derived fields reproduce what notify generation would write back to the same section:
 * - WindupDuration:   section start to the AnimNotify_AttackPhaseTransition to Active
 * - ActiveDuration:   Active transition to the transition to Recovery
 * - RecoveryDuration: Recovery transition to the section end
 * - HoldWindowStart:  section start to the earliest AnimNotify_HoldWindowStart checking the input generation writes
 * HoldWindowDuration has no montage source and is never derived.
 */
UCLASS()
class KATANACOMBATEDITOR_API UAttackTimingDerivationLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** Two derived or stored timing values closer than this are the same value. */
	static constexpr float TimingEqualityToleranceSeconds = 1.0e-4f;

	/**
	 * Derive timing from a montage's notifies for the section [SectionStart, SectionEnd).
	 * Only notifies whose trigger time falls in that range are read, the same filter asset validation and notify
	 * generation use. The phases need exactly one transition to Active and one to Recovery, in that order, after the
	 * section start. When the hold rule says the attack uses a hold, the earliest hold notify checking the generated
	 * input supplies HoldWindowStart; for a charged heavy every hold notify in the section, of any input, must also
	 * start before Active, because any of them can start the charge.
	 *
	 * @param Notifies     Timing markers from the whole montage, in any order
	 * @param SectionStart Montage time where the attack's section starts
	 * @param SectionEnd   Montage time where the next section starts, or the montage end
	 * @param HoldRule     Whether and how the attack uses a hold
	 * @return Derived durations, or errors explaining why they cannot be derived, plus warnings about notifies the
	 *         timing fields cannot describe
	 */
	UFUNCTION(BlueprintPure, Category = "Attack Data Tools|Timing")
	static FAttackTimingDerivation DeriveAttackTiming(
		const TArray<FAttackTimingNotifyMarker>& Notifies,
		float SectionStart,
		float SectionEnd,
		const FAttackTimingHoldRule& HoldRule);

	/**
	 * Return CurrentTiming with the derived fields replaced. Fields the derivation did not produce (HoldWindowDuration
	 * always, HoldWindowStart when the attack uses no hold) keep their current values. An invalid derivation changes
	 * nothing.
	 */
	UFUNCTION(BlueprintPure, Category = "Attack Data Tools|Timing")
	static FAttackPhaseTimingOverride MergeDerivedTiming(
		const FAttackPhaseTimingOverride& CurrentTiming,
		const FAttackTimingDerivation& Derivation);

	/** Whether every field of A and B agrees within TimingEqualityToleranceSeconds. */
	UFUNCTION(BlueprintPure, Category = "Attack Data Tools|Timing")
	static bool IsSameTiming(const FAttackPhaseTimingOverride& A, const FAttackPhaseTimingOverride& B);
};
