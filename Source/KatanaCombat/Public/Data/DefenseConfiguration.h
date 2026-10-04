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

	/** Skeleton bone to classify. On a block the exact hit bone is checked first, then its parent bones from nearest
	 * outward; the first bone listed in Bone Height Rows decides the height. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Defense")
	FName BoneName = NAME_None;

	/** Height (High, Middle or Low) given to a block that lands on this bone, or on a child bone with no closer entry.
	 * Only used to match presentation rows that check Height. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Defense")
	EAttackHeight Height = EAttackHeight::Middle;
};

USTRUCT(BlueprintType)
struct KATANACOMBAT_API FDefensePresentationRow
{
	GENERATED_BODY()

	/** Name for this row, shown in the row list and in defense telemetry. When two rows match equally well, the
	 * alphabetically first name wins. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Defense")
	FName RowName = NAME_None;

	/** Which defense result this row presents. Only Normal Block and Perfect Parry rows are ever used; rows for any
	 * other outcome are never selected. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Defense")
	EDefenseOutcome Outcome = EDefenseOutcome::NormalBlock;

	/** On: the row matches any attack height. Off: it matches only the Height below, which makes the row more
	 * specific so it outranks rows that match any height. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Defense")
	bool bMatchAnyHeight = true;

	/** Height the row requires when Match Any Height is off. On a block it comes from Bone Height Rows (or the
	 * attack's authored height); on a perfect parry it is always the attack's authored height. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Defense",
		meta = (EditCondition = "!bMatchAnyHeight"))
	EAttackHeight Height = EAttackHeight::Middle;

	/** On: the row matches any lane. Off: it matches only the Lane below, which makes the row more specific so it
	 * outranks rows that match any lane. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Defense")
	bool bMatchAnyLane = true;

	/** Lane the row requires when Match Any Lane is off. On a block it comes from the blade's travel direction (see
	 * Center Lane Half Angle); on a perfect parry it is always the attack's authored lane. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Defense",
		meta = (EditCondition = "!bMatchAnyLane"))
	EIncomingAttackLane Lane = EIncomingAttackLane::Center;

	/** On: the row matches any swing shape. Off: it matches only the Swing Shape below, which makes the row more
	 * specific so it outranks rows that match any swing shape. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Defense")
	bool bMatchAnySwingShape = true;

	/** Swing shape the row requires when Match Any Swing Shape is off, compared with the attack's authored swing
	 * shape. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Defense",
		meta = (EditCondition = "!bMatchAnySwingShape"))
	ESwingDirection SwingShape = ESwingDirection::Horizontal;

	/** The row matches only if the attack's tags include all of these (the attack's tags, not the defender's).
	 * Between equally specific rows, more required tags rank higher. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Defense")
	FGameplayTagContainer RequiredTags;

	/** The row does not match if the attack has any of these tags. A row with any required or excluded tag no
	 * longer counts as the generic fallback row. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Defense")
	FGameplayTagContainer ExcludedTags;

	/** Higher wins when matching rows are equally specific and have the same number of Required Tags. It is the
	 * only ranking among generic rows (all Match Any options on, no tags). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Defense")
	int32 Priority = 0;

	/** What the defender plays when this row is chosen. Some payload fields only apply to blocks or only to perfect
	 * parries; see each field. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Defense")
	FDefensePresentationPayload Payload;

	int32 GetExactFieldCount() const;
	bool IsGenericFallback() const;
};

USTRUCT(BlueprintType)
struct KATANACOMBAT_API FAttackerResponsePresentationRow
{
	GENERATED_BODY()

	/** Name for this row, shown in the row list and in defense telemetry. When two rows match equally well, the
	 * alphabetically first name wins. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Defense")
	FName RowName = NAME_None;

	/** Which attacker reaction this row presents. Parry Stagger plays after a perfect parry; Recoil plays when a
	 * block stops an attack tagged Attack.Defense.BlockInterruptible. Continue rows play nothing, and None rows are
	 * never selected. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Defense")
	EAttackerResponse Response = EAttackerResponse::None;

	/** On: the row matches any attack height. Off: it matches only the Height below, which makes the row more
	 * specific so it outranks rows that match any height. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Defense")
	bool bMatchAnyHeight = true;

	/** Height the row requires when Match Any Height is off. On a block it comes from the defender's Bone Height
	 * Rows (or the attack's authored height); on a perfect parry it is always the attack's authored height. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Defense",
		meta = (EditCondition = "!bMatchAnyHeight"))
	EAttackHeight Height = EAttackHeight::Middle;

	/** On: the row matches any lane. Off: it matches only the Lane below, which makes the row more specific so it
	 * outranks rows that match any lane. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Defense")
	bool bMatchAnyLane = true;

	/** Lane the row requires when Match Any Lane is off. On a block it comes from the blade's travel direction (see
	 * the defender's Center Lane Half Angle); on a perfect parry it is always the attack's authored lane. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Defense",
		meta = (EditCondition = "!bMatchAnyLane"))
	EIncomingAttackLane Lane = EIncomingAttackLane::Center;

	/** On: the row matches any swing shape. Off: it matches only the Swing Shape below, which makes the row more
	 * specific so it outranks rows that match any swing shape. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Defense")
	bool bMatchAnySwingShape = true;

	/** Swing shape the row requires when Match Any Swing Shape is off, compared with the attack's authored swing
	 * shape. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Defense",
		meta = (EditCondition = "!bMatchAnySwingShape"))
	ESwingDirection SwingShape = ESwingDirection::Horizontal;

	/** The row matches only if the attack's tags include all of these. Between equally specific rows, more required
	 * tags rank higher. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Defense")
	FGameplayTagContainer RequiredTags;

	/** The row does not match if the attack has any of these tags. A row with any required or excluded tag no
	 * longer counts as the generic fallback row. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Defense")
	FGameplayTagContainer ExcludedTags;

	/** Higher wins when matching rows are equally specific and have the same number of Required Tags. It is the
	 * only ranking among generic rows (all Match Any options on, no tags). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Defense")
	int32 Priority = 0;

	/** What the attacker plays. Only the montage, section, blend-in, rotation warp and (for Recoil) blend-out are
	 * used. Sound, effect, hitstop, translation and parry-bridge settings are ignored on attacker-response rows, both
	 * for what plays and for which row is chosen: these rows are always matched as if a bridge were usable. Continue
	 * rows play nothing. */
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

	/** How far to the side, in degrees from the defender's facing, an attacker can be and still be perfect-parried;
	 * it also slightly affects which attacker the guard faces. It does not limit normal blocks (see Normal Block
	 * Final Tolerance) and does not stop the guard turning toward attackers outside it. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Defense|Alignment", meta = (ClampMin = "0.0", ClampMax = "180.0", Units = "Degrees"))
	float HardGuardConeHalfAngle = 70.0f;

	/** Most this character turns automatically toward one incoming attack, in degrees. It caps guard facing, the
	 * extra turn a perfect parry may count on, turn-to-face on a block or on this character's own stagger or
	 * recoil, and each parry, counter and finisher stage. The guard's budget refills when a new attack is targeted. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Defense|Alignment", meta = (ClampMin = "0.0", ClampMax = "180.0", Units = "Degrees"))
	float MaximumAutomaticTurn = 70.0f;

	/** How fast this character turns automatically, in degrees per second: guard facing, the turn a perfect parry
	 * may count on before the hit lands, and turn-to-face on block, stagger, recoil and parry, counter and finisher
	 * stages. At 0 none of these turns happen, so a perfect parry needs the attacker already within Perfect Parry
	 * Final Tolerance. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Defense|Alignment", meta = (ClampMin = "0.0", Units = "DegreesPerSecond"))
	float DefenseTurnRate = 180.0f;

	/** The block angle: while guarding, a hit is blocked (no damage) only if the attacker is within this many
	 * degrees of the defender's facing at contact; otherwise it lands as a normal hit. This is the only angle a
	 * normal block checks; Hard Guard Cone Half Angle does not apply. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Defense|Alignment", meta = (ClampMin = "0.0", ClampMax = "180.0", Units = "Degrees"))
	float NormalBlockFinalTolerance = 35.0f;

	/** How far off-facing, in degrees, a perfect parry may still end. A perfect parry is only possible if the
	 * attacker's angle is within this plus the turn the defender can still make before the hit (Defense Turn Rate x
	 * time left, capped by Maximum Automatic Turn). It is also the facing tolerance for lining up the parry bridge,
	 * counter and finisher. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Defense|Alignment", meta = (ClampMin = "0.0", ClampMax = "180.0", Units = "Degrees"))
	float PerfectParryFinalTolerance = 10.0f;

	/** On a block, how close to straight-on (in degrees) the blade's travel must be to count as the Center lane;
	 * wider counts as Left or Right. Only matters for presentation rows that match on Lane, and perfect parries use
	 * the attack's authored lane instead. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Defense|Direction", meta = (ClampMin = "0.0", ClampMax = "90.0", Units = "Degrees"))
	float CenterLaneHalfAngle = 12.0f;

	/** Once the guard picks an attacker to face, it keeps that target for at least this many seconds (game time)
	 * before it may switch. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Defense|Threat", meta = (ClampMin = "0.0", Units = "s"))
	float ThreatLockMinSeconds = 0.15f;

	/** After Threat Lock Min Seconds, another attacker takes over the guard's target only if its attack's deadline
	 * (predicted contact, or the end of its parry window if sooner) comes at least this many seconds before the
	 * current target's. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Defense|Threat", meta = (ClampMin = "0.0", Units = "s"))
	float ThreatSwitchLeadSeconds = 0.10f;

	/** How often, in seconds (game time), a held guard re-checks which attacker to face. 0 turns off the timed
	 * re-check, leaving only event-driven updates. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Defense|Threat", meta = (ClampMin = "0.0", Units = "s"))
	float GuardedThreatRefreshSeconds = 0.05f;

	/** How old, in seconds (game time), an attacker's swing prediction may be and still count as reliable. An older
	 * prediction is treated as unreliable: that attack cannot be perfect-parried and ranks lower as a guard target. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Defense|Threat", meta = (ClampMin = "0.0", Units = "s"))
	float MaximumHighConfidencePredictionAge = 0.10f;

	/** How far away, in cm, an attacker can be for the guard to treat it as a threat. It is also capped by the
	 * targeting settings' Max Target Distance, so raising it above that has no effect. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Defense|Threat", meta = (ClampMin = "0.0", Units = "cm"))
	float DefenseThreatRange = 1000.0f;

	/** How far the horizontal camera-look input (0 to 1) must be pushed to take guard facing away from auto-facing.
	 * While at or above it, the guard turns toward the pushed side instead of the target, with the same turn rate
	 * and budget. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Defense|Threat", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float GuardManualOverrideThreshold = 0.25f;

	/** How long, in real seconds (ignores slow motion), the look input must stay below Guard Manual Override
	 * Threshold before the guard goes back to auto-facing its target. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Defense|Threat", meta = (ClampMin = "0.0", Units = "s"))
	float GuardAutoFacingResumeSeconds = 0.10f;

	/** [NOT WIRED] Not read at runtime; changing it has no effect. It was meant to set how long a finished block or
	 * parry stays on record; a fixed 1 second is used instead. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Defense|Interaction", meta = (ClampMin = "0.0", Units = "s"))
	float InteractionTombstoneSeconds = 1.0f;

	/** [NOT WIRED] Not read at runtime; changing it has no effect. It was meant to cap how many finished blocks and
	 * parries stay on record; a fixed 128 is used instead. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Defense|Interaction", meta = (ClampMin = "1"))
	int32 TerminalInteractionCacheCap = 128;

	/** When a perfect parry cannot play its paired parry bridge, how long, in seconds (game time), before the counter
	 * window opens. 0 opens it on the next frame. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Defense|Sequence", meta = (ClampMin = "0.0", Units = "s"))
	float NoMontageParryBridgeSeconds = 0.15f;

	/** How long, in seconds, this character stays staggered after its own attack is perfect-parried; read from the
	 * attacker's configuration, not the defender's. 0 is treated as 1.5 seconds. Enemy AI separately waits its own
	 * Stagger Recovery Time before acting again. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Defense|Sequence", meta = (ClampMin = "0.0", Units = "s"))
	float ParryStaggerDuration = 1.5f;

	/** How long, in real seconds (ignores slow motion), the defender has after a perfect parry to press attack for a
	 * counter. A parry bridge with a Response Window Override above 0 replaces this. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Defense|Sequence", meta = (ClampMin = "0.0", Units = "s"))
	float CounterWindowSeconds = 2.0f;

	/** How long, in real seconds (ignores slow motion), the defender has after a counter to press attack for the
	 * finisher. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Defense|Sequence", meta = (ClampMin = "0.0", Units = "s"))
	float FinisherReadySeconds = 2.0f;

	/** Safety limit, in real seconds: slow motion from a parry, counter or finisher stage is forced back to normal
	 * speed after this long if nothing else ends it. It also covers slow motion in other paired animations such as
	 * finishers (whichever is longer: this or their Slow Motion Duration). 0 means 10 seconds. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Defense|Sequence", meta = (ClampMin = "0.0", Units = "s"))
	float TimeDilationLeaseWatchdogSeconds = 10.0f;

	/** How far, in cm, the block's turn-to-face warp may also move the defender; 0 means no movement. It only has an
	 * effect when the chosen Normal Block row has Enable Rotation Warp on and its montage's motion-warping window
	 * allows translation. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Defense|Translation", meta = (ClampMin = "0.0", Units = "cm"))
	float NormalBlockTranslationAllowance = 0.0f;

	/** [EDITOR/PROOF ONLY] Not read at runtime; changing it has no effect in play. Asset validation and proof
	 * authoring use it as the most horizontal root-motion travel, in cm, a block montage may have. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Defense|Translation", meta = (ClampMin = "0.0", Units = "cm"))
	float NormalBlockTranslationDriftTolerance = 1.0f;

	/** How far, in cm, each character may slide into position for the paired parry bridge; the defender's value
	 * applies to both characters, and the bridge asset's own warp limits also apply (smallest wins). If either
	 * would have to move farther, that bridge is skipped (see Paired Bridge Data on the presentation row for what
	 * plays instead). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Defense|Translation", meta = (ClampMin = "0.0", Units = "cm"))
	float PerfectParryTranslationAllowancePerRole = 75.0f;

	/** Maps skeleton bones to High, Middle or Low for blocks: the hit bone is looked up first, then its parents, and
	 * if none is listed the attack's authored height is used. Only affects presentation rows that match on Height;
	 * perfect parries always use the attack's authored height. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Defense|Skeleton", meta = (TitleProperty = "BoneName"))
	TArray<FDefenseBoneHeightRow> BoneHeightRows;

	/** [NOT WIRED] Not read at runtime; changing it has no effect. This montage is never played (the held guard pose
	 * comes from the Animation Blueprint), but it still loads with this asset. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Defense|Guard")
	TObjectPtr<UAnimMontage> GuardEnterMontage = nullptr;

	/** [NOT WIRED] Not read at runtime; changing it has no effect. This montage is never played (the held guard pose
	 * comes from the Animation Blueprint), but it still loads with this asset. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Defense|Guard")
	TObjectPtr<UAnimMontage> GuardExitMontage = nullptr;

	/** Block sound used when nothing more specific supplies one. Order: a specific (non-generic) Normal Block row with
	 * Override Impact Audio and a sound, then the attack's Blocked Impact Audio, then the generic Normal Block row's
	 * sound, then this. With no sound set here, the attacking weapon's FX pool, then its Hit Sound, are tried. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Defense|Impact")
	FImpactAudioConfig DefaultBlockImpactAudio;

	/** Block effect used when nothing more specific supplies one. Order: a specific (non-generic) Normal Block row with
	 * Override Impact VFX and an effect, then the attack's Blocked Impact VFX, then the generic Normal Block row's
	 * effect, then this. With no effect set here, the attacking weapon's FX pool, then its Hit VFX, are tried. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Defense|Impact")
	FImpactVFXConfig DefaultBlockImpactVFX;

	/** Perfect-parry sound, unless the chosen Perfect Parry row has Override Impact Audio on. The attack's Blocked
	 * Impact Audio is never used for a parry. With no sound set here, the attacking weapon's FX pool, then its Hit
	 * Sound, are tried. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Defense|Impact")
	FImpactAudioConfig DefaultParryImpactAudio;

	/** Perfect-parry effect, unless the chosen Perfect Parry row has Override Impact VFX on. The attack's Blocked
	 * Impact VFX is never used for a parry. With no effect set here, the attacking weapon's FX pool, then its Hit
	 * VFX, are tried. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Defense|Impact")
	FImpactVFXConfig DefaultParryImpactVFX;

	/** What the defender plays on a block or perfect parry, chosen from the defender's own configuration. Only Normal
	 * Block and Perfect Parry rows are ever used. Among matching rows the most specific wins (height, lane, swing
	 * shape), then the most Required Tags, then the highest Priority. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Defense|Presentation", meta = (TitleProperty = "RowName"))
	TArray<FDefensePresentationRow> DefenderPresentationRows;

	/** What the attacker plays when its attack is blocked or perfect-parried, chosen from the attacker's own
	 * configuration. Only Parry Stagger and Recoil rows ever play anything. If the chosen row's montage or section is
	 * unusable, the generic row for that response is tried instead. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Defense|Presentation", meta = (TitleProperty = "RowName"))
	TArray<FAttackerResponsePresentationRow> AttackerResponseRows;

	FDefenseHeightResolution ResolveHeight(
		FName HitBone,
		const TArray<FName>& ParentBoneChain,
		EAttackHeight AuthoredHeight) const;
};
