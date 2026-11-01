// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Data/AttackTimingDerivationTypes.h"

class UAnimMontage;
class UAttackData;

/**
 * Reads an attack's montage section and writes the timing its notifies describe into UAttackData::ManualTiming,
 * the timing notify generation uses, so the data matches the animation without hand entry.
 *
 * UObject layer over UAttackTimingDerivationLibrary, shaped like FAttackDataNotifyGenerationService: static, with no
 * editor-time state. Section bounds come from UAttackData::GetSectionTimeRange and the hold rule from the notify
 * generation service, the sources asset validation and generation use, so the three cannot disagree on which
 * notifies belong to the attack.
 */
class KATANACOMBATEDITOR_API FAttackTimingDerivationService
{
public:
	/** Timing markers for every phase transition, hold start and deprecated phase state on the montage timeline. */
	static TArray<FAttackTimingNotifyMarker> CollectTimingMarkers(const UAnimMontage* Montage);

	/** The hold rule notify generation applies to this attack. */
	static FAttackTimingHoldRule MakeHoldRule(const UAttackData* AttackData);

	/** Read-only: derive the attack's timing from its montage section. Modifies nothing. */
	static FAttackTimingDerivation DeriveTimingFromMontage(const UAttackData* AttackData);

	/**
	 * Derive the attack's timing and write it into ManualTiming in an undoable transaction, marking the asset dirty.
	 * Writes nothing when the derivation has errors or when notify generation's own analysis would reject the
	 * derived timing; writes nothing and opens no transaction when the timing already matches. Inside an enclosing
	 * transaction the write joins it, so one undo reverts a batch.
	 */
	static FAttackTimingApplyResult ApplyTimingFromMontage(UAttackData* AttackData);

	/** Apply to each attack under one transaction, so a single undo reverts every write. */
	static TArray<FAttackTimingApplyResult> BatchApplyTimingFromMontage(TConstArrayView<UAttackData*> Attacks);

	/** A readable account of one result: every field before and after, warnings, and the reasons for a refusal. */
	static FString DescribeResult(const FAttackTimingApplyResult& Result);

	/** A readable account of several results, with counts first. */
	static FString DescribeResults(TConstArrayView<FAttackTimingApplyResult> Results);
};
