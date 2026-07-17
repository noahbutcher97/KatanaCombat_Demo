// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "CombatTypes.h"
#include "Engine/DataAsset.h"
#include "DefenseConfiguration.generated.h"

USTRUCT(BlueprintType)
struct FDefenseBoneHeightRow
{
	GENERATED_BODY()

	FDefenseBoneHeightRow() = default;
	FDefenseBoneHeightRow(const FName InBoneName, const EAttackHeight InHeight)
		: BoneName(InBoneName)
		, Height(InHeight)
	{
	}

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Defense")
	FName BoneName = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Defense")
	EAttackHeight Height = EAttackHeight::Middle;
};

USTRUCT(BlueprintType)
struct KATANACOMBAT_API FDefensePresentationRow
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Defense")
	FName RowName = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Defense")
	EDefenseOutcome Outcome = EDefenseOutcome::NormalBlock;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Defense")
	bool bMatchAnyHeight = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Defense",
		meta = (EditCondition = "!bMatchAnyHeight"))
	EAttackHeight Height = EAttackHeight::Middle;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Defense")
	bool bMatchAnyLane = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Defense",
		meta = (EditCondition = "!bMatchAnyLane"))
	EIncomingAttackLane Lane = EIncomingAttackLane::Center;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Defense")
	bool bMatchAnySwingShape = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Defense",
		meta = (EditCondition = "!bMatchAnySwingShape"))
	ESwingDirection SwingShape = ESwingDirection::Horizontal;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Defense")
	FGameplayTagContainer RequiredTags;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Defense")
	FGameplayTagContainer ExcludedTags;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Defense")
	int32 Priority = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Defense")
	FDefensePresentationPayload Payload;

	int32 GetExactFieldCount() const;
	bool IsGenericFallback() const;
};

USTRUCT(BlueprintType)
struct KATANACOMBAT_API FAttackerResponsePresentationRow
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Defense")
	FName RowName = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Defense")
	EAttackerResponse Response = EAttackerResponse::None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Defense")
	bool bMatchAnyHeight = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Defense",
		meta = (EditCondition = "!bMatchAnyHeight"))
	EAttackHeight Height = EAttackHeight::Middle;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Defense")
	bool bMatchAnyLane = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Defense",
		meta = (EditCondition = "!bMatchAnyLane"))
	EIncomingAttackLane Lane = EIncomingAttackLane::Center;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Defense")
	bool bMatchAnySwingShape = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Defense",
		meta = (EditCondition = "!bMatchAnySwingShape"))
	ESwingDirection SwingShape = ESwingDirection::Horizontal;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Defense")
	FGameplayTagContainer RequiredTags;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Defense")
	FGameplayTagContainer ExcludedTags;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Defense")
	int32 Priority = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Defense")
	FDefensePresentationPayload Payload;

	int32 GetExactFieldCount() const;
	bool IsGenericFallback() const;
};

UCLASS(BlueprintType)
class KATANACOMBAT_API UDefenseConfiguration : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UDefenseConfiguration();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Defense|Alignment", meta = (ClampMin = "0.0", ClampMax = "180.0"))
	float HardGuardConeHalfAngle = 70.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Defense|Alignment", meta = (ClampMin = "0.0", ClampMax = "180.0"))
	float MaximumAutomaticTurn = 70.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Defense|Alignment", meta = (ClampMin = "0.0"))
	float DefenseTurnRate = 180.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Defense|Alignment", meta = (ClampMin = "0.0", ClampMax = "180.0"))
	float NormalBlockFinalTolerance = 35.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Defense|Alignment", meta = (ClampMin = "0.0", ClampMax = "180.0"))
	float PerfectParryFinalTolerance = 10.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Defense|Direction", meta = (ClampMin = "0.0", ClampMax = "90.0"))
	float CenterLaneHalfAngle = 12.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Defense|Threat", meta = (ClampMin = "0.0"))
	float ThreatLockMinSeconds = 0.15f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Defense|Threat", meta = (ClampMin = "0.0"))
	float ThreatSwitchLeadSeconds = 0.10f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Defense|Threat", meta = (ClampMin = "0.0"))
	float GuardedThreatRefreshSeconds = 0.05f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Defense|Threat", meta = (ClampMin = "0.0"))
	float MaximumHighConfidencePredictionAge = 0.10f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Defense|Threat", meta = (ClampMin = "0.0"))
	float DefenseThreatRange = 1000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Defense|Threat", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float GuardManualOverrideThreshold = 0.25f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Defense|Threat", meta = (ClampMin = "0.0"))
	float GuardAutoFacingResumeSeconds = 0.10f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Defense|Interaction", meta = (ClampMin = "0.0"))
	float InteractionTombstoneSeconds = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Defense|Interaction", meta = (ClampMin = "1"))
	int32 TerminalInteractionCacheCap = 128;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Defense|Sequence", meta = (ClampMin = "0.0"))
	float NoMontageParryBridgeSeconds = 0.15f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Defense|Sequence", meta = (ClampMin = "0.0"))
	float ParryStaggerDuration = 1.5f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Defense|Sequence", meta = (ClampMin = "0.0"))
	float CounterWindowSeconds = 2.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Defense|Sequence", meta = (ClampMin = "0.0"))
	float FinisherReadySeconds = 2.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Defense|Sequence", meta = (ClampMin = "0.0"))
	float TimeDilationLeaseWatchdogSeconds = 10.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Defense|Translation", meta = (ClampMin = "0.0"))
	float NormalBlockTranslationAllowance = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Defense|Translation", meta = (ClampMin = "0.0"))
	float NormalBlockTranslationDriftTolerance = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Defense|Translation", meta = (ClampMin = "0.0"))
	float PerfectParryTranslationAllowancePerRole = 75.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Defense|Skeleton")
	TArray<FDefenseBoneHeightRow> BoneHeightRows;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Defense|Guard")
	TObjectPtr<UAnimMontage> GuardEnterMontage = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Defense|Guard")
	TObjectPtr<UAnimMontage> GuardExitMontage = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Defense|Impact")
	FImpactAudioConfig DefaultBlockImpactAudio;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Defense|Impact")
	FImpactVFXConfig DefaultBlockImpactVFX;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Defense|Impact")
	FImpactAudioConfig DefaultParryImpactAudio;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Defense|Impact")
	FImpactVFXConfig DefaultParryImpactVFX;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Defense|Presentation")
	TArray<FDefensePresentationRow> DefenderPresentationRows;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Defense|Presentation")
	TArray<FAttackerResponsePresentationRow> AttackerResponseRows;

	FDefenseHeightResolution ResolveHeight(
		FName HitBone,
		const TArray<FName>& ParentBoneChain,
		EAttackHeight AuthoredHeight) const;
};
