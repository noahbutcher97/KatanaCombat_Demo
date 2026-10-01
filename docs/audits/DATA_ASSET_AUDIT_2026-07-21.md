# Data Asset & Dead Code Audit — 2026-07-21

**Scope**: All combat data asset classes (`Source/KatanaCombat/Public/Data/`) plus the shared authored structs they embed from `CombatTypes.h` and `PairedAnimationTypes.h`. Every `UPROPERTY` was traced to its runtime consumers across all three modules (KatanaCombat, KatanaCombatEditor, KatanaCombatTest), cross-checked against the serialized content of the 58 data assets under `Content/ProjectFiles/Data/`.

**Method**: Six parallel source-tracing passes (one per class cluster) with file:line evidence for every verdict; content-side sampling via asset-registry scan + serialized-property reads; best-practices research against Epic documentation, Lyra conventions, and the unreal-garden specifier reference.

**Verdict vocabulary**:
- **WIRED** — runtime gameplay code reads it and the value changes behavior
- **PARTIAL** — read, but part of the documented behavior is missing
- **EDITOR-ONLY** — consumed only by editor tools/validation/commandlets
- **SCAFFOLD** — property or code path exists, nothing reaches it
- **DEAD** — no reader anywhere (or reader itself is unreachable)
- **DEPRECATED** — explicitly marked deprecated

---

## Executive Summary

| Class | Params | Wired | Dead/Inert at runtime | Tooltip coverage | Grade |
|---|---|---|---|---|---|
| UAttackData | 40 (+ embedded structs) | 22 | **11 dead, 2 deprecated, 7 editor-only** | ~83% | ⚠️ Fair |
| UPairedAnimationData | 44 | 32 | **6 dead, 5 scaffold, 1 editor-only** | 100% (several wrong) | ⚠️ Fair |
| UDefenseConfiguration | 32 | 25 | **7 runtime-dead** | **0%** | ⚠️ Fair |
| FDefensePresentationPayload | 17 | 15 | 2 runtime-dead | **0%** | 🟢 Good |
| UWeaponData | 25 | 22 | 2 dead, 1 test-only | 100% (2 wrong) | 🟢 Good |
| UCombatFXData | 4 + pool structs | wired | SurfacePools scaffold | 100% | 🟢 Good |
| UAttackConfiguration | 5 | 2 | 3 dead (documented) | 100% | 🟢 Good |
| UCombatSettings | 5 | 4 | **1 dead slot** | 100% | 🟢 Good |
| **UMotionWarpingSettings** | 6 | 0 | **entire class dead** | 100% | 🔴 Delete |
| **UHitReactionData** | 22 | 0 | **entire class dead** (0 instances, BP-only path) | 100% | 🔴 Delete/decide |
| UHitReactionSettings | 15 | 8 | **6 scaffold, 1 dead** | 100% (2 wrong) | ⚠️ Fair |
| FHitReactionEntry | 13 | 10 | 1 dead, 2 partial | 100% | 🟢 Good |
| **FFinisherTriggerConfig** | 8 | 0 | **entire struct orphaned** (no owner) | 100% (all wrong) | 🔴 Delete/wire |
| UTargetingSettings | 9 | 9 | 0 | 100% | ✅ Excellent |

**Headline numbers**: of ~250 authored parameters, **~65 are dead, scaffold, or inert at runtime**, including two entire classes and one entire struct. **~28 tooltips describe behavior that does not exist.** The tooltip gap is concentrated: the defense family (~70 params across UDefenseConfiguration + rows + payload + FDefenseAttackProfile) has **zero tooltips** despite being almost fully wired.

**The healthy core**: combo chains, montage sections, hold easing, charge sections, the entire impact-effect pipeline (hitstop/audio/VFX incl. blocked paths), FAttackWarpConfig (fully honored), FDefenseAttackProfile, the defense presentation row/payload system, paired-animation chain policy + warp configs + sync effects + damage flow, directional hit reactions with the variety system, and targeting settings. The data-driven backbone works.

---

## Part 1 — Per-Class Findings

### 1.1 UAttackData (`Data/AttackData.h`)

**DEAD (11)** — no runtime reader; tooltip promises unimplemented behavior:

| Property | Evidence | The lie |
|---|---|---|
| `StaggerPower` | No reader anywhere; stagger is duration-based via `ApplyStagger(Duration)` (CombatComponent.cpp:3167, PairedAnimationComponent.cpp:4205) | Tooltip describes a 0–1 stagger-chance system that does not exist |
| `MaxChargeTime` | Only feeds `CalculateChargeLevel` (MontageUtilityLibrary.cpp:411) which has **zero callers** | Charge level is never computed |
| `ChargeTimeScale` | Header-only | Charge windup playrate is never scaled |
| `MaxChargeDamageMultiplier` | Header-only | Charged heavies do **not** scale damage |
| `bEnforceMaxHoldTime` / `MaxHoldTime` | Header-only (no editor read either) | No hold-time cap exists |
| `ComboBlendOutTime` / `ComboBlendInTime` | Superseded by procedural blend (CombatComponent.cpp:4115-4117 comment says so explicitly) | Per-attack blend values ignored |
| `bJumpToSectionStart` | Jump at CombatComponent.cpp:4313 is unconditional | Setting false does nothing |
| `TimingFallbackMode` | No reader anywhere, not even editor timing tools | Fallback mode never consulted |
| `CounterDamageMultiplier` | Only a commented-out test | Correctly labeled "[NOT YET IMPLEMENTED]" |

**DEPRECATED (2)**: `PostureDamage`, `ChargedPostureDamage` — correctly marked, never read; ready for the deletion workflow (Part 6.3).

**EDITOR-ONLY (7)**: `Direction` (commandlet diagnostics only — directional follow-ups key off *input* direction, not this), `ComboInputWindow` (only authors the ComboWindow notify; runtime combo timing is notify-driven), `bCanHold` (only gates notify generation — runtime never re-checks), `bUseAnimNotifyTiming`, `ManualTiming` (timing generation tools — as documented), `bCanTriggerFinisher`, `bHasCounterVariant` (see redundancy finding below).

**Redundant boolean+reference pairs**: Runtime finisher/counter gates are **pure null-checks** on `FinisherData` (PairedAnimationComponent.cpp:2600) and `CounterData` (:4278). The booleans are enforced to agree by editor validation, then ignored at runtime. Content already co-authors both (LightAttack_1 sets `bCanTriggerFinisher=true` *and* `FinisherData`) — two hand-maintained sources of truth for one fact.

**Repurposed with misleading tooltips**: `AttackHand` ("procedural IK") and `DefaultContactBone` ("IK adjustment… during paired animations") are actually **defense contact-point fallbacks** (source socket fallback at CombatComponent.cpp:2016; target bone via `GetDefenseTargetBoneFallback()` at WeaponComponent.cpp:899). No IK or paired-anim code reads either.

**PARTIAL**: `HitStunDuration` reaches `ApplyHitStun` only in the legacy fallback path (HitReactionComponent.cpp:489-505); the primary directional-reaction path (474-484) applies the Settings entry's `StunDuration` and returns early — the per-attack value is silently bypassed whenever a directional entry resolves.

**Fully WIRED and healthy**: `AttackType`, `AttackMontage`, `MontageSection`, `bUseSectionOnly`, `BaseDamage`, `MaxHitCount` (enforced at WeaponComponent.cpp:791,854), all four combo-chain maps, charge sections + blend times, all five hold-easing fields, `WarpConfig` (see 1.2), `DefenseProfile` (all 9 fields), `HitstopConfig`/`ImpactAudioConfig`/`ImpactVFXConfig` (all fields, incl. blocked paths), `AttackTags` + `RequiredContextTags` — **both tag containers are genuinely consumed by runtime resolution** (DefenseResolver.cpp:239/338/370, DefensePresentationSelector.cpp:51-52, MontageUtilityLibrary.cpp:1115), satisfying Design Rule 7.

**Stale comment**: `ImpactVFXConfig` is still labeled `[SCAFFOLD FOR U-16]` (AttackData.h:129) but is fully wired via `ResolveAndSpawnImpactVFX` (CinematicEffectsUtilityLibrary.cpp:630).

### 1.2 FAttackWarpConfig (CombatTypes.h:1078)

**All 11 fields honored.** `SetupAttackWarp` honors bEnableWarp, warp names, distances, `TargetRelativeOffset` (TargetingComponent.cpp:604,609), rotation speed/budget/tolerance; `AlreadyFacingThreshold` and `NoInputFacingCone` are consumed upstream by the CombatComponent caller (5955-6022, 5978).

> **CLAUDE.md is stale**: the "Editor/Runtime Unification Gap" section claims `SetupAttackWarp()` ignores offsets and `FAttackWarpConfig` lacks an offset field. Both claims are outdated — `TargetRelativeOffset` exists and is honored.

**DEAD**: legacy typedefs `FDirectionalWarpConfig` / `FMotionWarpingConfig` (CombatTypes.h:1161-1162) — zero references; the comment says "remove after updating all references" and that day has come.

### 1.3 UPairedAnimationData (`Data/PairedAnimationData.h`)

**DEAD/inert (6)**:
- `Description` — no reader at all (not even validation).
- `SyncPointTime` + `SyncPointName` — runtime sync is **entirely notify-driven**: `AnimNotifyState_PairedAnimationSync::NotifyBegin` passes *its own* SyncPointName (AnimNotifyState_PairedAnimationSync.cpp:41). The asset fields are validated-only duplicates that can silently disagree with the notify.
- `VictimRelativePosition` / `VictimFacingMode` / `VictimRelativeRotation` — superseded by `VictimWarpConfig.RelativeOffset` (the field that actually drives warping, TargetingComponent.cpp:1707); reachable only via test-only utility functions. Also: `VictimFacingMode` is an `int32` with -1/0/1 semantics — a Rule 7 violation (closed state belongs to enums) — but since it is dead, deletion beats conversion.

**SCAFFOLD (5)** — confirmed no reader: `MusicDuckingDB`, `SlowMoPostProcessMaterial`, `SlowMoPostProcessWeight`, `ScreenBloodMaterial`, `bSpawnBloodDecals` (matches CLAUDE.md's scaffold table).

**EDITOR-ONLY**: `AnimationName` — tooltip claims "used in TMap lookups"; **no such lookup exists**. Display/log strings only.

**WIRED with nuances**:
- `ChainTransitionPolicy` — all 9 subfields consumed (marker gating, ready sections, response window, auto-continue, retry).
- Blend fields (`AttackerBlendIn/Out`, `VictimBlendIn/Out`) apply **only on the defense-chain path** (comp:3290,3337); the legacy finisher path plays with no blend (comp:3674).
- `VictimStartOffset`: only negative (victim-later) honored on the finisher path — positive clamps to 0 (comp:3724).
- Top-level `MaxWarpDistance`/`MinTriggerDistance`/`MaxTriggerDistance`: consumed **only by defense-chain bridge/retained-stage preflights** (comp:1864-1916, 2847-2887). The finisher acquisition path uses `TargetingSettings->SoftAimRange` (comp:2661), as CLAUDE.md documents — meaning `DA_Finisher_GateA`'s authored `MinTriggerDistance=0` had no effect on finisher behavior.
- Both `FPairedWarpConfig`s fully honored, including `bAdjustToTerrain`.
- `AttackerVoiceLine` tooltip says "plays at animation start or configurable time" — it plays at the **sync-point notify** (comp:4684); no configurable-time mechanism exists.
- Damage flow verified: finishers enforce `max(FinalDamage, CurrentHealth+1)` (comp:1217); counters clamp to `CurrentHealth-1` unless data+component opt into lethal (comp:1204-1207). **Gap**: the clamp lives only in the defense-chain `ApplyActivePairedDamageOnce`; the legacy `CompletePairedAnimation` path applies **unclamped** damage for non-lethal counters (comp:4988). Bug-shaped.
- `IsValid()` is never called by runtime gameplay (editor commandlet + tests only); the component uses its own `HasValidPairedRuntimeNumerics`.

### 1.4 UDefenseConfiguration (`Data/DefenseConfiguration.h`)

**25 of 32 fields fully wired** through the resolver/selector/component pipeline — the class works. The exceptions are the most dangerous kind:

**Shadow constants (runtime-dead, editor-visible)**:
- `InteractionTombstoneSeconds` — runtime uses hardcoded `static constexpr DefenseInteractionTombstoneSeconds = 1.0` (CombatComponent.h:1148, used :870).
- `TerminalInteractionCacheCap` — runtime uses `constexpr … = 128` (CombatComponent.h:1149, used :886).
Designers can edit these knobs, the editor validation report echoes the authored values (DefenseAssetValidationService.cpp:1732-1734), and the runtime ignores them entirely. **This actively masks its own disconnect.**

**Runtime-dead (3 more)**: `NormalBlockTranslationDriftTolerance` (validation/proof-authoring only), `GuardEnterMontage` / `GuardExitMontage` (validated, dependency-gathered, **never played** by any component).

**Undocumented ranking semantics**: presentation row selection ranks `GetExactFieldCount()` (height/lane/swing exactness) → `RequiredTags.Num()` → `Priority` (DefensePresentationSelector.cpp:23-34). Authored `Priority` is the *weakest* key — it only breaks ties among equally specific rows. Zero tooltips exist to tell a designer this.

**Resolution chain (undocumented layers)**: `GetEffectiveDefenseConfiguration()` = newest `DefenseStanceOverrides` entry → `DefenseConfigurationOverride` → `CombatSettings->DefenseConfiguration` → **CDO fallback** (`GetDefault<UDefenseConfiguration>()`, CombatComponent.cpp:2819-2824). The stance-override layer and CDO fallback appear in no documentation.

### 1.5 FDefensePresentationPayload (CombatTypes.h:2655)

15 of 17 fields honored (montage/section/blends/warp/audio/VFX/hitstop overrides/bridge data/deflection marker/preflight flag). **Runtime-dead**: `SourceSocketOverride` and `TargetBoneOverride` — copied into `Resolution.Presentation` by struct assignment, validated by the editor (DefenseAssetValidationService.cpp:2593-2599), but the runtime contact pipeline takes socket/bone exclusively from `FDefenseDecision` (DefenseResolver.cpp:168-179). Authored overrides are silently dropped. Note also `MaximumTranslation` is honored only on the paired-bridge path; normal block warping uses config-level `NormalBlockTranslationAllowance` instead — two translation budgets, zero tooltips explaining which applies where.

### 1.6 UWeaponData / UCombatFXData / UAttackConfiguration / UCombatSettings

- **UWeaponData**: 22/25 wired. Dead: `Description`. Test-only: `WeaponReach` — tooltip "adjusts soft aim assist range" is **false** (targeting uses `SoftAimRange`; only caller is a test). `DamageMultiplier` applies to weapon-trace hits only (BaseCombatCharacter.cpp:1554) — **not** to finisher/counter damage, contradicting "all attacks with this weapon". `WeaponMesh` (`TSoftObjectPtr`) is loaded via **`LoadSynchronous()`** (WeaponComponent.cpp:1369).
- **UCombatFXData**: pools, blocked-pool routing, pitch variation, VFX-pool selection all wired. `SurfacePools`/`GetPoolForSurface` — scaffold confirmed: `HitInfo.SurfaceType` is computed (BaseCombatCharacter.cpp:1561) but never forwarded; `ResolvePool` is always called with 2 args.
- **UAttackConfiguration**: `SprintAttack`/`JumpAttack`/`PlungingAttack` dead (correctly documented "not yet implemented").
- **UCombatSettings**: `MotionWarpingSettings` slot **never dereferenced**. Deprecated `AttackConfiguration` migration works as designed. Header doc names a `WeaponDataOverride` field that doesn't exist (the actual override is `UWeaponComponent::WeaponData`). `UWeaponComponent::GetOwnerCombatSettings()` reaches CombatSettings via reflection `FindPropertyByName("CombatSettings")` (WeaponComponent.cpp:1435-1439) — fragile.

### 1.7 UMotionWarpingSettings — entire class DEAD 🔴

All 6 fields header-only. The class is `#include`d once and never dereferenced; every same-named field read in the codebase belongs to `FAttackWarpConfig` or `FPairedWarpConfig`. The authored asset `DA_MotionWarping_Config` and the `CombatSettings` slot are inert. `FAttackWarpConfig` (per-attack) fully superseded the class-level-defaults idea; no resolver was ever built.

### 1.8 UHitReactionData — entire class effectively DEAD 🔴

Triple-confirmed dead surface:
1. **Zero instances exist** across all 7,638 content assets.
2. The only consuming code path (`PlayReactionFromData` ← `PlaySpecialReaction`/`PlayPairedReaction`, HitReactionComponent.cpp:903-998) has **no C++ callers** (BlueprintCallable-only, unverifiable BP reachability, and with no assets there is nothing to pass anyway).
3. Per-field: `KnockbackForce`, `LaunchForce`, `SyncPointTime`, `ReactionTags`, `RequiredContextTags`, `IsInIFrameWindow()` have no reader at all; the paired-reaction block (`bIsPairedReaction`/`PairedType`/`PairedReactionName`) is editor-validation-only — real pairing lives in `UPairedAnimationData`. Even `StunDuration` is dropped by `PlayReactionFromData` (never calls `ApplyHitStun`) despite a tooltip claiming it enables finisher vulnerability.

Two dedicated editor customizations (`HitReactionDataCustomization`, `HitReactionEntryCustomization` partial) serve this dead class.

### 1.9 UHitReactionSettings + FHitReactionEntry

**Live**: `DirectionalReactions` (× the montage-variant variety system with n-2 history — confirmed working), `DeathReactions` map, `HeavyDamageHealthPercent`, `GetIntensityFromAttack`, `GetDirectionalReaction`, `GetDeathReaction`.

**Scaffold (gated on the dead UHitReactionData)**: `GuardBrokenReaction`, `KnockdownReaction`, `LaunchReaction`, `DeathReaction` (single slot), `CounterReactions`, `FinisherVictimReactions`, `GetSpecialReaction`, `GetPairedReaction`.

**Mismatches**:
- `DeathReaction` (slot) vs `DeathReactions` (map): two competing death surfaces; **only the map works** (HitReactionComponent.cpp:1110).
- `PlayGuardBrokenReaction` plays the component's legacy `GuardBrokenMontage` (HitReactionComponent.cpp:530,541), bypassing the advertised `GuardBrokenReaction` slot.
- **No knockback physics exists**: `KnockbackForce` (FHitReactionEntry + UHitReactionData) and `GlobalKnockbackMultiplier` are never read — yet `KnockbackForce=200` is serialized throughout `DA_HitReaction`, and on death entries (where it is doubly meaningless).
- `Outcome`/`RagdollBlendTime` on FHitReactionEntry honored **only for death entries**; directional entries ignore them (`PlayReactionFromEntry` never reads Outcome).
- `FHitReactionAnimSet` — legacy fallback only (SelectHitReactionMontage when settings absent); candidates for deletion after confirming no BP usage.

### 1.10 FFinisherTriggerConfig — orphan struct 🔴

**No class holds an instance.** `GetFinisherTriggerReason` hardcodes every rule the struct claims to configure: health threshold **hardcoded to 0.25** (HitReactionComponent.cpp:1518), staggered/stunned checks unconditional, `MinStunTimeRemaining` never evaluated, `bShowFinisherPrompt` unread, and the slow-mo fields shadowed by `UPairedAnimationData::SlowMotionScale/Duration` (the fields that actually drive it). Every tooltip in this struct is wrong by omission.

### 1.11 UTargetingSettings ✅

All 9 fields wired. The one nuance: `FindBestTargetForDirection` uses the SoftAim* subset + `bRequireLineOfSight` + weights; `DirectionalConeAngle`/`LineOfSightChannel` are consumed by other targeting paths (IsTargetInCone :365, HasLineOfSightTo :398). Nothing dead. Model class.

---

## Part 2 — Dead Code Inventory (non-property)

| Item | Location | Status |
|---|---|---|
| `FDirectionalWarpConfig`, `FMotionWarpingConfig` typedefs | CombatTypes.h:1161-1162 | Zero references — delete |
| `CalculateChargeLevel` | MontageUtilityLibrary.cpp:411 | Zero callers |
| `GetEffectiveTiming` | AttackData.cpp:160 | Test-only despite "Editor/query helper" doc |
| `ValidatePairedAnimation`, `CalculateVictimTransform`, `CalculateVictimTransformFromData`, `GetTerrainAdjustedVictimTransform`, `IsInTriggerRange`, `IsWithinWarpDistance`, `CalculateWarpTarget` | PairedAnimationUtilityLibrary.cpp | No runtime callers (test/internal only); live equivalents are in PairedAnimationComponent/TargetingComponent |
| `UPairedAnimationData::IsValid()` | PairedAnimationData.cpp | Editor commandlet + tests only; runtime uses `HasValidPairedRuntimeNumerics` |
| `UHitReactionData::IsInIFrameWindow` | HitReactionData.cpp:78 | Zero callers |
| `PlaySpecialReaction`, `PlayPairedReaction`, `PlayReactionFromData` | HitReactionComponent.cpp:903-998 | No C++ callers; gate the dead UHitReactionData system |
| `GetPoolForSurface` | CombatFXData.cpp:21 | Runtime-unreachable (surface never forwarded) |
| `UWeaponComponent::GetWeaponReach()` | WeaponComponent.cpp:1313 | Test-only |
| `FHitReactionAnimSet` struct + `SelectHitReactionMontage` fallback | CombatTypes.h:800, HitReactionComponent.cpp:593 | Legacy fallback only |
| Editor: `AttackDataCustomization` categories | AttackDataCustomization.cpp:73,107 | `EditCategory("Heavy Attack")` and `EditCategory("Timing")` don't match header categories `Attack Type\|Heavy Attack` / `Editor Tools\|Timing Generation` → custom rows render in **separate stray panel sections**, duplicating the raw section-name properties elsewhere |

---

## Part 3 — Content-Side Findings (58 assets)

Census: 29 AttackData, 16 PairedAnimationData, 3 CombatFXData, 2 DefenseConfiguration, 2 CombatSettings, 2 TargetingSettings, 1 each WeaponData/AttackConfiguration/HitReactionSettings/MotionWarpingSettings, **0 HitReactionData**.

1. **Wasted authoring on dead fields**: `KnockbackForce=200` serialized on every reaction entry (no knockback system); `MinTriggerDistance=0` explicitly authored on `DA_Finisher_GateA` (finisher path ignores it); `StunDuration=0.3` on death-reaction entries (meaningless for a death outcome — a symptom of every field rendering on every entry); `bCanTriggerFinisher`/`bHasCounterVariant` co-maintained with their data references.
2. **`DirectionalFollowUps` contains an `EAttackDirection::None` key** (LightAttack_1) and a `DA_DirectionalAttack_None` asset exists to serve it. If intentional (neutral-release follow-up), the tooltip should say so; if not, validation should reject None keys.
3. **Editor-only engine content referenced by shipping assets**: `Counter_LightAttack_1` references `/Engine/VREditor/Sounds/VR_ungrab_Cue`, `/Engine/EditorSounds/Notifications/CompileFailed_Cue`, `CompileSuccess_Cue`. These do not exist in packaged builds → silent audio dropout (or cook warnings). Placeholder hygiene needed before any packaging milestone.
4. **Orphan assets**: `DA_Targeting` (settings point at `DA_TargetingConfig`); `DA_MotionWarping_Config` (slot never dereferenced). Verify no BP references, then delete.
5. **Empty fallback tiers**: `DA_Weapon_Katana` authors no `HitSound`/`HitVFX`, so tier 3 of the documented 4-tier FX chain is empty (fine — pools cover it — but worth knowing when debugging silent impacts).

---

## Part 4 — "Does what it says" — Tooltip/Doc Mismatch Register

Fix-the-text (behavior is fine, words are wrong):
1. `AttackData.ImpactVFXConfig` "[SCAFFOLD FOR U-16]" → fully wired.
2. `AttackHand` / `DefaultContactBone` → actual role is defense socket/bone fallback, not IK.
3. `AttackerVoiceLine` → plays at sync point, not "animation start or configurable time".
4. `AnimationName` → no "TMap lookups" exist.
5. `WeaponData.DamageMultiplier` → weapon-trace hits only, not "all attacks".
6. CLAUDE.md "Editor/Runtime Unification Gap" → `SetupAttackWarp` honors offsets; section is stale.
7. `CombatSettings` header → references nonexistent `WeaponDataOverride`; defense resolution chain missing stance-override layer + CDO fallback.
8. `UHitReactionData.StunDuration` → claims finisher-vulnerability wiring; the data-asset path never applies stun.

Fix-the-code-or-delete (words promise a system that is absent):
9. `StaggerPower` (stagger chance), 10. `MaxChargeTime`/`ChargeTimeScale`/`MaxChargeDamageMultiplier` (charge scaling), 11. `bEnforceMaxHoldTime`/`MaxHoldTime` (hold cap), 12. `bJumpToSectionStart` (conditional jump), 13. `KnockbackForce`/`GlobalKnockbackMultiplier`/`LaunchForce` (knockback/launch physics), 14. `FFinisherTriggerConfig` (all 8 fields vs hardcoded logic), 15. `InteractionTombstoneSeconds`/`TerminalInteractionCacheCap` (shadow constants), 16. `GuardEnterMontage`/`GuardExitMontage` (never played), 17. Payload `SourceSocketOverride`/`TargetBoneOverride` (silently dropped), 18. `DeathReaction` slot + `GuardBrokenReaction` slot (bypassed by map/legacy montage).

---

## Part 5 — Cross-Cutting Design Issues

1. **Shadow constants are the worst failure mode found** — an editable knob the runtime ignores, with editor validation echoing the ignored value. Either the constexpr goes or the field goes; they cannot coexist.
2. **Parallel/duplicate systems** (each pair needs a single winner):
   - `UHitReactionData` assets vs inline `FHitReactionEntry` → inline won (content proves it).
   - `SyncPointTime`/`SyncPointName` (asset) vs sync notify → notify won.
   - `VictimRelativePosition`+`FacingMode`+`Rotation` vs `VictimWarpConfig.RelativeOffset` → warp config won.
   - `DeathReaction` slot vs `DeathReactions` map → map won.
   - `UMotionWarpingSettings` vs `FAttackWarpConfig` → per-attack config won.
   - `FFinisherTriggerConfig` vs hardcoded trigger logic → neither won; the hardcode is a data-driven-design violation.
3. **Redundant boolean+reference pairs** (`bCanTriggerFinisher`+`FinisherData`, `bHasCounterVariant`+`CounterData`) — Rule 7 says the data reference owns the payload; a null-check is the gate. The booleans add a desync class that editor validation must police.
4. **Mutability**: `UPairedAnimationData` uses `BlueprintReadWrite` on all ~44 properties, as do most embedded structs (`FHitstopConfig`, `FImpactAudioConfig`, `FHitReactionEntry`, `FDefensePresentationPayload`, `FPairedWarpConfig`…). Data assets are shared singletons — a BP write mutates every user and can persist past PIE (worse in 5.6 where PIE editing is opt-out). `UAttackData`/`UDefenseConfiguration` correctly use `BlueprintReadOnly`.
5. **Loading**: all content references are hard `TObjectPtr` except `WeaponMesh` (soft but `LoadSynchronous`). Every AttackData hard-pulls its montage + finisher/counter chains at load. At 29 attacks this is harmless; it becomes a load-time bomb as the catalog grows (Lyra pattern: soft refs + explicit async for heavy/situational payloads).
6. **Editor customization drift**: `AttackDataCustomization` targets category names that no longer match the header, splitting related controls across stray panel sections and duplicating raw properties.

---

## Part 6 — Improvement Strategy (research-backed)

Sources: Epic data-asset/asset-manager/data-validation/core-redirects docs, Lyra conventions, unreal-garden (formerly benui) UPROPERTY reference. Full citations in the research appendix at bottom.

### 6.1 Phase A — Restore truth (decide per dead cluster: WIRE or DELETE)

The guiding rule: **a parameter that exists must do what its tooltip says**; everything else is either implemented, explicitly future-labeled, or deleted. Recommended dispositions (⚡ = decision point where product intent matters):

| Cluster | Recommendation |
|---|---|
| Shadow constants (`InteractionTombstoneSeconds`, `TerminalInteractionCacheCap`) | **WIRE** — delete the constexpr twins, read the config (thorough-solution rule; the fields already exist and validation already checks them) |
| `FFinisherTriggerConfig` | **WIRE** — give it an owner (`UHitReactionSettings` is the natural home), replace the hardcoded 0.25/unconditional checks in `GetFinisherTriggerReason`. This un-hardcodes a core tunable. ⚡ or delete if hardcoded triggers are considered final |
| Charge scaling (`MaxChargeTime`, `ChargeTimeScale`, `MaxChargeDamageMultiplier`) | ⚡ **WIRE** if charged heavies should scale damage/speed (design intent suggests yes; `CalculateChargeLevel` already exists) — otherwise delete all three + the uncalled function |
| Hold cap (`bEnforceMaxHoldTime`, `MaxHoldTime`) | ⚡ WIRE (auto-release timer) or DELETE. Rule 4 says hold ≠ duration tracking, which argues **DELETE** |
| `StaggerPower` | ⚡ DELETE (stagger is contextual/duration-based by design; the field contradicts the shipped model) or park in an explicit `Future` category with an honest "[NOT IMPLEMENTED]" tooltip |
| `ComboBlendIn/OutTime` | **DELETE** — procedural blend explicitly replaced them |
| `bJumpToSectionStart` (AttackData) | **DELETE** — unconditional jump is the only sensible behavior when a section is set |
| `TimingFallbackMode` | **DELETE** (nothing, including editor tools, reads it) |
| `CounterDamageMultiplier` | **DELETE** (or move to Future with honest label) |
| `bCanTriggerFinisher` / `bHasCounterVariant` | **DELETE** — null-check on FinisherData/CounterData is the gate (runtime already agrees); simplify editor validation accordingly |
| Deprecated `PostureDamage` / `ChargedPostureDamage` | **DELETE** via 6.3 workflow |
| `UMotionWarpingSettings` (class + slot + asset) | **DELETE** — superseded by FAttackWarpConfig; no resolver was ever built |
| `UHitReactionData` (class + 6 settings slots/maps + 3 Play* functions + customizations) | ⚡ **DELETE** — zero instances, no callers, inline entries won. (Alternative: invest in wiring it as the reusable-reaction layer — only if reuse-across-settings is a real near-term need) |
| `KnockbackForce` / `GlobalKnockbackMultiplier` / `LaunchForce` / `LaunchReaction` | ⚡ WIRE (a knockback impulse on reaction is cheap and sells hits) or DELETE. Authored content (200 everywhere) suggests the intent existed |
| Paired positioning trio (`VictimRelativePosition`/`FacingMode`/`Rotation`) | **DELETE** — WarpConfig.RelativeOffset is the real control |
| `SyncPointTime` / `SyncPointName` (asset copies) | **DELETE** from asset (notify owns sync). If a validation cross-check is wanted, validate *against the notify* instead |
| Paired scaffolds (`MusicDuckingDB`, post-process, blood) | KEEP but move to a collapsed `Future (Not Wired)` category with `AdvancedDisplay` + honest tooltips — they are planned polish per CLAUDE.md |
| `SurfacePools` | KEEP (already honestly labeled scaffold) — or wire by forwarding `HitInfo.SurfaceType` (small change, `bReturnPhysicalMaterial=true` + one parameter) |
| `GuardEnterMontage`/`GuardExitMontage` | ⚡ WIRE (guard enter/exit anims are a real feature gap) or DELETE |
| Payload `SourceSocketOverride`/`TargetBoneOverride` | ⚡ WIRE into the contact pipeline (respect payload override before Decision fallback) or DELETE from payload |
| `WeaponReach` | ⚡ WIRE into targeting range (tooltip already promises it; would make long weapons matter) or DELETE |
| `Description` (WeaponData + PairedAnimationData) | KEEP — designer-facing notes are legitimately read by humans, not code; exempt from dead-field rules |
| Movement attacks (Sprint/Jump/Plunging) | KEEP — honestly documented future slots |
| Legacy typedefs, `CalculateChargeLevel` (if charge deleted), dead utility functions, `IsInIFrameWindow`, `FHitReactionAnimSet` (after BP-reference check) | **DELETE** |

Also fix two behavioral gaps found in passing (bug-shaped, not data-asset design):
- Legacy `CompletePairedAnimation` missing the counter one-health clamp (PairedAnimationComponent.cpp:4988).
- `HitStunDuration` bypass: define the intended precedence (per-attack vs per-reaction stun) and implement it once — e.g. `max(AttackData.HitStunDuration, ReactionEntry.StunDuration)` or explicit override semantics — then document it in both tooltips.

### 6.2 Phase B — Metadata & designer-UX pass

1. **Tooltips**: bring the defense family to 100% (`UDefenseConfiguration` all fields, both row structs, `FDefensePresentationPayload`, `FDefenseAttackProfile`) and fix the 8 wrong tooltips in Part 4. Every tooltip states: what it does, which system reads it, and units/interactions ("Priority breaks ties only among equally specific rows; specificity and tag count rank first").
2. **Units metadata**: `meta=(Units="s")` on every seconds field (ComboInputWindow, all blend/window/duration fields, defense timers), `Units="deg"` on angles (tolerances, cone half-angles), `Units="cm"` on distances (warp/trigger/reach). Free correctness + display win; supports "100ms" entry.
3. **`TitleProperty`** on `DefenderPresentationRows`/`AttackerResponseRows` (`meta=(TitleProperty="RowName")` or a format string with Outcome) and on `BoneHeightRows`/`ReactionMontages` — collapsed array entries become readable.
4. **`InlineEditConditionToggle`** for every boolean+payload override pair (`bOverrideBlockedImpactAudio`+`BlockedImpactAudio`, payload override trios, `bApplySlowMotion`+scale/duration) — halves visual row count.
5. **Category hygiene on UAttackData**: after Phase A deletions the layout collapses naturally. Target ≤2 levels: Basic / Damage & Impact / Combos / Heavy Charge / Light Hold / Motion Warp / Defense / Paired / Tags / Editor Tools. Use `AdvancedDisplay` for expert knobs (substep tuning, pitch variation). Fix or delete the stale `AttackDataCustomization` category names (`"Heavy Attack"` → `"Attack Type|Heavy Attack"`, `"Timing"` → actual), and drop custom rows that duplicate raw properties.
6. **Death-entry noise**: the DA_HitReaction sample shows irrelevant fields authored on death entries. If FHitReactionEntry stays the universal reaction struct, add `EditCondition`-based hiding for outcome-dependent fields, or split a slim death-entry struct. (An `IPropertyTypeCustomization` on FHitReactionEntry is justified here — it appears in many places, so one customization pays off everywhere.)

### 6.3 Phase C — Safety & mutability

1. **`BlueprintReadWrite` → `BlueprintReadOnly`** on all `UPairedAnimationData` properties and all embedded authored structs (FHitstopConfig, FImpactAudioConfig, FImpactVFXConfig, FImpactFXPool + entries, FPairedWarpConfig, FPairedChainTransitionPolicy, FHitReactionEntry family, FDefensePresentationPayload). Shared assets must not be writable from BP; 5.6's opt-out PIE editing raises the stakes. Lyra convention: `EditDefaultsOnly, BlueprintReadOnly` + const access.
2. **Deletion mechanics** (for every Phase A deletion): rename to `Foo_DEPRECATED` + `meta=(DeprecatedProperty, DeprecationMessage=…)`, add a `PropertyRedirects` entry in DefaultEngine.ini, migrate any needed value in `PostLoad()` under a custom version guard, resave the 58 assets (commandlet), then physically delete field + redirect next milestone. The existing `PostureDamage` pair should go through this now as the pilot.
3. Replace the reflection-based `FindPropertyByName("CombatSettings")` lookup in WeaponComponent with a typed interface/accessor.

### 6.4 Phase D — Validation & CI

1. Extend `IsDataValid(FDataValidationContext&)` (the only overload guaranteed to run in 5.6):
   - AttackData: reject `EAttackDirection::None` keys in follow-up maps (or bless them with an explicit tooltip); require montage section exists when `MontageSection` set (exists today — keep); after Phase A, validation for the deleted boolean pair disappears.
   - PairedAnimationData: validate the sync **notify** exists on AttackerMontage (replacing the dead SyncPointTime check with one that checks the mechanism that actually runs).
   - All classes: **no `/Engine/VREditor/` or `/Engine/EditorSounds/` references** — editor-only content in gameplay assets is a packaging failure waiting to happen.
2. Add a **consumed-fields manifest check** (project pattern, extends Rule 7): a validator that warns when an asset authors a non-default value into a field on the project's dead/scaffold list — prevents new wasted authoring while Phase A is in flight.
3. Wire `UnrealEditor-Cmd.exe <proj> -run=DataValidation` into `Tools/Codex/run-agent-baseline.ps1` so asset validation runs with every baseline.

### 6.5 Phase E — Optimization (right-sized for this project)

1. Keep hard refs for the active moveset (correct for a single-player game at this scale — per Epic guidance chains only become a problem as catalogs grow).
2. Convert **situational heavy payloads** to `TSoftObjectPtr` + async preload when the catalog grows past prototype: finisher/counter `PairedAnimationData` montages and per-attack Niagara/audio are natural bundle candidates (`meta=(AssetBundles=…)`).
3. Replace `WeaponMesh.LoadSynchronous()` with `FStreamableManager` async + attach-on-loaded (it is already a soft ref — the sync load discards the benefit).
4. Periodic Reference Viewer / Size Map pass on `DA_CombatSettings_Default` to catch accidental hard-ref chains.
5. Registry ergonomics: consider `AssetRegistrySearchable` on `AttackType` (and a future attack-catalog tag) to enable no-load catalog queries; use Bulk Edit via Property Matrix for cross-asset balancing sessions (the practical answer to tuning 29 AttackData assets at once).

### 6.6 Documentation sync

Update CLAUDE.md: remove/replace the stale "Editor/Runtime Unification Gap" items (warp offsets now honored), correct the scaffold table (ImpactVFX wired; UHitReactionData status), document the real defense-config resolution chain (stance overrides + CDO fallback), and the presentation-row ranking order. Update `docs/architecture/API_REFERENCE.md` and `ATTACK_CREATION.md` for whatever Phase A decides.

---

## Suggested execution order

1. **Quick wins, zero risk** (hours): stale comments/tooltips (Part 4 items 1–8), CLAUDE.md sync, delete dead typedefs + uncalled functions, placeholder-audio validation rule.
2. **Phase A decisions** (the ⚡ rows need a product call, everything else is mechanical), executed with the 6.3 deprecation workflow. Biggest single item: UHitReactionData delete-vs-wire.
3. **Shadow-constant wiring + the two bug-shaped gaps** (counter clamp, stun precedence) — with regression tests.
4. **Metadata/UX pass** (6.2) — one sweep per class, editor-only risk.
5. **Validation & CI** (6.4), then **optimization** (6.5) as the catalog grows.

---

## Execution log

**2026-07-21 — Zero-risk quick wins executed** (Phase A/B decisions still pending user input on ⚡ items):
- Deleted dead typedefs `FDirectionalWarpConfig`/`FMotionWarpingConfig` (CombatTypes.h) — verified zero references.
- Honest-labeled every dead/inert/editor-only field found by the audit with `[NOT WIRED]` / `[SCAFFOLD — NOT WIRED]` / `[EDITOR …]` tooltips across AttackData.h, PairedAnimationData.h, CombatTypes.h (payload socket/bone overrides, FHitReactionEntry KnockbackForce/Outcome), DefenseConfiguration.h (shadow constants, guard montages, drift tolerance), CombatSettings.h (MotionWarpingSettings slot), WeaponData.h (WeaponReach, DamageMultiplier scope), HitReactionData.h (class-level audit note, StunDuration), HitReactionSettings.h (special slots, paired maps, GlobalKnockbackMultiplier), PairedAnimationTypes.h (FFinisherTriggerConfig orphan note).
- Fixed wrong tooltips: ImpactVFXConfig stale scaffold note, AttackHand/DefaultContactBone actual role, AttackerVoiceLine sync-point timing, AnimationName lookup claim, HitStunDuration bypass caveat, blend/trigger-distance path scoping.
- CLAUDE.md: marked warp schema/runtime parity gaps RESOLVED (only PerformWeaponTrace item remains); added this audit to the docs table; corrected CombatSettings override-pattern docs (WeaponData naming, defense stance-override + CDO fallback layers).
- Validation: `UPairedAnimationData::IsDataValid` now warns on `/Engine/VREditor` / `/Engine/EditorSounds` sound references (packaging risk found in Counter_LightAttack assets) and the AnimationName warning no longer claims TMap lookups.
- Deliberately NOT touched: any property deletion (requires deprecation workflow), anything on a ⚡ decision, `CalculateChargeLevel` (tied to charge decision), test-referenced utility functions.

**2026-09-29 — Step 2 deletions (user-approved decisions)**:
- Removed `bEnforceMaxHoldTime`/`MaxHoldTime` and `StaggerPower` (commit 2ffa1cb4). A binary scan of `Content` found no saved asset serializing them, so plain deletion was safe.
- Removed AC3 counter mode and the unreachable legacy counter entry path (commit 44abc8eb).
- Removed `UHitReactionData`, its six `UHitReactionSettings` slots, `GetSpecialReaction`/`GetPairedReaction`, `UHitReactionComponent::PlayReactionFromData`/`PlayPairedReaction`/`PlaySpecialReaction`, `FHitReactionDataCustomization`, and the now-unused `ESpecialReactionType`/`EHitReactionType` enums. Zero instances, zero callers, zero asset references.

**Restoring `UHitReactionData`** (kept deliberately easy): the last commit that contains it is `44abc8eb`.

```bash
git checkout 44abc8eb --   Source/KatanaCombat/Public/Data/HitReactionData.h   Source/KatanaCombat/Private/Data/HitReactionData.cpp   Source/KatanaCombatEditor/Public/Customizations/HitReactionDataCustomization.h   Source/KatanaCombatEditor/Private/Customizations/HitReactionDataCustomization.cpp
```

Then re-add the enums, the settings slots and getters, and the editor registration from the same commit's `CombatTypes.h`, `HitReactionSettings.h/.cpp`, `HitReactionComponent.h/.cpp` and `KatanaCombatEditor.cpp`.

**Preferred future design** if per-reaction reuse is ever needed: do not restore the old class as it was. It had drifted from `FHitReactionEntry` (no montage-variant pool, no death `Outcome`). Instead, add a thin data asset that *wraps* an `FHitReactionEntry` (one definition, so the two cannot drift), and let inline entries optionally reference it. Whole reaction sets are already swappable per character via `UHitReactionComponent::HitReactionSettingsOverride` or `CombatSettings->HitReactionSettings`.

**2026-09-30**: knockback wired (FKnockbackConfig/FKnockbackOverride, KnockbackScale); FHitReactionEntry::KnockbackForce removed.

## Research appendix (key citations)

- UPROPERTY specifier/meta reference (tooltips, EditCondition, Units, TitleProperty, DisplayPriority, AdvancedDisplay, Categories, AssetRegistrySearchable): unreal-garden.com/docs/uproperty/ (formerly benui.ca)
- Data assets & Asset Manager (UDataAsset vs UPrimaryDataAsset, PrimaryAssetId, AssetBundles, scan settings): dev.epicgames.com/documentation/en-us/unreal-engine/data-assets-in-unreal-engine, …/asset-management-in-unreal-engine
- DataAssets vs DataTables tradeoffs: dev.epicgames.com/community/learning/tutorials/Z13; data-driven design overview: unreal-garden.com/tutorials/data-driven-design/
- Hard vs soft references, async loading: dev.epicgames.com/documentation/en-us/unreal-engine/referencing-assets-in-unreal-engine, …/asynchronous-asset-loading-in-unreal-engine
- Data validation (IsDataValid FDataValidationContext form, UEditorValidatorBase, `-run=DataValidation`): dev.epicgames.com/documentation/en-us/unreal-engine/data-validation-in-unreal-engine
- Core redirects / property deprecation workflow: dev.epicgames.com/documentation/en-us/unreal-engine/core-redirects-in-unreal-engine; ikrima.dev/ue4guide/…/deprecating-uproperties-ufunctions/
- Lyra composition/mutability conventions: dev.epicgames.com/documentation/unreal-engine/lyra-sample-game-in-unreal-engine; x157.github.io/UE5/LyraStarterGame/Experience/
- Details customization cost/benefit: dev.epicgames.com/documentation/unreal-engine/details-panel-customizations-in-unreal-engine
- PIE mutation risk of writable data assets: forums.unrealengine.com/t/modify-data-asset-during-pie-and-discard-changes/229469
