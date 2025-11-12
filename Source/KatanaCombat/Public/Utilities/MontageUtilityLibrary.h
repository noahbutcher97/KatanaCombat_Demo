// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Animation/AnimMontage.h"
#include "Curves/CurveFloat.h"
#include "GameplayTagContainer.h"
#include "CombatTypes.h"
#include "MontageUtilityLibrary.generated.h"

// Forward declarations
struct FTimerCheckpoint;
enum class EActionWindowType : uint8;

/**
 * Procedural easing types for smooth transitions
 * Alternative to authoring UCurveFloat assets
 */
UENUM(BlueprintType)
enum class EEasingType : uint8
{
	/** No easing - linear interpolation */
	Linear,

	/** Quadratic ease in - slow start, accelerates (t^2) */
	EaseInQuad,

	/** Quadratic ease out - fast start, decelerates (1 - (1-t)^2) */
	EaseOutQuad,

	/** Quadratic ease in-out - S-curve, slow start/end (smooth) */
	EaseInOutQuad,

	/** Cubic ease in - very slow start, sharp acceleration (t^3) */
	EaseInCubic,

	/** Cubic ease out - sharp start, smooth deceleration (1 - (1-t)^3) */
	EaseOutCubic,

	/** Cubic ease in-out - dramatic S-curve */
	EaseInOutCubic,

	/** Exponential ease in - extremely slow start, explosive acceleration */
	EaseInExpo,

	/** Exponential ease out - explosive start, gentle deceleration */
	EaseOutExpo,

	/** Sine ease in-out - very smooth, natural feeling transition */
	EaseInOutSine
};

// ============================================================================
// ATTACK RESOLUTION SYSTEM (Phase 1 - Context-Aware Resolution)
// ============================================================================

/**
 * Enum tracking HOW an attack was resolved
 * Provides metadata about resolution path for debugging and system behavior
 */
UENUM(BlueprintType)
enum class EResolutionPath : uint8
{
	/** Default attack (no combo chain, no context) */
	Default,

	/** Normal combo chain (NextComboAttack or HeavyComboAttack) */
	NormalCombo,

	/** Directional follow-up after hold-and-release */
	DirectionalFollowUp,

	/** Parry counter attack (future use) */
	ParryCounter,

	/** Low-health finisher (future use) */
	LowHealthFinisher,

	/** Custom context-sensitive attack (future use) */
	ContextSensitive
};

/**
 * Result of attack resolution with metadata
 * Replaces simple UAttackData* return value with rich context information
 *
 * Key Benefits:
 * - Clear directional input after directional follow-ups (fixes infinite loop)
 * - Track resolution path for debugging and telemetry
 * - Detect cycles during resolution (safety check)
 * - Future-proof for context-sensitive attacks
 */
USTRUCT(BlueprintType)
struct KATANACOMBAT_API FAttackResolutionResult
{
	GENERATED_BODY()

	/** Resolved attack to execute (nullptr = no valid attack found) */
	UPROPERTY(BlueprintReadOnly, Category = "Resolution")
	TObjectPtr<class UAttackData> Attack = nullptr;

	/** How was this attack resolved? (for debugging/telemetry) */
	UPROPERTY(BlueprintReadOnly, Category = "Resolution")
	EResolutionPath Path = EResolutionPath::Default;

	/** Should clear LastDirectionalInput after this resolution? (KEY FIX for directional loop) */
	UPROPERTY(BlueprintReadOnly, Category = "Resolution")
	bool bShouldClearDirectionalInput = false;

	/** Was a cycle detected during resolution? (safety check) */
	UPROPERTY(BlueprintReadOnly, Category = "Resolution")
	bool bCycleDetected = false;

	/** Default constructor */
	FAttackResolutionResult()
		: Attack(nullptr)
		, Path(EResolutionPath::Default)
		, bShouldClearDirectionalInput(false)
		, bCycleDetected(false)
	{
	}

	/** Convenience constructor for simple cases */
	FAttackResolutionResult(UAttackData* InAttack, EResolutionPath InPath, bool bClearDirectional = false)
		: Attack(InAttack)
		, Path(InPath)
		, bShouldClearDirectionalInput(bClearDirectional)
		, bCycleDetected(false)
	{
	}

	/** Is resolution valid? (has attack and no cycle) */
	bool IsValid() const { return Attack != nullptr && !bCycleDetected; }
};

/**
 * Montage Utility Library
 *
 * Blueprint function library providing safe, encapsulated access to montage timing utilities.
 * Used by CombatComponentV2 for checkpoint discovery and timing queries.
 *
 * Features:
 * - Basic montage queries (time, playrate, sections)
 * - Checkpoint discovery (scan AnimNotifyStates)
 * - Procedural easing (smooth transitions without curve assets)
 * - Advanced hold mechanics (charge levels, multi-stage)
 * - Window state queries (active windows, time remaining)
 * - Montage blending (crossfades, blend-outs)
 *
 * Benefits:
 * - Separation of concerns (utility functions separate from component logic)
 * - Reusability (can be used by other systems like AI, animations)
 * - Blueprint exposure (designers can use these in animation blueprints)
 * - Reduced component bloat (keeps CombatComponentV2 focused on combat logic)
 * - Stateless design (components own state, library provides calculations)
 */
UCLASS()
class KATANACOMBAT_API UMontageUtilityLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	// ============================================================================
	// MONTAGE TIME QUERIES
	// ============================================================================

	/**
	 * Get current montage time from character
	 * Encapsulates Character→Mesh→AnimInstance→Montage→Position chain
	 *
	 * @param Character - Character to query
	 * @return Current montage time, or -1.0f if no active montage
	 */
	UFUNCTION(BlueprintPure, Category = "Combat|Montage Utilities", meta = (DisplayName = "Get Current Montage Time"))
	static float GetCurrentMontageTime(ACharacter* Character);

	/**
	 * Get current active montage from character
	 *
	 * @param Character - Character to query
	 * @return Active montage, or nullptr if none playing
	 */
	UFUNCTION(BlueprintPure, Category = "Combat|Montage Utilities", meta = (DisplayName = "Get Current Montage"))
	static UAnimMontage* GetCurrentMontage(ACharacter* Character);

	/**
	 * Get AnimInstance from character
	 * Safe accessor with null checks
	 *
	 * @param Character - Character to query
	 * @return AnimInstance, or nullptr if character/mesh is invalid
	 */
	UFUNCTION(BlueprintPure, Category = "Combat|Montage Utilities", meta = (DisplayName = "Get Anim Instance"))
	static UAnimInstance* GetAnimInstance(ACharacter* Character);

	// ============================================================================
	// MONTAGE PLAYBACK CONTROL
	// ============================================================================

	/**
	 * Set playrate for current montage
	 * Safe - checks for valid montage before setting
	 *
	 * @param Character - Character whose montage to modify
	 * @param PlayRate - New playrate (0.0 = frozen, 1.0 = normal, >1.0 = fast)
	 * @return True if playrate was set, false if no active montage
	 */
	UFUNCTION(BlueprintCallable, Category = "Combat|Montage Utilities", meta = (DisplayName = "Set Montage Play Rate"))
	static bool SetMontagePlayRate(ACharacter* Character, float PlayRate);

	/**
	 * Get current montage playrate
	 *
	 * @param Character - Character to query
	 * @return Current playrate, or 1.0f if no active montage
	 */
	UFUNCTION(BlueprintPure, Category = "Combat|Montage Utilities", meta = (DisplayName = "Get Montage Play Rate"))
	static float GetMontagePlayRate(ACharacter* Character);

	// ============================================================================
	// CHECKPOINT DISCOVERY
	// ============================================================================

	/**
	 * Discover all AnimNotifyState windows in montage
	 * Scans montage for window timing information (Combo, Parry, Hold, Cancel)
	 *
	 * Used by CombatComponentV2 to populate checkpoint arrays for timer-based execution.
	 *
	 * @param Montage - Montage to scan
	 * @param OutCheckpoints - Array to populate with discovered checkpoints (cleared first)
	 * @return Number of checkpoints discovered
	 */
	UFUNCTION(BlueprintCallable, Category = "Combat|Montage Utilities", meta = (DisplayName = "Discover Montage Checkpoints"))
	static int32 DiscoverCheckpoints(UAnimMontage* Montage, TArray<struct FTimerCheckpoint>& OutCheckpoints);

	/**
	 * Get montage total duration
	 *
	 * @param Montage - Montage to query
	 * @return Total duration in seconds, or 0.0f if invalid
	 */
	UFUNCTION(BlueprintPure, Category = "Combat|Montage Utilities", meta = (DisplayName = "Get Montage Duration"))
	static float GetMontageDuration(UAnimMontage* Montage);

	/**
	 * Check if montage time is within range
	 * Utility for checkpoint validation
	 *
	 * @param CurrentTime - Current montage time
	 * @param StartTime - Window start time
	 * @param Duration - Window duration
	 * @return True if CurrentTime is within [StartTime, StartTime + Duration]
	 */
	UFUNCTION(BlueprintPure, Category = "Combat|Montage Utilities", meta = (DisplayName = "Is Time In Window"))
	static bool IsTimeInWindow(float CurrentTime, float StartTime, float Duration);

	// ============================================================================
	// PROCEDURAL EASING (Stateless calculations)
	// ============================================================================

	/**
	 * Evaluate procedural easing function
	 * Pure calculation - no state storage
	 *
	 * @param Alpha - Input value (0.0 - 1.0)
	 * @param EasingType - Type of easing to apply
	 * @return Eased alpha value (0.0 - 1.0)
	 */
	UFUNCTION(BlueprintPure, Category = "Combat|Montage Utilities|Easing", meta = (DisplayName = "Evaluate Easing"))
	static float EvaluateEasing(float Alpha, EEasingType EasingType);

	/**
	 * Interpolate between two values using procedural easing
	 * Alternative to FMath::Lerp with easing applied
	 *
	 * @param Start - Starting value
	 * @param End - Ending value
	 * @param Alpha - Interpolation factor (0.0 - 1.0)
	 * @param EasingType - Type of easing to apply
	 * @return Interpolated value with easing applied
	 */
	UFUNCTION(BlueprintPure, Category = "Combat|Montage Utilities|Easing", meta = (DisplayName = "Ease Lerp"))
	static float EaseLerp(float Start, float End, float Alpha, EEasingType EasingType);

	/**
	 * Calculate interpolated playrate for smooth transition
	 * Can use either procedural easing or custom curve
	 *
	 * @param StartRate - Starting playrate
	 * @param TargetRate - Target playrate
	 * @param ElapsedTime - Time elapsed in transition
	 * @param Duration - Total transition duration
	 * @param EasingType - Procedural easing type (used if Curve is null)
	 * @param Curve - Optional custom curve (overrides EasingType if provided)
	 * @return Current playrate based on elapsed time
	 */
	UFUNCTION(BlueprintPure, Category = "Combat|Montage Utilities|Easing", meta = (DisplayName = "Calculate Transition Play Rate"))
	static float CalculateTransitionPlayRate(
		float StartRate,
		float TargetRate,
		float ElapsedTime,
		float Duration,
		EEasingType EasingType = EEasingType::EaseInOutQuad,
		UCurveFloat* Curve = nullptr
	);

	// ============================================================================
	// ADVANCED HOLD MECHANICS (Stateless calculations)
	// ============================================================================

	/**
	 * Calculate charge level (0.0 - 1.0) based on hold duration
	 * Can use procedural easing or custom curve
	 *
	 * @param HoldDuration - How long button has been held
	 * @param MaxChargeTime - Time to reach full charge (1.0)
	 * @param EasingType - Procedural easing type (used if Curve is null)
	 * @param ChargeCurve - Optional custom curve (overrides EasingType if provided)
	 * @return Charge level (0.0 = no charge, 1.0 = full charge)
	 */
	UFUNCTION(BlueprintPure, Category = "Combat|Montage Utilities|Hold Mechanics", meta = (DisplayName = "Calculate Charge Level"))
	static float CalculateChargeLevel(
		float HoldDuration,
		float MaxChargeTime,
		EEasingType EasingType = EEasingType::EaseInQuad,
		UCurveFloat* ChargeCurve = nullptr
	);

	/**
	 * Get playrate for multi-stage hold progression
	 * Example: 1.0 → 0.5 (Stage 1) → 0.2 (Stage 2) → 0.0 (Fully charged)
	 *
	 * @param HoldDuration - How long button has been held
	 * @param StageThresholds - Array of time thresholds for each stage (must be sorted ascending)
	 * @param StagePlayRates - Array of playrates for each stage (must match threshold count)
	 * @return Current playrate based on hold duration
	 */
	UFUNCTION(BlueprintPure, Category = "Combat|Montage Utilities|Hold Mechanics", meta = (DisplayName = "Get Multi-Stage Hold Play Rate"))
	static float GetMultiStageHoldPlayRate(
		float HoldDuration,
		const TArray<float>& StageThresholds,
		const TArray<float>& StagePlayRates
	);

	/**
	 * Get current stage index in multi-stage hold
	 *
	 * @param HoldDuration - How long button has been held
	 * @param StageThresholds - Array of time thresholds for each stage
	 * @return Current stage index (0-based), or -1 if before first stage
	 */
	UFUNCTION(BlueprintPure, Category = "Combat|Montage Utilities|Hold Mechanics", meta = (DisplayName = "Get Hold Stage Index"))
	static int32 GetHoldStageIndex(float HoldDuration, const TArray<float>& StageThresholds);

	// ============================================================================
	// MONTAGE SECTION UTILITIES
	// ============================================================================

	/**
	 * Get all section names in montage
	 *
	 * @param Montage - Montage to query
	 * @return Array of section names
	 */
	UFUNCTION(BlueprintPure, Category = "Combat|Montage Utilities|Sections", meta = (DisplayName = "Get Montage Sections"))
	static TArray<FName> GetMontageSections(UAnimMontage* Montage);

	/**
	 * Get montage time for section start
	 *
	 * @param Montage - Montage to query
	 * @param SectionName - Name of section
	 * @return Section start time, or -1.0f if not found
	 */
	UFUNCTION(BlueprintPure, Category = "Combat|Montage Utilities|Sections", meta = (DisplayName = "Get Section Start Time"))
	static float GetSectionStartTime(UAnimMontage* Montage, FName SectionName);

	/**
	 * Get duration of specific montage section
	 *
	 * @param Montage - Montage to query
	 * @param SectionName - Name of section
	 * @return Section duration, or -1.0f if not found
	 */
	UFUNCTION(BlueprintPure, Category = "Combat|Montage Utilities|Sections", meta = (DisplayName = "Get Section Duration"))
	static float GetSectionDuration(UAnimMontage* Montage, FName SectionName);

	/**
	 * Get current section name from character
	 *
	 * @param Character - Character to query
	 * @return Current section name, or NAME_None if no active montage
	 */
	UFUNCTION(BlueprintPure, Category = "Combat|Montage Utilities|Sections", meta = (DisplayName = "Get Current Section Name"))
	static FName GetCurrentSectionName(ACharacter* Character);

	/**
	 * Jump to montage section with optional blend time
	 *
	 * @param Character - Character playing montage
	 * @param SectionName - Section to jump to
	 * @param BlendTime - Time to blend into new section (0.0 = instant, not yet implemented in UE)
	 * @return True if jump succeeded
	 */
	UFUNCTION(BlueprintCallable, Category = "Combat|Montage Utilities|Sections", meta = (DisplayName = "Jump To Section With Blend"))
	static bool JumpToSectionWithBlend(ACharacter* Character, FName SectionName, float BlendTime = 0.0f);

	// ============================================================================
	// WINDOW STATE QUERIES
	// ============================================================================

	/**
	 * Get all active window types at current montage time
	 *
	 * @param Character - Character to query
	 * @param Checkpoints - Array of checkpoints to search
	 * @return Array of active window types
	 */
	UFUNCTION(BlueprintPure, Category = "Combat|Montage Utilities|Windows", meta = (DisplayName = "Get Active Windows"))
	static TArray<EActionWindowType> GetActiveWindows(
		ACharacter* Character,
		const TArray<FTimerCheckpoint>& Checkpoints
	);

	/**
	 * Check if specific window type is currently active
	 *
	 * @param Character - Character to query
	 * @param Checkpoints - Array of checkpoints to search
	 * @param WindowType - Type to check for
	 * @return True if window is currently active
	 */
	UFUNCTION(BlueprintPure, Category = "Combat|Montage Utilities|Windows", meta = (DisplayName = "Is Window Active"))
	static bool IsWindowActive(
		ACharacter* Character,
		const TArray<FTimerCheckpoint>& Checkpoints,
		EActionWindowType WindowType
	);

	/**
	 * Get time remaining in specific window
	 *
	 * @param Character - Character to query
	 * @param Checkpoint - Window checkpoint to check
	 * @return Time remaining (seconds), or 0.0f if not in window
	 */
	UFUNCTION(BlueprintPure, Category = "Combat|Montage Utilities|Windows", meta = (DisplayName = "Get Window Time Remaining"))
	static float GetWindowTimeRemaining(ACharacter* Character, const FTimerCheckpoint& Checkpoint);

	/**
	 * Get next checkpoint of specific type
	 *
	 * @param Character - Character to query
	 * @param Checkpoints - Array of checkpoints to search
	 * @param WindowType - Type to search for
	 * @param OutCheckpoint - Output checkpoint if found
	 * @return True if next checkpoint was found
	 */
	UFUNCTION(BlueprintPure, Category = "Combat|Montage Utilities|Windows", meta = (DisplayName = "Get Next Checkpoint"))
	static bool GetNextCheckpoint(
		ACharacter* Character,
		const TArray<FTimerCheckpoint>& Checkpoints,
		EActionWindowType WindowType,
		FTimerCheckpoint& OutCheckpoint
	);

	// ============================================================================
	// MONTAGE BLENDING
	// ============================================================================

	/**
	 * Crossfade from current montage to new montage
	 * Maintains animation continuity for similar poses
	 *
	 * @param Character - Character to modify
	 * @param TargetMontage - Montage to transition to
	 * @param BlendTime - Duration of crossfade
	 * @param StartPosition - Where to start in target montage (default: 0.0)
	 * @param PlayRate - Playrate for new montage (default: 1.0)
	 * @return True if crossfade initiated
	 */
	UFUNCTION(BlueprintCallable, Category = "Combat|Montage Utilities|Blending", meta = (DisplayName = "Crossfade Montage"))
	static bool CrossfadeMontage(
		ACharacter* Character,
		UAnimMontage* TargetMontage,
		float BlendTime = 0.2f,
		float StartPosition = 0.0f,
		float PlayRate = 1.0f
	);

	/**
	 * Blend out current montage over time
	 * Alternative to instant Montage_Stop
	 *
	 * @param Character - Character to modify
	 * @param BlendOutTime - Duration of blend out
	 * @return True if blend out started
	 */
	UFUNCTION(BlueprintCallable, Category = "Combat|Montage Utilities|Blending", meta = (DisplayName = "Blend Out Montage"))
	static bool BlendOutMontage(ACharacter* Character, float BlendOutTime = 0.2f);

	// ============================================================================
	// DEBUG & VISUALIZATION
	// ============================================================================

	/**
	 * Draw debug timeline for montage checkpoints
	 * Shows all windows on screen as visual timeline
	 *
	 * @param World - World context for drawing
	 * @param Character - Character to visualize
	 * @param Checkpoints - Array of checkpoints to display
	 * @param DrawDuration - How long to display (seconds)
	 * @param YOffset - Vertical offset for timeline display
	 */
	UFUNCTION(BlueprintCallable, Category = "Combat|Montage Utilities|Debug", meta = (DisplayName = "Draw Checkpoint Timeline", WorldContext = "World"))
	static void DrawCheckpointTimeline(
		UObject* World,
		ACharacter* Character,
		const TArray<FTimerCheckpoint>& Checkpoints,
		float DrawDuration = 5.0f,
		float YOffset = 100.0f
	);

	/**
	 * Log checkpoint information to console
	 *
	 * @param Checkpoints - Array to log
	 * @param Prefix - Optional prefix for log messages
	 */
	UFUNCTION(BlueprintCallable, Category = "Combat|Montage Utilities|Debug", meta = (DisplayName = "Log Checkpoints"))
	static void LogCheckpoints(const TArray<FTimerCheckpoint>& Checkpoints, const FString& Prefix = TEXT(""));

	// ============================================================================
	// HOLD SYSTEM HELPERS
	// ============================================================================

	/**
	 * Set montage section to loop back to itself
	 * Used for heavy attack charge loops
	 *
	 * @param Character - Character playing montage
	 * @param LoopSectionName - Section to loop
	 * @return True if loop was set up successfully
	 */
	UFUNCTION(BlueprintCallable, Category = "Combat|Montage Utilities|Hold System", meta = (DisplayName = "Loop Montage Section"))
	static bool LoopMontageSection(ACharacter* Character, FName LoopSectionName);

	/**
	 * Convert directional input (movement stick) to attack direction enum
	 *
	 * @param DirectionInput - 2D input from movement stick (X=Forward/Back, Y=Left/Right)
	 * @param DeadzoneThreshold - Minimum magnitude to register direction (default: 0.3)
	 * @return Attack direction based on dominant axis
	 */
	UFUNCTION(BlueprintPure, Category = "Combat|Montage Utilities|Hold System", meta = (DisplayName = "Get Direction From Input"))
	static EAttackDirection GetDirectionFromInput(FVector2D DirectionInput, float DeadzoneThreshold = 0.3f);

	// ============================================================================
	// ATTACK RESOLUTION (Combo Progression)
	// ============================================================================

	/**
	 * Resolve next attack in combo chain based on context
	 * Returns appropriate attack considering combo window, previous attack, and input type
	 *
	 * Resolution Logic:
	 * - If combo window active + current attack exists → traverse combo chain
	 * - If no combo window or chain ends → return default attack for input type
	 * - Supports Light→Light (NextComboAttack), Light→Heavy (HeavyComboAttack), Directional follow-ups
	 *
	 * @param CurrentAttack - Currently executing attack (null if no combo active)
	 * @param InputType - Input type being triggered (Light/Heavy)
	 * @param bComboWindowActive - Is combo window currently active?
	 * @param bIsHolding - Is button currently held? (for directional follow-ups)
	 * @param DefaultLightAttack - Fallback light attack (Attack_1)
	 * @param DefaultHeavyAttack - Fallback heavy attack (Heavy_1)
	 * @param Direction - Movement direction (for directional follow-ups, default: None)
	 * @return Resolved attack to execute, or nullptr if no valid attack found
	 */
	UFUNCTION(BlueprintPure, Category = "Combat|Montage Utilities|Attack Resolution", meta = (DisplayName = "Resolve Next Attack"))
	static class UAttackData* ResolveNextAttack(
		class UAttackData* CurrentAttack,
		EInputType InputType,
		bool bComboWindowActive,
		bool bIsHolding,
		class UAttackData* DefaultLightAttack,
		class UAttackData* DefaultHeavyAttack,
		EAttackDirection Direction = EAttackDirection::None
	);

	/**
	 * V2 Context-Aware Attack Resolution (Phase 1)
	 *
	 * Enhanced resolution with:
	 * - Context tag filtering (parry counters, finishers, etc.)
	 * - Cycle detection (prevents infinite loops)
	 * - Resolution path tracking (debugging/telemetry)
	 * - Directional input clear signal (KEY FIX for loop bug)
	 *
	 * Resolution Priority:
	 * 1. Context-sensitive attacks (if RequiredContextTags match ActiveContext)
	 * 2. Directional follow-ups (if bIsHolding + Direction != None)
	 * 3. Normal combo chain (if bComboWindowActive)
	 * 4. Default attacks (fallback)
	 *
	 * @param CurrentAttack - Currently executing attack (null if no combo active)
	 * @param InputType - Input type being triggered (Light/Heavy)
	 * @param Direction - Movement direction (for directional follow-ups)
	 * @param bIsHolding - Is button currently held? (for directional follow-ups)
	 * @param bComboWindowActive - Is combo window currently active?
	 * @param DefaultLightAttack - Fallback light attack
	 * @param DefaultHeavyAttack - Fallback heavy attack
	 * @param ActiveContext - Current runtime context tags (parry counter, low health, etc.)
	 * @param VisitedAttacks - Set of already-visited attacks for cycle detection (pass by reference, modified during traversal)
	 * @return Resolution result with attack, path metadata, and clear signal
	 */
	UFUNCTION(BlueprintPure, Category = "Combat|Montage Utilities|Attack Resolution", meta = (DisplayName = "Resolve Next Attack V2"))
	static FAttackResolutionResult ResolveNextAttack_V2(
		class UAttackData* CurrentAttack,
		EInputType InputType,
		EAttackDirection Direction,
		bool bIsHolding,
		bool bComboWindowActive,
		class UAttackData* DefaultLightAttack,
		class UAttackData* DefaultHeavyAttack,
		const FGameplayTagContainer& ActiveContext,
		UPARAM(ref) TSet<class UAttackData*>& VisitedAttacks
	);

	/**
	 * Get combo attack from current attack data
	 * Traverses AttackData combo pointers based on input type
	 *
	 * @param CurrentAttack - Attack to get combo from
	 * @param InputType - Input type (Light/Heavy)
	 * @param Direction - Movement direction (for directional follow-ups)
	 * @return Next attack in combo chain, or nullptr if chain ends
	 */
	UFUNCTION(BlueprintPure, Category = "Combat|Montage Utilities|Attack Resolution", meta = (DisplayName = "Get Combo Attack"))
	static class UAttackData* GetComboAttack(
		class UAttackData* CurrentAttack,
		EInputType InputType,
		EAttackDirection Direction = EAttackDirection::None
	);
};