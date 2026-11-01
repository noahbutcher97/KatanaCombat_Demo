// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "CombatTypes.h"
#include "AttackTimingDerivationTypes.generated.h"

/**
 * Data types for deriving an attack's notify-generation timing (UAttackData::ManualTiming) from the notifies
 * its montage section actually plays. Pure data: the math lives in UAttackTimingDerivationLibrary and the
 * UObject reads and writes in FAttackTimingDerivationService.
 */

/** What a montage notify means to timing derivation. Notifies of any other class are not passed in. */
UENUM(BlueprintType)
enum class EAttackTimingNotifyKind : uint8
{
	/** AnimNotify_AttackPhaseTransition to Active: Windup ends here. */
	ActiveTransition,
	/** AnimNotify_AttackPhaseTransition to Recovery: Active ends here. */
	RecoveryTransition,
	/** AnimNotify_AttackPhaseTransition to any other phase (None or Windup). Never a timing source. */
	OtherPhaseTransition,
	/** AnimNotify_HoldWindowStart: the button-state check that starts a hold or a charge. */
	HoldWindowStart,
	/** Deprecated AnimNotifyState_AttackPhase. Never a timing source; reported so a failed derivation explains itself. */
	LegacyPhaseState
};

/** Why a derivation failed, or what it could not describe. */
UENUM(BlueprintType)
enum class EAttackTimingDerivationIssue : uint8
{
	/** No attack data was given. */
	MissingAttackData,
	/** The attack has no montage. */
	MissingMontage,
	/** The attack names a montage section the montage does not have. */
	MissingSection,
	/** The section's end is not after its start. */
	InvalidSectionRange,
	/** The section has no transition to Active, so Windup has no end. */
	MissingActiveTransition,
	/** The section has no transition to Recovery, so Active has no end. */
	MissingRecoveryTransition,
	/** More than one transition to Active; the timing fields describe a single Active window. */
	MultipleActiveTransitions,
	/** More than one transition to Recovery; the timing fields describe a single Active window. */
	MultipleRecoveryTransitions,
	/** Active starts at the section start, so Windup would be zero; notify generation needs a positive Windup. */
	ActiveAtSectionStart,
	/** Recovery does not come after Active, so Active would be zero or negative. */
	RecoveryNotAfterActive,
	/** The attack uses a hold, but no hold notify in the section checks the input that generation writes. */
	MissingHoldWindowStart,
	/** A charged heavy's hold notify starts at or after Active; any hold notify can start the charge. */
	ChargedHoldNotBeforeActive,
	/** Warning: a hold notify the timing fields do not describe; regenerating notifies removes it. */
	UndescribedHoldWindowStart,
	/** Warning: the section has hold notifies, but this attack does not use a hold. */
	HoldWindowStartNotUsed,
	/** Warning: a phase transition to a phase other than Active or Recovery; regenerating notifies removes it. */
	OtherPhaseTransition,
	/** Warning: HoldWindowDuration has no source in the montage, because a hold start is a point event. */
	HoldWindowDurationNotDerived,
	/** Warning: a deprecated AnimNotifyState_AttackPhase in the section; it is not read as a timing source. */
	LegacyPhaseStateIgnored,
	/** Notify generation's own preflight rejects the derived timing, so writing it would not let notifies regenerate. */
	NotifyGenerationRefuses
};

/** One problem or note from a derivation, with a message that names the times involved. */
USTRUCT(BlueprintType)
struct KATANACOMBATEDITOR_API FAttackTimingDerivationIssue
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Attack Timing")
	EAttackTimingDerivationIssue Code = EAttackTimingDerivationIssue::MissingActiveTransition;

	UPROPERTY(BlueprintReadOnly, Category = "Attack Timing")
	FString Message;
};

/** One timing-relevant montage notify, read from the montage timeline. */
USTRUCT(BlueprintType)
struct KATANACOMBATEDITOR_API FAttackTimingNotifyMarker
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attack Timing")
	EAttackTimingNotifyKind Kind = EAttackTimingNotifyKind::ActiveTransition;

	/** Montage time at which the notify fires (FAnimNotifyEvent::GetTriggerTime). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attack Timing")
	float TriggerTime = 0.0f;

	/** For OtherPhaseTransition: the phase the notify transitions to. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attack Timing")
	EAttackPhase TransitionPhase = EAttackPhase::None;

	/** For HoldWindowStart: the input whose held state the notify checks. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attack Timing")
	EInputType HoldInputType = EInputType::None;

	static FAttackTimingNotifyMarker Transition(const EAttackPhase Phase, const float Time)
	{
		FAttackTimingNotifyMarker Marker;
		Marker.Kind = Phase == EAttackPhase::Active
			? EAttackTimingNotifyKind::ActiveTransition
			: Phase == EAttackPhase::Recovery
				? EAttackTimingNotifyKind::RecoveryTransition
				: EAttackTimingNotifyKind::OtherPhaseTransition;
		Marker.TriggerTime = Time;
		Marker.TransitionPhase = Phase;
		return Marker;
	}

	static FAttackTimingNotifyMarker Hold(const EInputType InputType, const float Time)
	{
		FAttackTimingNotifyMarker Marker;
		Marker.Kind = EAttackTimingNotifyKind::HoldWindowStart;
		Marker.TriggerTime = Time;
		Marker.HoldInputType = InputType;
		return Marker;
	}

	static FAttackTimingNotifyMarker LegacyPhase(const EAttackPhase Phase, const float Time)
	{
		FAttackTimingNotifyMarker Marker;
		Marker.Kind = EAttackTimingNotifyKind::LegacyPhaseState;
		Marker.TriggerTime = Time;
		Marker.TransitionPhase = Phase;
		return Marker;
	}
};

/** How the attack uses a hold, which decides whether and how HoldWindowStart is derived. */
USTRUCT(BlueprintType)
struct KATANACOMBATEDITOR_API FAttackTimingHoldRule
{
	GENERATED_BODY()

	/** Whether notify generation writes a hold notify for this attack (a light attack that can hold, or a charged heavy). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attack Timing")
	bool bUsesHold = false;

	/** The input of the hold notify that generation writes; HoldWindowStart is derived from a notify checking it. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attack Timing")
	EInputType GeneratedHoldInput = EInputType::LightAttack;

	/**
	 * A charged heavy: its hold jumps to the charge loop, so every hold notify in the section, of any input, must
	 * start strictly before the section's Active transition (the rule UAttackData::ValidateChargedHoldOrdering applies).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attack Timing")
	bool bHoldMustPrecedeActive = false;
};

/**
 * Timing derived from a montage section's notifies. Durations are section-relative, in the units of
 * FAttackPhaseTimingOverride. A field is meaningful only when its flag says it was derived; nothing is guessed.
 */
USTRUCT(BlueprintType)
struct KATANACOMBATEDITOR_API FAttackTimingDerivation
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Attack Timing")
	float SectionStart = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Attack Timing")
	float SectionEnd = 0.0f;

	/** True when Windup, Active and Recovery were all derived. */
	UPROPERTY(BlueprintReadOnly, Category = "Attack Timing")
	bool bHasPhaseTiming = false;

	/** Section start to the Active transition. */
	UPROPERTY(BlueprintReadOnly, Category = "Attack Timing")
	float WindupDuration = 0.0f;

	/** Active transition to the Recovery transition. */
	UPROPERTY(BlueprintReadOnly, Category = "Attack Timing")
	float ActiveDuration = 0.0f;

	/** Recovery transition to the section end. */
	UPROPERTY(BlueprintReadOnly, Category = "Attack Timing")
	float RecoveryDuration = 0.0f;

	/** True when HoldWindowStart was derived; false when the attack does not use a hold or the hold could not be found. */
	UPROPERTY(BlueprintReadOnly, Category = "Attack Timing")
	bool bHasHoldWindowStart = false;

	/** Section start to the hold notify that generation would reproduce. */
	UPROPERTY(BlueprintReadOnly, Category = "Attack Timing")
	float HoldWindowStart = 0.0f;

	/** Problems that stop the derivation; when any exist, nothing may be written. */
	UPROPERTY(BlueprintReadOnly, Category = "Attack Timing")
	TArray<FAttackTimingDerivationIssue> Errors;

	/** Notes about what the timing fields cannot describe; they do not stop the derivation. */
	UPROPERTY(BlueprintReadOnly, Category = "Attack Timing")
	TArray<FAttackTimingDerivationIssue> Warnings;

	bool IsValid() const { return Errors.IsEmpty() && bHasPhaseTiming; }

	bool HasError(const EAttackTimingDerivationIssue Code) const
	{
		return Errors.ContainsByPredicate([Code](const FAttackTimingDerivationIssue& Issue) { return Issue.Code == Code; });
	}

	bool HasWarning(const EAttackTimingDerivationIssue Code) const
	{
		return Warnings.ContainsByPredicate([Code](const FAttackTimingDerivationIssue& Issue) { return Issue.Code == Code; });
	}

	void AddError(const EAttackTimingDerivationIssue Code, FString Message)
	{
		FAttackTimingDerivationIssue& Issue = Errors.AddDefaulted_GetRef();
		Issue.Code = Code;
		Issue.Message = MoveTemp(Message);
	}

	void AddWarning(const EAttackTimingDerivationIssue Code, FString Message)
	{
		FAttackTimingDerivationIssue& Issue = Warnings.AddDefaulted_GetRef();
		Issue.Code = Code;
		Issue.Message = MoveTemp(Message);
	}
};

/** What applying derived timing to an attack did. */
UENUM(BlueprintType)
enum class EAttackTimingApplyOutcome : uint8
{
	/** The derived timing was written in an undoable transaction and the asset was marked dirty. */
	Applied,
	/** The attack's timing already matches its montage; nothing was written. */
	Unchanged,
	/** The timing could not be derived or would not regenerate notifies; nothing was written. */
	Refused
};

/** The result of deriving one attack's timing from its montage and applying it. */
USTRUCT(BlueprintType)
struct KATANACOMBATEDITOR_API FAttackTimingApplyResult
{
	GENERATED_BODY()

	/** Path of the attack data asset. */
	UPROPERTY(BlueprintReadOnly, Category = "Attack Timing")
	FString AttackData;

	/** Name of the montage the timing was read from. */
	UPROPERTY(BlueprintReadOnly, Category = "Attack Timing")
	FString Montage;

	/** The attack's montage section (None means the whole montage). */
	UPROPERTY(BlueprintReadOnly, Category = "Attack Timing")
	FName Section = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "Attack Timing")
	EAttackTimingApplyOutcome Outcome = EAttackTimingApplyOutcome::Refused;

	/** What the montage says, including the reasons for a refusal. */
	UPROPERTY(BlueprintReadOnly, Category = "Attack Timing")
	FAttackTimingDerivation Derivation;

	/** ManualTiming before the action. */
	UPROPERTY(BlueprintReadOnly, Category = "Attack Timing")
	FAttackPhaseTimingOverride PreviousTiming;

	/** ManualTiming after the action; equal to PreviousTiming unless the outcome is Applied. */
	UPROPERTY(BlueprintReadOnly, Category = "Attack Timing")
	FAttackPhaseTimingOverride NewTiming;
};
