// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "CombatTypes.h"

class UAnimMontage;
class UAttackData;

struct FAttackDataNotifyAnalysis
{
	const UAttackData* AttackData = nullptr;
	UAnimMontage* Montage = nullptr;
	FName SectionName = NAME_None;
	float SectionStart = 0.0f;
	float SectionEnd = 0.0f;
	float SectionLength = 0.0f;
	float WindupDuration = 0.0f;
	float ActiveDuration = 0.0f;
	float RecoveryDuration = 0.0f;
	float TimingTotal = 0.0f;
	float HoldWindowStart = 0.0f;
	float ActiveTransitionTime = 0.0f;
	float RecoveryTransitionTime = 0.0f;
	float HoldStartTime = 0.0f;
	bool bValid = false;
	bool bShouldHaveHoldStart = false;
	bool bHasActiveTransition = false;
	bool bHasRecoveryTransition = false;
	bool bHasHoldStart = false;
	bool bHasParryWindow = false;
	bool bHasCounterWindow = false;
	bool bCounterVariantHasData = false;
	bool bFinisherHasData = false;
	TArray<FString> LegacyNotifiesFound;
	TArray<FString> CanonicalNotifiesFound;
	TArray<FString> StaleCanonicalNotifiesFound;
	TArray<FString> CanonicalNotifiesMissing;
	TArray<FString> BranchReadinessWarnings;
	TArray<FString> Errors;
	TArray<int32> LegacyNotifyIndices;
	TArray<int32> CanonicalNotifyIndices;
	TArray<int32> StaleCanonicalNotifyIndices;
};

struct FAttackDataNotifyPlan
{
	UAnimMontage* Montage = nullptr;
	FName SectionName = NAME_None;
	TArray<int32> RemovalNotifyIndices;
	TArray<FString> PlannedRemovals;
	TArray<FString> PlannedAdditions;
	TArray<FString> Errors;
	float ActiveTransitionTime = 0.0f;
	float RecoveryTransitionTime = 0.0f;
	float HoldStartTime = 0.0f;
	bool bValid = false;
	bool bAddActiveTransition = false;
	bool bAddRecoveryTransition = false;
	bool bAddHoldStart = false;
	EInputType HoldInputType = EInputType::LightAttack;

	bool HasChanges() const
	{
		return RemovalNotifyIndices.Num() > 0 || PlannedAdditions.Num() > 0;
	}
};

class KATANACOMBATEDITOR_API FAttackDataNotifyGenerationService
{
public:
	static FAttackDataNotifyAnalysis AnalyzeAttackDataNotifies(const UAttackData* AttackData);
	static FAttackDataNotifyPlan BuildAttackDataNotifyPlan(const FAttackDataNotifyAnalysis& Analysis, bool bRegenerateCanonicalNotifies = false);
	static bool ApplyAttackDataNotifyPlan(UAttackData* AttackData, const FAttackDataNotifyPlan& Plan);
	static bool ShouldGenerateHoldWindowStart(const UAttackData* AttackData);
};
