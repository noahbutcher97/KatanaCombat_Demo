// Copyright Epic Games, Inc. All Rights Reserved.

#include "Data/PairedAnimationData.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimNotify_ChainStageTransition.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

namespace
{
int32 CountMatchingChainMarkers(const UPairedAnimationData& Data)
{
	const UAnimMontage* DriverMontage =
		Data.ChainTransitionPolicy.DriverRole == EPairedAnimationRole::Attacker
		? Data.AttackerMontage.Get()
		: Data.VictimMontage.Get();
	if (!DriverMontage || Data.ChainTransitionPolicy.RequiredMarker.IsNone())
	{
		return 0;
	}

	const EChainStageTransitionType RequiredTransition =
		Data.ChainTransitionPolicy.bAutoContinue
		? EChainStageTransitionType::AutoContinue
		: EChainStageTransitionType::OpenCounterWindow;
	int32 Count = 0;
	for (const FAnimNotifyEvent& Event : DriverMontage->Notifies)
	{
		const UAnimNotify_ChainStageTransition* Notify =
			Cast<UAnimNotify_ChainStageTransition>(Event.Notify);
		if (Notify
			&& Notify->Transition == RequiredTransition
			&& Notify->MarkerName == Data.ChainTransitionPolicy.RequiredMarker)
		{
			++Count;
		}
	}
	return Count;
}

bool HasValidReadySections(const UPairedAnimationData& Data)
{
	return (Data.ChainTransitionPolicy.AttackerReadySection.IsNone()
			|| (Data.AttackerMontage
				&& Data.AttackerMontage->IsValidSectionName(
					Data.ChainTransitionPolicy.AttackerReadySection)))
		&& (Data.ChainTransitionPolicy.VictimReadySection.IsNone()
			|| (Data.VictimMontage
				&& Data.VictimMontage->IsValidSectionName(
					Data.ChainTransitionPolicy.VictimReadySection)));
}

bool HasValidNumericConfiguration(const UPairedAnimationData& Data)
{
	return FMath::IsFinite(Data.SyncPointTime)
		&& Data.SyncPointTime >= 0.0f
		&& FMath::IsFinite(Data.VictimStartOffset)
		&& !Data.VictimRelativePosition.ContainsNaN()
		&& !Data.VictimRelativeRotation.ContainsNaN()
		&& Data.VictimFacingMode >= -1
		&& Data.VictimFacingMode <= 1
		&& FMath::IsFinite(Data.AttackerBlendIn)
		&& Data.AttackerBlendIn >= 0.0f
		&& FMath::IsFinite(Data.AttackerBlendOut)
		&& Data.AttackerBlendOut >= 0.0f
		&& FMath::IsFinite(Data.VictimBlendIn)
		&& Data.VictimBlendIn >= 0.0f
		&& FMath::IsFinite(Data.VictimBlendOut)
		&& Data.VictimBlendOut >= 0.0f
		&& FMath::IsFinite(Data.MaxWarpDistance)
		&& Data.MaxWarpDistance >= 0.0f
		&& FMath::IsFinite(Data.MinTriggerDistance)
		&& Data.MinTriggerDistance >= 0.0f
		&& FMath::IsFinite(Data.MaxTriggerDistance)
		&& Data.MaxTriggerDistance > Data.MinTriggerDistance
		&& FMath::IsFinite(Data.AttackerWarpConfig.MaxWarpDistance)
		&& Data.AttackerWarpConfig.MaxWarpDistance >= 0.0f
		&& !Data.AttackerWarpConfig.RelativeOffset.ContainsNaN()
		&& FMath::IsFinite(Data.VictimWarpConfig.MaxWarpDistance)
		&& Data.VictimWarpConfig.MaxWarpDistance >= 0.0f
		&& !Data.VictimWarpConfig.RelativeOffset.ContainsNaN()
		&& FMath::IsFinite(Data.BaseDamage)
		&& Data.BaseDamage >= 0.0f
		&& FMath::IsFinite(Data.DamageMultiplier)
		&& Data.DamageMultiplier >= 0.0f
		&& static_cast<double>(Data.BaseDamage) * static_cast<double>(Data.DamageMultiplier)
			<= static_cast<double>(TNumericLimits<float>::Max())
		&& FMath::IsFinite(Data.ChainTransitionPolicy.ResponseWindowOverride)
		&& Data.ChainTransitionPolicy.ResponseWindowOverride >= 0.0f
		&& FMath::IsFinite(Data.RagdollBlendTime)
		&& Data.RagdollBlendTime >= 0.0f
		&& (!Data.bApplySlowMotion
			|| (FMath::IsFinite(Data.SlowMotionScale)
				&& Data.SlowMotionScale >= 0.0f
				&& Data.SlowMotionScale <= 1.0f
				&& FMath::IsFinite(Data.SlowMotionDuration)
				&& Data.SlowMotionDuration >= 0.0f));
}
}

UPairedAnimationData::UPairedAnimationData()
{
    // Default warp configs
    AttackerWarpConfig.WarpTargetName = "PairedTarget";
    AttackerWarpConfig.bWarpTranslation = false;  // Attacker usually stays in place
    AttackerWarpConfig.bWarpRotation = true;
    AttackerWarpConfig.bAdjustToTerrain = true;
    AttackerWarpConfig.MaxWarpDistance = 100.0f;

    VictimWarpConfig.WarpTargetName = "PairedTarget";
    VictimWarpConfig.bWarpTranslation = true;  // Victim warps to attacker
    VictimWarpConfig.bWarpRotation = true;
    VictimWarpConfig.bAdjustToTerrain = true;
    VictimWarpConfig.MaxWarpDistance = 300.0f;
}

bool UPairedAnimationData::IsValid() const
{
	if (!HasValidNumericConfiguration(*this))
	{
		return false;
	}

    // Must have at least the attacker montage
    if (!AttackerMontage)
    {
        return false;
    }

    // Victim montage is typically required but could be optional for some animations
    // (e.g., if victim just ragdolls)
    if (!VictimMontage)
    {
        return false;
    }

    // Sync point must be within attacker montage duration
    if (AttackerMontage && SyncPointTime > AttackerMontage->GetPlayLength())
    {
        return false;
    }

    // Distance validation
    if (MinTriggerDistance >= MaxTriggerDistance)
    {
        return false;
    }

	const bool bHasChainPolicy = !ChainTransitionPolicy.RequiredMarker.IsNone()
		|| ChainTransitionPolicy.bAutoContinue;
	if (bHasChainPolicy
		&& (CountMatchingChainMarkers(*this) != 1
			|| (!ChainTransitionPolicy.bAutoContinue
				&& !ChainTransitionPolicy.HasRetainableReadyPose())
			|| !HasValidReadySections(*this)
			|| !FMath::IsFinite(ChainTransitionPolicy.ResponseWindowOverride)
			|| ChainTransitionPolicy.ResponseWindowOverride < 0.0f))
	{
		return false;
	}

    return true;
}

FString UPairedAnimationData::GetDisplayName() const
{
    if (!AnimationName.IsNone())
    {
        return AnimationName.ToString();
    }

    // Fall back to asset name
    return GetName();
}

#if WITH_EDITOR
EDataValidationResult UPairedAnimationData::IsDataValid(FDataValidationContext& Context) const
{
    EDataValidationResult Result = Super::IsDataValid(Context);
	if (!HasValidNumericConfiguration(*this))
	{
		Context.AddError(FText::FromString(FString::Printf(
			TEXT("%s: Paired animation numeric values must be finite and within runtime bounds"),
			*GetName())));
		Result = EDataValidationResult::Invalid;
	}

    // Check attacker montage
    if (!AttackerMontage)
    {
        Context.AddError(FText::FromString(FString::Printf(
            TEXT("%s: AttackerMontage is required"), *GetName())));
        Result = EDataValidationResult::Invalid;
    }

    // Check victim montage
    if (!VictimMontage)
    {
        Context.AddError(FText::FromString(FString::Printf(
            TEXT("%s: VictimMontage is required for paired animations"), *GetName())));
        Result = EDataValidationResult::Invalid;
    }

    // Validate sync point timing
    if (AttackerMontage && SyncPointTime > AttackerMontage->GetPlayLength())
    {
        Context.AddError(FText::FromString(FString::Printf(
            TEXT("%s: SyncPointTime (%.2f) exceeds AttackerMontage length (%.2f)"),
            *GetName(), SyncPointTime, AttackerMontage->GetPlayLength())));
        Result = EDataValidationResult::Invalid;
    }

    // Validate distance configuration
    if (MinTriggerDistance >= MaxTriggerDistance)
    {
        Context.AddError(FText::FromString(FString::Printf(
            TEXT("%s: MinTriggerDistance must be less than MaxTriggerDistance"), *GetName())));
        Result = EDataValidationResult::Invalid;
    }

    if (MaxWarpDistance < (MaxTriggerDistance - MinTriggerDistance))
    {
        Context.AddWarning(FText::FromString(FString::Printf(
            TEXT("%s: MaxWarpDistance may be too small for trigger distance range"), *GetName())));
    }

	const bool bHasChainPolicy = !ChainTransitionPolicy.RequiredMarker.IsNone()
		|| ChainTransitionPolicy.bAutoContinue;
	if (bHasChainPolicy && CountMatchingChainMarkers(*this) != 1)
	{
		Context.AddError(FText::FromString(FString::Printf(
			TEXT("%s: Chain policy requires exactly one matching marker on its driver montage"),
			*GetName())));
		Result = EDataValidationResult::Invalid;
	}
	if (bHasChainPolicy
		&& !ChainTransitionPolicy.bAutoContinue
		&& !ChainTransitionPolicy.HasRetainableReadyPose())
	{
		Context.AddError(FText::FromString(FString::Printf(
			TEXT("%s: Counter-window handoff requires a reviewed ready section or terminal pose for both roles"),
			*GetName())));
		Result = EDataValidationResult::Invalid;
	}
	if (bHasChainPolicy && !HasValidReadySections(*this))
	{
		Context.AddError(FText::FromString(FString::Printf(
			TEXT("%s: Chain ready sections must exist on their corresponding role montages"),
			*GetName())));
		Result = EDataValidationResult::Invalid;
	}
	if (!FMath::IsFinite(ChainTransitionPolicy.ResponseWindowOverride)
		|| ChainTransitionPolicy.ResponseWindowOverride < 0.0f)
	{
		Context.AddError(FText::FromString(FString::Printf(
			TEXT("%s: Chain response-window override must be finite and nonnegative"),
			*GetName())));
		Result = EDataValidationResult::Invalid;
	}

    // Validate slow motion settings
    if (bApplySlowMotion && SlowMotionScale >= 1.0f)
    {
        Context.AddWarning(FText::FromString(FString::Printf(
            TEXT("%s: SlowMotionScale >= 1.0 will not produce slow motion effect"), *GetName())));
    }

    // Validate blend times
    if (AttackerMontage && (AttackerBlendIn + AttackerBlendOut) > AttackerMontage->GetPlayLength())
    {
        Context.AddWarning(FText::FromString(FString::Printf(
            TEXT("%s: Attacker blend times exceed montage length"), *GetName())));
    }

    if (VictimMontage && (VictimBlendIn + VictimBlendOut) > VictimMontage->GetPlayLength())
    {
        Context.AddWarning(FText::FromString(FString::Printf(
            TEXT("%s: Victim blend times exceed montage length"), *GetName())));
    }

    // Check animation name
    if (AnimationName.IsNone())
    {
        Context.AddWarning(FText::FromString(FString::Printf(
            TEXT("%s: AnimationName should be set for TMap lookups"), *GetName())));
    }

    return Result;
}
#endif
