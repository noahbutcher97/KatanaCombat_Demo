// Copyright Epic Games, Inc. All Rights Reserved.

#include "Data/PairedAnimationData.h"
#include "Animation/AnimMontage.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

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

    // Check attacker montage
    if (!AttackerMontage)
    {
        Context.AddError(FText::FromString(TEXT("AttackerMontage is required")));
        Result = EDataValidationResult::Invalid;
    }

    // Check victim montage
    if (!VictimMontage)
    {
        Context.AddError(FText::FromString(TEXT("VictimMontage is required for paired animations")));
        Result = EDataValidationResult::Invalid;
    }

    // Validate sync point timing
    if (AttackerMontage && SyncPointTime > AttackerMontage->GetPlayLength())
    {
        Context.AddError(FText::FromString(FString::Printf(
            TEXT("SyncPointTime (%.2f) exceeds AttackerMontage length (%.2f)"),
            SyncPointTime, AttackerMontage->GetPlayLength())));
        Result = EDataValidationResult::Invalid;
    }

    // Validate distance configuration
    if (MinTriggerDistance >= MaxTriggerDistance)
    {
        Context.AddError(FText::FromString(TEXT("MinTriggerDistance must be less than MaxTriggerDistance")));
        Result = EDataValidationResult::Invalid;
    }

    if (MaxWarpDistance < (MaxTriggerDistance - MinTriggerDistance))
    {
        Context.AddWarning(FText::FromString(TEXT("MaxWarpDistance may be too small for trigger distance range")));
    }

    // Validate slow motion settings
    if (bApplySlowMotion && SlowMotionScale >= 1.0f)
    {
        Context.AddWarning(FText::FromString(TEXT("SlowMotionScale >= 1.0 will not produce slow motion effect")));
    }

    // Validate blend times
    if (AttackerMontage && (AttackerBlendIn + AttackerBlendOut) > AttackerMontage->GetPlayLength())
    {
        Context.AddWarning(FText::FromString(TEXT("Attacker blend times exceed montage length")));
    }

    if (VictimMontage && (VictimBlendIn + VictimBlendOut) > VictimMontage->GetPlayLength())
    {
        Context.AddWarning(FText::FromString(TEXT("Victim blend times exceed montage length")));
    }

    // Check animation name
    if (AnimationName.IsNone())
    {
        Context.AddWarning(FText::FromString(TEXT("AnimationName should be set for TMap lookups")));
    }

    return Result;
}
#endif
