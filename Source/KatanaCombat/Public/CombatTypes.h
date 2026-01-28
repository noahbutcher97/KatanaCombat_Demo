
// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Components/SkeletalMeshComponent.h"
#include "CombatTypes.generated.h"

// Forward declarations
class UAttackData;
class UAnimMontage;
class AActor;

// ============================================================================
// ENUMS
// ============================================================================

/**
 * Combat state for the state machine
 */
UENUM(BlueprintType)
enum class ECombatState : uint8
{
    Idle                UMETA(DisplayName = "Idle"),
    Attacking           UMETA(DisplayName = "Attacking"),
    HoldingLightAttack  UMETA(DisplayName = "Holding Light Attack"),
    ChargingHeavyAttack UMETA(DisplayName = "Charging Heavy Attack"),
    Blocking            UMETA(DisplayName = "Blocking"),
    Parrying            UMETA(DisplayName = "Parrying"),
    GuardBroken         UMETA(DisplayName = "Guard Broken"),
    Finishing           UMETA(DisplayName = "Finishing"),
    HitStunned          UMETA(DisplayName = "Hit Stunned"),
    Evading             UMETA(DisplayName = "Evading"),
    Dead                UMETA(DisplayName = "Dead")
};

/**
 * Attack type classification
 */
UENUM(BlueprintType)
enum class EAttackType : uint8
{
    None            UMETA(DisplayName = "None"),
    Light           UMETA(DisplayName = "Light"),
    Heavy           UMETA(DisplayName = "Heavy"),
    Special         UMETA(DisplayName = "Special")
};

/**
 * Attack phase within animation
 * Phases are MUTUALLY EXCLUSIVE - only one active at a time
 * Controlled by AnimNotifyState_AttackPhase in montages
 *
 * NOTE: Hold/Combo/Parry/Cancel are WINDOWS (not phases)
 * Windows are tracked independently via booleans and can overlap
 */
UENUM(BlueprintType)
enum class EAttackPhase : uint8
{
    None            UMETA(DisplayName = "None"),
    Windup          UMETA(DisplayName = "Windup"),
    Active          UMETA(DisplayName = "Active"),
    Recovery        UMETA(DisplayName = "Recovery")
};

/**
 * Directional input for attacks and targeting (4-way for data configuration)
 */
UENUM(BlueprintType)
enum class EAttackDirection : uint8
{
    None            UMETA(DisplayName = "None"),
    Forward         UMETA(DisplayName = "Forward"),
    Backward        UMETA(DisplayName = "Backward"),
    Left            UMETA(DisplayName = "Left"),
    Right           UMETA(DisplayName = "Right")
};

/**
 * Input direction captured from movement stick/keys (8-way for gameplay)
 * Used for directional attacks, evades, targeting, hold follow-ups
 */
UENUM(BlueprintType)
enum class EInputDirection : uint8
{
    None            UMETA(DisplayName = "None"),
    Forward         UMETA(DisplayName = "Forward"),
    ForwardRight    UMETA(DisplayName = "Forward-Right"),
    Right           UMETA(DisplayName = "Right"),
    BackwardRight   UMETA(DisplayName = "Backward-Right"),
    Backward        UMETA(DisplayName = "Backward"),
    BackwardLeft    UMETA(DisplayName = "Backward-Left"),
    Left            UMETA(DisplayName = "Left"),
    ForwardLeft     UMETA(DisplayName = "Forward-Left")
};

/**
 * Hit reaction type classification
 */
UENUM(BlueprintType)
enum class EHitReactionType : uint8
{
    None            UMETA(DisplayName = "No Reaction"),
    Flinch          UMETA(DisplayName = "Flinch"),
    Light           UMETA(DisplayName = "Light Stagger"),
    Medium          UMETA(DisplayName = "Medium Stagger"),
    Heavy           UMETA(DisplayName = "Heavy Stagger"),
    Knockback       UMETA(DisplayName = "Knockback"),
    Knockdown       UMETA(DisplayName = "Knockdown"),
    Launch          UMETA(DisplayName = "Launch"),
    Custom          UMETA(DisplayName = "Custom Reaction")
};

/**
 * Input type for buffering system
 */
UENUM(BlueprintType)
enum class EInputType : uint8
{
    None            UMETA(DisplayName = "None"),
    LightAttack     UMETA(DisplayName = "Light Attack"),
    HeavyAttack     UMETA(DisplayName = "Heavy Attack"),
    Block           UMETA(DisplayName = "Block"),
    Evade           UMETA(DisplayName = "Evade"),
    Special         UMETA(DisplayName = "Special")
};

/**
 * Timing fallback strategy when AnimNotifyStates are missing
 */
UENUM(BlueprintType)
enum class ETimingFallbackMode : uint8
{
    AutoCalculate           UMETA(DisplayName = "Auto Calculate"),
    RequireManualOverride   UMETA(DisplayName = "Require Manual Override"),
    UseSafeDefaults         UMETA(DisplayName = "Use Safe Defaults"),
    DisallowMontage         UMETA(DisplayName = "Disallow Montage")
};

// ============================================================================
// STRUCTS
// ============================================================================

/**
 * Manual timing override when not using AnimNotifyStates
 */
USTRUCT(BlueprintType)
struct FAttackPhaseTimingOverride
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Timing")
    float WindupDuration = 0.3f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Timing")
    float ActiveDuration = 0.2f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Timing")
    float RecoveryDuration = 0.5f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Timing")
    float HoldWindowStart = 0.5f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Timing")
    float HoldWindowDuration = 0.3f;
};

/**
 * Buffered input for combo system
 */
USTRUCT(BlueprintType)
struct FBufferedInput
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite)
    EInputType Type = EInputType::None;

    UPROPERTY(BlueprintReadWrite)
    FVector2D Direction = FVector2D::ZeroVector;

    UPROPERTY(BlueprintReadWrite)
    float Timestamp = 0.0f;

    UPROPERTY(BlueprintReadWrite)
    bool bConsumed = false;

    FBufferedInput() {}
};

/**
 * Attack phase timing configuration
 */
USTRUCT(BlueprintType)
struct FAttackPhaseTiming
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Timing")
    float WindupStart = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Timing")
    float WindupEnd = 0.3f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Timing")
    float ActiveStart = 0.3f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Timing")
    float ActiveEnd = 0.5f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Timing")
    float RecoveryStart = 0.5f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Timing")
    float RecoveryEnd = 1.0f;

    // Optional phases
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Timing")
    bool bHasHoldWindow = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Timing", 
              meta = (EditCondition = "bHasHoldWindow"))
    float HoldWindowStart = 0.4f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Timing", 
              meta = (EditCondition = "bHasHoldWindow"))
    float HoldWindowEnd = 0.5f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Timing")
    bool bHasCancelWindow = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Timing", 
              meta = (EditCondition = "bHasCancelWindow"))
    float CancelWindowStart = 0.35f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Timing", 
              meta = (EditCondition = "bHasCancelWindow"))
    float CancelWindowEnd = 0.6f;
};

/**
 * Hit reaction configuration data
 */
USTRUCT(BlueprintType)
struct FHitReactionData
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Reaction")
    EHitReactionType ReactionType = EHitReactionType::Light;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Reaction")
    float StunDuration = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physics")
    float KnockbackForce = 200.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physics")
    float LaunchForce = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animation", 
              meta = (EditCondition = "ReactionType == EHitReactionType::Custom"))
    TObjectPtr<UAnimMontage> CustomReactionMontage = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Reaction")
    bool bForceInterruptCurrentAction = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Defense")
    bool bCanBeBlocked = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Defense")
    bool bCanBeParried = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Defense")
    bool bUnblockable = false;
};

/**
 * Target selection scoring data
 */
USTRUCT(BlueprintType)
struct FTargetScore
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    TObjectPtr<AActor> Target = nullptr;

    UPROPERTY(BlueprintReadOnly)
    float TotalScore = 0.0f;

    UPROPERTY(BlueprintReadOnly)
    float DistanceScore = 0.0f;

    UPROPERTY(BlueprintReadOnly)
    float DirectionScore = 0.0f;

    UPROPERTY(BlueprintReadOnly)
    float FacingScore = 0.0f;

    UPROPERTY(BlueprintReadOnly)
    float ThreatScore = 0.0f;
};

/**
 * Hit reaction information passed when applying damage
 */
USTRUCT(BlueprintType)
struct FHitReactionInfo
{
    GENERATED_BODY()

    /** Attacker who dealt the damage */
    UPROPERTY(BlueprintReadWrite, Category = "Hit Reaction")
    TObjectPtr<AActor> Attacker = nullptr;

    /** Direction of the hit (normalized, in world space) */
    UPROPERTY(BlueprintReadWrite, Category = "Hit Reaction")
    FVector HitDirection = FVector::ForwardVector;

    /** Attack data that caused this hit */
    UPROPERTY(BlueprintReadWrite, Category = "Hit Reaction")
    TObjectPtr<UAttackData> AttackData = nullptr;

    /** Final damage amount (after all modifiers) */
    UPROPERTY(BlueprintReadWrite, Category = "Hit Reaction")
    float Damage = 0.0f;

    /** Hitstun duration to apply */
    UPROPERTY(BlueprintReadWrite, Category = "Hit Reaction")
    float StunDuration = 0.0f;

    /** Was this a counter attack (during counter window)? */
    UPROPERTY(BlueprintReadWrite, Category = "Hit Reaction")
    bool bWasCounter = false;

    /** Impact location in world space */
    UPROPERTY(BlueprintReadWrite, Category = "Hit Reaction")
    FVector ImpactPoint = FVector::ZeroVector;

    FHitReactionInfo()
        : Attacker(nullptr)
        , HitDirection(FVector::ForwardVector)
        , AttackData(nullptr)
        , Damage(0.0f)
        , StunDuration(0.0f)
        , bWasCounter(false)
        , ImpactPoint(FVector::ZeroVector)
    {
    }
};

/**
 * Hit reaction animation set based on direction
 */
USTRUCT(BlueprintType)
struct FHitReactionAnimSet
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hit Reactions")
    TObjectPtr<UAnimMontage> FrontHit = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hit Reactions")
    TObjectPtr<UAnimMontage> BackHit = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hit Reactions")
    TObjectPtr<UAnimMontage> LeftHit = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hit Reactions")
    TObjectPtr<UAnimMontage> RightHit = nullptr;
};

/**
 * Motion warping configuration for an attack
 */
USTRUCT(BlueprintType)
struct FMotionWarpingConfig
{
    GENERATED_BODY()

    /** Use motion warping for this attack */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Motion Warping")
    bool bUseMotionWarping = true;

    /** Name of the warp target (must match in animation) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Motion Warping")
    FName MotionWarpingTargetName = "AttackTarget";

    /** Minimum distance to target before warping kicks in */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Motion Warping")
    float MinWarpDistance = 50.0f;

    /** Maximum distance we'll chase the target */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Motion Warping")
    float MaxWarpDistance = 400.0f;

    /** Speed of rotation toward target (degrees per second) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Motion Warping")
    float WarpRotationSpeed = 720.0f;

    /** Warp position toward target */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Motion Warping")
    bool bWarpTranslation = true;

    /** Require line of sight to target */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Motion Warping")
    bool bRequireLineOfSight = true;
};

/**
 * Configuration for directional rotation warping (input direction-based, not target-based)
 *
 * Used for directional attacks where player holds attack button, deflects stick,
 * and releases to attack in that direction. Separate from FMotionWarpingConfig
 * which handles target-based warping toward enemies.
 */
USTRUCT(BlueprintType)
struct FDirectionalWarpConfig
{
    GENERATED_BODY()

    /** Enable directional rotation warp for this attack */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Directional Warp")
    bool bEnableDirectionalWarp = true;

    /** Name of the warp target (must match AnimNotifyState_MotionWarping in animation) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Directional Warp",
        meta = (EditCondition = "bEnableDirectionalWarp"))
    FName DirectionalWarpTargetName = "DirectionTarget";

    /** Rotation warp speed (degrees per second) - higher = snappier */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Directional Warp",
        meta = (EditCondition = "bEnableDirectionalWarp", ClampMin = "90.0", ClampMax = "1800.0"))
    float DirectionalWarpRotationSpeed = 720.0f;
};

#if WITH_AUTOMATION_TESTS
/**
 * Debug arrow information for testing
 * Contains all data needed to verify an arrow's position, style, and appearance
 */
struct FDebugArrowInfo
{
	FVector StartPosition;
	FVector EndPosition;
	FVector LabelPosition;
	FString Label;
	FColor Color;
	float Thickness;
	bool bIsDashed;
	float Length;

	FDebugArrowInfo()
		: StartPosition(FVector::ZeroVector)
		, EndPosition(FVector::ZeroVector)
		, LabelPosition(FVector::ZeroVector)
		, Label(TEXT(""))
		, Color(FColor::White)
		, Thickness(1.0f)
		, bIsDashed(false)
		, Length(0.0f)
	{}
};

/**
 * Complete debug visualization data for testing
 * Allows unit tests to verify positioning, coloring, and visibility logic
 * without requiring actual rendering
 */
struct FDebugVisualizationData
{
	TArray<FDebugArrowInfo> Arrows;
	TArray<FVector> ArcPoints;
	FString HoldStateLabel;
	bool bShowHoldIndicator;
	FVector ChestOffset;
	float YawDelta;

	FDebugVisualizationData()
		: HoldStateLabel(TEXT(""))
		, bShowHoldIndicator(false)
		, ChestOffset(FVector::ZeroVector)
		, YawDelta(0.0f)
	{}
};
#endif // WITH_AUTOMATION_TESTS

// ============================================================================
// DELEGATES
// ============================================================================

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnCombatStateChanged, ECombatState, NewState);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnAttackHit, AActor*, HitActor, float, Damage);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPostureChanged, float, NewPosture);

// Combat System Event Delegates
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnAttackStarted, UAttackData*, AttackData, EInputType, InputType, bool, bIsCombo);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnPhaseChanged, EAttackPhase, OldPhase, EAttackPhase, NewPhase);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnComboWindowChanged, bool, bActive, float, Duration);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnHoldActivated, EInputType, InputType, float, HoldDuration);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnMontageEvent, UAnimMontage*, Montage, bool, bInterrupted, FName, EventName);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnGuardBroken);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPerfectParry, AActor*, ParriedActor);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPerfectEvade, AActor*, EvadedActor);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnFinisherAvailable, AActor*, Target);

// ============================================================================
// HELPER FUNCTIONS (Input Direction Conversion)
// ============================================================================

namespace CombatHelpers
{
	/**
	 * Convert 8-way input direction to 4-way attack direction (for data lookups)
	 * Diagonal inputs map to their primary direction based on design rules
	 */
	inline EAttackDirection InputToAttackDirection(EInputDirection InputDir)
	{
		switch (InputDir)
		{
			case EInputDirection::Forward:
			case EInputDirection::ForwardRight:
			case EInputDirection::ForwardLeft:
				return EAttackDirection::Forward;

			case EInputDirection::Backward:
			case EInputDirection::BackwardRight:
			case EInputDirection::BackwardLeft:
				return EAttackDirection::Backward;

			case EInputDirection::Right:
				return EAttackDirection::Right;

			case EInputDirection::Left:
				return EAttackDirection::Left;

			case EInputDirection::None:
			default:
				return EAttackDirection::None;
		}
	}

	/**
	 * Calculate 8-way input direction from 2D input vector (CAMERA-RELATIVE)
	 * WARNING: This function assumes input is in camera space (X=camera right, Y=camera forward)
	 * For character-relative directions, use VectorToCharacterRelativeDirection() instead
	 *
	 * @param InputVector - Normalized 2D input in CAMERA SPACE (X=right, Y=forward relative to camera)
	 * @param DeadZone - Minimum magnitude to register input (default 0.2)
	 * @return 8-way direction enum in camera space
	 */
	inline EInputDirection VectorToInputDirection(const FVector2D& InputVector, float DeadZone = 0.2f)
	{
		if (InputVector.Size() < DeadZone)
		{
			return EInputDirection::None;
		}

		// Calculate angle in degrees
		// Unreal 2D character space convention: X=Forward/Backward, Y=Right/Left
		// Atan2(X, Y) gives: 0°=Right, 90°=Forward, 180°=Left, 270°=Backward
		// CRITICAL: Parameter order is Atan2(X, Y) NOT Atan2(Y, X) for correct mapping
		float Angle = FMath::Atan2(InputVector.X, InputVector.Y) * (180.0f / PI);

		// Normalize to 0-360 range
		if (Angle < 0.0f)
		{
			Angle += 360.0f;
		}

		// Map angle to 8-way direction (45-degree sectors)
		// Right: 337.5-22.5°, ForwardRight: 22.5-67.5°, Forward: 67.5-112.5°, etc.
		// FIXED (2025-11-21): Mappings rotated 90° to match corrected Atan2(X,Y) parameter order
		if (Angle >= 337.5f || Angle < 22.5f)
			return EInputDirection::Right;
		else if (Angle >= 22.5f && Angle < 67.5f)
			return EInputDirection::ForwardRight;
		else if (Angle >= 67.5f && Angle < 112.5f)
			return EInputDirection::Forward;
		else if (Angle >= 112.5f && Angle < 157.5f)
			return EInputDirection::ForwardLeft;
		else if (Angle >= 157.5f && Angle < 202.5f)
			return EInputDirection::Left;
		else if (Angle >= 202.5f && Angle < 247.5f)
			return EInputDirection::BackwardLeft;
		else if (Angle >= 247.5f && Angle < 292.5f)
			return EInputDirection::Backward;
		else // 292.5f - 337.5f
			return EInputDirection::BackwardRight;
	}

	/**
	 * Get character rotation for directional calculations
	 *
	 * For characters using bOrientRotationToMovement (standard), actor rotation = visual facing direction.
	 * The mesh relative rotation is typically model correction (e.g., -90° for Y+-facing models),
	 * NOT an intentional facing offset.
	 *
	 * @param Character - Character to query
	 * @param bIncludeMeshOffset - If true, adds mesh relative rotation to actor rotation.
	 *                             Default false (standard UE characters with model correction offset).
	 *                             Set true only for characters where mesh is intentionally rotated
	 *                             relative to movement direction.
	 * @return Actor rotation, optionally with mesh offset applied
	 */
	inline FRotator GetMeshCompensatedRotation(const ACharacter* Character, bool bIncludeMeshOffset = false)
	{
		if (!Character)
		{
			return FRotator::ZeroRotator;
		}

		FRotator ActorRotation = Character->GetActorRotation();

		// Default: Return actor rotation directly
		// For bOrientRotationToMovement characters, this IS the visual facing direction
		if (!bIncludeMeshOffset)
		{
			return ActorRotation;
		}

		// Optional: Include mesh offset (rare - only for intentionally offset meshes)
		USkeletalMeshComponent* Mesh = Character->GetMesh();
		if (!Mesh)
		{
			return ActorRotation;
		}

		FRotator MeshRelativeRotation = Mesh->GetRelativeRotation();
		FRotator TrueRotation = ActorRotation;
		TrueRotation.Yaw += MeshRelativeRotation.Yaw;

		// Normalize yaw to -180 to +180 range
		TrueRotation.Yaw = FMath::Fmod(TrueRotation.Yaw + 180.0f, 360.0f) - 180.0f;

		return TrueRotation;
	}

	/**
	 * Get mesh rotation offset from actor rotation
	 * Convenience wrapper around DirectionDebugLibrary::GetMeshRotationOffset for inline use
	 *
	 * @param Character - Character to query
	 * @return Mesh rotation offset (ZeroRotator if no offset or invalid)
	 */
	inline FRotator GetMeshRotationOffset(const ACharacter* Character)
	{
		if (!Character)
		{
			return FRotator::ZeroRotator;
		}

		USkeletalMeshComponent* Mesh = Character->GetMesh();
		if (!Mesh)
		{
			return FRotator::ZeroRotator;
		}

		// Return mesh's relative rotation (offset from actor)
		return Mesh->GetRelativeRotation();
	}

	/**
	 * Calculate 8-way input direction from camera-relative input vector, transformed to CHARACTER-RELATIVE space
	 * This function properly handles cases where character facing != camera facing
	 *
	 * CRITICAL FIX (2025-11-20): Now accounts for mesh rotation offset to prevent 90° calculation errors
	 * when character mesh is rotated in editor (common UE pattern: -90° yaw for character meshes)
	 *
	 * Coordinate Space Transformations:
	 * 1. Camera-Relative Input (from gamepad) → World Space (rotate by camera yaw)
	 * 2. World Space → Character-Relative Space (rotate by -character yaw WITH mesh offset)
	 * 3. Character-Relative 3D → 2D (project to XY plane)
	 * 4. 2D Vector → 8-way direction enum
	 *
	 * Example: Character faces North (0°), Camera faces East (90°), Player presses "Forward" on stick
	 *   - Camera-relative input: (0, 1) = Forward relative to camera = East in world
	 *   - World vector: Rotate (0,1) by 90° = (1, 0) = East
	 *   - Character-relative: Rotate (1,0) by -0° = (1, 0) = Right relative to character
	 *   - Result: EInputDirection::Right ✓ (correct - player wants to move camera-forward = character-right)
	 *
	 * @param CameraRelativeInput - Normalized 2D input from gamepad (X=right, Y=forward relative to camera view)
	 * @param CameraRotation - Current camera rotation (only Yaw is used)
	 * @param Character - Character for mesh offset detection (if nullptr, uses CharacterRotation as-is)
	 * @param CharacterRotation - Current character ACTOR rotation (mesh offset applied automatically if Character provided)
	 * @param DeadZone - Minimum magnitude to register input (default 0.2)
	 * @return 8-way direction enum relative to character's mesh-compensated facing direction
	 */
	inline EInputDirection VectorToCharacterRelativeDirection(
		const FVector2D& CameraRelativeInput,
		const FRotator& CameraRotation,
		const ACharacter* Character,
		const FRotator& CharacterRotation,
		float DeadZone = 0.2f)
	{
		// Early exit for zero input
		if (CameraRelativeInput.Size() < DeadZone)
		{
			return EInputDirection::None;
		}

		// STEP 0: Get mesh-compensated character rotation (actor rotation + mesh offset)
		// CRITICAL: Character meshes are often rotated -90° in editor
		// We must use the mesh's actual forward direction, not the actor's root rotation
		FRotator MeshCompensatedRotation = CharacterRotation;
		if (Character)
		{
			MeshCompensatedRotation = GetMeshCompensatedRotation(Character);
		}

		// STEP 1: Convert camera-relative 2D input to world space 3D vector
		// CRITICAL: Flatten camera rotation to yaw-only to prevent pitch/roll from corrupting WorldInput
		// When player looks up/down (pitch != 0), we still want horizontal directional input only
		const FRotator FlatCameraRotation = FRotator(0.0f, CameraRotation.Yaw, 0.0f);

		// Unreal Engine convention: X=Forward, Y=Right, Z=Up
		const FVector CameraForward = FRotationMatrix(FlatCameraRotation).GetScaledAxis(EAxis::X); // Camera's forward (X axis)
		const FVector CameraRight = FRotationMatrix(FlatCameraRotation).GetScaledAxis(EAxis::Y);   // Camera's right (Y axis)

		// Combine input components: InputX * CameraRight + InputY * CameraForward
		FVector WorldInput = (CameraRight * CameraRelativeInput.X) + (CameraForward * CameraRelativeInput.Y);
		WorldInput.Z = 0.0f; // Project to horizontal plane (ignore vertical component)
		WorldInput.Normalize();

		// STEP 2: Convert world space vector to character-relative space
		// Rotate by inverse of character's mesh-compensated yaw to get direction relative to character's ACTUAL facing
		const FRotator InverseCharacterYaw(0.0f, -MeshCompensatedRotation.Yaw, 0.0f);
		const FVector CharacterRelative = InverseCharacterYaw.RotateVector(WorldInput);

		// STEP 3: Project 3D character-relative vector to 2D (XY plane)
		const FVector2D CharacterRelative2D(CharacterRelative.X, CharacterRelative.Y);

		// STEP 4: Convert 2D vector to 8-way direction using existing helper
		return VectorToInputDirection(CharacterRelative2D, DeadZone);
	}

	/**
	 * Convert EInputDirection enum to world space direction vector
	 * Used by directional attack system to determine attack direction from buffered input
	 *
	 * @param Direction - 8-way input direction
	 * @param Character - Character to get rotation from
	 * @return World space direction vector (normalized), or ZeroVector if Direction is None
	 */
	inline FVector InputDirectionToWorldVector(EInputDirection Direction, const ACharacter* Character)
	{
		if (!Character || Direction == EInputDirection::None)
		{
			return FVector::ZeroVector;
		}

		const FRotator CharRotation = Character->GetActorRotation();
		const FVector Forward = CharRotation.Vector();
		const FVector Right = FRotationMatrix(CharRotation).GetScaledAxis(EAxis::Y);

		switch (Direction)
		{
			case EInputDirection::Forward:
				return Forward;
			case EInputDirection::ForwardRight:
				return (Forward + Right).GetSafeNormal();
			case EInputDirection::Right:
				return Right;
			case EInputDirection::BackwardRight:
				return (-Forward + Right).GetSafeNormal();
			case EInputDirection::Backward:
				return -Forward;
			case EInputDirection::BackwardLeft:
				return (-Forward - Right).GetSafeNormal();
			case EInputDirection::Left:
				return -Right;
			case EInputDirection::ForwardLeft:
				return (Forward - Right).GetSafeNormal();
			default:
				return Forward;
		}
	}

	/**
	 * Convert EInputDirection enum to world rotation (yaw only)
	 * Convenience wrapper for setting motion warp targets
	 *
	 * @param Direction - 8-way input direction
	 * @param Character - Character to get rotation from
	 * @return World rotation (yaw only), or character's rotation if Direction is None
	 */
	inline FRotator InputDirectionToWorldRotation(EInputDirection Direction, const ACharacter* Character)
	{
		FVector WorldDir = InputDirectionToWorldVector(Direction, Character);
		if (WorldDir.IsNearlyZero())
		{
			return Character ? Character->GetActorRotation() : FRotator::ZeroRotator;
		}
		return WorldDir.Rotation();
	}
}