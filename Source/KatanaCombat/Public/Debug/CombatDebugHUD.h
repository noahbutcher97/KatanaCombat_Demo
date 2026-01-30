// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "CombatTypes.h"
#include "Data/PairedAnimationTypes.h"
#include "CombatDebugHUD.generated.h"

class UCombatComponent;
class ABaseCombatCharacter;
class UHitReactionComponent;

/**
 * Debug arrow data for coordinated 3D/2D rendering
 * 3D renderer draws arrows, HUD draws labels at projected positions
 */
USTRUCT()
struct FDebugArrowData
{
	GENERATED_BODY()

	/** World-space start position */
	FVector WorldStart = FVector::ZeroVector;

	/** World-space end position (where label appears) */
	FVector WorldEnd = FVector::ZeroVector;

	/** Arrow color */
	FColor Color = FColor::White;

	/** Label text */
	FString Label;

	/** Arrow thickness */
	float Thickness = 2.0f;

	/** Arrow head size */
	float ArrowSize = 20.0f;

	/** Is this a dashed arrow? */
	bool bIsDashed = false;

	/** Priority for label collision (higher = more important) */
	int32 LabelPriority = 0;
};

/**
 * Complete debug visualization data - single source of truth
 * Generated once per frame, consumed by both 3D and HUD renderers
 */
USTRUCT()
struct FCombatDebugData
{
	GENERATED_BODY()

	/** Character being debugged */
	UPROPERTY()
	TWeakObjectPtr<ABaseCombatCharacter> Character;

	/** All arrows to render */
	TArray<FDebugArrowData> Arrows;

	/** Arc points for camera-character offset visualization */
	TArray<FVector> ArcPoints;

	/** Arc label position and text */
	FVector ArcLabelPosition = FVector::ZeroVector;
	FString ArcLabel;

	// ========================================================================
	// STATUS INFO (for fixed HUD panel)
	// ========================================================================

	/** Current attack phase */
	EAttackPhase Phase = EAttackPhase::None;

	/** Queue status */
	int32 QueuedCount = 0;
	int32 PendingCount = 0;

	/** Hold state */
	bool bIsHolding = false;
	EInputType HeldInputType = EInputType::None;
	float HoldDuration = 0.0f;
	bool bHoldCompleted = false;

	/** Direction info */
	EInputDirection InputDirection = EInputDirection::None;
	EAttackDirection AttackDirection = EAttackDirection::None;

	/** Movement state */
	bool bMovementDisabled = false;

	/** Current attack name */
	FString CurrentAttackName;

	/** Weapon/Hit detection state */
	bool bHitDetectionActive = false;
	int32 HitActorCount = 0;
	bool bUsingWeaponMeshSockets = false;

	/** Camera-character yaw delta */
	float CamCharYawDelta = 0.0f;

	/** Is data valid this frame? */
	bool bIsValid = false;

	// ========================================================================
	// TARGET/ENEMY INFO (for displaying enemy state)
	// ========================================================================

	/** Current soft-aim target (if any) */
	TWeakObjectPtr<AActor> CurrentTarget;

	/** Target display name */
	FString TargetName;

	/** Target health info */
	float TargetCurrentHealth = 0.0f;
	float TargetMaxHealth = 0.0f;
	bool bTargetIsAlive = true;
	bool bTargetIsDying = false;
	bool bTargetIsDead = false;
};

/**
 * Debug data for paired animation visualization
 * Captures full state of paired animation system for debug rendering
 */
USTRUCT()
struct FPairedAnimDebugData
{
	GENERATED_BODY()

	// ========================================================================
	// STATE INFO
	// ========================================================================

	/** Is character currently in a paired animation? */
	bool bInPairedAnimation = false;

	/** Role in paired animation (Attacker or Victim) */
	FString Role;  // "ATTACKER", "VICTIM", "NONE"

	/** Current paired animation state description */
	FString StateDescription;  // "IDLE", "EXECUTING_FINISHER", "RECEIVING_FINISHER", etc.

	/** Type of paired animation (Finisher, Counter, Parry) */
	EPairedReactionType PairedAnimationType = EPairedReactionType::Counter;

	// ========================================================================
	// PARTNER INFO
	// ========================================================================

	/** Primary paired partner (victim for attacker, attacker for victim) */
	TWeakObjectPtr<AActor> PrimaryPartner;

	/** All paired partners (for multi-target scenarios) */
	TArray<TWeakObjectPtr<AActor>> AllPartners;

	/** Partner display names */
	TArray<FString> PartnerNames;

	// ========================================================================
	// WARP TRACKING
	// ========================================================================

	/** Is attacker warp tracking active? */
	bool bAttackerWarpActive = false;

	/** Is victim warp tracking active? */
	bool bVictimWarpActive = false;

	/** Attacker warp target location */
	FVector AttackerWarpTarget = FVector::ZeroVector;

	/** Victim warp target location */
	FVector VictimWarpTarget = FVector::ZeroVector;

	/** Current warp config offset being used */
	FVector WarpOffset = FVector::ZeroVector;

	/** Distance to warp target */
	float DistanceToWarpTarget = 0.0f;

	/** Max allowed warp distance */
	float MaxWarpDistance = 300.0f;

	/** Current distance between characters */
	float CurrentPartnerDistance = 0.0f;

	// ========================================================================
	// VULNERABILITY INFO (for target)
	// ========================================================================

	/** Name of currently tracked target (for debug display) */
	FString TrackedTargetName;

	/** Is this a soft-aim target (vs hard-locked)? */
	bool bUsingSoftAimTarget = false;

	/** Is current target vulnerable to finisher? */
	bool bTargetVulnerable = false;

	/** Reason for vulnerability */
	EFinisherTriggerReason VulnerabilityReason = EFinisherTriggerReason::None;

	/** Target health percentage (0-1) */
	float TargetHealthPercent = 1.0f;

	/** Health threshold for finisher eligibility */
	float HealthThreshold = 0.25f;

	/** Is target guard broken? */
	bool bTargetGuardBroken = false;

	/** Is target stunned? */
	bool bTargetStunned = false;

	/** Is target already a finisher target (mutex)? */
	bool bTargetIsFinisherTarget = false;

	// ========================================================================
	// SYNC POINT INFO
	// ========================================================================

	/** Current sync point name (if active) */
	FName CurrentSyncPointName = NAME_None;

	/** Time into current montage */
	float MontagePosition = 0.0f;

	/** Expected sync point time */
	float SyncPointTime = 0.0f;

	/** Sync point progress (0-1) */
	float SyncPointProgress = 0.0f;

	/** Alignment distance at sync point */
	float AlignmentDistance = 0.0f;

	/** Max allowed alignment distance */
	float MaxAlignmentDistance = 150.0f;

	/** Was last sync alignment OK? */
	bool bAlignmentOK = true;

	// ========================================================================
	// EFFECTS INFO
	// ========================================================================

	/** Is slow motion currently active? */
	bool bSlowMotionActive = false;

	/** Current time dilation scale */
	float TimeDilationScale = 1.0f;

	/** Slow motion time remaining */
	float SlowMotionRemaining = 0.0f;

	/** Is hitstop currently active? */
	bool bHitstopActive = false;

	/** Hitstop time remaining */
	float HitstopRemaining = 0.0f;

	/** Is camera shake queued/active? */
	bool bCameraShakeQueued = false;

	// ========================================================================
	// INPUT BLOCKING
	// ========================================================================

	/** Is combat input currently blocked? */
	bool bInputBlocked = false;

	// ========================================================================
	// VALIDATION
	// ========================================================================

	/** Is data valid this frame? */
	bool bIsValid = false;
};

/**
 * Combat Debug HUD
 *
 * Renders debug visualization in screen-space for readability.
 * Works in coordination with 3D DrawDebug calls:
 * - 3D system: Draws arrows in world space
 * - This HUD: Draws labels at projected positions + status panel
 *
 * Controlled by CVars in DebugConfig.h:
 * - Combat.Debug.All
 * - Combat.Debug.Direction
 * - Combat.Debug.Phase
 * - etc.
 */
UCLASS()
class KATANACOMBAT_API ACombatDebugHUD : public AHUD
{
	GENERATED_BODY()

public:
	ACombatDebugHUD();

	/** Main HUD draw function - called every frame */
	virtual void DrawHUD() override;

	/** Generate debug data for a character (call once per frame) */
	static FCombatDebugData GenerateDebugData(ABaseCombatCharacter* Character);

	/** Generate paired animation debug data for a character */
	static FPairedAnimDebugData GeneratePairedAnimDebugData(ABaseCombatCharacter* Character);

	/** Draw 3D arrows using DrawDebug functions */
	static void Draw3DArrows(UWorld* World, const FCombatDebugData& Data);

	/** Draw 3D paired animation visualization */
	static void Draw3DPairedAnimVisualization(UWorld* World, ABaseCombatCharacter* Character, const FPairedAnimDebugData& Data);

protected:
	// ========================================================================
	// HUD DRAWING HELPERS
	// ========================================================================

	/** Draw the status panel (fixed screen position) */
	void DrawStatusPanel(const FCombatDebugData& Data);

	/** Draw paired animation debug panel (fixed screen position) */
	void DrawPairedAnimPanel(const FPairedAnimDebugData& Data);

	/** Draw labels at projected arrow positions */
	void DrawArrowLabels(const FCombatDebugData& Data);

	/** Draw arc label if visible */
	void DrawArcLabel(const FCombatDebugData& Data);

	/** Project world position to screen, returns false if behind camera */
	bool ProjectToScreen(const FVector& WorldPos, FVector2D& OutScreenPos) const;

	/** Clamp screen position to viewport with margin */
	FVector2D ClampToViewport(const FVector2D& ScreenPos, float Margin = 20.0f) const;

	/** Get color for attack phase */
	static FColor GetPhaseColor(EAttackPhase Phase);

	// ========================================================================
	// CONFIGURATION
	// ========================================================================

	/** Margin from screen edges for status panel */
	UPROPERTY(EditDefaultsOnly, Category = "Layout")
	float ScreenMargin = 20.0f;

	/** Line height for status text */
	UPROPERTY(EditDefaultsOnly, Category = "Layout")
	float LineHeight = 18.0f;

	/** Font scale for status panel */
	UPROPERTY(EditDefaultsOnly, Category = "Layout")
	float StatusFontScale = 1.2f;

	/** Font scale for arrow labels */
	UPROPERTY(EditDefaultsOnly, Category = "Layout")
	float LabelFontScale = 1.0f;

	/** Offset for labels from projected arrow endpoint */
	UPROPERTY(EditDefaultsOnly, Category = "Layout")
	FVector2D LabelOffset = FVector2D(10.0f, -10.0f);

private:
	/** Cached debug data for current frame */
	FCombatDebugData CachedDebugData;

	/** Cached paired animation debug data for current frame */
	FPairedAnimDebugData CachedPairedAnimData;

	/** Find the player's combat character */
	ABaseCombatCharacter* GetPlayerCombatCharacter() const;
};
