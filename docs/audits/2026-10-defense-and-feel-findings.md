# Defense and feel findings, October 2026

Findings from the week of 29 September to 4 October 2026 that still matter for the defense redesign and for combat feel.

- **What was re-checked.** Every finding was re-checked for this document against `main` at `a7693bb4`, including the
  shipped assets in that commit. Line references point at that commit. A few asset values were read by parsing the
  `.uasset` property data; those are marked.
- **Two kinds of claim.** Each finding separates what the source shows from what was only derived or reported. A figure
  that was derived by reading code, and never measured or play-tested, says so.
- **Where to look next.** The decisions that respond to these findings are in the [decision log](../reference/DECISIONS.md)
  and the [design pillars](../reference/DESIGN_PILLARS.md).

## 1. Blocking has two rules on `main`

**Weapon contacts follow the defense resolver only.**
- A weapon hit on a combat character is resolved by `ResolveWeaponContactCandidate` and returns before `OnWeaponHit` is
  broadcast (`WeaponComponent.cpp:789-838`, broadcast at `:865`).
- The resolver blocks when the contact yaw is within `NormalBlockFinalTolerance` (`DefenseResolver.cpp:354-364`).
- That tolerance defaults to 35° (`CombatTypes.h:2710`, `DefenseConfiguration.h:211`). Neither shipped defense
  configuration asset overrides it: the property name is absent from both packages.

**An older 70° cone still exists.**
- `BlockFacingConeHalfAngle = 70.0f` (`CombatComponent.h:1140`) is checked by `CanBlockAttackFrom` and `CanBlockHit`
  (`CombatComponent.cpp:3368-3410`). Both are `BlueprintPure` (`CombatComponent.h:369-375`).
- It still decides damage on two paths:
  - `ApplyDamage_Implementation` returns 0 damage when `CanBlockHit` is true (`BaseCombatCharacter.cpp:1273-1285`). The
    paired counter and finisher damage reaches this path through `Execute_ApplyDamage`
    (`PairedAnimationComponent.cpp:1244`, `:1311`), and so would any Blueprint caller.
  - `OnWeaponHitTarget` uses it for `bWasBlocked` (`BaseCombatCharacter.cpp:1607-1612`). That handler only runs for
    targets that are not combat characters, because combat characters never get the `OnWeaponHit` broadcast.
- Between 35° and 70° the two rules disagree: the Blueprint queries say "blocked" for a hit the resolver lets through.

**Status.** PR #136, which is open, removes the 70° cone. After it merges, the resolver's tolerance is the only block
angle. That PR's new test shows weapon contacts already followed the resolver before the change. Its author mutated the
old cone into the damage path to check that the test fails.

## 2. A block and a perfect parry feel the same at contact

Both outcomes go through `UHitReactionComponent::PlayDefensePresentation` (`HitReactionComponent.cpp:285`). It passes the
"blocked" flag as a literal `true` to the impact sound, the impact VFX and the hitstop (`:342-351`, `:359-369`,
`:374-383`).

- **Hitstop.** Unless a presentation row overrides it, both outcomes use the attacking attack's own `HitstopConfig`: block
  at `BaseCombatCharacter.cpp:155-161`, parry at `CombatComponent.cpp:184-190`. With the blocked flag set, both are scaled
  by `BlockedDurationMultiplier` (default 0.5, `CombatTypes.h:1359`; applied at `CinematicEffectsUtilityLibrary.cpp:285-292`).
  - In the owner's playtest log of 3 October, all 7 blocks logged `[HITSTOP] Applied: 0.025s ... (blocked: YES)`. That
    playtest ran on the owner's working copy, not on `main`.
  - No parry happened in that playtest. That a parry gets the same 0.025 s is a reading of the code, not an observation.
- **Sound and VFX.** The block falls back to the configuration's `DefaultBlockImpact*` fields, and the parry to its
  `DefaultParryImpact*` fields (`BaseCombatCharacter.cpp:121`, `:151`; `CombatComponent.cpp:174`, `:181`). The shipped
  defense configuration's import table holds one sound cue (`Impact_Sword_Parry_01_Cue`) and one Niagara system
  (`NS_GFXI_Yellow_Parry`), so the block and parry defaults point at the same two assets.

## 3. Only one attack can be parried

- Across every `.uasset` in `Content/` on `main`, only `AM_Light_Combo_1` references `AnimNotifyState_ParryWindow`.
- **Asset read:** the montage holds one parry-window notify. It starts at 0.20 s, lasts 0.10 s, and sits in the first
  section (0 to 1.367 s).
- A parry also needs the attack to carry the `Attack.Defense.Parryable` tag (`DefenseResolver.cpp:239`).
- **The defender must be idle.** A perfect-parry press is refused while the defender has a current attack or attack phase
  (`CombatComponent.cpp:3024-3027`), and it needs a fresh Block press (`CombatComponent.cpp:1456-1458`).
- **Not re-verified:** the defense trace read the window's end at 0.30 s as the moment the swing goes Active, which would
  leave the press 100 ms before the blade can connect.
- **There is no code default.** `CLAUDE.md` listed a 0.3 s `ParryWindow` default, but no such value exists in source. The
  window lasts as long as the authored notify. This document's change corrects that row.

## 4. The counter chain on `main`

- **The counter comes from the defender's own attack.**
  - A Light or Heavy press during the counter window calls `GetAttackForInput(InputType)` on the defender
    (`CombatComponent.cpp:1470-1477`).
  - The counter's paired data is that attack's `CounterData` (`PairedAnimationComponent.cpp:4301`). The parried attack
    doesn't supply it.
- **The "notify fallback" never reads the notify.**
  - With `bAllowNotifyCounterDataFallback` set (default `false`, `PairedAnimationComponent.h:641`), the fallback uses
    `ActiveChainContext.SpecificCounterData` (`PairedAnimationComponent.cpp:4302-4305`).
  - That value comes from the parried attack's own `UAttackData::CounterData` (`:1815`).
  - The `CounterData` set on `AnimNotifyState_CounterWindow` is stored in `CounterWindowData.SpecificCounterData` (`:4190`,
    from `AnimNotifyState_CounterWindow.cpp:46`). The chain never reads it.
  - `CLAUDE.md` describes the notify's data as the fallback source, and the notify's header comment
    (`AnimNotifyState_CounterWindow.h:16-20`) implies it.
- **A Heavy press can't start a counter.**
  - In the shipped attack data, `LightAttack_1` to `LightAttack_11` have `CounterData`, and no Heavy attack does.
  - A Heavy press therefore finds no paired counter data and fails (`PairedAnimationComponent.cpp:4307-4312`).
- **The window can't outlive the parry animation.**
  - When the parry animation's montage ends uninterrupted in any state other than counter or finisher, the whole sequence
    is cleaned up (`PairedAnimationComponent.cpp:2497-2612`, reason `BridgeEndedBeforeCounter`).
  - **Derived, not measured:** a code reading on 3 October estimated that the shipped parry animation ends about 50 ms
    after the counter window opens. That figure was derived from engine source, never logged and never play-tested.
  - PR #137, which is open, keeps the window open after the parry animation and frees the player.

## 5. Paired-animation roles are named from the counter's point of view

- Every stage of the defense chain goes through `TryStartDefenseChainStage`: the parry, the counter and the finisher
  (`PairedAnimationComponent.cpp:3891`, `:4322`, `:4351`, `:2420`).
- That function plays the asset's `AttackerMontage` on the defender (`:3605-3606`) and its `VictimMontage` on the enemy that
  was parried (`:3652-3653`). Stage markers are matched the same way (`:506-516`).
- **For the counter and the finisher** the names fit, because the defender strikes.
- **For the parry animation they are inverted.** The "Attacker" montage is the defender parrying, and the "Victim" montage
  is the enemy who attacked. Anyone authoring parry data has to know this.

## 6. Fields that do nothing at runtime

| Field | Where | What happens |
|---|---|---|
| `FFinisherTriggerConfig` (all fields) | `PairedAnimationTypes.h:129` | No member of this type exists anywhere. The low-health finisher threshold is the literal `0.25f` (`HitReactionComponent.cpp:1423`). |
| `GuardEnterMontage`, `GuardExitMontage` | `DefenseConfiguration.h:328`, `:333` | No runtime read. Only editor authoring and validation tools touch them. |
| `SourceSocketOverride`, `TargetBoneOverride` | `CombatTypes.h:2902`, `:2907` | Only an emptiness check reads them (`CombatTypes.h:2927-2928`). |
| A `Continue` attacker response | `HitReactionComponent.cpp:397-401` | Returns without playing anything, so a montage set on that row never plays. |
| `MusicDuckingDB`, `SlowMoPostProcessMaterial`, `ScreenBloodMaterial`, `bSpawnBloodDecals` | `PairedAnimationData.h:270`, `:288`, `:303`, `:310` | Declared only, as `CLAUDE.md` already notes. |

## 7. Hit reactions: every hit was Heavy, and nothing stuns

**The intensity rule.**
- For Light attacks, `GetIntensityFromAttack` returns Heavy when damage divided by the victim's current health is at
  least `HeavyDamageHealthPercent` (`HitReactionSettings.cpp:65-87`).
- The threshold defaults to 0.25 (`HitReactionSettings.h:83`), and the shipped `DA_HitReaction` doesn't override it.
- The ratio only grows as health falls. For example, 25 damage against 100 health is already at the threshold.

**The playtest.** The owner's playtest log of 2 October (`KatanaCombat-backup-2026.10.02-19.27.47.log`) comes from their
working copy, before they raised the threshold to 1.0 as a workaround. In it:
- 49 hit-reaction montages ended, each logged once on blend-out and once on end;
- all 49 were `AM_HitReactions_Heavy_*`, and there was no Light reaction.

**Stun comes from two places.**
- When a settings entry plays, its `StunDuration` is used (`HitReactionComponent.cpp:503-506`). The attack's
  `HitStunDuration`, copied into the hit at `WeaponComponent.cpp:928`, is used only on the legacy fallback path
  (`HitReactionComponent.cpp:533-536`).
- **Asset read.** All eight directional hit-reaction entries in the shipped `DA_HitReaction` have `StunDuration` 0, so no
  hit stuns through that path. Three death entries carry 0.3 s, which is not hit stun.

## 8. Systems that are missing or can't run

- **Evade is a stub.** The input is bound (`PlayerCharacter.cpp:139`) and reaches `ExecuteAction`, where the `Evade` case
  is an empty `// Handle evade` (`CombatComponent.cpp:4147-4149`).
- **The counter indicator can never display.**
  - The component starts with ticking disabled (`CounterIndicatorComponent.cpp:10`).
  - It only checks the counter window inside its tick (`:40-45`, `:79-96`).
  - Ticking is only enabled by `ShowIndicator` (`:47-55`). Its only caller in source is that same tick-driven check
    (`:91`), so the check never starts.
  - No asset in `Content/` uses the component.
- **No defending out of your own attack.** See finding 3: a perfect parry is refused while the defender is attacking.
- **Block facing depends on authored warp windows.** A block turns the defender through a motion-warping alignment request
  (`HitReactionComponent.cpp:794-816`, executor `MotionWarping`). Motion warping only moves a character inside a warp
  window authored into the animation, so a block clip without one doesn't turn.

## 9. Animation content and gaps

**Verified from file names and asset references on `main`:**
- **One skeleton.** `SKM_CyberpunkRunnerr_B`, `SKM_Manny_Simple` and the GhostSamurai clips all reference
  `/Game/Assets/Characters/Mannequins/Meshes/SK_Mannequin`. Clips are interchangeable between the player and human-sized
  enemies.
- **Eight owned two-character counter animations**, all in the GhostSamurai pack:
  - six `*_CounterExecution`/`*_CounterExecuted` pairs: four slashes (`LAttack`/`RAttack`) and two thrusts (`LSting`/`RSting`);
  - two `DefenseL`/`DefenseR` `Parry_Up` pairs.
- **The GhostSamurai deflect and "Fail" clips:** `{L,R}{Attack,Sting}_Deflect{L,R}{90,180}` and `*_Fail*`.

**From an offline check of those clips, which extracted root motion from bone data and read the pack's preview map.
Re-read here from the check's output, not re-run:**
- Every exchange starts with the attacker in front.
- **The `Deflect…90/180` clips are the defender.** It deflects first, then sidesteps and turns 90° or 180° (root turn
  ±90°/±180°). They are not deflects of attacks from the side or behind.
- **The `Fail` clips are the attacker.** Its lunge is deflected and it stays standing. They partly fill the missing "my
  attack was parried" reaction, but only for those lunges.
- Root motion is switched off on these clips, while the root travels about 1.6 to 4.9 m.

**Approximate, from an offline asset scan, not re-counted here:**
- about 1,500 motions on the shared skeleton, about 1,000 of them holding a katana, and only about 9% wired into
  montages, blend spaces or data;
- plenty of single-character defense content: guard sets, side block impacts, about 23 parries, and several full
  8-direction evade sets.

**Scarce content:**
- dedicated attacker reactions to being parried or blocked;
- counter pairs against attacks from the side or behind, of which none are owned;
- two-character pairs for non-human-sized enemies.

**A throwaway rendered prototype with owned clips** measured side and rear parry-counters at 60 fps, with the clash at
0.633 s after the attack starts:

| Case | Defender response | Commit to clash | Measured |
|---|---|---|---|
| Front | Front counter pair directly | 0.250 s | Baseline |
| Left | Owned guarded 90° turn (0.571 s), then the pair | 0.685 s | Peak capsule yaw rate 245°/s |
| Right | The same turn's right-hand clip | 0.685 s | Peak capsule yaw rate 1,603°/s, overshooting 61° |
| Right, alternative | A second owned 90° block turn (0.833 s), then the pair | 0.947 s | Peak capsule yaw rate 169°/s, the lowest of the turns |
| Left, 1.6 times speed | The owned 90° turn played in 0.357 s | 0.483 s | Peak capsule yaw rate 453°/s |
| Left, code spin | 90° code spin over 0.20 s | 0.366 s | 669°/s; 170 cm of planted-foot slide |
| Behind, code spin | 180° code spin over 0.20 s | 0.367 s | 1,338°/s; 333 cm of planted-foot slide |
| Behind, owned turn | Owned 180° turn (0.867 s), then the pair | 0.980 s | The defender commits 0.347 s before the attack starts |
| 60° off-axis | The 90° turn warped down to 60° | 0.685 s | The pair lands with no error at the clash |

The prototype's own reading of the renders:
- the side turns look acceptable but would need about three times the speed;
- the 90° code spin is a usable fallback;
- the rear code spin reads as a turntable with skating feet;
- the owned 180° turn looks right but starts too early.

## 10. Tests that pin configurable values

The owner's rule is that tests never pin values meant to be configurable.

- **The audit.** A code-reading audit of 2 October covered 808 of the then 864 tests; it skipped the knockback and
  displacement files. It counted:
  - **168 violations** in total:
    - 38 assert a literal equal to a tunable default or a shipped asset field;
    - 81 depend silently on a default or shipped value;
    - 49 use shipped content as a proof or a fixture;
  - **60 of the 168 break when a designer edits a shipped asset.** One of those enforces its pins only in rendered runs.
- **These counts were not re-counted here.** The suite has grown since.
- **Spot checks on `main` match the audit's examples:**
  - `KatanaCombat.EnemyAI.ProofAssetsLoadAndMapReady` asserts the shipped Block input mappings: Thumb Mouse Button,
    Gamepad Left Shoulder, and not Right Mouse Button (`EnemyCombatAITests.cpp:3111` onward).
  - `KatanaCombat.EnemyAI.QueuedTokenTimeoutRemovesRequest` runs against the shipped `ST_EnemyCombatProof` StateTree
    (`EnemyCombatAITests.cpp:591-600`).

## Corrections to earlier records

- **Hit-reaction stun values.** Earlier notes said the shipped reaction entries are all 0. The eight directional
  hit-reaction entries are 0, but three death entries carry 0.3 s.
- **The parry window default.** A 0.3 s `ParryWindow` default appeared in `CLAUDE.md` and in some later analysis. No such
  default exists. The only shipped window is 0.10 s.
- **The deflect clips.** An earlier inventory read the GhostSamurai `Deflect…90/180` clips as possible side and rear
  deflects. The clip check found they are frontal deflects followed by a turn.
