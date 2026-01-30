// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "CombatTypes.h"
#include "PairedAnimationTypes.h"
#include "PairedAnimationData.generated.h"

class UAnimMontage;

/**
 * Data asset defining a paired animation sequence (finisher, counter, throw, etc.)
 * Contains configuration for both attacker and victim animations with sync points
 *
 * Design: AC3-inspired paired animation with motion warping integration
 * - Both characters play synced montages
 * - Victim warps to relative position from attacker
 * - Sync points trigger damage/effects at key frames
 * - Terrain adjustment prevents floating
 */
UCLASS(BlueprintType)
class KATANACOMBAT_API UPairedAnimationData : public UPrimaryDataAsset
{
    GENERATED_BODY()

public:
    UPairedAnimationData();

    // ========================================================================
    // IDENTIFICATION
    // ========================================================================

    /** Unique name for this paired animation (used in TMap lookups) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Identification")
    FName AnimationName;

    /** Type of paired reaction (Counter, Finisher, Parry, Throw) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Identification")
    EPairedReactionType ReactionType = EPairedReactionType::Finisher;

    /** Optional description for editor reference */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Identification", meta = (MultiLine = true))
    FString Description;

    // ========================================================================
    // ANIMATION REFERENCES
    // ========================================================================

    /** Montage played by the attacker (initiator) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animation")
    TObjectPtr<UAnimMontage> AttackerMontage;

    /** Montage played by the victim (receiver) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animation")
    TObjectPtr<UAnimMontage> VictimMontage;

    // ========================================================================
    // SYNC CONFIGURATION
    // ========================================================================

    /** Time in attacker montage when sync point is reached (damage application) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sync",
        meta = (ClampMin = "0.0", ClampMax = "10.0"))
    float SyncPointTime = 0.5f;

    /** Name identifier for the sync point (for AnimNotify lookup) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sync")
    FName SyncPointName = "Impact";

    /** Time offset for victim montage start relative to attacker (negative = victim starts later) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sync",
        meta = (ClampMin = "-2.0", ClampMax = "2.0"))
    float VictimStartOffset = 0.0f;

    // ========================================================================
    // BLEND TIMES
    // ========================================================================

    /** Blend-in time for attacker montage */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Blending",
        meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float AttackerBlendIn = 0.1f;

    /** Blend-out time for attacker montage */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Blending",
        meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float AttackerBlendOut = 0.2f;

    /** Blend-in time for victim montage (often faster for reactive animations) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Blending",
        meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float VictimBlendIn = 0.05f;

    /** Blend-out time for victim montage */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Blending",
        meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float VictimBlendOut = 0.2f;

    // ========================================================================
    // POSITIONING
    // ========================================================================

    /** Victim position relative to attacker at start of paired animation (local space) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Positioning")
    FVector VictimRelativePosition = FVector(100.0f, 0.0f, 0.0f);

    /** Whether victim should face toward (-1) or away from (1) attacker, or use fixed rotation (0) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Positioning",
        meta = (ClampMin = "-1", ClampMax = "1"))
    int32 VictimFacingMode = -1;  // -1 = face attacker, 1 = face away, 0 = use VictimRelativeRotation

    /** Fixed victim rotation relative to attacker (only used if VictimFacingMode == 0) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Positioning",
        meta = (EditCondition = "VictimFacingMode == 0"))
    FRotator VictimRelativeRotation = FRotator::ZeroRotator;

    /** Maximum distance victim can be warped to attacker position */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Positioning",
        meta = (ClampMin = "0.0", ClampMax = "1000.0"))
    float MaxWarpDistance = 400.0f;

    /** Minimum distance required to trigger paired animation */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Positioning",
        meta = (ClampMin = "0.0", ClampMax = "500.0"))
    float MinTriggerDistance = 50.0f;

    /** Maximum distance to trigger paired animation */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Positioning",
        meta = (ClampMin = "0.0", ClampMax = "1000.0"))
    float MaxTriggerDistance = 300.0f;

    // ========================================================================
    // MOTION WARPING
    // ========================================================================

    /** Warp configuration for attacker (usually just rotation) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Motion Warping")
    FPairedWarpConfig AttackerWarpConfig;

    /** Warp configuration for victim (usually translation + rotation) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Motion Warping")
    FPairedWarpConfig VictimWarpConfig;

    // ========================================================================
    // EFFECTS
    // ========================================================================

    /** Apply slow motion during this paired animation */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Effects")
    bool bApplySlowMotion = false;

    /** Slow motion time dilation scale (0.0 = paused, 1.0 = normal) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Effects",
        meta = (EditCondition = "bApplySlowMotion", ClampMin = "0.0", ClampMax = "1.0"))
    float SlowMotionScale = 0.3f;

    /** Duration of slow motion effect in real-time seconds */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Effects",
        meta = (EditCondition = "bApplySlowMotion", ClampMin = "0.0", ClampMax = "3.0"))
    float SlowMotionDuration = 0.5f;

    /** Camera shake to play during sync point */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Effects")
    TSubclassOf<UCameraShakeBase> ImpactCameraShake;

    // ========================================================================
    // DAMAGE
    // ========================================================================

    /** Base damage dealt at sync point (can be multiplied by attack data) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Damage",
        meta = (ClampMin = "0.0"))
    float BaseDamage = 100.0f;

    /** Damage multiplier applied on top of base damage */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Damage",
        meta = (ClampMin = "0.0", ClampMax = "10.0"))
    float DamageMultiplier = 1.0f;

    /** Whether this paired animation is lethal (kills regardless of remaining health) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Damage")
    bool bIsLethal = true;

    // ========================================================================
    // VALIDATION
    // ========================================================================

    /** Runtime validation - checks if this paired animation data is properly configured */
    UFUNCTION(BlueprintPure, Category = "Validation")
    bool IsValid() const;

    /** Get display name for this paired animation */
    UFUNCTION(BlueprintPure, Category = "Identification")
    FString GetDisplayName() const;

#if WITH_EDITOR
    /** Editor validation */
    virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif
};
