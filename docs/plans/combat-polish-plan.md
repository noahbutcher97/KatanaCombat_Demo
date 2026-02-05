# Combat Polish Plan - Normal Attack Effects & Camera

> **Created**: 2026-02-03 | **Status**: ✅ IMPACT EFFECTS COMPLETE | Camera work remaining
> **Priority**: P2 - Camera polish is optional enhancement
> **Implemented**: 879d1c2, 0e6ae4e, 150cd3a, 3038b21, f27a068

---

## Problem Statement

~~Normal attacks have **zero cinematic effects**. No hitstop, no camera shake, no impact audio, no VFX.~~ **RESOLVED** - Impact effects now fully implemented.

Additionally, the camera/spring arm system uses default UE5 collision behavior with no combat-aware adjustments, causing camera compression and clipping during close-range combat (both normal attacks and finishers). **Camera work remains TODO.**

## Current State (Updated 2026-02-05)

### Normal Attacks - Effect Coverage

| Effect | Status | Commit | Notes |
|--------|--------|--------|-------|
| Hitstop | ✅ WIRED | 879d1c2 | `FHitstopConfig` on AttackData, `ApplyHitstop()` in OnWeaponHitTarget |
| Camera Shake | ✅ WIRED | 879d1c2 | Via `FHitstopConfig.CameraShake` in `ApplyHitstop()` |
| Impact Audio | ✅ WIRED | 0e6ae4e, 150cd3a | 4-tier resolution via `ResolveAndPlayImpactSound()` |
| Impact VFX | ✅ WIRED | 3038b21 | 4-tier resolution via `ResolveAndSpawnImpactVFX()` |
| Pooled FX | ✅ WIRED | 150cd3a | `UCombatFXData` with random selection per attack type |

### Finishers - Effect Coverage

| Effect | Status | Commit | Notes |
|--------|--------|--------|-------|
| Hitstop | ✅ WIRED | 879d1c2 | `bApplyHitstop` in sync notify, duration configurable |
| Camera Shake | ✅ WIRED | - | `TriggerSyncPointEffects()` calls `PlayCameraShakeOnActor()` |
| Slow-Mo | ✅ WIRED | - | `BeginPairedAnimation()` calls `ApplySlowMotion()` |
| Impact Audio | ✅ WIRED | f27a068 | `TriggerSyncPointEffects()` plays 3 sounds |
| Impact VFX | ✅ WIRED | f27a068 | `TriggerSyncPointEffects()` spawns VFX at contact point |

### Camera System

| Aspect | Status | Notes |
|--------|--------|-------|
| Spring Arm | Default UE5 | `TargetArmLength = 100cm`, lag enabled |
| Collision | Default probe | Standard `bDoCollisionTest` behavior |
| Combat Adjustment | None | No FOV, arm length, or collision channel changes during combat |
| Finisher Framing | None | Camera follows passively during finisher |

---

## Architecture Decision: Where Do Effect Properties Live?

### Option A: Add to AttackData (Per-Attack Customization)

Add effect properties directly to `UAttackData`. Each attack asset configures its own effects.

**Pros**: Maximum per-attack control (Marth's sword tip vs blade edge), consistent with existing data-driven pattern
**Cons**: Adds properties to every attack asset, most will use defaults

### Option B: CombatSettings Global Defaults Only

Add global effect settings to `UCombatSettings`. All attacks use the same hitstop/shake/audio.

**Pros**: Simple, single configuration point
**Cons**: Can't differentiate light vs heavy, no per-attack tuning

### Option C: CombatSettings Defaults + AttackData Overrides (Recommended)

`UCombatSettings` provides sensible defaults. `AttackData` can optionally override per-attack.

**Pros**: Best of both worlds - zero-config works, per-attack tuning available
**Cons**: Slightly more complex property resolution

**Decision**: Option C. This follows the existing pattern (e.g., `TargetingSettingsOverride` on TargetingComponent).

---

## Implementation Plan

### Step 1: Impact Effects Data Structure

Create a shared effect configuration struct used by both `CombatSettings` (defaults) and `AttackData` (overrides).

**File**: `Source/KatanaCombat/Public/CombatTypes.h`

```cpp
USTRUCT(BlueprintType)
struct FImpactEffectConfig
{
    GENERATED_BODY()

    /** Hitstop duration on hit (0 = no hitstop). Both attacker and victim freeze. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hitstop",
        meta = (ClampMin = "0.0", ClampMax = "0.3"))
    float HitstopDuration = 0.0f;

    /** Camera shake to play on hit */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera")
    TSubclassOf<UCameraShakeBase> ImpactCameraShake;

    /** Camera shake intensity scale */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera",
        meta = (ClampMin = "0.0", ClampMax = "2.0"))
    float CameraShakeScale = 1.0f;

    /** Impact sound played at hit location */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio")
    TObjectPtr<USoundBase> ImpactSound;

    /** Victim reaction sound (grunt/stagger) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio")
    TObjectPtr<USoundBase> VictimHitSound;

    /** Niagara system spawned at impact point */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VFX")
    TObjectPtr<UNiagaraSystem> ImpactVFX;

    /** Whether this config has any effect enabled */
    bool HasAnyEffect() const
    {
        return HitstopDuration > 0.0f || ImpactCameraShake || ImpactSound || VictimHitSound || ImpactVFX;
    }
};
```

### Step 2: Add Default Configs to CombatSettings

**File**: `Source/KatanaCombat/Public/Data/CombatSettings.h`

```cpp
// === IMPACT EFFECTS (Defaults for normal attacks) ===

/** Default impact effects for light attacks */
UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Impact Effects")
FImpactEffectConfig LightAttackImpactDefaults;

/** Default impact effects for heavy attacks */
UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Impact Effects")
FImpactEffectConfig HeavyAttackImpactDefaults;
```

Default values (set in constructor, calibrated to industry standards):
- Light: `HitstopDuration = 0.07f` (4-5 frames at 60fps), light camera shake
- Heavy: `HitstopDuration = 0.13f` (7-8 frames at 60fps), stronger camera shake

### Step 3: Add Optional Override to AttackData

**File**: `Source/KatanaCombat/Public/Data/AttackData.h`

```cpp
// === IMPACT EFFECTS (Optional per-attack override) ===

/** Override impact effects for this specific attack. If not set, uses CombatSettings defaults. */
UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Impact Effects")
bool bOverrideImpactEffects = false;

UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Impact Effects",
    meta = (EditCondition = "bOverrideImpactEffects", EditConditionHides))
FImpactEffectConfig ImpactEffectOverride;
```

### Step 4: Resolve Effects at Hit Time

**File**: `Source/KatanaCombat/Private/Characters/BaseCombatCharacter.cpp`

In `OnWeaponHitTarget()`, after damage application succeeds, apply cinematic effects:

```cpp
// After IDamageableInterface::Execute_ApplyDamage(HitActor, HitInfo):

// Resolve impact effects (per-attack override or settings default)
FImpactEffectConfig EffectConfig;
if (AttackData->bOverrideImpactEffects)
{
    EffectConfig = AttackData->ImpactEffectOverride;
}
else if (UCombatSettings* Settings = /* get from CombatComponent */)
{
    EffectConfig = (AttackData->AttackType == EAttackType::Light)
        ? Settings->LightAttackImpactDefaults
        : Settings->HeavyAttackImpactDefaults;
}

ApplyImpactEffects(EffectConfig, HitActor, HitResult.ImpactPoint);
```

### Step 5: Implement ApplyImpactEffects

**File**: `Source/KatanaCombat/Private/Characters/BaseCombatCharacter.cpp`

```cpp
void ABaseCombatCharacter::ApplyImpactEffects(
    const FImpactEffectConfig& Config,
    AActor* HitActor,
    const FVector& ImpactPoint)
{
    // --- HITSTOP (Sakurai technique) ---
    if (Config.HitstopDuration > 0.0f)
    {
        // Freeze both attacker and victim using CustomTimeDilation
        // Uses FPlatformTime::Seconds() + FTSTicker for accurate real-time restore
        UCinematicEffectsUtilityLibrary::ApplyHitstop(this, HitActor, Config.HitstopDuration);
    }

    // --- CAMERA SHAKE ---
    if (Config.ImpactCameraShake)
    {
        UCinematicEffectsUtilityLibrary::PlayCameraShakeOnActor(
            this, Config.ImpactCameraShake, Config.CameraShakeScale);
    }

    // --- AUDIO ---
    if (Config.ImpactSound)
    {
        UGameplayStatics::PlaySoundAtLocation(
            GetWorld(), Config.ImpactSound, ImpactPoint);
    }
    if (Config.VictimHitSound && HitActor)
    {
        UGameplayStatics::PlaySoundAtLocation(
            GetWorld(), Config.VictimHitSound, HitActor->GetActorLocation());
    }

    // --- VFX ---
    if (Config.ImpactVFX)
    {
        UNiagaraFunctionLibrary::SpawnSystemAtLocation(
            GetWorld(), Config.ImpactVFX, ImpactPoint,
            FRotator::ZeroRotator, FVector(1.0f), true);
    }
}
```

### Step 6: Add Hitstop to CinematicEffectsUtilityLibrary

**File**: `Source/KatanaCombat/Public/Utilities/CinematicEffectsUtilityLibrary.h`

```cpp
/**
 * Apply Sakurai-style hitstop: freeze attacker + victim for Duration seconds.
 * Uses CustomTimeDilation (selective) so world continues.
 * Uses FPlatformTime::Seconds() for accurate real-time tracking.
 */
UFUNCTION(BlueprintCallable, Category = "Cinematic Effects|Hitstop")
static void ApplyHitstop(AActor* Attacker, AActor* Victim, float Duration);
```

Implementation uses `FPlatformTime::Seconds()` + `FTSTicker` (same pattern as the paired animation sync notify hitstop, which already works but defaults to off).

### Step 7: Wire Finisher Audio/VFX (Parity)

The finisher flow already has `TriggerSyncPointEffects()` in CombatComponent. Add the missing audio/VFX calls there:

**File**: `Source/KatanaCombat/Private/Core/CombatComponent.cpp` (in `TriggerSyncPointEffects()`)

```cpp
// --- AUDIO (using PairedAnimationData properties) ---
if (ActivePairedAnimData->ImpactSound)
{
    UGameplayStatics::PlaySoundAtLocation(GetWorld(),
        ActivePairedAnimData->ImpactSound, SyncPointLocation);
}
if (ActivePairedAnimData->VictimReactionSound && PairedAnimationVictim.IsValid())
{
    UGameplayStatics::PlaySoundAtLocation(GetWorld(),
        ActivePairedAnimData->VictimReactionSound,
        PairedAnimationVictim->GetActorLocation());
}

// --- VFX (using PairedAnimationData properties) ---
if (ActivePairedAnimData->ImpactVFX)
{
    UNiagaraFunctionLibrary::SpawnSystemAtLocation(GetWorld(),
        ActivePairedAnimData->ImpactVFX, SyncPointLocation);
}
```

### Step 8: Camera/Spring Arm Combat Awareness

**Problem**: Default spring arm compression causes jarring camera behavior during close combat.

**Solution**: Combat-aware spring arm adjustments via CombatComponent delegate.

**File**: `Source/KatanaCombat/Variant_Combat/CombatCharacter.h/.cpp`

```cpp
// Properties
UPROPERTY(EditAnywhere, Category = "Camera|Combat")
float CombatCameraDistance = 200.0f;  // Pull back during combat

UPROPERTY(EditAnywhere, Category = "Camera|Combat")
float FinisherCameraDistance = 300.0f;  // Further back for finisher framing

UPROPERTY(EditAnywhere, Category = "Camera|Combat")
float CameraTransitionSpeed = 3.0f;  // Interpolation speed

// In BeginPlay, bind to combat state changes
CombatComponent->OnCombatStateChanged.AddDynamic(this, &HandleCombatCameraTransition);
```

On combat state change:
- `Idle` -> `DefaultCameraDistance` (100cm)
- `Attacking`/`Recovering` -> `CombatCameraDistance` (200cm) - slight pullback prevents clipping
- `Finishing` -> `FinisherCameraDistance` (300cm) - wide framing for paired animation

Spring arm collision channel override during finishers:
- Temporarily ignore character collision channel on spring arm probe
- Prevents victim/attacker meshes from compressing the camera

---

## Implementation Order

| Step | What | Files | Dependency |
|------|------|-------|------------|
| 1 | `FImpactEffectConfig` struct | CombatTypes.h | None |
| 2 | CombatSettings defaults | CombatSettings.h/.cpp | Step 1 |
| 3 | AttackData override | AttackData.h | Step 1 |
| 4 | `ApplyHitstop()` in CinematicEffectsUtilityLibrary | CinematicEffectsUtilityLibrary.h/.cpp | None |
| 5 | `ApplyImpactEffects()` + wire in `OnWeaponHitTarget()` | BaseCombatCharacter.h/.cpp | Steps 1-4 |
| 6 | Wire finisher audio/VFX | CombatComponent.cpp | None |
| 7 | Camera combat awareness | CombatCharacter.h/.cpp | None |

Steps 1-5 are the core normal attack polish. Steps 6-7 are independent and can be done in parallel.

---

## Verification

### Normal Attack Effects
- [ ] Light attack hit produces brief hitstop (2-3 frames)
- [ ] Heavy attack hit produces longer hitstop (5 frames)
- [ ] Camera shakes on hit with appropriate intensity
- [ ] Impact sound plays at hit location
- [ ] VFX spawns at impact point (when configured)
- [ ] Per-attack override works (bOverrideImpactEffects)
- [ ] Missing settings gracefully fall back (no crash if no CombatSettings)

### Finisher Effects Parity
- [ ] Audio plays at sync point (ImpactSound, VictimReactionSound)
- [ ] VFX spawns at sync point (ImpactVFX)
- [ ] Hitstop enabled by default on finisher sync notify

### Camera
- [ ] Camera pulls back slightly during attack sequences
- [ ] Camera pulls back further during finishers
- [ ] Spring arm doesn't clip through characters during close combat
- [ ] Smooth transition between camera distances
- [ ] Camera returns to default distance when combat ends

---

## Design Notes

### Hitstop Philosophy (Sakurai Technique)

> **Updated 2026-02-03** based on industry research (see `docs/audits/COMPREHENSIVE_AUDIT_2026-02-03.md`)

- Both parties freeze (attacker AND victim) -- industry standard
- Selective freeze via `CustomTimeDilation` -- world continues
- **CRITICAL**: Use `CustomTimeDilation = 0.0001f`, NOT `0.0f` (division-by-zero risk)
- **CRITICAL**: Save/restore pre-existing dilation value (don't clobber slow-mo with 1.0f)
- Damage-proportional: heavier hits = longer freeze
- VFX should play DURING hitstop (particles continue while characters freeze)
- Camera shake should play AFTER hitstop ends (not during)
- Input buffering must continue during hitstop (buffer on real-time, not game-time)
- Uses `FPlatformTime::Seconds()` for accurate real-time tracking (not affected by time dilation)

**Industry-Calibrated Frame Counts (at 60fps)**:

| Attack Type | Frames | Duration | Previous Plan | Change |
|-------------|--------|----------|---------------|--------|
| Light attacks | 4-5 | 0.067-0.083s | 0.04s (~2.4 frames) | **Increased** to match industry minimum |
| Heavy attacks | 7-9 | 0.117-0.150s | 0.08s (~5 frames) | **Increased** for proper impact feel |
| Finisher sync | 10-15 | 0.167-0.250s | 0.12-0.15s | Aligned with industry |
| Counter/Parry | 8-12 | 0.133-0.200s | N/A | **NEW** -- reward feel |

Reference: Smash Bros formula `floor((damage * 0.65 + 6) * hitlag_multiplier)`, SF2 normals 9-13 frames.

### Audio Spatial Strategy

> **Updated 2026-02-03** based on industry research

- Impact sounds: Play at `HitResult.ImpactPoint` (where weapon meets victim) -- industry confirmed
- Victim sounds: Play at victim's actor location -- correct
- Both use `PlaySoundAtLocation()` for 3D spatial audio
- **Use 5-10 variations per impact sound** to prevent repetition fatigue
- **Layer 4 audio per hit**: base impact, weapon whoosh (pre-impact), defender vocalization, environmental
- **Music ducking during finishers**: -6 to -9 dB, ramp down ~500ms before impact, hold during, ramp up ~1000ms after
- **Slow-mo audio**: pitch-shift SFX downward to match time dilation, keep music at normal speed

### Impact Effect Firing Order (Industry Standard)

Effects should fire in this specific order per hit:
1. **Hitstop** -- immediate (frame of hit), freeze both characters
2. **VFX** -- during hitstop (particles continue while characters are frozen)
3. **Audio** -- immediate, layered (impact + whoosh + vocalization)
4. **Camera Shake** -- AFTER hitstop ends (shaking during freeze feels wrong)
5. **Screen Effects** -- heavy hits only (chromatic aberration, vignette)
6. **Controller Rumble** -- matches audio timing

### Camera Distance Values

> Note: Industry standard for AAA melee games is 300-600cm. KatanaCombat's values
> are intentionally closer for GoT-style intimacy, but may need adjustment if clipping occurs.
> Consider `UCameraModifier` subclasses (Daedalic pattern) for proper architecture.

- Default: 100cm (close third-person, current setting) -- industry: 300-500cm
- Combat: 200cm (pulled back to show action space) -- industry: 400-600cm
- Finisher: 300cm (wide framing for paired animation) -- industry: variable/cinematic
- Death: 400cm (already exists in CombatCharacter)

### Controller Haptics (Future)

| Attack Type | Left Motor | Right Motor | Duration |
|-------------|-----------|------------|----------|
| Light swing | 0 | 0.2 | 50ms |
| Light hit | 0.3 | 0.4 | 80ms |
| Heavy swing | 0.2 | 0.1 | 100ms |
| Heavy hit | 0.7 | 0.5 | 150ms |
| Finisher | 1.0 ramp | 0.8 ramp | 300ms |
| Parry | 0.5 pulse | 0.8 pulse | 60ms |

---

## Audit Notes (2026-02-03)

The following issues were found by the comprehensive audit and affect this plan:

1. **`bApplyHitPause` location**: This property is on `AnimNotifyState_PairedAnimationSync`, NOT on `PairedAnimationData` as previously stated in some docs.
2. **`OnCombatStateChanged` delegate**: Declared as type in CombatTypes.h but NOT a UPROPERTY member on CombatComponent. Step 8 (camera binding) will need this delegate added to CombatComponent first.
3. **`FImpactEffectConfig` includes**: CombatTypes.h will need forward declarations for `USoundBase` and `UNiagaraSystem`.
4. **CustomTimeDilation 0.0f risk**: Existing sync notify hitstop uses 0.0f -- update to 0.0001f.
5. **Save/restore dilation**: Hitstop must save pre-hitstop `CustomTimeDilation` and restore it (not hardcode 1.0f) to coexist with slow-mo.
6. **Effect firing order**: VFX should play during hitstop, camera shake should play after hitstop ends.

## Related Gaps (from gap-tracker.md)

| Gap | Description | Addressed By |
|-----|-------------|--------------|
| 7.x | Hit Stop/Hit Pause | Step 4-5 (hitstop implementation) |
| 2.2 | Camera input handling | Step 7 (camera awareness) |
| 4.1-4.4 | Audio synchronization | Step 5-6 (audio wiring) |
| 15.1-15.6 | VFX scaffolding | Step 5-6 (VFX wiring) |
| 14.x | Polish gaps | Steps 1-7 (overall polish pass) |
| 22.10 | CustomTimeDilation 0.0f crash risk | Step 4 (use 0.0001f) |
| 18.13 | Hitstop vs slow-mo coordination | Step 4 (save/restore dilation) |
