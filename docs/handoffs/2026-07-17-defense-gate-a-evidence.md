# Defense Gate A Evidence

## Accepted Scope

Gate A proves one reviewed attack path in `/Game/ProjectFiles/Levels/Lvl_ThirdPerson1`: `LightAttack_1`, montage `AM_Light_Combo_1`, section `Attack_1`, with the reviewed parry window at section-relative `0.20-0.30` seconds. It covers held guard, Middle/Center normal block, perfect parry, marker-driven counter-to-finisher continuity, bounded alignment, one-attacker token policy, friendly-fire rejection, and same-enemy reattack without range reset.

It does not prove the High/Middle/Low x Left/Center/Right matrix, two-active-threat switching, all attacker-response semantics, broad montage-rate parity, Branching Point/time-stretch behavior, or production animation quality across the attack catalog. Those remain Gate B obligations.

## Content And Reports

The schema-v2 manifest is `Tools/Codex/manifests/defense-gate-a.json`; it owns the exact 12-case runtime proof ledger. Its only target ledger is `Config/AssetMigrations/DefenseGateATargets.txt`. The slice adds nine defense montages and four paired/configuration data assets under `Content/ProjectFiles/*/Defense/GateA/`, and intentionally updates:

- `ABP_SamuraiCharacter`
- `AM_Light_Combo_1`
- `LightAttack_1`
- `DA_CombatSettings_Default`

Version-3 deterministic authoring created and saved the accepted assets with fingerprint `A396E02F552CC63FB550F9DB970C4C458436941E`; its historical save report remains `Saved/Logs/Commandlets/KatanaAssetMigration/defense-gate-a-authoring-v3-save.json`. Version 4 hardens future approvals by binding recipe, direct dependency, destination, canonical-manifest, planned-addition, and package-ledger state. Its final clean read-only Plan is `Saved/Logs/Commandlets/KatanaAssetMigration/defense-gate-a-authoring-plan-final2.json`, fingerprint `22E58B55F3569BF22009E33E81CBB88DECBCBB90`; it reports one unchanged operation, zero proposed writes, and an empty package ledger. Final migration Audit is `Saved/Logs/Commandlets/KatanaAssetMigration/defense-gate-a-final-audit2.json` (27 unchanged, 0 would-change, 0 failed, 0 saved).

## Runtime Proof

Rendered automation `KatanaCombat.Defense.GateA.PIEProof` passed with physical weapon contact and 49/49 screenshots. Evidence paths:

- `Saved/DefenseProof/GateA/Rendered/defense-gate-a-evidence.json`
- `Saved/DefenseProof/GateA/Rendered/defense-gate-a-telemetry.csv`
- `Saved/DefenseProof/GateA/Rendered/defense-gate-a-proof.mp4`
- `Saved/DefenseProof/GateA/Rendered/frames/`

NullRHI runs write separately under `Saved/DefenseProof/GateA/Headless/`; full automation therefore cannot overwrite rendered acceptance evidence.

All 12 manifest-declared cases passed. Duplicate contact budget remained `1 -> 1`; normal-block drift and unexpected displacement were 0 cm; maximum yaw over budget was `0.0000093` degrees; maximum pelvis delta was `6.79` cm; both deterministic bridge actors stayed within the 75 cm cap. The proof records 62 alignment frames and 122 telemetry rows, including one counter-stage damage event, one finisher-stage damage event, and one terminal cleanup event. All 49 requested frames decoded as nontrivial pixels and passed subject-framing checks. Representative parry, counter, finisher, and cleanup frames were visually inspected; the regenerated 5 fps proof video contains exactly 49 frames.

## Automated Verification

- `KatanaCombat.Editor.DefenseValidation`: 35 passed.
- `KatanaCombat.Editor.AssetMigration`: 41 passed.
- `KatanaCombat.Defense`: 127 passed together, including Gate A followed by telemetry isolation tests.
- `KatanaCombat.EnemyAI`: 13 passed, including attack interruption releasing its owned warp.
- `KatanaCombat.Editor.DefenseMigration`: 16 passed.
- `KatanaCombat.MontageUtility.Checkpoints.ExplicitParryWindow`: passed; protects object-safe explicit-window discovery after Gate A exposed an invalid notify-object/class reinterpret cast.
- Standard baseline: editor build with `-NoUBA -NoUBTMakefiles -MaxParallelActions=1`; 609 tests discovered and completed, 0 failures/errors, explicit success marker present. Evidence prefix: `Saved/Logs/Codex-Agent-Baseline-20260717-164642`.

The final adversarial pass closed four acceptance holes with regression coverage: unsaved destination packages without an on-disk file now invalidate authoring approval; parry begin computes the exact remaining notify interval for the active montage instance; perfect-parry bridge startup revalidates weak ownership after synchronous callbacks; and interrupted AI attacks release their owned warp before montage termination. Remaining warnings are exercised rejection-path diagnostics, expected test-fixture warnings, NullRHI VFX spawn warnings, or installed Marketplace plugin/toolchain warnings; none produced an automation error.

## Next Gate

Start Gate B with read-only attack/content inventory. Do not replicate Gate A assets or timing broadly until all nine directional/height cells and semantic response cases have reviewed source assets and a closed manifest.
