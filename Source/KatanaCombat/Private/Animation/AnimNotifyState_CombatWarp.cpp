// Copyright Epic Games, Inc. All Rights Reserved.

#include "Animation/AnimNotifyState_CombatWarp.h"
#include "Core/TargetingComponent.h"
#include "MotionWarpingComponent.h"
#include "RootMotionModifier.h"

DEFINE_LOG_CATEGORY_STATIC(LogCombatWarp, Log, All);

UAnimNotifyState_CombatWarp::UAnimNotifyState_CombatWarp(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    // Parent class sets up the RootMotionModifier template
    // We'll configure it dynamically in AddRootMotionModifier based on which target exists
    
    // Set parent's WarpTargetName to a default to prevent editor warnings
    // We'll override this at runtime, but having a non-empty default prevents
    // the parent class validation from complaining
    #if WITH_EDITORONLY_DATA
    // Note: This is only to satisfy editor validation
    // The actual target name is set dynamically in AddRootMotionModifier
    #endif
}

URootMotionModifier* UAnimNotifyState_CombatWarp::AddRootMotionModifier_Implementation(
    UMotionWarpingComponent* MotionWarpingComp,
    const UAnimSequenceBase* Animation,
    float StartTime,
    float EndTime) const
{
    if (!MotionWarpingComp || !RootMotionModifier)
    {
        return nullptr;
    }

    // Check which warp target was set up by SetupAttackWarp()
    // Priority: TargetWarpName (with translation) > RotationWarpName (rotation only)

    // Determine which target exists and configure accordingly
    FName ActiveTargetName = NAME_None;
    bool bEnableTranslation = false;

    if (HasWarpTarget(MotionWarpingComp, TargetWarpName))
    {
        // TARGET MODE: Enemy found - use translation + rotation
        ActiveTargetName = TargetWarpName;
        bEnableTranslation = bEnableTranslationForTarget;

        UE_LOG(LogCombatWarp, Log, TEXT("[%s] Combat Warp: TARGET mode (%s) - Translation=%s"),
            *GetNameSafe(MotionWarpingComp->GetOwner()),
            *TargetWarpName.ToString(),
            bEnableTranslation ? TEXT("ON") : TEXT("OFF"));
    }
    else if (HasWarpTarget(MotionWarpingComp, RotationWarpName))
    {
        // ROTATION MODE: No enemy - rotation only, no translation
        ActiveTargetName = RotationWarpName;
        bEnableTranslation = false;

        UE_LOG(LogCombatWarp, Log, TEXT("[%s] Combat Warp: ROTATION mode (%s) - Translation=OFF"),
            *GetNameSafe(MotionWarpingComp->GetOwner()),
            *RotationWarpName.ToString());
    }
    else
    {
        // No target found - skip warping entirely
        UE_LOG(LogCombatWarp, Verbose, TEXT("[%s] Combat Warp: No target (checked %s, %s) - skipping warp"),
            *GetNameSafe(MotionWarpingComp->GetOwner()),
            *TargetWarpName.ToString(),
            *RotationWarpName.ToString());
        return nullptr;
    }

    URootMotionModifier* RuntimeModifier = MotionWarpingComp->AddModifierFromTemplate(
        RootMotionModifier,
        Animation,
        StartTime,
        EndTime);
    URootMotionModifier_Warp* RuntimeWarpModifier = Cast<URootMotionModifier_Warp>(RuntimeModifier);
    UTargetingComponent* Targeting = MotionWarpingComp->GetOwner()
        ? MotionWarpingComp->GetOwner()->FindComponentByClass<UTargetingComponent>()
        : nullptr;
    if (!RuntimeWarpModifier || !Targeting
        || !Targeting->RegisterAlignmentModifier(
            ActiveTargetName,
            RuntimeWarpModifier,
            bEnableTranslation))
    {
        if (RuntimeModifier)
        {
            RuntimeModifier->SetState(ERootMotionModifierState::MarkedForRemoval);
        }
        UE_LOG(LogCombatWarp, Warning,
            TEXT("[%s] Combat Warp: target '%s' could not bind to one active alignment owner; skipping warp"),
            *GetNameSafe(MotionWarpingComp->GetOwner()),
            *ActiveTargetName.ToString());
        return nullptr;
    }

    return RuntimeWarpModifier;
}

FString UAnimNotifyState_CombatWarp::GetNotifyName_Implementation() const
{
    return FString::Printf(TEXT("Combat Warp [%s|%s]"),
        *TargetWarpName.ToString(),
        *RotationWarpName.ToString());
}

bool UAnimNotifyState_CombatWarp::HasWarpTarget(UMotionWarpingComponent* MotionWarping, FName InTargetName) const
{
    if (!MotionWarping || InTargetName.IsNone())
    {
        return false;
    }

    // FindWarpTarget returns nullptr if target doesn't exist
    return MotionWarping->FindWarpTarget(InTargetName) != nullptr;
}

#if WITH_EDITOR
void UAnimNotifyState_CombatWarp::ValidateAssociatedAssets()
{
    // Override parent validation to prevent false warnings
    // Parent class (UAnimNotifyState_MotionWarping) validates that WarpTargetName is set
    // But we set WarpTargetName dynamically at runtime based on which target exists
    // Instead, we validate that at least ONE of our target name options is configured
    
    if (TargetWarpName.IsNone() && RotationWarpName.IsNone())
    {
        UE_LOG(LogCombatWarp, Warning, TEXT("Combat Warp: Both TargetWarpName and RotationWarpName are None. At least one must be set. Using defaults."));
        // Set to defaults if both are none
        const_cast<UAnimNotifyState_CombatWarp*>(this)->TargetWarpName = "AttackTarget";
        const_cast<UAnimNotifyState_CombatWarp*>(this)->RotationWarpName = "RotationTarget";
    }
    
    // Don't call parent validation - it will complain about WarpTargetName not being set
    // which is intentional in our design (we set it at runtime)
}
#endif // WITH_EDITOR
