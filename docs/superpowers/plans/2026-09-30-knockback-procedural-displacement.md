# Knockback and Procedural Displacement Implementation Plan (PR 3a-knockback)

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make authored knockback real: an interrupting hit pushes the victim a resolved distance, over real terrain, pausing during hitstop, through a new arbiter-owned procedural displacement executor.

**Architecture:** Pure math (`DisplacementMath`, `KnockbackResolution`) decides the curve and the push. A new `EAlignmentExecutor::ProceduralDisplacement` in `UTargetingComponent` applies it through the live channel: a runtime `URootMotionModifier_ProceduralDisplacement` added to the playing root-motion montage (character movement applies only animation root motion while one plays), otherwise a character-movement `FRootMotionSource_ConstantForce`. `UHitReactionComponent::StartKnockback` acquires the request right after a directional reaction starts.

**Tech Stack:** Unreal Engine 5.6 C++, MotionWarping plugin, CharacterMovementComponent root-motion sources, UE Automation (`IMPLEMENT_SIMPLE_AUTOMATION_TEST`, latent PIE commands).

**Spec:** `docs/superpowers/specs/2026-09-30-combat-feel-and-attack-reach-design.md` (Part A: executor and knockback). Read it before starting.

## Global Constraints

- UE 5.6; editor target `KatanaCombatEditor Win64 Development`.
- Branch: `feat/step3a-knockback`. Create it from `main` once the step 3 docs branch (`docs/step3-specs`, which holds the spec and this plan) has merged. If that branch has not merged yet, create it from `docs/step3-specs` and rebase onto `main` before opening the PR.
- Commit messages: plain message plus bullets and `Rollback checkpoint: <previous commit>`, where `<previous commit>` is the output of `git rev-parse --short HEAD` taken just before that commit. **No AI attribution, trailers, co-author lines or generated-by footers** in commits or PR text.
- Follow CLAUDE.md: `Execute_` for BlueprintNativeEvent interface calls; no new component tick (the targeting tick already exists and is enabled only while smooth requests exist); null-check every weak pointer; no `BlueprintReadOnly` on internal state.
- Existing alignment executors (`CharacterMovement`, `MotionWarping`, `BoundedMovement`) must behave exactly as before; the `KatanaCombat.Targeting.BoundedAlignment` and `KatanaCombat.Defense.Alignment` suites must stay green after every task.
- Step 3 implements only the fixed-curve displacement mode with `EDisplacementClock::ActorTime`. `WorldTime` and the goal-seeking mode arrive with the paired entry step; validation rejects `WorldTime` until then. The negative-generation rule stays `BoundedMovement`-only until then.
- Knockback defaults: Light `{25 cm, 0.2 s, AwayFromAttacker, EaseOut}`, Heavy `{60 cm, 0.25 s, AwayFromAttacker, EaseOut}`.
- Telemetry stays within the existing schema: rows use `EActionReactionTelemetryEvent::AlignmentChanged` with `AlignmentOwner = HitKnockback`. Do not add telemetry enum values or bump any `schema_version`.

### Build and focused test commands

Every "build" step means:

```bash
"/c/Program Files/Epic Games/UE_5.6/Engine/Build/BatchFiles/Build.bat" KatanaCombatEditor Win64 Development "-Project=D:\UnrealProjects\5.6\KatanaCombat\KatanaCombat.uproject" -NoHotReload -WaitMutex > "$TEMP/kb-build.log" 2>&1; grep -E "error [A-Z]+[0-9]+|error:|Result:" "$TEMP/kb-build.log" | tail -8
```

Expected on success: `Result: Succeeded`. Close any running editor first (Live Coding blocks the build).

Every "run tests `<Filter>`" step means (filters are joined with `+`):

```bash
cd /d/UnrealProjects/5.6/KatanaCombat && timeout 540 "/c/Program Files/Epic Games/UE_5.6/Engine/Binaries/Win64/UnrealEditor-Cmd.exe" "D:\UnrealProjects\5.6\KatanaCombat\KatanaCombat.uproject" "-ExecCmds=Automation RunTests <Filter>;Quit" -NullRHI -NoSplash -Unattended -nopause -stdout -FullStdOutLogOutput > "$TEMP/kb-tests.log" 2>&1; echo "passed=$(grep -c 'Result={Success}' $TEMP/kb-tests.log) failed=$(grep -c 'Result={Fail' $TEMP/kb-tests.log)"; grep -E "Result=\{Fail|LogAutomationController: Error" "$TEMP/kb-tests.log" | head -20
```

The final task runs the full baseline: `powershell -NoProfile -ExecutionPolicy Bypass -File "Tools\Codex\run-agent-baseline.ps1"` (about 15 minutes; expect `BASELINE GREEN`).

## Review Focus

1. A second hit lands while the first push is still running: the new push must replace the old one from the victim's current position, never stack two requests (Task 5, `ReplacesRunningPush`).
2. The player is the victim: pushing the player must not change `bOrientRotationToMovement` or `bUseControllerRotationYaw` (Task 4, `PlayerRotationSettingsUntouched`).
3. Hitstop freezes the victim (dilation 0.0001) the instant the push starts: no movement until the freeze ends, then the full push (Task 4, `PausesUnderHitstopDilation`).
4. The push meets a wall or a ledge: it stops at a wall and releases; over a ledge the victim falls rather than hovering (Task 4, `WallBlocksAndReleases`, `LedgeFallsInsteadOfHovering`).
5. A higher-priority alignment (block contact, paired or parry bridge) arrives mid-push: the push suspends without moving the victim and resumes afterwards; entering a paired animation releases it (Task 4, `SuspendsUnderHigherPriority`; Task 5 source test).

---

### Task 1: Displacement types and pure math

**Files:**
- Modify: `Source/KatanaCombat/Public/CombatTypes.h` (enums near line 399; new struct before `FAlignmentRequestSpec` at line 2935)
- Create: `Source/KatanaCombat/Public/Utilities/DisplacementMath.h`
- Create: `Source/KatanaCombat/Private/Utilities/DisplacementMath.cpp`
- Test: `Source/KatanaCombatTest/Private/DisplacementMathTests.cpp`

**Interfaces:**
- Produces:
  - `enum class EDisplacementSpeedProfile : uint8 { Linear, EaseOut };`
  - `enum class EDisplacementClock : uint8 { ActorTime, WorldTime };`
  - `enum class EDisplacementAnimationBlend : uint8 { AddToAnimation, ReplaceAnimation };`
  - `enum class EDisplacementChannel : uint8 { None, Animation, Movement };`
  - `struct FProceduralDisplacement { FVector Direction; float Distance; float Duration; EDisplacementSpeedProfile SpeedProfile; EDisplacementClock Clock; EDisplacementAnimationBlend AnimationBlend; bool operator==(...) const; };`
  - `namespace DisplacementMath { double Progress(EDisplacementSpeedProfile, double U); double DistanceBetween(EDisplacementSpeedProfile, double Distance, double Duration, double T0, double T1); EDisplacementChannel SelectChannel(bool bPlayingRootMotion, bool bHasMotionWarping); bool IsValid(const FProceduralDisplacement&); }`

- [ ] **Step 1: Write the failing tests**

Create `Source/KatanaCombatTest/Private/DisplacementMathTests.cpp`:

```cpp
#include "Misc/AutomationTest.h"
#include "Utilities/DisplacementMath.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDisplacementMathProgressTest, "KatanaCombat.Displacement.Math.Progress",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDisplacementMathProgressTest::RunTest(const FString&)
{
	TestEqual(TEXT("Linear midpoint"), DisplacementMath::Progress(EDisplacementSpeedProfile::Linear, 0.5), 0.5, 1e-9);
	TestEqual(TEXT("EaseOut midpoint is three quarters"), DisplacementMath::Progress(EDisplacementSpeedProfile::EaseOut, 0.5), 0.75, 1e-9);
	TestEqual(TEXT("Clamped below"), DisplacementMath::Progress(EDisplacementSpeedProfile::EaseOut, -1.0), 0.0, 1e-9);
	TestEqual(TEXT("Clamped above"), DisplacementMath::Progress(EDisplacementSpeedProfile::Linear, 2.0), 1.0, 1e-9);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDisplacementMathPartitionTest, "KatanaCombat.Displacement.Math.PartitionsSumToDistance",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDisplacementMathPartitionTest::RunTest(const FString&)
{
	for (const EDisplacementSpeedProfile Profile : {EDisplacementSpeedProfile::Linear, EDisplacementSpeedProfile::EaseOut})
	{
		double Sum = 0.0;
		double T = 0.0;
		for (const double Step : {0.013, 0.05, 0.001, 0.07, 0.2})
		{
			Sum += DisplacementMath::DistanceBetween(Profile, 60.0, 0.25, T, T + Step);
			T += Step;
		}
		TestEqual(TEXT("Uneven partitions sum to the distance"), Sum, 60.0, 1e-6);
	}
	TestTrue(TEXT("EaseOut covers more than half in the first half"),
		DisplacementMath::DistanceBetween(EDisplacementSpeedProfile::EaseOut, 60.0, 0.25, 0.0, 0.125) > 30.0);
	TestEqual(TEXT("Zero duration is a jump at the start"),
		DisplacementMath::DistanceBetween(EDisplacementSpeedProfile::Linear, 10.0, 0.0, 0.0, 0.1), 10.0, 1e-9);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDisplacementMathChannelTest, "KatanaCombat.Displacement.Math.ChannelSelection",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDisplacementMathChannelTest::RunTest(const FString&)
{
	TestEqual(TEXT("Root-motion montage with warping uses the animation channel"), DisplacementMath::SelectChannel(true, true), EDisplacementChannel::Animation);
	TestEqual(TEXT("No root motion uses movement"), DisplacementMath::SelectChannel(false, true), EDisplacementChannel::Movement);
	TestEqual(TEXT("Root motion without warping cannot modify the animation"), DisplacementMath::SelectChannel(true, false), EDisplacementChannel::Movement);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDisplacementMathValidityTest, "KatanaCombat.Displacement.Math.Validity",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDisplacementMathValidityTest::RunTest(const FString&)
{
	FProceduralDisplacement Valid;
	Valid.Direction = FVector(1, 0, 0); Valid.Distance = 25.f; Valid.Duration = 0.2f;
	TestTrue(TEXT("Horizontal unit direction is valid"), DisplacementMath::IsValid(Valid));
	FProceduralDisplacement Tilted = Valid; Tilted.Direction = FVector(0.8, 0, 0.6);
	TestFalse(TEXT("Vertical component is rejected"), DisplacementMath::IsValid(Tilted));
	FProceduralDisplacement Zero = Valid; Zero.Distance = 0.f;
	TestFalse(TEXT("Zero distance is rejected"), DisplacementMath::IsValid(Zero));
	FProceduralDisplacement World = Valid; World.Clock = EDisplacementClock::WorldTime;
	TestFalse(TEXT("WorldTime is not supported until the paired entry step"), DisplacementMath::IsValid(World));
	FProceduralDisplacement NaN = Valid; NaN.Duration = std::numeric_limits<float>::quiet_NaN();
	TestFalse(TEXT("Nonfinite duration is rejected"), DisplacementMath::IsValid(NaN));
	return true;
}
```

Add `#include <limits>` at the top of the file.

- [ ] **Step 2: Build to verify it fails**

Build. Expected: compile errors for the missing header `Utilities/DisplacementMath.h`.

- [ ] **Step 3: Add the types to `CombatTypes.h`**

After `EAlignmentMotionOutcome` (line 417), add:

```cpp
/** How a procedural displacement's speed evolves over its duration. */
UENUM(BlueprintType)
enum class EDisplacementSpeedProfile : uint8
{
	Linear,
	/** Quadratic ease-out: starts at twice the average speed and settles to zero. */
	EaseOut
};

/** Which clock advances a procedural displacement. */
UENUM(BlueprintType)
enum class EDisplacementClock : uint8
{
	/** The owner's dilated time: frozen while hitstop freezes the owner. */
	ActorTime,
	/** Undilated world simulation time (reserved for the paired entry step). */
	WorldTime
};

/** How a displacement combines with a playing root-motion animation. */
UENUM(BlueprintType)
enum class EDisplacementAnimationBlend : uint8
{
	AddToAnimation,
	ReplaceAnimation
};

/** The channel currently applying a displacement (runtime state, not authored). */
enum class EDisplacementChannel : uint8
{
	None,
	Animation,
	Movement
};
```

Before `struct FAlignmentRequestSpec` (line 2935), add:

```cpp
/** Fixed-curve horizontal displacement applied by EAlignmentExecutor::ProceduralDisplacement. */
USTRUCT(BlueprintType)
struct FProceduralDisplacement
{
	GENERATED_BODY()

	/** Horizontal unit direction in world space. */
	UPROPERTY(BlueprintReadOnly, Category = "Alignment")
	FVector Direction = FVector::ZeroVector;

	/** Total distance in centimeters. */
	UPROPERTY(BlueprintReadOnly, Category = "Alignment")
	float Distance = 0.0f;

	/** Seconds on the request's clock. */
	UPROPERTY(BlueprintReadOnly, Category = "Alignment")
	float Duration = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Alignment")
	EDisplacementSpeedProfile SpeedProfile = EDisplacementSpeedProfile::Linear;

	UPROPERTY(BlueprintReadOnly, Category = "Alignment")
	EDisplacementClock Clock = EDisplacementClock::ActorTime;

	UPROPERTY(BlueprintReadOnly, Category = "Alignment")
	EDisplacementAnimationBlend AnimationBlend = EDisplacementAnimationBlend::AddToAnimation;

	bool operator==(const FProceduralDisplacement& Other) const
	{
		return Direction.Equals(Other.Direction, 0.0) && Distance == Other.Distance && Duration == Other.Duration
			&& SpeedProfile == Other.SpeedProfile && Clock == Other.Clock && AnimationBlend == Other.AnimationBlend;
	}
};
```

- [ ] **Step 4: Write the math**

Create `Source/KatanaCombat/Public/Utilities/DisplacementMath.h`:

```cpp
#pragma once

#include "CoreMinimal.h"
#include "CombatTypes.h"

/** Actor-independent math for EAlignmentExecutor::ProceduralDisplacement. Times are in seconds. */
namespace DisplacementMath
{
/** Normalized distance covered at normalized time U (clamped to [0, 1]). */
KATANACOMBAT_API double Progress(EDisplacementSpeedProfile Profile, double U);

/** Distance covered between request times T0 and T1 (clamped to [0, Duration]). Duration <= 0 jumps at T0 == 0. */
KATANACOMBAT_API double DistanceBetween(EDisplacementSpeedProfile Profile, double Distance, double Duration, double T0, double T1);

/** Animation channel only when a root-motion montage plays and motion warping can modify it. */
KATANACOMBAT_API EDisplacementChannel SelectChannel(bool bPlayingRootMotion, bool bHasMotionWarping);

/** Finite, positive, horizontal unit direction, ActorTime clock (WorldTime arrives with paired entry). */
KATANACOMBAT_API bool IsValid(const FProceduralDisplacement& Displacement);
}
```

Create `Source/KatanaCombat/Private/Utilities/DisplacementMath.cpp`:

```cpp
#include "Utilities/DisplacementMath.h"

namespace DisplacementMath
{
double Progress(const EDisplacementSpeedProfile Profile, const double U)
{
	const double Clamped = FMath::Clamp(U, 0.0, 1.0);
	return Profile == EDisplacementSpeedProfile::EaseOut
		? 1.0 - FMath::Square(1.0 - Clamped)
		: Clamped;
}

double DistanceBetween(const EDisplacementSpeedProfile Profile, const double Distance, const double Duration,
	const double T0, const double T1)
{
	if (Duration <= 0.0)
	{
		return T0 <= 0.0 && T1 > T0 ? Distance : 0.0;
	}
	return Distance * (Progress(Profile, T1 / Duration) - Progress(Profile, T0 / Duration));
}

EDisplacementChannel SelectChannel(const bool bPlayingRootMotion, const bool bHasMotionWarping)
{
	return bPlayingRootMotion && bHasMotionWarping ? EDisplacementChannel::Animation : EDisplacementChannel::Movement;
}

bool IsValid(const FProceduralDisplacement& Displacement)
{
	return !Displacement.Direction.ContainsNaN()
		&& FMath::IsNearlyEqual(Displacement.Direction.Size(), 1.0, 1e-3)
		&& FMath::Abs(Displacement.Direction.Z) <= 1e-3
		&& FMath::IsFinite(Displacement.Distance) && Displacement.Distance > 0.0f
		&& FMath::IsFinite(Displacement.Duration) && Displacement.Duration > 0.0f
		&& Displacement.Clock == EDisplacementClock::ActorTime;
}
}
```

- [ ] **Step 5: Build and run tests `KatanaCombat.Displacement.Math`**

Expected: `passed=4 failed=0`.

- [ ] **Step 6: Commit**

```bash
git add Source/KatanaCombat/Public/CombatTypes.h Source/KatanaCombat/Public/Utilities/DisplacementMath.h Source/KatanaCombat/Private/Utilities/DisplacementMath.cpp Source/KatanaCombatTest/Private/DisplacementMathTests.cpp
git commit -m "Add procedural displacement types and pure math

- Speed profile, clock, animation blend and channel enums.
- FProceduralDisplacement fixed-curve description.
- DisplacementMath: progress, distance between times, channel selection
  and validity (ActorTime only until the paired entry step).

Rollback checkpoint: <previous commit>"
```

---

### Task 2: Knockback data and resolution

**Files:**
- Modify: `Source/KatanaCombat/Public/CombatTypes.h` (`FHitReactionInfo` lines 664-755; `FHitReactionEntry` lines 923-930)
- Modify: `Source/KatanaCombat/Public/Data/AttackData.h` (after `MaxHitCount`, line 113)
- Modify: `Source/KatanaCombat/Public/Data/CombatSettings.h` (after `DefenseConfiguration`, line 85)
- Modify: `Source/KatanaCombat/Private/Data/CombatSettings.cpp` (constructor, line 7)
- Modify: `Source/KatanaCombat/Public/Data/HitReactionSettings.h` (lines 85-88)
- Create: `Source/KatanaCombat/Public/Utilities/KnockbackResolution.h`
- Create: `Source/KatanaCombat/Private/Utilities/KnockbackResolution.cpp`
- Test: `Source/KatanaCombatTest/Private/KnockbackResolutionTests.cpp`

**Interfaces:**
- Consumes: `EDisplacementSpeedProfile` (Task 1), `CombatMath::FlatDirection`.
- Produces:
  - `enum class EKnockbackDirection : uint8 { AwayFromAttacker, AlongSwing };`
  - `struct FKnockbackConfig { bool bOverrideDistance; float Distance; bool bOverrideDuration; float Duration; bool bOverrideDirectionMode; EKnockbackDirection DirectionMode; bool bOverrideSpeedProfile; EDisplacementSpeedProfile SpeedProfile; };`
  - `UAttackData::Knockback` (`FKnockbackConfig`), `UAttackData::MaxChargeKnockbackMultiplier` (`float`, default 1).
  - `UCombatSettings::DefaultKnockback` (`TMap<EAttackType, FKnockbackConfig>`).
  - `UHitReactionSettings::KnockbackScale` (`float`, default 1).
  - `FHitReactionInfo::ChargeLevel` (`float`, default 0).
  - `namespace KnockbackResolution { FKnockbackConfig Resolve(const UAttackData*, const UCombatSettings*); float PushDistance(float Distance, float ChargeLevel, float MaxChargeKnockbackMultiplier, float VictimScale); FVector ResolveDirection(EKnockbackDirection, const FVector& AttackerLocation, const FVector& VictimLocation, const FVector& DirectionToAttacker); struct FEligibility { bool bReactionStarted; bool bSuperArmor; bool bReactionsSuppressed; bool bAlive; bool bSettingsPath; bool bInterruptingReaction; }; bool ShouldApply(const FEligibility&); }`. These are the spec's `ResolveKnockback` and `ShouldApplyKnockback`, namespaced.

- [ ] **Step 1: Write the failing tests**

Create `Source/KatanaCombatTest/Private/KnockbackResolutionTests.cpp`:

```cpp
#include "CombatTestHelpers.h"
#include "Utilities/KnockbackResolution.h"
#include "Data/AttackData.h"
#include "Data/CombatSettings.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FKnockbackResolveDefaultsTest, "KatanaCombat.Knockback.Resolution.Defaults",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FKnockbackResolveDefaultsTest::RunTest(const FString&)
{
	UCombatSettings* Settings = NewObject<UCombatSettings>();
	UAttackData* Light = FCombatTestHelpers::CreateTestAttack(EAttackType::Light);
	UAttackData* Heavy = FCombatTestHelpers::CreateTestAttack(EAttackType::Heavy);
	const FKnockbackConfig L = KnockbackResolution::Resolve(Light, Settings);
	TestEqual(TEXT("Light default distance"), L.Distance, 25.0f);
	TestEqual(TEXT("Light default duration"), L.Duration, 0.2f);
	TestEqual(TEXT("Default direction"), L.DirectionMode, EKnockbackDirection::AwayFromAttacker);
	TestEqual(TEXT("Default profile"), L.SpeedProfile, EDisplacementSpeedProfile::EaseOut);
	const FKnockbackConfig H = KnockbackResolution::Resolve(Heavy, Settings);
	TestEqual(TEXT("Heavy default distance"), H.Distance, 60.0f);
	TestEqual(TEXT("Heavy default duration"), H.Duration, 0.25f);
	UAttackData* Special = FCombatTestHelpers::CreateTestAttack(EAttackType::Special);
	TestEqual(TEXT("Missing type resolves to no push"), KnockbackResolution::Resolve(Special, Settings).Distance, 0.0f);
	TestEqual(TEXT("Null attack resolves to no push"), KnockbackResolution::Resolve(nullptr, Settings).Distance, 0.0f);
	TestEqual(TEXT("Null settings without overrides resolve to no push"), KnockbackResolution::Resolve(Light, nullptr).Distance, 0.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FKnockbackResolveOverridesTest, "KatanaCombat.Knockback.Resolution.IndependentOverrides",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FKnockbackResolveOverridesTest::RunTest(const FString&)
{
	UCombatSettings* Settings = NewObject<UCombatSettings>();
	UAttackData* Attack = FCombatTestHelpers::CreateTestAttack(EAttackType::Light);
	Attack->Knockback.bOverrideDuration = true;
	Attack->Knockback.Duration = 0.5f;
	FKnockbackConfig Resolved = KnockbackResolution::Resolve(Attack, Settings);
	TestEqual(TEXT("Overridden duration"), Resolved.Duration, 0.5f);
	TestEqual(TEXT("Inherited distance"), Resolved.Distance, 25.0f);
	Attack->Knockback.bOverrideDistance = true;
	Attack->Knockback.Distance = 90.0f;
	Attack->Knockback.bOverrideDirectionMode = true;
	Attack->Knockback.DirectionMode = EKnockbackDirection::AlongSwing;
	Attack->Knockback.bOverrideSpeedProfile = true;
	Attack->Knockback.SpeedProfile = EDisplacementSpeedProfile::Linear;
	Resolved = KnockbackResolution::Resolve(Attack, Settings);
	TestEqual(TEXT("Overridden distance"), Resolved.Distance, 90.0f);
	TestEqual(TEXT("Overridden direction"), Resolved.DirectionMode, EKnockbackDirection::AlongSwing);
	TestEqual(TEXT("Overridden profile"), Resolved.SpeedProfile, EDisplacementSpeedProfile::Linear);
	TestEqual(TEXT("No combat settings means no push, even with overrides"), KnockbackResolution::Resolve(Attack, nullptr).Distance, 0.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FKnockbackPushDistanceTest, "KatanaCombat.Knockback.Resolution.PushDistance",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FKnockbackPushDistanceTest::RunTest(const FString&)
{
	TestEqual(TEXT("Uncharged"), KnockbackResolution::PushDistance(60.f, 0.f, 2.f, 1.f), 60.0f);
	TestEqual(TEXT("Half charged"), KnockbackResolution::PushDistance(60.f, 0.5f, 2.f, 1.f), 90.0f);
	TestEqual(TEXT("Fully charged"), KnockbackResolution::PushDistance(60.f, 1.f, 2.f, 1.f), 120.0f);
	TestEqual(TEXT("Charge level is clamped"), KnockbackResolution::PushDistance(60.f, 3.f, 2.f, 1.f), 120.0f);
	TestEqual(TEXT("Victim scale applies last"), KnockbackResolution::PushDistance(60.f, 1.f, 2.f, 0.5f), 60.0f);
	TestEqual(TEXT("Immune victim"), KnockbackResolution::PushDistance(60.f, 1.f, 2.f, 0.f), 0.0f);
	TestEqual(TEXT("A multiplier below one never reduces the push"), KnockbackResolution::PushDistance(60.f, 1.f, 0.5f, 1.f), 60.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FKnockbackDirectionTest, "KatanaCombat.Knockback.Resolution.Direction",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FKnockbackDirectionTest::RunTest(const FString&)
{
	const FVector Attacker(0, 0, 0);
	const FVector Victim(100, 0, 20);
	const FVector SidewaysSwing = FVector(0, -1, 0); // DirectionToAttacker = negated blade velocity (+Y swing)
	TestTrue(TEXT("Away ignores the swing"), KnockbackResolution::ResolveDirection(EKnockbackDirection::AwayFromAttacker, Attacker, Victim, SidewaysSwing).Equals(FVector(1, 0, 0), 1e-4));
	const FVector DiagonalSwing = -FVector(1, 1, 0).GetSafeNormal();
	TestTrue(TEXT("Along swing follows the flattened blade velocity"), KnockbackResolution::ResolveDirection(EKnockbackDirection::AlongSwing, Attacker, Victim, DiagonalSwing).Equals(FVector(1, 1, 0).GetSafeNormal(), 1e-4));
	const FVector OverheadChop = -FVector(0.3, 0, -0.95).GetSafeNormal();
	TestTrue(TEXT("Mostly vertical swing falls back to away"), KnockbackResolution::ResolveDirection(EKnockbackDirection::AlongSwing, Attacker, Victim, OverheadChop).Equals(FVector(1, 0, 0), 1e-4));
	const FVector BackSwing = FVector(1, 0, 0); // blade moving toward the attacker
	TestTrue(TEXT("Swing toward the attacker falls back to away"), KnockbackResolution::ResolveDirection(EKnockbackDirection::AlongSwing, Attacker, Victim, BackSwing).Equals(FVector(1, 0, 0), 1e-4));
	TestTrue(TEXT("Stacked actors have no direction"), KnockbackResolution::ResolveDirection(EKnockbackDirection::AwayFromAttacker, FVector(0, 0, 0), FVector(0, 0, 150), SidewaysSwing).IsZero());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FKnockbackEligibilityTest, "KatanaCombat.Knockback.Resolution.Eligibility",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FKnockbackEligibilityTest::RunTest(const FString&)
{
	KnockbackResolution::FEligibility Eligible;
	Eligible.bReactionStarted = true;
	Eligible.bSettingsPath = true;
	TestTrue(TEXT("Started interrupting reaction pushes"), KnockbackResolution::ShouldApply(Eligible));
	auto Without = [&](auto Mutate) { KnockbackResolution::FEligibility Copy = Eligible; Mutate(Copy); return KnockbackResolution::ShouldApply(Copy); };
	// Blocked and parried hits never start a directional reaction (pinned by the Knockback.Architecture source tests).
	TestFalse(TEXT("Blocked or parried hit: no reaction started"), Without([](auto& E) { E.bReactionStarted = false; }));
	TestFalse(TEXT("Super armor"), Without([](auto& E) { E.bSuperArmor = true; }));
	TestFalse(TEXT("Suppressed paired state"), Without([](auto& E) { E.bReactionsSuppressed = true; }));
	TestFalse(TEXT("Dying victim"), Without([](auto& E) { E.bAlive = false; }));
	TestFalse(TEXT("Legacy fallback path"), Without([](auto& E) { E.bSettingsPath = false; }));
	TestFalse(TEXT("Non-interrupting reaction (step 4 additive flinch)"), Without([](auto& E) { E.bInterruptingReaction = false; }));
	return true;
}
```

- [ ] **Step 2: Build to verify it fails**

Build. Expected: errors for `Utilities/KnockbackResolution.h` and the missing `Knockback` members.

- [ ] **Step 3: Add the data types**

In `CombatTypes.h`, after the `EDisplacementChannel` enum from Task 1, add:

```cpp
/** Direction policy for a knockback push. */
UENUM(BlueprintType)
enum class EKnockbackDirection : uint8
{
	/** Straight away from the attacker (horizontal). */
	AwayFromAttacker,
	/** Along the blade's horizontal velocity at contact; falls back to AwayFromAttacker when that is mostly vertical or points toward the attacker. */
	AlongSwing
};

/**
 * Knockback authored on an attack (fields overridden independently) or as a per-attack-type
 * default in UCombatSettings::DefaultKnockback (where the override toggles are ignored).
 */
USTRUCT(BlueprintType)
struct FKnockbackConfig
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Knockback", meta = (InlineEditConditionToggle))
	bool bOverrideDistance = false;

	/** Uncharged push distance in centimeters. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Knockback", meta = (EditCondition = "bOverrideDistance", ClampMin = "0.0", ClampMax = "500.0"))
	float Distance = 0.0f;

	UPROPERTY(EditAnywhere, Category = "Knockback", meta = (InlineEditConditionToggle))
	bool bOverrideDuration = false;

	/** Seconds over which the push happens (on the victim's own time). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Knockback", meta = (EditCondition = "bOverrideDuration", ClampMin = "0.05", ClampMax = "1.0"))
	float Duration = 0.2f;

	UPROPERTY(EditAnywhere, Category = "Knockback", meta = (InlineEditConditionToggle))
	bool bOverrideDirectionMode = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Knockback", meta = (EditCondition = "bOverrideDirectionMode"))
	EKnockbackDirection DirectionMode = EKnockbackDirection::AwayFromAttacker;

	UPROPERTY(EditAnywhere, Category = "Knockback", meta = (InlineEditConditionToggle))
	bool bOverrideSpeedProfile = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Knockback", meta = (EditCondition = "bOverrideSpeedProfile"))
	EDisplacementSpeedProfile SpeedProfile = EDisplacementSpeedProfile::EaseOut;
};
```

In `FHitReactionInfo`, after `HitConfidence` (line 735), add:

```cpp
    /** Attacker's latched charge level (0..1) for this hit; scales knockback. Set by the damage sites. */
    UPROPERTY(BlueprintReadWrite, Category = "Hit Reaction|Metadata")
    float ChargeLevel = 0.0f;
```

and add `, ChargeLevel(0.0f)` after `, HitConfidence(1.0f)` in its constructor.

In `FHitReactionEntry`, delete the `PHYSICS` section (lines 923-930: the comment banner, the `[NOT WIRED]` comment, the `UPROPERTY` and `float KnockbackForce = 200.0f;`).

In `AttackData.h`, after `int32 MaxHitCount = 0;` (line 113), add:

```cpp

    /** Knockback for this attack; fields not overridden come from the attacker's CombatSettings default for this attack type. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Damage|Knockback")
    FKnockbackConfig Knockback;
```

In `AttackData.h`, after `float MaxChargeDamageMultiplier = 2.5f;` (line 203), add:

```cpp

    /** Knockback multiplier at full charge (1 = charge does not affect knockback). */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attack Type|Heavy Attack",
        meta = (EditCondition = "AttackType == EAttackType::Heavy", EditConditionHides, ClampMin = "1.0"))
    float MaxChargeKnockbackMultiplier = 1.0f;
```

In `CombatSettings.h`, add `#include "CombatTypes.h"` after `#include "Engine/DataAsset.h"`, and after the `DefenseConfiguration` property (line 85) add:

```cpp

    /** Knockback per attack type; attacks override fields individually. Types missing from the map do not push. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Knockback")
    TMap<EAttackType, FKnockbackConfig> DefaultKnockback;
```

Replace the `UCombatSettings` constructor body in `CombatSettings.cpp`:

```cpp
UCombatSettings::UCombatSettings()
{
	FKnockbackConfig Light;
	Light.Distance = 25.0f;
	Light.Duration = 0.2f;
	Light.DirectionMode = EKnockbackDirection::AwayFromAttacker;
	Light.SpeedProfile = EDisplacementSpeedProfile::EaseOut;
	DefaultKnockback.Add(EAttackType::Light, Light);

	FKnockbackConfig Heavy = Light;
	Heavy.Distance = 60.0f;
	Heavy.Duration = 0.25f;
	DefaultKnockback.Add(EAttackType::Heavy, Heavy);
}
```

In `HitReactionSettings.h`, replace lines 85-88 with:

```cpp
    /** Victim-side knockback distance scale: 1 = normal, 0 = immune (e.g. 0.3 for a large enemy). Duration is never scaled. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Parameters",
        meta = (ClampMin = "0.0", ClampMax = "5.0"))
    float KnockbackScale = 1.0f;
```

- [ ] **Step 4: Write the resolution functions**

Create `Source/KatanaCombat/Public/Utilities/KnockbackResolution.h`:

```cpp
#pragma once

#include "CoreMinimal.h"
#include "CombatTypes.h"

class UAttackData;
class UCombatSettings;

/** Pure knockback rules. The owning component supplies actors and settings. */
namespace KnockbackResolution
{
/** Field-wise: the attack's override when set, else the settings default for its type. A null attack or null settings gives Distance 0 (spec: no settings, no push), as does a missing type default without a distance override. */
KATANACOMBAT_API FKnockbackConfig Resolve(const UAttackData* AttackData, const UCombatSettings* Settings);

/** Distance x Lerp(1, max(1, MaxChargeKnockbackMultiplier), clamp(ChargeLevel, 0, 1)) x max(0, VictimScale). */
KATANACOMBAT_API float PushDistance(float Distance, float ChargeLevel, float MaxChargeKnockbackMultiplier, float VictimScale);

/** Horizontal unit direction, or zero when the actors are stacked vertically. */
KATANACOMBAT_API FVector ResolveDirection(EKnockbackDirection Mode, const FVector& AttackerLocation,
	const FVector& VictimLocation, const FVector& DirectionToAttacker);

struct FEligibility
{
	/** A directional hit reaction started. Blocked and parried hits never start one. */
	bool bReactionStarted = false;
	bool bSuperArmor = false;
	bool bReactionsSuppressed = false;
	bool bAlive = true;
	/** The reaction came from the settings-driven directional path (not the legacy fallback). */
	bool bSettingsPath = false;
	/** Step 4 sets false for additive flinches; every reaction interrupts until then. */
	bool bInterruptingReaction = true;
};

KATANACOMBAT_API bool ShouldApply(const FEligibility& Eligibility);
}
```

Create `Source/KatanaCombat/Private/Utilities/KnockbackResolution.cpp`:

```cpp
#include "Utilities/KnockbackResolution.h"
#include "Utilities/CombatMath.h"
#include "Data/AttackData.h"
#include "Data/CombatSettings.h"

namespace KnockbackResolution
{
FKnockbackConfig Resolve(const UAttackData* AttackData, const UCombatSettings* Settings)
{
	FKnockbackConfig Result;
	Result.Distance = 0.0f;
	if (!AttackData || !Settings)
	{
		return Result;
	}

	const FKnockbackConfig* Default = Settings->DefaultKnockback.Find(AttackData->AttackType);
	const FKnockbackConfig& Authored = AttackData->Knockback;
	Result.Distance = Authored.bOverrideDistance ? Authored.Distance : (Default ? Default->Distance : 0.0f);
	Result.Duration = Authored.bOverrideDuration ? Authored.Duration : (Default ? Default->Duration : 0.2f);
	Result.DirectionMode = Authored.bOverrideDirectionMode ? Authored.DirectionMode
		: (Default ? Default->DirectionMode : EKnockbackDirection::AwayFromAttacker);
	Result.SpeedProfile = Authored.bOverrideSpeedProfile ? Authored.SpeedProfile
		: (Default ? Default->SpeedProfile : EDisplacementSpeedProfile::EaseOut);
	return Result;
}

float PushDistance(const float Distance, const float ChargeLevel, const float MaxChargeKnockbackMultiplier, const float VictimScale)
{
	const float ChargeMultiplier = FMath::Lerp(1.0f, FMath::Max(1.0f, MaxChargeKnockbackMultiplier), FMath::Clamp(ChargeLevel, 0.0f, 1.0f));
	return FMath::Max(0.0f, Distance) * ChargeMultiplier * FMath::Max(0.0f, VictimScale);
}

FVector ResolveDirection(const EKnockbackDirection Mode, const FVector& AttackerLocation,
	const FVector& VictimLocation, const FVector& DirectionToAttacker)
{
	const FVector Away = CombatMath::FlatDirection(AttackerLocation, VictimLocation);
	if (Mode != EKnockbackDirection::AlongSwing || Away.IsZero())
	{
		return Away;
	}

	const FVector Swing = -DirectionToAttacker;
	const FVector FlatSwing(Swing.X, Swing.Y, 0.0);
	const double SwingLength = Swing.Size();
	if (SwingLength <= UE_SMALL_NUMBER || FlatSwing.Size() < 0.5 * SwingLength)
	{
		return Away;
	}
	const FVector Along = FlatSwing.GetSafeNormal();
	return FVector::DotProduct(Along, Away) > 0.0 ? Along : Away;
}

bool ShouldApply(const FEligibility& Eligibility)
{
	return Eligibility.bReactionStarted
		&& !Eligibility.bSuperArmor
		&& !Eligibility.bReactionsSuppressed
		&& Eligibility.bAlive
		&& Eligibility.bSettingsPath
		&& Eligibility.bInterruptingReaction;
}
}
```

- [ ] **Step 5: Fix references to the removed and renamed fields**

Run: `cd /d/UnrealProjects/5.6/KatanaCombat && git grep -n "KnockbackForce\|GlobalKnockbackMultiplier" -- Source docs/guides docs/architecture CLAUDE.md`
Expected remaining hits: only documentation. Update each documentation mention to describe `UAttackData::Knockback`, `UCombatSettings::DefaultKnockback` and `UHitReactionSettings::KnockbackScale`. Any C++ hit is a compile error to fix by removing the stale read.

- [ ] **Step 6: Build and run tests `KatanaCombat.Knockback.Resolution+KatanaCombat.HitReaction+KatanaCombat.Displacement`**

Expected: all pass (the Knockback.Resolution group reports 5 passes).

- [ ] **Step 7: Commit**

```bash
git add -A Source/KatanaCombat Source/KatanaCombatTest/Private/KnockbackResolutionTests.cpp docs
git commit -m "Add knockback data and pure resolution rules

- FKnockbackConfig with independent per-field overrides on UAttackData;
  per-type defaults in UCombatSettings (Light 25 cm / 0.2 s, Heavy 60 cm /
  0.25 s, AwayFromAttacker, EaseOut).
- UHitReactionSettings::KnockbackScale replaces GlobalKnockbackMultiplier;
  FHitReactionEntry::KnockbackForce removed (saved values ignored).
- UAttackData::MaxChargeKnockbackMultiplier and FHitReactionInfo::ChargeLevel
  (inert until the charge PR).
- KnockbackResolution: resolve, push distance, direction with AlongSwing
  fallbacks, eligibility.

Rollback checkpoint: <previous commit>"
```

---

### Task 3: Procedural displacement root-motion modifier

**Files:**
- Create: `Source/KatanaCombat/Public/Animation/RootMotionModifier_ProceduralDisplacement.h`
- Create: `Source/KatanaCombat/Private/Animation/RootMotionModifier_ProceduralDisplacement.cpp`
- Test: `Source/KatanaCombatTest/Private/ProceduralDisplacementModifierTests.cpp`

**Interfaces:**
- Consumes: `FProceduralDisplacement`, `DisplacementMath::DistanceBetween` (Task 1).
- Produces:
  - `UCLASS() class URootMotionModifier_ProceduralDisplacement : public URootMotionModifier` with `void Configure(const FProceduralDisplacement& Displacement, double StartElapsed);`, `double GetElapsed() const;`, `bool IsComplete() const;`, `double ConsumeAnimationTravel();` (the animation's own world travel along the push direction since the last call; zero under `ReplaceAnimation`), `static FTransform ApplyDisplacement(const FTransform& InRootMotion, const FVector& LocalDelta, EDisplacementAnimationBlend Blend);`, and the `ProcessRootMotion` override.

Engine facts this task relies on (verified in UE 5.6 MotionWarping source):
- `UMotionWarpingComponent::ProcessRootMotionPreConvertToWorld` calls `UpdateWithContext` (each modifier's `Update`, then removal of `MarkedForRemoval` modifiers), then `ProcessRootMotion(FinalRootMotion, DeltaSeconds)` on `Active` modifiers only. `DeltaSeconds` is the movement delta, which is already dilated.
- `URootMotionModifier::Update` activates when `PreviousPosition >= StartTime`. It marks the modifier for removal when the context animation differs from `Animation`, or when `PreviousPosition >= EndTime`.
- The character adapter's context uses the root-motion montage (`Context.Animation = Montage`) and `PlayRate = Montage->RateScale * Instance->GetPlayRate()`.
- `GetOwnerAdapter()` returns null when the outer is not a `UMotionWarpingComponent`, so the unit tests below run without an owner.
- The base constructor, `Update` and `OnStateChanged` are exported (`UE_API`), so subclassing across modules links.

- [ ] **Step 1: Write the failing tests**

Create `Source/KatanaCombatTest/Private/ProceduralDisplacementModifierTests.cpp`:

```cpp
#include "Misc/AutomationTest.h"
#include "Animation/RootMotionModifier_ProceduralDisplacement.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProceduralDisplacementBlendTest, "KatanaCombat.Displacement.Modifier.Blend",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FProceduralDisplacementBlendTest::RunTest(const FString&)
{
	const FTransform Authored(FRotator(0, 15, 0), FVector(-3, 1, 0));
	const FVector Push(5, 0, 0);
	const FTransform Added = URootMotionModifier_ProceduralDisplacement::ApplyDisplacement(Authored, Push, EDisplacementAnimationBlend::AddToAnimation);
	TestTrue(TEXT("Add keeps the authored step and adds the push"), Added.GetTranslation().Equals(FVector(2, 1, 0), 1e-4));
	TestTrue(TEXT("Add keeps the authored rotation"), Added.GetRotation().Equals(Authored.GetRotation(), 1e-4));
	const FTransform Replaced = URootMotionModifier_ProceduralDisplacement::ApplyDisplacement(Authored, Push, EDisplacementAnimationBlend::ReplaceAnimation);
	TestTrue(TEXT("Replace substitutes the translation"), Replaced.GetTranslation().Equals(Push, 1e-4));
	TestTrue(TEXT("Replace keeps the authored rotation"), Replaced.GetRotation().Equals(Authored.GetRotation(), 1e-4));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProceduralDisplacementElapsedTest, "KatanaCombat.Displacement.Modifier.ElapsedTracksDeltaSeconds",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FProceduralDisplacementElapsedTest::RunTest(const FString&)
{
	auto* Modifier = NewObject<URootMotionModifier_ProceduralDisplacement>();
	FProceduralDisplacement Displacement;
	Displacement.Direction = FVector(1, 0, 0); Displacement.Distance = 30.f; Displacement.Duration = 0.2f;
	Modifier->Configure(Displacement, 0.05);
	TestEqual(TEXT("Starts at the resumed elapsed time"), Modifier->GetElapsed(), 0.05, 1e-9);
	Modifier->ProcessRootMotion(FTransform::Identity, 0.0f);
	TestEqual(TEXT("A frozen frame (hitstop) does not advance"), Modifier->GetElapsed(), 0.05, 1e-9);
	Modifier->ProcessRootMotion(FTransform::Identity, 0.1f);
	TestEqual(TEXT("Advances by DeltaSeconds"), Modifier->GetElapsed(), 0.15, 1e-6);
	TestEqual(TEXT("No owner mesh means no measured animation travel"), Modifier->ConsumeAnimationTravel(), 0.0, 1e-9);
	Modifier->ProcessRootMotion(FTransform::Identity, 1.0f);
	TestTrue(TEXT("Clamped at the duration"), Modifier->IsComplete());
	const FTransform After = Modifier->ProcessRootMotion(FTransform(FVector(4, 0, 0)), 0.1f);
	TestTrue(TEXT("A complete modifier passes animation through"), After.GetTranslation().Equals(FVector(4, 0, 0), 1e-4));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProceduralDisplacementDirectionTest, "KatanaCombat.Displacement.Modifier.WorldDirectionOnCharacter",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FProceduralDisplacementDirectionTest::RunTest(const FString&)
{
	UWorld* World = FCombatTestHelpers::CreateTestWorld();
	APlayerCharacter* Character = FCombatTestHelpers::CreateTestPlayerCharacter(World, FVector(0, 0, 100));
	Character->SetActorRotation(FRotator(0, 37, 0)); // the mesh also carries its own yaw offset
	UMotionWarpingComponent* Warping = Character->MotionWarpingComponent;
	// The test world initializes actors before spawning, so InitializeComponent created the adapter.
	if (TestNotNull(TEXT("Character has motion warping"), Warping)
		&& TestNotNull(TEXT("Motion warping adapter initialized"), Warping->GetOwnerAdapter()))
	{
		const FTransform MeshTransform = Character->GetMesh()->GetComponentTransform();
		FProceduralDisplacement Displacement;
		Displacement.Direction = FVector(0, 1, 0);
		Displacement.Distance = 10.f;
		Displacement.Duration = 1.f;
		Displacement.AnimationBlend = EDisplacementAnimationBlend::ReplaceAnimation;

		auto* Replacing = NewObject<URootMotionModifier_ProceduralDisplacement>(Warping);
		Replacing->Configure(Displacement, 0.0);
		const FTransform Local = Replacing->ProcessRootMotion(FTransform::Identity, 0.5f);
		TestTrue(TEXT("Mesh-local delta converts back to the world push"),
			MeshTransform.TransformVector(Local.GetTranslation()).Equals(FVector(0, 5, 0), 1e-3));

		Displacement.AnimationBlend = EDisplacementAnimationBlend::AddToAnimation;
		auto* Adding = NewObject<URootMotionModifier_ProceduralDisplacement>(Warping);
		Adding->Configure(Displacement, 0.0);
		const FVector AnimationLocal = MeshTransform.InverseTransformVector(FVector(0, 2, 0));
		Adding->ProcessRootMotion(FTransform(AnimationLocal), 0.5f);
		TestEqual(TEXT("Kept animation travel is measured in world space"), Adding->ConsumeAnimationTravel(), 2.0, 1e-3);
		TestEqual(TEXT("Consuming resets the measurement"), Adding->ConsumeAnimationTravel(), 0.0, 1e-9);
	}
	FCombatTestHelpers::DestroyTestWorld(World);
	return true;
}
```

Add these includes at the top of the file: `CombatTestHelpers.h`, `MotionWarpingComponent.h`, `Components/SkeletalMeshComponent.h`.

- [ ] **Step 2: Build to verify it fails**

Build. Expected: missing header error.

- [ ] **Step 3: Write the modifier**

Create `Source/KatanaCombat/Public/Animation/RootMotionModifier_ProceduralDisplacement.h`:

```cpp
#pragma once

#include "CoreMinimal.h"
#include "RootMotionModifier.h"
#include "CombatTypes.h"
#include "RootMotionModifier_ProceduralDisplacement.generated.h"

/**
 * Runtime-only modifier added by UTargetingComponent's ProceduralDisplacement executor to the
 * playing root-motion montage. Character movement applies only animation root motion while
 * such a montage plays, so the push is delivered inside that root motion. Its clock advances
 * by the root-motion DeltaSeconds, so a hitstop-frozen owner does not move.
 */
UCLASS()
class KATANACOMBAT_API URootMotionModifier_ProceduralDisplacement : public URootMotionModifier
{
	GENERATED_BODY()

public:
	void Configure(const FProceduralDisplacement& InDisplacement, double StartElapsed);
	double GetElapsed() const { return Elapsed; }
	bool IsComplete() const { return Elapsed >= Displacement.Duration; }

	/** World travel of the animation's own root motion along the push direction since the last call (kept motion only). */
	double ConsumeAnimationTravel()
	{
		const double Travel = AnimationTravel;
		AnimationTravel = 0.0;
		return Travel;
	}

	virtual FTransform ProcessRootMotion(const FTransform& InRootMotion, float DeltaSeconds) override;

	/** Combine a mesh-local push delta with the animation's root motion. */
	static FTransform ApplyDisplacement(const FTransform& InRootMotion, const FVector& LocalDelta, EDisplacementAnimationBlend Blend);

private:
	FProceduralDisplacement Displacement;
	double Elapsed = 0.0;
	double AnimationTravel = 0.0;
};
```

Create `Source/KatanaCombat/Private/Animation/RootMotionModifier_ProceduralDisplacement.cpp`:

```cpp
#include "Animation/RootMotionModifier_ProceduralDisplacement.h"
#include "Utilities/DisplacementMath.h"
#include "MotionWarpingAdapter.h"
#include "Components/SkeletalMeshComponent.h"

void URootMotionModifier_ProceduralDisplacement::Configure(const FProceduralDisplacement& InDisplacement, const double StartElapsed)
{
	Displacement = InDisplacement;
	Elapsed = FMath::Clamp(StartElapsed, 0.0, static_cast<double>(InDisplacement.Duration));
}

FTransform URootMotionModifier_ProceduralDisplacement::ProcessRootMotion(const FTransform& InRootMotion, const float DeltaSeconds)
{
	if (IsComplete() || !FMath::IsFinite(DeltaSeconds) || DeltaSeconds < 0.0f)
	{
		return InRootMotion;
	}

	const double T0 = Elapsed;
	Elapsed = FMath::Min(static_cast<double>(Displacement.Duration), Elapsed + DeltaSeconds);
	const double Step = DisplacementMath::DistanceBetween(
		Displacement.SpeedProfile, Displacement.Distance, Displacement.Duration, T0, Elapsed);

	FVector LocalDelta = FVector::ZeroVector;
	if (const UMotionWarpingBaseAdapter* Adapter = GetOwnerAdapter())
	{
		if (const USkeletalMeshComponent* Mesh = Adapter->GetMesh())
		{
			const FTransform& ComponentTransform = Mesh->GetComponentTransform();
			// Inverse of USkeletalMeshComponent::ConvertLocalRootMotionToWorld for a translation.
			LocalDelta = ComponentTransform.InverseTransformVector(Displacement.Direction * Step);
			if (Displacement.AnimationBlend == EDisplacementAnimationBlend::AddToAnimation)
			{
				// The executor's blocked check expects this motion too.
				AnimationTravel += FVector::DotProduct(
					ComponentTransform.TransformVector(InRootMotion.GetTranslation()), Displacement.Direction);
			}
		}
	}
	return ApplyDisplacement(InRootMotion, LocalDelta, Displacement.AnimationBlend);
}

FTransform URootMotionModifier_ProceduralDisplacement::ApplyDisplacement(
	const FTransform& InRootMotion, const FVector& LocalDelta, const EDisplacementAnimationBlend Blend)
{
	FTransform Result = InRootMotion;
	Result.SetTranslation(Blend == EDisplacementAnimationBlend::AddToAnimation
		? InRootMotion.GetTranslation() + LocalDelta
		: LocalDelta);
	return Result;
}
```

- [ ] **Step 4: Build and run tests `KatanaCombat.Displacement.Modifier`**

Expected: `passed=3 failed=0`. Both `KatanaCombat.Build.cs` and `KatanaCombatTest.Build.cs` already depend on `MotionWarping`.

- [ ] **Step 5: Commit**

```bash
git add Source/KatanaCombat/Public/Animation/RootMotionModifier_ProceduralDisplacement.h Source/KatanaCombat/Private/Animation/RootMotionModifier_ProceduralDisplacement.cpp Source/KatanaCombatTest/Private/ProceduralDisplacementModifierTests.cpp
git commit -m "Add a root-motion modifier that delivers procedural displacement

Adds or replaces the playing montage's root-motion translation with the
displacement curve's per-frame delta, converted into mesh-local space, and
advances on the root-motion DeltaSeconds so hitstop pauses it.

Rollback checkpoint: <previous commit>"
```

---

### Task 4: ProceduralDisplacement executor in the alignment arbiter

**Files:**
- Modify: `Source/KatanaCombat/Public/CombatTypes.h` (`EDefenseAlignmentPriority` line 390, `EAlignmentExecutor` line 400, `FAlignmentRequestSpec` line 2936)
- Modify: `Source/KatanaCombat/Public/Core/TargetingComponent.h` (record struct line 390; private helpers near line 565)
- Modify: `Source/KatanaCombat/Private/Core/TargetingComponent.cpp` (`TickComponent` 141; `AcquireAlignmentRequest` 778-855; `UpdateAlignmentRequest` 857-886; `ReleaseAlignmentRequest` 888-906; `ReleaseAllAlignmentRequests` 908-935; `ValidateAlignmentSpec` 1040-1085; `HasSmoothAlignmentRequest` 1152-1163; `ReevaluateAlignmentRequests` 1165-1212)
- Modify: `Source/KatanaCombat/Private/Core/TargetingComponent_BoundedAlignment.cpp` (`GetAlignmentMotionState`, lines 6-14)
- Create: `Source/KatanaCombat/Private/Core/TargetingComponent_ProceduralDisplacement.cpp`
- Modify: `Source/KatanaCombat/Public/Debug/DebugConfig.h`, `Source/KatanaCombat/Private/Debug/DebugConfig.cpp` (`Combat.Debug.Knockback`)
- Test: `Source/KatanaCombatTest/Private/ProceduralDisplacementExecutorTests.cpp`

**Interfaces:**
- Consumes: Task 1 types and math, Task 3 modifier.
- Produces:
  - `EAlignmentExecutor::ProceduralDisplacement` (appended after `BoundedMovement`).
  - `EDefenseAlignmentPriority::HitKnockback` (between `ActiveAttackWarp` and `BlockContact`).
  - `FAlignmentRequestSpec::Displacement` (`FProceduralDisplacement`) and `FAlignmentRequestSpec::bReleaseWhenFinished` (`bool`, default false).
  - `UTargetingComponent::GetAlignmentMotionState` also serves `ProceduralDisplacement` (`Elapsed` = request time, `Travel` = measured progress along the direction).

- [ ] **Step 1: Prove the movement-channel premise**

The test world never calls `BeginPlay`, and no existing test ticks character movement. Before building on it, prove a root-motion source moves a character here. Create `Source/KatanaCombatTest/Private/ProceduralDisplacementExecutorTests.cpp` with the fixture and the premise test:

```cpp
#include "CombatTestHelpers.h"
#include "Core/TargetingComponent.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/RootMotionSource.h"

namespace
{
struct FDisplacementFixture
{
	UWorld* World = FCombatTestHelpers::CreateTestWorld();
	APlayerCharacter* Character = nullptr;

	FDisplacementFixture()
	{
		Box(FVector(0, 0, -10), FVector(2000, 2000, 10));
		Character = FCombatTestHelpers::CreateTestPlayerCharacter(World, FVector(0, 0, 100));
		UCharacterMovementComponent* Movement = Character->GetCharacterMovement();
		Movement->bRunPhysicsWithNoController = true;
		Movement->SetMovementMode(MOVE_Walking);
		Character->SetActorRotation(FRotator::ZeroRotator);
		Settle();
	}
	~FDisplacementFixture() { FCombatTestHelpers::DestroyTestWorld(World); }

	UBoxComponent* Box(const FVector& Center, const FVector& Extent, const FRotator& Rotation = FRotator::ZeroRotator)
	{
		AActor* Actor = World->SpawnActor<AActor>();
		auto* Component = NewObject<UBoxComponent>(Actor);
		Actor->AddInstanceComponent(Component);
		Actor->SetRootComponent(Component);
		Component->SetBoxExtent(Extent);
		Component->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		Component->SetCollisionObjectType(ECC_WorldStatic);
		Component->SetCollisionResponseToAllChannels(ECR_Block);
		Component->CanCharacterStepUpOn = ECB_Yes;
		Component->RegisterComponent();
		Component->SetWorldLocationAndRotation(Center, Rotation);
		return Component;
	}

	UTargetingComponent* Targeting() const { return Character->TargetingComponent; }
	UCharacterMovementComponent* Movement() const { return Character->GetCharacterMovement(); }

	/** One frame: movement first, then the targeting executor (its registered prerequisite order). */
	void Step(const float ComponentDelta)
	{
		Movement()->TickComponent(ComponentDelta, LEVELTICK_All, nullptr);
		Targeting()->ResetAlignmentExecutionFrameForTesting();
		Targeting()->TickComponent(ComponentDelta, LEVELTICK_All, nullptr);
	}
	void Settle() { for (int32 I = 0; I < 30; ++I) { Movement()->TickComponent(1.f / 60, LEVELTICK_All, nullptr); } }

	FAlignmentRequestSpec Push(const float Distance, const float Duration, const EDisplacementSpeedProfile Profile = EDisplacementSpeedProfile::Linear) const
	{
		FAlignmentRequestSpec Spec;
		Spec.OwnerId = TEXT("DisplacementTest");
		Spec.OwnerGeneration = 1;
		Spec.Priority = EDefenseAlignmentPriority::HitKnockback;
		Spec.Executor = EAlignmentExecutor::ProceduralDisplacement;
		Spec.bReleaseWhenFinished = true;
		Spec.Displacement.Direction = FVector(1, 0, 0);
		Spec.Displacement.Distance = Distance;
		Spec.Displacement.Duration = Duration;
		Spec.Displacement.SpeedProfile = Profile;
		return Spec;
	}
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDisplacementMovementPremiseTest, "KatanaCombat.Displacement.Executor.MovementPremise",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDisplacementMovementPremiseTest::RunTest(const FString&)
{
	FDisplacementFixture F;
	TestEqual(TEXT("Character is walking on the floor"), F.Movement()->MovementMode.GetValue(), MOVE_Walking);
	auto Source = MakeShared<FRootMotionSource_ConstantForce>();
	Source->Force = FVector(100, 0, 0);
	Source->Duration = 0.5f;
	Source->AccumulateMode = ERootMotionAccumulateMode::Override;
	F.Movement()->ApplyRootMotionSource(Source);
	const FVector Start = F.Character->GetActorLocation();
	for (int32 I = 0; I < 30; ++I) { F.Movement()->TickComponent(1.f / 60, LEVELTICK_All, nullptr); }
	const double Moved = F.Character->GetActorLocation().X - Start.X;
	TestTrue(FString::Printf(TEXT("Root-motion source moves the character in the test world (moved %.2f cm)"), Moved), Moved > 40.0);
	return true;
}
```

Build (this compiles only the fixture and the premise; later steps add types) and run tests `KatanaCombat.Displacement.Executor.MovementPremise`.

- If it **passes**, continue.
- If it **fails** (the character does not move or is not walking), stop and report. Record the result in the PR. The movement-channel tests in Step 7 then move to the PIE test in Task 6 (as flat, wall and ledge variants), and Step 7 keeps only the arbiter tests that do not require movement.

- [ ] **Step 2: Extend the enums and the request spec**

In `CombatTypes.h`, change `EDefenseAlignmentPriority` to:

```cpp
enum class EDefenseAlignmentPriority : uint8
{
	GuardFacing,
	ActiveAttackWarp,
	/** Hit knockback: overrides the victim's own attack warp, yields to defense and paired moves. */
	HitKnockback,
	BlockContact,
	PairedOrParryBridge,
	Terminal
};
```

Append to `EAlignmentExecutor` after `BoundedMovement`:

```cpp
	BoundedMovement,
	/** Fixed-curve displacement applied through a root-motion modifier or a character-movement root-motion source. */
	ProceduralDisplacement
```

At the end of `FAlignmentRequestSpec` (after `MotionLimits`, line 2992), add:

```cpp

	/** ProceduralDisplacement only. Immutable after acquisition. */
	UPROPERTY(BlueprintReadOnly, Category = "Alignment")
	FProceduralDisplacement Displacement;

	/** When set, the arbiter releases this request itself on any terminal outcome. Immutable. */
	UPROPERTY(BlueprintReadOnly, Category = "Alignment")
	bool bReleaseWhenFinished = false;
```

- [ ] **Step 3: Extend the targeting component's declarations**

In `TargetingComponent.h`, add a forward declaration near the top: `class URootMotionModifier_ProceduralDisplacement;`. Inside `FAlignmentRequestRecord` (line 390) add:

```cpp
        // ProceduralDisplacement runtime state
        double DisplacementElapsed = 0.0;
        double DisplacementChannelStartElapsed = 0.0;
        EDisplacementChannel DisplacementChannel = EDisplacementChannel::None;
        TWeakObjectPtr<URootMotionModifier_ProceduralDisplacement> DisplacementModifier;
        uint16 DisplacementSourceId = 0;
        int32 DisplacementBlockedTicks = 0;
        FVector DisplacementLastLocation = FVector::ZeroVector;
        bool bDisplacementHasLastLocation = false;
```

Next to the other private helpers (line 565), add:

```cpp
    void AdvanceProceduralDisplacement(float DeltaTime);
    bool InstallDisplacementChannel(FAlignmentRequestRecord& Record, float StepEstimate);
    void SteerDisplacementMovement(FAlignmentRequestRecord& Record, float StepEstimate);
    void SyncDisplacementElapsed(FAlignmentRequestRecord& Record);
    void ReportDisplacementOutcome(const FAlignmentRequestRecord& Record, EAlignmentMotionOutcome Outcome) const;
    void RemoveDisplacementChannel(FAlignmentRequestRecord& Record);
    static bool RequestCanRotate(const FAlignmentRequestSpec& Spec);
    bool HasRotatingAlignmentRequest() const;
```

Movement-channel engine facts (verified in UE 5.6 `RootMotionSource.cpp`):
- `FRootMotionSource::IsTimeOutEnabled()` is `Duration >= 0`, so a negative duration never times out. The executor then owns termination.
- `FRootMotionSource_ConstantForce::PrepareRootMotion` samples `StrengthOverTime` at the start of each step. A decaying curve would therefore overshoot by about one step's share (about 6% at 60 fps over 0.25 s). The executor instead sets `Force` each tick to the exact average velocity for the next step.
- On removal, `SetVelocity` overwrites the whole velocity, Z included, which would stall a falling victim. `ClampVelocity` with 0 clamps horizontal speed to 0 and only clamps positive Z, so a victim pushed off a ledge keeps falling.

- [ ] **Step 4: Write the executor**

Create `Source/KatanaCombat/Private/Core/TargetingComponent_ProceduralDisplacement.cpp`:

```cpp
#include "Core/TargetingComponent.h"
#include "Animation/RootMotionModifier_ProceduralDisplacement.h"
#include "Utilities/DisplacementMath.h"
#include "MotionWarpingComponent.h"
#include "Animation/AnimMontage.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/RootMotionSource.h"
#include "Characters/BaseCombatCharacter.h"
#include "Core/CombatComponent.h"
#include "Debug/ActionReactionTelemetry.h"
#include "Debug/DebugConfig.h"
#include "DrawDebugHelpers.h"

namespace
{
constexpr int32 DisplacementBlockedTickLimit = 3;
constexpr double DisplacementBlockedProgressFraction = 0.1;
constexpr double DisplacementMinimumExpectedStep = 0.1;
const FName DisplacementSourceName(TEXT("KatanaProceduralDisplacement"));
}

void UTargetingComponent::ReportDisplacementOutcome(const FAlignmentRequestRecord& Record, const EAlignmentMotionOutcome Outcome) const
{
	const FName OutcomeName(*StaticEnum<EAlignmentMotionOutcome>()->GetNameStringByValue(static_cast<int64>(Outcome)));
	const ABaseCombatCharacter* Character = Cast<ABaseCombatCharacter>(OwnerCharacter);
	if (UCombatComponent* Combat = Character ? Character->GetCombatComponent() : nullptr)
	{
		// Within the existing schema: the owner's started row plus this terminal row.
		FActionReactionTelemetryRecord Row;
		Row.Event = EActionReactionTelemetryEvent::AlignmentChanged;
		Row.Actor = OwnerCharacter.Get();
		Row.AlignmentOwner = Record.Spec.OwnerId;
		Row.AlignmentDisposition = OutcomeName;
		Row.MovementMagnitude = static_cast<float>(Record.MotionState.Travel);
		Combat->AppendActionReactionTelemetry(MoveTemp(Row));
	}
	if (CombatDebug::IsKnockbackDebugEnabled())
	{
		UE_LOG(LogTemp, Log, TEXT("[DISPLACEMENT] %s %s: %s after %.3f s, %.1f of %.1f cm"),
			*GetNameSafe(GetOwner()), *Record.Spec.OwnerId.ToString(), *OutcomeName.ToString(),
			Record.DisplacementElapsed, Record.MotionState.Travel, Record.Spec.Displacement.Distance);
	}
}

bool UTargetingComponent::RequestCanRotate(const FAlignmentRequestSpec& Spec)
{
	return Spec.Executor != EAlignmentExecutor::ProceduralDisplacement;
}

bool UTargetingComponent::HasRotatingAlignmentRequest() const
{
	for (const TPair<FAlignmentRequestHandle, FAlignmentRequestRecord>& Pair : AlignmentRequests)
	{
		if (RequestCanRotate(Pair.Value.Spec))
		{
			return true;
		}
	}
	return false;
}

void UTargetingComponent::SteerDisplacementMovement(FAlignmentRequestRecord& Record, const float StepEstimate)
{
	UCharacterMovementComponent* Movement = OwnerCharacter ? OwnerCharacter->GetCharacterMovement() : nullptr;
	const TSharedPtr<FRootMotionSource> Source = Movement ? Movement->GetRootMotionSourceByID(Record.DisplacementSourceId) : nullptr;
	if (!Source.IsValid())
	{
		return;
	}
	// Average velocity over the next step, assuming it lasts as long as this one: exact for both
	// profiles at a steady frame rate, and it lands on the curve's end instead of overshooting.
	const FProceduralDisplacement& Displacement = Record.Spec.Displacement;
	const double Step = FMath::Max(static_cast<double>(StepEstimate), UE_KINDA_SMALL_NUMBER);
	const double NextDistance = DisplacementMath::DistanceBetween(Displacement.SpeedProfile, Displacement.Distance,
		Displacement.Duration, Record.DisplacementElapsed, Record.DisplacementElapsed + Step);
	StaticCastSharedPtr<FRootMotionSource_ConstantForce>(Source)->Force = Displacement.Direction * (NextDistance / Step);
}

bool UTargetingComponent::InstallDisplacementChannel(FAlignmentRequestRecord& Record, const float StepEstimate)
{
	UCharacterMovementComponent* Movement = OwnerCharacter ? OwnerCharacter->GetCharacterMovement() : nullptr;
	if (!Movement || Movement->MovementMode == MOVE_None)
	{
		return false;
	}
	const FProceduralDisplacement& Displacement = Record.Spec.Displacement;
	const double Remaining = Displacement.Duration - Record.DisplacementElapsed;
	if (Remaining <= 0.0)
	{
		return false;
	}

	const EDisplacementChannel Channel = DisplacementMath::SelectChannel(
		OwnerCharacter->IsPlayingRootMotion(), MotionWarpingComponent != nullptr);
	if (Channel == EDisplacementChannel::Animation)
	{
		const FAnimMontageInstance* Instance = OwnerCharacter->GetRootMotionAnimMontageInstance();
		if (Instance && Instance->Montage)
		{
			auto* Modifier = NewObject<URootMotionModifier_ProceduralDisplacement>(MotionWarpingComponent);
			Modifier->Animation = Instance->Montage;
			Modifier->StartTime = Instance->GetPosition();
			const float Rate = FMath::Max(0.01f, FMath::Abs(Instance->Montage->RateScale * Instance->GetPlayRate()));
			// Backstop only: completion is tracked by the modifier's own clock.
			Modifier->EndTime = Modifier->StartTime + static_cast<float>(Remaining) * Rate * 1.5f + 0.1f;
			Modifier->Configure(Displacement, Record.DisplacementElapsed);
			MotionWarpingComponent->AddModifier(Modifier);
			Record.DisplacementModifier = Modifier;
			Record.DisplacementChannel = EDisplacementChannel::Animation;
			return true;
		}
	}

	auto Source = MakeShared<FRootMotionSource_ConstantForce>();
	Source->InstanceName = DisplacementSourceName;
	Source->AccumulateMode = ERootMotionAccumulateMode::Override;
	Source->Priority = 500;
	Source->Duration = -1.0f; // never times out; the executor removes it
	Source->Settings.SetFlag(ERootMotionSourceSettingsFlags::IgnoreZAccumulate);
	// On removal: horizontal speed to zero, a fall keeps its downward speed.
	Source->FinishVelocityParams.Mode = ERootMotionFinishVelocityMode::ClampVelocity;
	Source->FinishVelocityParams.ClampVelocity = 0.0f;
	Record.DisplacementSourceId = Movement->ApplyRootMotionSource(Source);
	if (Record.DisplacementSourceId == static_cast<uint16>(ERootMotionSourceID::Invalid))
	{
		return false;
	}
	Record.DisplacementChannelStartElapsed = Record.DisplacementElapsed;
	Record.DisplacementChannel = EDisplacementChannel::Movement;
	SteerDisplacementMovement(Record, StepEstimate);
	return true;
}

void UTargetingComponent::SyncDisplacementElapsed(FAlignmentRequestRecord& Record)
{
	// Each channel keeps its own exact clock; read it rather than re-integrating our tick delta.
	if (Record.DisplacementChannel == EDisplacementChannel::Animation)
	{
		if (const URootMotionModifier_ProceduralDisplacement* Modifier = Record.DisplacementModifier.Get())
		{
			Record.DisplacementElapsed = Modifier->GetElapsed();
		}
	}
	else if (Record.DisplacementChannel == EDisplacementChannel::Movement && OwnerCharacter)
	{
		UCharacterMovementComponent* Movement = OwnerCharacter->GetCharacterMovement();
		const TSharedPtr<FRootMotionSource> Source = Movement ? Movement->GetRootMotionSourceByID(Record.DisplacementSourceId) : nullptr;
		if (Source.IsValid())
		{
			// The source's time advances by the dilated simulation time it was applied for.
			Record.DisplacementElapsed = FMath::Min(static_cast<double>(Record.Spec.Displacement.Duration),
				Record.DisplacementChannelStartElapsed + Source->GetTime());
		}
	}
}

void UTargetingComponent::RemoveDisplacementChannel(FAlignmentRequestRecord& Record)
{
	SyncDisplacementElapsed(Record); // a suspension between movement and this tick keeps the applied step
	if (URootMotionModifier_ProceduralDisplacement* Modifier = Record.DisplacementModifier.Get())
	{
		Modifier->SetState(ERootMotionModifierState::MarkedForRemoval);
	}
	if (Record.DisplacementSourceId != 0 && OwnerCharacter)
	{
		if (UCharacterMovementComponent* Movement = OwnerCharacter->GetCharacterMovement())
		{
			Movement->RemoveRootMotionSourceByID(Record.DisplacementSourceId);
		}
	}
	Record.DisplacementModifier.Reset();
	Record.DisplacementSourceId = 0;
	Record.DisplacementChannel = EDisplacementChannel::None;
	Record.bDisplacementHasLastLocation = false;
}

void UTargetingComponent::AdvanceProceduralDisplacement(const float DeltaTime)
{
	if (LastAlignmentExecutionFrame == GFrameCounter || !EnsureAlignmentDependencies())
	{
		return;
	}
	const FAlignmentRequestHandle Handle = ActiveAlignmentRequest;
	FAlignmentRequestRecord* Record = AlignmentRequests.Find(Handle);
	if (!Record)
	{
		return;
	}
	LastAlignmentExecutionFrame = GFrameCounter;
	LastAlignmentExecutor = EAlignmentExecutor::ProceduralDisplacement;

	const FProceduralDisplacement& Displacement = Record->Spec.Displacement;
	const double PreviousElapsed = Record->DisplacementElapsed;
	UCharacterMovementComponent* Movement = OwnerCharacter->GetCharacterMovement();

	// Advance the request clock from whichever channel delivered this frame.
	SyncDisplacementElapsed(*Record);
	double AnimationTravel = 0.0;
	if (Record->DisplacementChannel == EDisplacementChannel::Animation)
	{
		URootMotionModifier_ProceduralDisplacement* Modifier = Record->DisplacementModifier.Get();
		AnimationTravel = Modifier ? Modifier->ConsumeAnimationTravel() : 0.0;
		if (!Modifier || Modifier->GetState() == ERootMotionModifierState::MarkedForRemoval)
		{
			// The montage ended or was replaced; the remainder is reinstalled below on the live channel.
			Record->DisplacementModifier.Reset();
			Record->DisplacementChannel = EDisplacementChannel::None;
		}
	}
	else if (Record->DisplacementChannel == EDisplacementChannel::Movement
		&& !(Movement && Movement->GetRootMotionSourceByID(Record->DisplacementSourceId).IsValid()))
	{
		Record->DisplacementSourceId = 0;
		Record->DisplacementChannel = EDisplacementChannel::None;
	}

	// Re-select the channel every tick: animation root motion overrides root-motion sources, so a
	// root-motion montage that starts mid-push must take the push over.
	const EDisplacementChannel LiveChannel = DisplacementMath::SelectChannel(
		OwnerCharacter->IsPlayingRootMotion(), MotionWarpingComponent != nullptr);
	if (Record->DisplacementChannel != EDisplacementChannel::None && Record->DisplacementChannel != LiveChannel)
	{
		RemoveDisplacementChannel(*Record);
	}

	// Progress along the push direction, measured from actual movement against the expected
	// push plus any animation root motion that was kept.
	const FVector Location = OwnerCharacter->GetActorLocation();
	double Actual = 0.0;
	if (Record->bDisplacementHasLastLocation)
	{
		Actual = FVector::DotProduct(Location - Record->DisplacementLastLocation, Displacement.Direction);
		const double Expected = AnimationTravel + DisplacementMath::DistanceBetween(
			Displacement.SpeedProfile, Displacement.Distance, Displacement.Duration, PreviousElapsed, Record->DisplacementElapsed);
		if (Expected > DisplacementMinimumExpectedStep && Actual < DisplacementBlockedProgressFraction * Expected)
		{
			++Record->DisplacementBlockedTicks;
		}
		else if (Expected > DisplacementMinimumExpectedStep)
		{
			Record->DisplacementBlockedTicks = 0;
		}
	}
	Record->DisplacementLastLocation = Location;
	Record->bDisplacementHasLastLocation = true;
	if (CombatDebug::IsKnockbackDebugEnabled())
	{
		DrawDebugPoint(GetWorld(), Location, 8.0f,
			Record->DisplacementChannel == EDisplacementChannel::Animation ? FColor::Cyan : FColor::Orange,
			false, CombatDebug::GetDebugDrawDuration());
	}

	EAlignmentMotionOutcome Outcome = EAlignmentMotionOutcome::Running;
	if (Record->DisplacementElapsed >= Displacement.Duration)
	{
		Outcome = EAlignmentMotionOutcome::Reached;
	}
	else if (Record->DisplacementBlockedTicks >= DisplacementBlockedTickLimit)
	{
		Outcome = EAlignmentMotionOutcome::Blocked;
	}
	else if (Record->DisplacementChannel == EDisplacementChannel::None && !InstallDisplacementChannel(*Record, DeltaTime))
	{
		Outcome = EAlignmentMotionOutcome::Invalid;
	}
	else if (Record->DisplacementChannel == EDisplacementChannel::Movement)
	{
		// ActorTime: the component delta is already scaled by the owner's time dilation.
		SteerDisplacementMovement(*Record, DeltaTime);
	}

	Record->MotionState.Outcome = Outcome;
	Record->MotionState.Elapsed = Record->DisplacementElapsed;
	Record->MotionState.Travel += FMath::Max(0.0, Actual);

	if (Outcome != EAlignmentMotionOutcome::Running)
	{
		RemoveDisplacementChannel(*Record);
		ReportDisplacementOutcome(*Record, Outcome);
		if (Record->Spec.bReleaseWhenFinished)
		{
			ReleaseAlignmentRequest(Handle);
		}
	}
}
```

- [ ] **Step 5: Wire the executor into the arbiter**

In `TargetingComponent.cpp`, add `#include "Utilities/DisplacementMath.h"`. Then make these edits:

1. `TickComponent` (line 148), before the `BoundedMovement` branch:

```cpp
    if (ActiveRecord && ActiveRecord->Spec.Executor == EAlignmentExecutor::ProceduralDisplacement)
    {
        AdvanceProceduralDisplacement(DeltaTime);
        return;
    }
```

2. `AcquireAlignmentRequest` (line 808): replace `if (AlignmentRequests.IsEmpty() && !CaptureAlignmentRotationSettings())` with `if (RequestCanRotate(Spec) && !CaptureAlignmentRotationSettings())`. `CaptureAlignmentRotationSettings` is already idempotent.

3. `UpdateAlignmentRequest` (lines 867-872): extend the immutability condition with:

```cpp
        || (Spec.Executor == EAlignmentExecutor::ProceduralDisplacement && !(Spec.Displacement == Record->Spec.Displacement))
        || Spec.bReleaseWhenFinished != Record->Spec.bReleaseWhenFinished
```

4. `ReleaseAlignmentRequest` (line 896): before `const FAlignmentRequestRecord ReleasedRecord = *Record;`, change the lookup to a mutable pointer and remove the channel:

```cpp
    FAlignmentRequestRecord* Record = AlignmentRequests.Find(Handle);
    if (!Record)
    {
        return;
    }
    RemoveDisplacementChannel(*Record);
```

5. `ReleaseAllAlignmentRequests`: before `RemoveRegisteredAlignmentModifiersForHandle` loop, add:

```cpp
    for (TPair<FAlignmentRequestHandle, FAlignmentRequestRecord>& Pair : AlignmentRequests)
    {
        RemoveDisplacementChannel(Pair.Value);
    }
```

6. `ValidateAlignmentSpec`: after the `BoundedMovement` block (line 1075), add:

```cpp
    if (Spec.Executor == EAlignmentExecutor::ProceduralDisplacement)
    {
        return DisplacementMath::IsValid(Spec.Displacement);
    }
```

7. `HasSmoothAlignmentRequest`: add `|| Pair.Value.Spec.Executor == EAlignmentExecutor::ProceduralDisplacement` to the condition.

8. `ReevaluateAlignmentRequests`:
   - In the invalid-handle loop, before `RemoveRegisteredAlignmentModifiersForHandle(Handle);`, call `RemoveDisplacementChannel(*AlignmentRequests.Find(Handle));`. Change that loop's `const FAlignmentRequestRecord* Record` to non-const.
   - After `ActiveAlignmentRequest = ChooseActiveAlignmentRequest();`, suspend displacement requests that are no longer active:

```cpp
    for (TPair<FAlignmentRequestHandle, FAlignmentRequestRecord>& Pair : AlignmentRequests)
    {
        if (Pair.Key != ActiveAlignmentRequest
            && Pair.Value.Spec.Executor == EAlignmentExecutor::ProceduralDisplacement
            && Pair.Value.DisplacementChannel != EDisplacementChannel::None)
        {
            RemoveDisplacementChannel(Pair.Value); // resumes from DisplacementElapsed when active again
        }
    }
```

   - Replace `if (AlignmentRequests.IsEmpty()) { RestoreAlignmentRotationSettings(); }` with `if (!HasRotatingAlignmentRequest()) { RestoreAlignmentRotationSettings(); }`.

In `TargetingComponent_BoundedAlignment.cpp` `GetAlignmentMotionState`, change the executor check to accept both:

```cpp
	if (!Record || (Record->Spec.Executor != EAlignmentExecutor::BoundedMovement
		&& Record->Spec.Executor != EAlignmentExecutor::ProceduralDisplacement))
```

Add the knockback debug toggle, used by the executor here and by `StartKnockback` in Task 5. In `DebugConfig.h`, add next to the other externs: `extern TAutoConsoleVariable<int32> CVarDebugKnockback;` and:

```cpp
    /** Check if knockback debug is enabled (standalone or via master toggle) */
    FORCEINLINE bool IsKnockbackDebugEnabled()
    {
        return IsDebugEnabled() || CVarDebugKnockback.GetValueOnGameThread() != 0;
    }
```

In `DebugConfig.cpp`, next to `CVarDebugHold`:

```cpp
    TAutoConsoleVariable<int32> CVarDebugKnockback(
        TEXT("Combat.Debug.Knockback"),
        0,
        TEXT("Enable knockback debug visualization\n")
        TEXT("Shows: push direction and distance, resolved config, charge and scale,\n")
        TEXT("       the live displacement channel (cyan animation, orange movement) and the outcome\n")
        TEXT("  0: Disabled (default)\n")
        TEXT("  1: Enabled"),
        ECVF_Default);
```

- [ ] **Step 6: Build and run the regression suites `KatanaCombat.Targeting+KatanaCombat.Defense.Alignment+KatanaCombat.PairedAnimation`**

Expected: all pass. This proves the existing executors are unaffected before new behavior is tested.

- [ ] **Step 7: Write the executor tests**

Append to `ProceduralDisplacementExecutorTests.cpp`:

```cpp
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDisplacementFlatLinearTest, "KatanaCombat.Displacement.Executor.FlatLinearReachesAndReleases",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDisplacementFlatLinearTest::RunTest(const FString&)
{
	FDisplacementFixture F;
	const FVector Start = F.Character->GetActorLocation();
	const FAlignmentRequestHandle Handle = F.Targeting()->AcquireAlignmentRequest(F.Push(50.f, 0.25f));
	if (!TestTrue(TEXT("Displacement request acquired"), Handle.IsValid())) { return false; }
	TestTrue(TEXT("Targeting tick enabled while pushing"), F.Targeting()->IsComponentTickEnabled());
	for (int32 I = 0; I < 30; ++I) { F.Step(1.f / 60); }
	const FVector Moved = F.Character->GetActorLocation() - Start;
	TestTrue(FString::Printf(TEXT("Pushed ~50 cm along X (moved %.2f)"), Moved.X), FMath::IsNearlyEqual(Moved.X, 50.0, 4.0));
	TestTrue(TEXT("No lateral drift"), FMath::Abs(Moved.Y) < 1.0);
	TestEqual(TEXT("Released itself when finished"), F.Targeting()->GetAlignmentRequestCountForTesting(), 0);
	TestFalse(TEXT("Tick disabled after release"), F.Targeting()->IsComponentTickEnabled());
	TestFalse(TEXT("Root-motion source removed"), F.Movement()->GetRootMotionSource(TEXT("KatanaProceduralDisplacement")).IsValid());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDisplacementEaseOutTest, "KatanaCombat.Displacement.Executor.EaseOutFrontLoads",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDisplacementEaseOutTest::RunTest(const FString&)
{
	FDisplacementFixture F;
	const FVector Start = F.Character->GetActorLocation();
	F.Targeting()->AcquireAlignmentRequest(F.Push(60.f, 0.24f, EDisplacementSpeedProfile::EaseOut));
	for (int32 I = 0; I < 8; ++I) { F.Step(1.f / 60); } // ~half the duration
	const double Half = F.Character->GetActorLocation().X - Start.X;
	TestTrue(FString::Printf(TEXT("More than half the distance in the first half (%.2f)"), Half), Half > 32.0);
	for (int32 I = 0; I < 20; ++I) { F.Step(1.f / 60); }
	TestTrue(TEXT("Total ~60 cm"), FMath::IsNearlyEqual(F.Character->GetActorLocation().X - Start.X, 60.0, 5.0));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDisplacementHitstopTest, "KatanaCombat.Displacement.Executor.PausesUnderHitstopDilation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDisplacementHitstopTest::RunTest(const FString&)
{
	FDisplacementFixture F;
	const FVector Start = F.Character->GetActorLocation();
	const FAlignmentRequestHandle Handle = F.Targeting()->AcquireAlignmentRequest(F.Push(40.f, 0.2f));
	// Component deltas are already scaled by actor dilation; hitstop freezes at 0.0001.
	for (int32 I = 0; I < 30; ++I) { F.Step(1.f / 60 * 0.0001f); }
	TestTrue(TEXT("Frozen victim does not move"), FVector::Dist2D(F.Character->GetActorLocation(), Start) < 0.5);
	FAlignmentMotionState State;
	TestTrue(TEXT("Request still exists"), F.Targeting()->GetAlignmentMotionState(Handle, State));
	TestEqual(TEXT("Still running while frozen"), State.Outcome, EAlignmentMotionOutcome::Running);
	for (int32 I = 0; I < 25; ++I) { F.Step(1.f / 60); }
	TestTrue(TEXT("Completes the push after the freeze"), FMath::IsNearlyEqual(F.Character->GetActorLocation().X - Start.X, 40.0, 4.0));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDisplacementWallTest, "KatanaCombat.Displacement.Executor.WallBlocksAndReleases",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDisplacementWallTest::RunTest(const FString&)
{
	FDisplacementFixture F;
	const float Radius = F.Character->GetCapsuleComponent()->GetScaledCapsuleRadius();
	F.Box(FVector(F.Character->GetActorLocation().X + Radius + 15.f, 0, 100), FVector(5, 500, 200));
	F.Targeting()->AcquireAlignmentRequest(F.Push(80.f, 0.3f));
	for (int32 I = 0; I < 40; ++I) { F.Step(1.f / 60); }
	TestEqual(TEXT("Blocked push released itself"), F.Targeting()->GetAlignmentRequestCountForTesting(), 0);
	TestTrue(TEXT("Stopped at the wall"), F.Character->GetActorLocation().X < 20.0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDisplacementLedgeTest, "KatanaCombat.Displacement.Executor.LedgeFallsInsteadOfHovering",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDisplacementLedgeTest::RunTest(const FString&)
{
	FDisplacementFixture F;
	// Replace the large floor with one that ends 20 cm ahead of the character.
	for (TActorIterator<AActor> It(F.World); It; ++It)
	{
		if (It->GetRootComponent() && It->GetRootComponent()->IsA<UBoxComponent>()) { It->Destroy(); }
	}
	F.Box(FVector(-1000 + 20, 0, -10), FVector(1000, 1000, 10));
	F.Settle();
	const double StartZ = F.Character->GetActorLocation().Z;
	F.Targeting()->AcquireAlignmentRequest(F.Push(100.f, 0.25f));
	bool bFell = false;
	for (int32 I = 0; I < 60; ++I)
	{
		F.Step(1.f / 60);
		bFell |= F.Movement()->MovementMode == MOVE_Falling;
	}
	TestTrue(TEXT("Victim entered falling"), bFell);
	TestTrue(TEXT("Victim dropped instead of hovering at the start height"), F.Character->GetActorLocation().Z < StartZ - 5.0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDisplacementRotationSettingsTest, "KatanaCombat.Displacement.Executor.PlayerRotationSettingsUntouched",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDisplacementRotationSettingsTest::RunTest(const FString&)
{
	FDisplacementFixture F;
	F.Movement()->bOrientRotationToMovement = true;
	F.Character->bUseControllerRotationYaw = true;
	F.Targeting()->AcquireAlignmentRequest(F.Push(30.f, 0.2f));
	TestTrue(TEXT("Orient-to-movement untouched during a push"), F.Movement()->bOrientRotationToMovement);
	TestTrue(TEXT("Controller yaw untouched during a push"), F.Character->bUseControllerRotationYaw);
	for (int32 I = 0; I < 20; ++I) { F.Step(1.f / 60); }
	TestTrue(TEXT("Orient-to-movement untouched after a push"), F.Movement()->bOrientRotationToMovement);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDisplacementPriorityTest, "KatanaCombat.Displacement.Executor.SuspendsUnderHigherPriority",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDisplacementPriorityTest::RunTest(const FString&)
{
	FDisplacementFixture F;
	const FVector Start = F.Character->GetActorLocation();
	F.Targeting()->AcquireAlignmentRequest(F.Push(40.f, 0.2f));
	F.Step(1.f / 60);
	FAlignmentRequestSpec Block;
	Block.OwnerId = TEXT("BlockTest"); Block.OwnerGeneration = 1;
	Block.Priority = EDefenseAlignmentPriority::BlockContact;
	Block.Executor = EAlignmentExecutor::CharacterMovement;
	Block.DesiredRotation = FRotator::ZeroRotator; Block.MaximumTurnRate = 90.f; Block.RemainingTurnBudget = 10.f;
	const FAlignmentRequestHandle BlockHandle = F.Targeting()->AcquireAlignmentRequest(Block);
	if (!TestTrue(TEXT("Block request acquired"), BlockHandle.IsValid())) { return false; }
	const double Suspended = F.Character->GetActorLocation().X;
	for (int32 I = 0; I < 10; ++I) { F.Step(1.f / 60); }
	TestTrue(TEXT("No push while suspended"), FMath::Abs(F.Character->GetActorLocation().X - Suspended) < 1.0);
	// A removed source is only marked; the next movement tick drops it, so check after stepping.
	TestFalse(TEXT("Suspended push has no source"), F.Movement()->GetRootMotionSource(TEXT("KatanaProceduralDisplacement")).IsValid());
	F.Targeting()->ReleaseAlignmentRequest(BlockHandle);
	for (int32 I = 0; I < 30; ++I) { F.Step(1.f / 60); }
	TestTrue(TEXT("Push resumes and completes"), FMath::IsNearlyEqual(F.Character->GetActorLocation().X - Start.X, 40.0, 4.0));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDisplacementInvalidTest, "KatanaCombat.Displacement.Executor.InvalidWithoutChannelReleases",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDisplacementInvalidTest::RunTest(const FString&)
{
	FDisplacementFixture F;
	const FVector Start = F.Character->GetActorLocation();
	F.Targeting()->AcquireAlignmentRequest(F.Push(40.f, 0.2f));
	F.Movement()->DisableMovement(); // MOVE_None: no channel can deliver the push
	F.Step(1.f / 60);
	TestEqual(TEXT("Invalid push released itself"), F.Targeting()->GetAlignmentRequestCountForTesting(), 0);
	TestFalse(TEXT("Tick disabled after release"), F.Targeting()->IsComponentTickEnabled());
	TestTrue(TEXT("Nothing moved"), FVector::Dist(F.Character->GetActorLocation(), Start) < 0.5);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDisplacementReleaseTest, "KatanaCombat.Displacement.Executor.ReleaseRemovesChannel",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDisplacementReleaseTest::RunTest(const FString&)
{
	FDisplacementFixture F;
	const FAlignmentRequestHandle Handle = F.Targeting()->AcquireAlignmentRequest(F.Push(60.f, 0.5f));
	for (int32 I = 0; I < 5; ++I) { F.Step(1.f / 60); }
	TestTrue(TEXT("Push is running through the movement channel"), F.Movement()->GetRootMotionSource(TEXT("KatanaProceduralDisplacement")).IsValid());
	F.Targeting()->ReleaseAlignmentRequest(Handle);
	F.Step(1.f / 60); // the marked source is dropped on the next movement tick
	const double Released = F.Character->GetActorLocation().X;
	for (int32 I = 0; I < 10; ++I) { F.Step(1.f / 60); }
	TestFalse(TEXT("Source removed on release"), F.Movement()->GetRootMotionSource(TEXT("KatanaProceduralDisplacement")).IsValid());
	TestTrue(TEXT("No drift after release (finish velocity clamps horizontal speed)"), FMath::Abs(F.Character->GetActorLocation().X - Released) < 1.0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDisplacementPriorityOrderTest, "KatanaCombat.Displacement.Executor.PriorityOrdering",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDisplacementPriorityOrderTest::RunTest(const FString&)
{
	TestTrue(TEXT("Knockback beats the victim's attack warp"),
		static_cast<uint8>(EDefenseAlignmentPriority::HitKnockback) > static_cast<uint8>(EDefenseAlignmentPriority::ActiveAttackWarp));
	TestTrue(TEXT("Block contact beats knockback"),
		static_cast<uint8>(EDefenseAlignmentPriority::BlockContact) > static_cast<uint8>(EDefenseAlignmentPriority::HitKnockback));
	return true;
}
```

Add `#include "EngineUtils.h"` to the includes (for `TActorIterator`).

- [ ] **Step 8: Build and run tests `KatanaCombat.Displacement+KatanaCombat.Targeting+KatanaCombat.Defense.Alignment+KatanaCombat.PairedAnimation`**

Expected: all pass. If a movement test fails, inspect before adjusting tolerances. The only legitimate tolerance changes are for character-movement floor snapping (±2 cm Z) and the first frame's installation latency (the push starts one frame after acquisition, because the executor ticks after movement).

- [ ] **Step 9: Commit**

```bash
git add -A Source/KatanaCombat Source/KatanaCombatTest/Private/ProceduralDisplacementExecutorTests.cpp
git commit -m "Add the ProceduralDisplacement alignment executor

- New executor applies a fixed displacement curve through a runtime
  root-motion modifier when a root-motion montage plays, else a
  character-movement override root-motion source (IgnoreZAccumulate,
  steered each tick with the exact per-step velocity, horizontal speed
  clamped to zero on removal), so terrain, walls, ledges and hitstop come
  from character movement.
- Channel re-selected every tick; each channel's own clock is the record
  of applied time.
- New HitKnockback priority between ActiveAttackWarp and BlockContact.
- bReleaseWhenFinished lets the arbiter release finished requests itself.
- Non-rotating requests no longer capture rotation settings.
- Suspended requests drop their channel and resume from their clock.

Rollback checkpoint: <previous commit>"
```

---

### Task 5: StartKnockback on the victim

**Files:**
- Modify: `Source/KatanaCombat/Public/Core/HitReactionComponent.h` (friend list line 48; member handles near line 543)
- Modify: `Source/KatanaCombat/Private/Core/HitReactionComponent.cpp` (`PlayHitReaction` 426-514; `EndPlay` 66-76; `EnterPairedAnimationState` 1460)
- Test: `Source/KatanaCombatTest/Private/KnockbackStartTests.cpp`
- Test: `Source/KatanaCombatTest/Private/DefenseArchitectureSourceTests.cpp` (append two source-structure tests)

**Interfaces:**
- Consumes: `KnockbackResolution` (Task 2), `EAlignmentExecutor::ProceduralDisplacement`, `EDefenseAlignmentPriority::HitKnockback` and `CombatDebug::IsKnockbackDebugEnabled()` (Task 4), `UCombatComponent::AppendActionReactionTelemetry(FActionReactionTelemetryRecord)`.
- Produces: `bool UHitReactionComponent::StartKnockback(const FHitReactionInfo& HitInfo)` (private, friend-tested), `void UHitReactionComponent::ReleaseKnockback()`, and `FAlignmentRequestHandle KnockbackAlignmentHandle`.

- [ ] **Step 1: Write the failing tests**

Create `Source/KatanaCombatTest/Private/KnockbackStartTests.cpp`:

```cpp
#include "CombatTestHelpers.h"
#include "Core/HitReactionComponent.h"
#include "Core/TargetingComponent.h"
#include "Data/AttackData.h"
#include "Data/HitReactionSettings.h"

namespace
{
struct FKnockbackFixture
{
	UWorld* World = FCombatTestHelpers::CreateTestWorld();
	APlayerCharacter* Attacker = FCombatTestHelpers::CreateTestPlayerCharacter(World, FVector(0, 0, 100));
	AEnemyCharacter* Victim = FCombatTestHelpers::CreateTestEnemyCharacter(World, FVector(100, 0, 100));
	UAttackData* Light = FCombatTestHelpers::CreateTestAttack(EAttackType::Light);
	~FKnockbackFixture() { FCombatTestHelpers::DestroyTestWorld(World); }

	FHitReactionInfo Hit() const
	{
		FHitReactionInfo Info = FCombatTestHelpers::CreateTestHitInfo(Attacker, 10.f, FVector(-1, 0, 0), Light);
		return Info;
	}
	UTargetingComponent* VictimTargeting() const { return Victim->GetTargetingComponent(); }
	FAlignmentRequestSpec ActiveSpec() const
	{
		FAlignmentRequestSpec Spec;
		VictimTargeting()->GetAlignmentRequestSpec(VictimTargeting()->GetActiveAlignmentRequest(), Spec);
		return Spec;
	}
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FKnockbackStartRequestTest, "KatanaCombat.Knockback.Start.AcquiresResolvedRequest",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FKnockbackStartRequestTest::RunTest(const FString&)
{
	FKnockbackFixture F;
	TestTrue(TEXT("Push started"), F.Victim->HitReactionComponent->StartKnockback(F.Hit()));
	const FAlignmentRequestSpec Spec = F.ActiveSpec();
	TestEqual(TEXT("Executor"), Spec.Executor, EAlignmentExecutor::ProceduralDisplacement);
	TestEqual(TEXT("Priority"), Spec.Priority, EDefenseAlignmentPriority::HitKnockback);
	TestTrue(TEXT("Self-releasing"), Spec.bReleaseWhenFinished);
	TestEqual(TEXT("Light default distance"), Spec.Displacement.Distance, 25.0f);
	TestEqual(TEXT("Light default duration"), Spec.Displacement.Duration, 0.2f);
	TestEqual(TEXT("Default profile"), Spec.Displacement.SpeedProfile, EDisplacementSpeedProfile::EaseOut);
	TestEqual(TEXT("Actor clock"), Spec.Displacement.Clock, EDisplacementClock::ActorTime);
	TestTrue(TEXT("Pushed away from the attacker"), Spec.Displacement.Direction.Equals(FVector(1, 0, 0), 1e-4));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FKnockbackStartChargeTest, "KatanaCombat.Knockback.Start.ChargeAndVictimScale",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FKnockbackStartChargeTest::RunTest(const FString&)
{
	FKnockbackFixture F;
	F.Light->MaxChargeKnockbackMultiplier = 2.0f;
	FHitReactionInfo Charged = F.Hit();
	Charged.ChargeLevel = 1.0f;
	F.Victim->HitReactionComponent->StartKnockback(Charged);
	TestEqual(TEXT("Fully charged doubles the push"), F.ActiveSpec().Displacement.Distance, 50.0f);

	UHitReactionSettings* Immune = NewObject<UHitReactionSettings>();
	Immune->KnockbackScale = 0.0f;
	F.Victim->HitReactionComponent->HitReactionSettingsOverride = Immune;
	TestFalse(TEXT("Immune victim gets no push"), F.Victim->HitReactionComponent->StartKnockback(F.Hit()));
	TestEqual(TEXT("No request remains"), F.VictimTargeting()->GetAlignmentRequestCountForTesting(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FKnockbackReplaceTest, "KatanaCombat.Knockback.Start.ReplacesRunningPush",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FKnockbackReplaceTest::RunTest(const FString&)
{
	FKnockbackFixture F;
	F.Victim->HitReactionComponent->StartKnockback(F.Hit());
	const FAlignmentRequestHandle First = F.VictimTargeting()->GetActiveAlignmentRequest();
	F.Victim->HitReactionComponent->StartKnockback(F.Hit());
	TestEqual(TEXT("A second hit replaces, never stacks"), F.VictimTargeting()->GetAlignmentRequestCountForTesting(), 1);
	TestTrue(TEXT("The running request is the new one"), F.VictimTargeting()->GetActiveAlignmentRequest() != First);
	F.Victim->HitReactionComponent->ReleaseKnockback();
	TestEqual(TEXT("ReleaseKnockback clears it"), F.VictimTargeting()->GetAlignmentRequestCountForTesting(), 0);
	return true;
}
```

Append to `Source/KatanaCombatTest/Private/DefenseArchitectureSourceTests.cpp` after its last test. Its anonymous namespace already provides `StripCppComments`, `ExtractFunctionBody` (bounds every check to one function body) and `LoadProjectSource`:

```cpp
namespace
{
int32 CountOccurrences(const FString& Text, const TCHAR* Needle)
{
	int32 Count = 0;
	for (int32 From = Text.Find(Needle, ESearchCase::CaseSensitive); From != INDEX_NONE;
		From = Text.Find(Needle, ESearchCase::CaseSensitive, ESearchDir::FromStart, From + 1))
	{
		++Count;
	}
	return Count;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FKnockbackReactionOrderSourceTest,
	"KatanaCombat.Knockback.Architecture.StartsAfterDirectionalReaction",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FKnockbackReactionOrderSourceTest::RunTest(const FString& Parameters)
{
	FString Source;
	if (!TestTrue(TEXT("HitReactionComponent source loads"), LoadProjectSource(
		TEXT("Source/KatanaCombat/Private/Core/HitReactionComponent.cpp"), Source)))
	{
		return false;
	}
	Source = StripCppComments(Source);

	FString Body;
	if (!TestTrue(TEXT("PlayHitReaction has an extractable body"), ExtractFunctionBody(
		Source, TEXT("UHitReactionComponent::PlayHitReaction"), Body)))
	{
		return false;
	}
	const int32 EntryCall = Body.Find(TEXT("PlayReactionFromEntry(*ReactionEntry"));
	const int32 StartCall = Body.Find(TEXT("StartKnockback(HitInfo)"));
	const int32 LegacyFallback = Body.Find(TEXT("SelectHitReactionMontage(HitInfo)"));
	TestTrue(TEXT("The push starts after the directional reaction starts"), EntryCall != INDEX_NONE && StartCall > EntryCall);
	TestTrue(TEXT("The legacy fallback path never pushes"), LegacyFallback != INDEX_NONE && StartCall < LegacyFallback);
	TestEqual(TEXT("PlayHitReaction has one push site"), CountOccurrences(Body, TEXT("StartKnockback(")), 1);
	TestTrue(TEXT("Super armor feeds the eligibility decision"), Body.Contains(TEXT("bSuperArmor = bHasSuperArmor")));

	for (const TCHAR* Releaser : { TEXT("UHitReactionComponent::EnterPairedAnimationState"), TEXT("UHitReactionComponent::EndPlay") })
	{
		FString ReleaserBody;
		TestTrue(FString::Printf(TEXT("%s releases the push"), Releaser),
			ExtractFunctionBody(Source, Releaser, ReleaserBody) && ReleaserBody.Contains(TEXT("ReleaseKnockback()")));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FKnockbackDefenseOutcomeSourceTest,
	"KatanaCombat.Knockback.Architecture.BlockParryAndSuperArmorStartNoReaction",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FKnockbackDefenseOutcomeSourceTest::RunTest(const FString& Parameters)
{
	FString HitReaction;
	FString Character;
	if (!TestTrue(TEXT("Sources load"),
		LoadProjectSource(TEXT("Source/KatanaCombat/Private/Core/HitReactionComponent.cpp"), HitReaction)
		&& LoadProjectSource(TEXT("Source/KatanaCombat/Private/Characters/BaseCombatCharacter.cpp"), Character)))
	{
		return false;
	}
	HitReaction = StripCppComments(HitReaction);
	Character = StripCppComments(Character);

	// Blocked and parried hits resolve to a non-applying damage disposition and never build a reaction commit.
	FString DefenseCommit;
	if (TestTrue(TEXT("CommitResolvedDefenseDamage has an extractable body"), ExtractFunctionBody(
		Character, TEXT("ABaseCombatCharacter::CommitResolvedDefenseDamage"), DefenseCommit)))
	{
		const int32 Gate = DefenseCommit.Find(TEXT("!= EDefenseDamageDisposition::ApplyRequestedDamage"));
		const int32 Commit = DefenseCommit.Find(TEXT("CommitResolvedDamage("));
		TestTrue(TEXT("Only applied damage reaches the reaction commit"), Gate != INDEX_NONE && Commit > Gate);
	}

	FString DamageCommit;
	TestTrue(TEXT("Super armor commits damage without a reaction"),
		ExtractFunctionBody(HitReaction, TEXT("UHitReactionComponent::CommitResolvedDamage"), DamageCommit)
		&& DamageCommit.Contains(TEXT("bShouldPlayReaction = !bHasSuperArmor")));

	FString ReactionGate;
	if (TestTrue(TEXT("PlayCommittedDamageReaction has an extractable body"), ExtractFunctionBody(
		HitReaction, TEXT("UHitReactionComponent::PlayCommittedDamageReaction"), ReactionGate)))
	{
		const int32 Gate = ReactionGate.Find(TEXT("if (Commit.bShouldPlayReaction)"));
		const int32 Play = ReactionGate.Find(TEXT("PlayHitReaction(Commit.HitInfo)"));
		TestTrue(TEXT("The reaction plays only behind the commit's reaction gate"), Gate != INDEX_NONE && Play > Gate);
	}

	// The gate above is the only way into PlayHitReaction in the runtime module.
	TArray<FString> Files;
	IFileManager::Get().FindFilesRecursive(Files, *(FPaths::ProjectDir() / TEXT("Source/KatanaCombat")), TEXT("*.cpp"), true, false);
	int32 Calls = 0;
	for (const FString& File : Files)
	{
		FString Text;
		if (FFileHelper::LoadFileToString(Text, *File))
		{
			Text = StripCppComments(Text);
			Calls += CountOccurrences(Text, TEXT("PlayHitReaction(")) - CountOccurrences(Text, TEXT("::PlayHitReaction("));
		}
	}
	TestEqual(TEXT("PlayHitReaction has exactly one caller"), Calls, 1);
	return true;
}
```

Add `#include "HAL/FileManager.h"` to that file's includes if it is not already present.

- [ ] **Step 2: Build to verify it fails**

Build. Expected: errors for `StartKnockback`, `ReleaseKnockback` and inaccessible members.

- [ ] **Step 3: Implement on the component**

In `HitReactionComponent.h`, add to the friend list (line 48):

```cpp
	friend class FKnockbackStartRequestTest;
	friend class FKnockbackStartChargeTest;
	friend class FKnockbackReplaceTest;
```

In its public section add:

```cpp
    /** Release the running knockback push, if any. */
    void ReleaseKnockback();
```

In its private section (near `DefensePresentationAlignmentHandle`, line 543) add:

```cpp
	/** Push the victim for a started directional reaction. Returns true when a push was acquired. */
	bool StartKnockback(const FHitReactionInfo& HitInfo);

	FAlignmentRequestHandle KnockbackAlignmentHandle;
	int32 NextKnockbackAlignmentGeneration = 1;
```

In `HitReactionComponent.cpp`, add includes `Utilities/KnockbackResolution.h`, `Debug/DebugConfig.h`, `Debug/ActionReactionTelemetry.h`, `Core/CombatComponent.h`, `DrawDebugHelpers.h` (skip any already present). Then:

In `PlayHitReaction`, inside `if (PlayReactionFromEntry(*ReactionEntry, RelativeDir, bIsHeavy, Intensity))`, before the stun block, add:

```cpp
                KnockbackResolution::FEligibility Eligibility;
                Eligibility.bReactionStarted = true;
                Eligibility.bSuperArmor = bHasSuperArmor; // already gated upstream; kept so the decision owns the rule
                Eligibility.bReactionsSuppressed = bReactionsSuppressed;
                Eligibility.bAlive = true; // the alive check above already returned for dead owners
                Eligibility.bSettingsPath = true;
                if (KnockbackResolution::ShouldApply(Eligibility))
                {
                    StartKnockback(HitInfo);
                }
```

At the top of `EnterPairedAnimationState` (line 1461, first statement) add `ReleaseKnockback();`. In `EndPlay` (line 67, first statement) add `ReleaseKnockback();`.

Add the functions:

```cpp
void UHitReactionComponent::ReleaseKnockback()
{
	if (!KnockbackAlignmentHandle.IsValid())
	{
		return;
	}
	if (const ABaseCombatCharacter* Character = Cast<ABaseCombatCharacter>(GetOwnerCharacterCached()))
	{
		if (UTargetingComponent* Targeting = Character->GetTargetingComponent())
		{
			Targeting->ReleaseAlignmentRequest(KnockbackAlignmentHandle);
		}
	}
	KnockbackAlignmentHandle = {};
}

bool UHitReactionComponent::StartKnockback(const FHitReactionInfo& HitInfo)
{
	ReleaseKnockback();

	ABaseCombatCharacter* Victim = Cast<ABaseCombatCharacter>(GetOwnerCharacterCached());
	UTargetingComponent* Targeting = Victim ? Victim->GetTargetingComponent() : nullptr;
	if (!Targeting)
	{
		return false;
	}

	const ABaseCombatCharacter* AttackerCharacter = Cast<ABaseCombatCharacter>(HitInfo.Attacker);
	const UCombatSettings* AttackerSettings = AttackerCharacter && AttackerCharacter->CombatSettings
		? AttackerCharacter->CombatSettings.Get()
		: Victim->CombatSettings.Get();
	const FKnockbackConfig Config = KnockbackResolution::Resolve(HitInfo.AttackData, AttackerSettings);
	const UHitReactionSettings* Settings = GetEffectiveSettings();
	const float Distance = KnockbackResolution::PushDistance(
		Config.Distance,
		HitInfo.ChargeLevel,
		HitInfo.AttackData ? HitInfo.AttackData->MaxChargeKnockbackMultiplier : 1.0f,
		Settings ? Settings->KnockbackScale : 1.0f);
	if (Distance <= KINDA_SMALL_NUMBER)
	{
		return false;
	}

	const FVector VictimLocation = Victim->GetActorLocation();
	const FVector AttackerLocation = HitInfo.Attacker
		? HitInfo.Attacker->GetActorLocation()
		: VictimLocation + HitInfo.DirectionToAttacker * 100.0;
	const FVector Direction = KnockbackResolution::ResolveDirection(
		Config.DirectionMode, AttackerLocation, VictimLocation, HitInfo.DirectionToAttacker);
	if (Direction.IsZero())
	{
		return false;
	}

	const int32 Generation = FMath::Max(1, NextKnockbackAlignmentGeneration);
	NextKnockbackAlignmentGeneration = NextKnockbackAlignmentGeneration == MAX_int32 ? 1 : NextKnockbackAlignmentGeneration + 1;

	FAlignmentRequestSpec Spec;
	Spec.OwnerId = TEXT("HitKnockback");
	Spec.OwnerGeneration = Generation;
	Spec.Priority = EDefenseAlignmentPriority::HitKnockback;
	Spec.Executor = EAlignmentExecutor::ProceduralDisplacement;
	Spec.bReleaseWhenFinished = true;
	Spec.Displacement.Direction = Direction;
	Spec.Displacement.Distance = Distance;
	Spec.Displacement.Duration = Config.Duration;
	Spec.Displacement.SpeedProfile = Config.SpeedProfile;
	Spec.Displacement.Clock = EDisplacementClock::ActorTime;
	Spec.Displacement.AnimationBlend = EDisplacementAnimationBlend::AddToAnimation;
	KnockbackAlignmentHandle = Targeting->AcquireAlignmentRequest(Spec);

	const bool bStarted = KnockbackAlignmentHandle.IsValid();
	if (UCombatComponent* Combat = Victim->GetCombatComponent())
	{
		FActionReactionTelemetryRecord Record;
		Record.Event = EActionReactionTelemetryEvent::AlignmentChanged;
		Record.Actor = Victim;
		Record.Counterpart = HitInfo.Attacker.Get();
		Record.AlignmentOwner = TEXT("HitKnockback");
		Record.AlignmentDisposition = bStarted ? FName(TEXT("Started")) : FName(TEXT("Rejected"));
		Record.MovementMagnitude = Distance;
		Record.AttackDataPath = FSoftObjectPath(HitInfo.AttackData.Get());
		Combat->AppendActionReactionTelemetry(MoveTemp(Record));
	}
	if (CombatDebug::IsKnockbackDebugEnabled())
	{
		UE_LOG(LogTemp, Log, TEXT("[KNOCKBACK] %s pushed %.1f cm over %.2f s (mode %s, charge %.2f, scale %.2f) -> %s"),
			*Victim->GetName(), Distance, Config.Duration, *UEnum::GetValueAsString(Config.DirectionMode),
			HitInfo.ChargeLevel, Settings ? Settings->KnockbackScale : 1.0f, bStarted ? TEXT("started") : TEXT("rejected"));
		DrawDebugDirectionalArrow(GetWorld(), VictimLocation, VictimLocation + Direction * Distance, 20.f,
			FColor::Orange, false, CombatDebug::GetDebugDrawDuration(), 0, 2.f);
	}
	return bStarted;
}
```

Task 4 added `CombatDebug::IsKnockbackDebugEnabled()` (`Combat.Debug.Knockback`); `StartKnockback` uses it as written above.

- [ ] **Step 4: Build and run tests `KatanaCombat.Knockback+KatanaCombat.HitReaction+KatanaCombat.Displacement+KatanaCombat.Damage+KatanaCombat.Death`**

Expected: all pass.

- [ ] **Step 5: Commit**

```bash
git add -A Source/KatanaCombat Source/KatanaCombatTest/Private/KnockbackStartTests.cpp Source/KatanaCombatTest/Private/DefenseArchitectureSourceTests.cpp
git commit -m "Push the victim when a directional hit reaction starts

- StartKnockback resolves the attack's knockback, charge and victim scale
  into a self-releasing ProceduralDisplacement request.
- Called only after PlayReactionFromEntry succeeds on the settings path;
  released on paired-state entry and EndPlay; a new hit replaces a push.
- Combat.Debug.Knockback and AlignmentChanged telemetry rows (owner
  HitKnockback) within the existing schema.

Rollback checkpoint: <previous commit>"
```

---

### Task 6: PIE measurement of the real reaction

**Files:**
- Create: `Source/KatanaCombatTest/Private/KnockbackPIETests.cpp`

**Interfaces:**
- Consumes: Tasks 1-5; `AutomationCommon::GetAnyGameWorld()`, `FEditorLoadMap`, `FStartPIECommand`, `FEndPlayMapCommand` (pattern from `DefenseGateBThreatPIEProofTests.cpp`).
- Produces: test `KatanaCombat.Knockback.PIE.ReactionMeasurement` and `Saved/Logs/KnockbackMeasurement.json`.

The spec's Verification section names this test. It replaced a capture-harness scenario because the harness is one ~900-line latent command.

- [ ] **Step 1: Write the PIE test**

Create `Source/KatanaCombatTest/Private/KnockbackPIETests.cpp`:

```cpp
#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "Tests/AutomationEditorCommon.h"
#include "EngineUtils.h"
#include "Characters/PlayerCharacter.h"
#include "Characters/EnemyCharacter.h"
#include "Core/HitReactionComponent.h"
#include "Data/AttackData.h"
#include "Data/HitReactionSettings.h"
#include "Interfaces/DamageableInterface.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "AIController.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonWriter.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/FileHelper.h"

namespace
{
const TCHAR* KnockbackMap = TEXT("/Game/ProjectFiles/Levels/Lvl_ThirdPerson1");
const TCHAR* LightAttackPath = TEXT("/Game/ProjectFiles/Data/PDA/Attack/AttackData/Light/New/LightAttack_1.LightAttack_1");

class FKnockbackMeasurementCommand final : public IAutomationLatentCommand
{
public:
	explicit FKnockbackMeasurementCommand(FAutomationTestBase* InTest) : Test(InTest), Start(FPlatformTime::Seconds()) {}

	virtual bool Update() override
	{
		UWorld* World = AutomationCommon::GetAnyGameWorld();
		if (!World)
		{
			if (FPlatformTime::Seconds() - Start > 20.0) { Test->AddError(TEXT("PIE did not start")); return true; }
			return false;
		}
		if (!Player.IsValid() && !Setup(World)) { return true; }

		const double Now = World->GetTimeSeconds();
		if (Phase == 0) { BeginRun(false, Now); return false; }                   // control: no push
		if (Phase == 1 && Now - RunStart >= 1.5) { EndRun(false); BeginRun(true, Now); return false; }
		if (Phase == 2 && Now - RunStart >= 1.5) { EndRun(true); Report(); return true; }
		return false;
	}

private:
	bool Setup(UWorld* World)
	{
		for (TActorIterator<APlayerCharacter> It(World); It; ++It) { Player = *It; break; }
		for (TActorIterator<AEnemyCharacter> It(World); It; ++It) { Enemy = *It; break; }
		Attack = LoadObject<UAttackData>(nullptr, LightAttackPath);
		if (!Player.IsValid() || !Enemy.IsValid() || !Attack)
		{
			Test->AddError(TEXT("Fixture needs a player, an enemy and LightAttack_1")); return false;
		}
		if (AController* Controller = Enemy->GetController()) { Controller->UnPossess(); }
		Enemy->GetCharacterMovement()->bRunPhysicsWithNoController = true;
		// Transient settings copy so the saved DA_HitReaction is never modified in PIE.
		UHitReactionSettings* Base = Enemy->HitReactionComponent->GetEffectiveSettings();
		PushSettings = Base ? DuplicateObject<UHitReactionSettings>(Base, GetTransientPackage()) : nullptr;
		ControlSettings = Base ? DuplicateObject<UHitReactionSettings>(Base, GetTransientPackage()) : nullptr;
		if (!PushSettings || !ControlSettings) { Test->AddError(TEXT("Enemy has no hit reaction settings")); return false; }
		PushSettings->KnockbackScale = 1.0f;
		ControlSettings->KnockbackScale = 0.0f;
		PlayerStart = Player->GetActorLocation();
		return true;
	}

	void BeginRun(const bool bPush, const double Now)
	{
		const FVector Forward = Player->GetActorForwardVector().GetSafeNormal2D();
		Enemy->SetActorLocation(PlayerStart + Forward * 150.0 + FVector(0, 0, 5));
		Enemy->SetActorRotation((-Forward).Rotation());
		Enemy->HitReactionComponent->HitReactionSettingsOverride = bPush ? PushSettings : ControlSettings;
		Direction = Forward;
		RunOrigin = Enemy->GetActorLocation();
		FHitReactionInfo Hit;
		Hit.Attacker = Player.Get();
		Hit.AttackData = Attack;
		Hit.Damage = 1.0f;
		Hit.DirectionToAttacker = -Forward;
		IDamageableInterface::Execute_ApplyDamage(Enemy.Get(), Hit);
		RunStart = Now;
		++Phase;
	}

	void EndRun(const bool bPush)
	{
		const double Along = FVector::DotProduct(Enemy->GetActorLocation() - RunOrigin, Direction);
		(bPush ? PushDisplacement : ControlDisplacement) = Along;
	}

	void Report()
	{
		const double Added = PushDisplacement - ControlDisplacement;
		Test->AddInfo(FString::Printf(TEXT("Reaction root motion alone: %.1f cm; with knockback: %.1f cm; added: %.1f cm"),
			ControlDisplacement, PushDisplacement, Added));
		Test->TestTrue(TEXT("Knockback adds roughly the resolved light push (25 cm)"), FMath::IsNearlyEqual(Added, 25.0, 8.0));
		TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
		Json->SetNumberField(TEXT("reaction_root_motion_cm"), ControlDisplacement);
		Json->SetNumberField(TEXT("with_knockback_cm"), PushDisplacement);
		Json->SetNumberField(TEXT("added_cm"), Added);
		FString Out;
		FJsonSerializer::Serialize(Json, TJsonWriterFactory<>::Create(&Out));
		FFileHelper::SaveStringToFile(Out, *(FPaths::ProjectSavedDir() / TEXT("Logs/KnockbackMeasurement.json")));
	}

	FAutomationTestBase* Test;
	double Start;
	TWeakObjectPtr<APlayerCharacter> Player;
	TWeakObjectPtr<AEnemyCharacter> Enemy;
	UAttackData* Attack = nullptr;
	UHitReactionSettings* PushSettings = nullptr;
	UHitReactionSettings* ControlSettings = nullptr;
	FVector PlayerStart = FVector::ZeroVector;
	FVector RunOrigin = FVector::ZeroVector;
	FVector Direction = FVector::ForwardVector;
	double RunStart = 0.0;
	int32 Phase = 0;
	double ControlDisplacement = 0.0;
	double PushDisplacement = 0.0;
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FKnockbackPIEMeasurementTest, "KatanaCombat.Knockback.PIE.ReactionMeasurement",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FKnockbackPIEMeasurementTest::RunTest(const FString&)
{
	ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(KnockbackMap));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(1.0f));
	ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
	ADD_LATENT_AUTOMATION_COMMAND(FKnockbackMeasurementCommand(this));
	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(1.0f));
	return true;
}
```

`UHitReactionComponent::GetEffectiveSettings` must be callable here. It is public (`HitReactionComponent.h:72`).

- [ ] **Step 2: Build and run tests `KatanaCombat.Knockback.PIE`**

Expected: pass, with an info line reporting the reaction's own root motion and the added push, and `Saved/Logs/KnockbackMeasurement.json` written.

- [ ] **Step 3: Decide defaults from the measurement (checkpoint with the user)**

Read `Saved/Logs/KnockbackMeasurement.json` and report both numbers to the user. The spec decisions this step informs:

- If the reaction's own step-back along the push is under 15 cm, keep `AddToAnimation` and the 25/60 cm defaults.
- At 15 cm or more, ask the user before continuing: keep `AddToAnimation`, switch the knockback default to `ReplaceAnimation`, or lower the defaults. Record the decision in the PR description.

Do not change defaults without that answer.

- [ ] **Step 4: Commit**

```bash
git add Source/KatanaCombatTest/Private/KnockbackPIETests.cpp
git commit -m "Measure knockback against a real root-motion hit reaction in PIE

Lands a hit on the ThirdPerson map with knockback disabled and enabled,
reports the reaction's own root motion and the added push, and writes
Saved/Logs/KnockbackMeasurement.json.

Rollback checkpoint: <previous commit>"
```

---

### Task 7: Proof regressions, documentation, baseline and PR

**Files:**
- Modify (only if a proof fails for position reasons): the failing proof's test file
- Modify: `CLAUDE.md`, `docs/guides/ATTACK_CREATION.md`, `docs/architecture/API_REFERENCE.md`, `docs/audits/DATA_ASSET_AUDIT_2026-07-21.md`

- [ ] **Step 1: Run the proofs the push can disturb**

Run tests `KatanaCombat.Defense.GateA+KatanaCombat.Defense.GateB+KatanaCombat.Capture.Scenarios`.
Expected: all pass. If one fails:

1. Read the failure. Confirm it is caused by a victim position change after a landed hit, for example the Gate A parry bridge's 75 cm per-role budget, or the Gate B semantic proof's unblockable hit on the player.
2. Only then disable knockback transiently in that proof's fixture. Before the scenario runs, assign the affected character's `HitReactionComponent->HitReactionSettingsOverride` a `DuplicateObject` of its effective settings with `KnockbackScale = 0` (the pattern from Task 6's `Setup`). Never edit saved assets.
3. Re-run and confirm the proof passes. Note the change and its reason in the PR description.

If a failure is not position-related, stop and investigate it as a bug.

- [ ] **Step 2: Update documentation**

- `CLAUDE.md` "Key Default Values" table: add rows `Knockback (Light) | 25 cm / 0.2 s | UCombatSettings::DefaultKnockback, EaseOut` and `Knockback (Heavy) | 60 cm / 0.25 s | per-attack overrides in UAttackData::Knockback`. Add `Combat.Debug.Knockback 1` to the debug CVar list.
- `docs/guides/ATTACK_CREATION.md`: a "Knockback" subsection describing `UAttackData::Knockback` fields and inline overrides, per-type defaults, `KnockbackScale` on the victim, and that only interrupting reactions push.
- `docs/architecture/API_REFERENCE.md`: document `EAlignmentExecutor::ProceduralDisplacement`, `FProceduralDisplacement`, `EDefenseAlignmentPriority::HitKnockback`, `bReleaseWhenFinished`, and `UHitReactionComponent::ReleaseKnockback`.
- `docs/audits/DATA_ASSET_AUDIT_2026-07-21.md` execution log: add "2026-09-30: knockback wired (FKnockbackConfig, KnockbackScale); FHitReactionEntry::KnockbackForce removed."

- [ ] **Step 3: Run the full baseline**

Run `powershell -NoProfile -ExecutionPolicy Bypass -File "Tools\Codex\run-agent-baseline.ps1"`.
Expected: `BASELINE GREEN`. The completed count equals the previous 814 plus the new tests. Record the exact count.

- [ ] **Step 4: Commit the docs**

```bash
git add CLAUDE.md docs/guides/ATTACK_CREATION.md docs/architecture/API_REFERENCE.md docs/audits/DATA_ASSET_AUDIT_2026-07-21.md
git commit -m "Document knockback and the procedural displacement executor

Rollback checkpoint: <previous commit>"
```

- [ ] **Step 5: Push and open the PR**

```bash
git push -u origin feat/step3a-knockback
gh pr create --base main --title "Knockback through a procedural displacement executor" --body-file "$SCRATCHPAD/pr-3a-knockback.md"
```

`$SCRATCHPAD` is the session's scratchpad directory. Write the PR body there first, never in the repository.

The PR body lists:
- the tasks;
- the measurement from Task 6 and the default decision;
- any proof fixture change from Step 1;
- the baseline count;
- the spec's manual PIE checks for this PR. With `Combat.Debug.Knockback 1`, check both speed profiles, and check that there is no push on a blocked hit, a parried hit or a super-armor victim. Ask the user to play these in the editor, and record what they observed. An agent cannot play-test.

**No AI attribution in the PR body.**
