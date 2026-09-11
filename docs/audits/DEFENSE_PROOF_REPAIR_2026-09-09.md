# Defense Proof Repair - 2026-09-09

## Intent and scope

The user directed this follow-up after the fresh [health triage](COMBAT_HEALTH_RESUME_2026-09-09.md): preserve current finisher authoring, reconcile its proof definitions and role mapping, and rerun the failing tests. The intended repair keeps current finisher montages and the edited counter asset. Large runtime refactors and unrelated content migration remain outside this slice.

Evidence: `Saved/Logs/DefenseProofRepair-20260909/`. The preceding full baseline completed 739 tests with 732 passed and seven failed. That result is pre-repair evidence.

## Loaded asset findings

Read-only native Unreal Python inspection loaded the actual assets. The first inspection used a protected montage property and failed; the second used `AnimationLibrary.get_animation_notify_events` and completed successfully. The process still returned 1 because the default DDC graph logged an error before using the memory-cache fallback. `asset-inspection.json` and `asset-inspection-2.log` retain both asset facts and execution limits.

| Property | Pre-repair selected `Counter_LightAttack_1` | Existing `DA_Counter_GateA` |
|---|---|---|
| Attacker montage | `AM_Counter_Attacker` | `AM_Counter_Defender` |
| Victim montage | `AM_Counter_Defender` | `AM_Counter_Attacker` |
| Sections | Both unset | Both `Counter` |
| Driver / marker | Attacker / unset | Attacker / `FinisherReady` |
| Auto-continue | False | True |
| Terminal compatibility | Both false | Both true |
| Base damage | 100 | 25 |

The montage names retain the roles of the original incoming attack. In a counter, the original defender becomes the paired attacker. `AM_Counter_Defender` contains the counter attack and its handoff marker. The selected generic asset reverses those participants; simply changing the expected driver role would retain the reversed presentation and incomplete policy.

At preflight, `LightAttack_1.CounterData` selected the generic asset, while `FinisherData` already selected `DA_Finisher_GateA`. The repair points its counter reference back to the existing reviewed Gate A counter. The generic counter asset and all its edited properties remain preserved.

The current `DA_Finisher_GateA` uses `AM_Finisher_Attacker` and `AM_Finisher_Victim`, both section `Finisher`. Its attacker warp offset is zero with translation disabled; its victim offset is 50 cm with translation enabled. Both retain rotation warping. The montages use the authored GhostSamurai Ambush/Ambushed animations. The historical Defender montage is deleted WIP and must stay absent.

## Concrete change plan

1. Gate A recipe V5 removes the two finisher montages from its writable destinations. They become required authored dependencies; their package bytes, referenced animations, and skeletons bind approval. A missing montage or `Finisher` section fails planning. The recipe still validates the paired data against explicit current role and warp settings.
2. Both manifests retain the existing proof cases and marker requirements, replace the historical finisher mapping with current Attacker/Victim paths, and declare the two GhostSamurai dependencies. Gate B also declares the currently referenced generic counter as a supporting dependency.
3. Extend existing recipe tests to reject writing current finishers or recreating the retired montage, and reject approval when either authored finisher package is dirty. Approval drift/refusal tests remain intact.
4. Build, then run fresh Gate A/B authoring plans and manifest plans. Inspect exact proposed fields and package ledger before saving. The intended asset write is only `/Game/ProjectFiles/Data/PDA/Attack/AttackData/Light/New/LightAttack_1` for the counter reference; any additional proposal requires investigation before execution.
5. Apply the exact reviewed plan, reload/audit, run affected authoring tests and Gate A proof, then adjacent/full verification as warranted. Preserve separate evidence for the intermittent physical-contact failure and manual two-map acceptance.

## Results

The V5 editor/test build passed. The initial build attempted UBT's global log directory and was denied before compilation; rerunning with the repository's explicit workspace `-Log` and no-UBA/no-makefile options succeeded.

Gate A authoring Plan reports `Unchanged`, zero errors, zero proposed package writes. The first Gate B authoring Plan exposed a second historical montage reference in `supportingAssets`; that declaration has also been replaced with the current Victim montage and will be replanned.

Gate A manifest Plan has no errors and exactly one unique change:

```text
SetAttackCounter|/Game/ProjectFiles/Data/PDA/Attack/AttackData/Light/New/LightAttack_1.LightAttack_1|/Game/ProjectFiles/Data/PDA/Defense/GateA/DA_Counter_GateA.DA_Counter_GateA
```

Reviewed report: `gate-a-manifest-plan.json`; fingerprint `188CE3CFF21BE3B7FF7ED7F8178EBB5D1D59B00F`. Its single ledger package is `LightAttack_1`, initially clean in memory, with action `Modify`. The three `WouldChange` rows describe that same single reference change in different proof contexts; they are not three package writes. The original package bytes are backed up as `LightAttack_1.before.uasset`. This plan is within the user's directed role-mapping repair. Current finisher and generic counter assets remain excluded from its save ledger.

Gate B manifest Plan also reports no errors and the same single-package counter-reference repair. The obsolete supporting montage declaration is resolved.

`gate-a-reference-save.json` reports one package saved and reloaded, zero errors, process exit 0. The only Content hash change among the 55 protected paths is `LightAttack_1.uasset`; both current finisher montages, their animations/data, the edited generic counter, and the historical montage's absence remain preserved. The save report retains pre-apply mismatch warnings; fresh audits will establish the post-save state. No timing-mutation or dirty-package override was used.

The focused Gate A replay passed, exit 0, with all 12 proof cases passing and a complete ledger. `gate-a-post-repair-evidence.json` records direct counter-marker continuation to `FinisherActive`, health 75 before the finisher and 0 afterward, one finisher damage event, one canonical completed cleanup, and one token release. All three observed stage handoffs meet the existing continuity contract. This is fresh headless PIE evidence, not rendered or manual acceptance.

The full post-repair suite passed **739/739**, process exit 0, with a matching nonempty discovery count, explicit success marker, zero automation failure/error entries, and 369 automation warnings. `full-post-repair-summary.json` and `test-results.json` retain the counts and every test path. All six prior authoring/approval failures and Gate A PIE proof now pass; Gate B PIE proof also passes. The updated tests cover the authored-finisher write exclusion and dirty-source approval rejection. No approval rule or proof case was removed to obtain this result.

Fresh post-save manifest audits passed, exit 0: Gate A **27/27 unchanged**, Gate B **82/82 unchanged**, both with zero warnings/errors and empty package ledgers. The final Gate B authoring Plan also exits 0 with no errors; it reports `WouldChange` for its separate eleven-package regeneration proposal. Its former missing-dependency failure is resolved, but the proposed regeneration is not applied.

| Verification | Evidence file under `Saved/Logs/DefenseProofRepair-20260909/` |
|---|---|
| Editor/test build | `recipe-v5-build-2.out.log` |
| Reviewed one-package save/reload | `gate-a-reference-save.json` |
| Focused Gate A success | `gate-a-post-repair-summary.json`, `gate-a-post-repair-evidence.json` |
| Full 739/739 success | `full-post-repair-summary.json`, `test-results.json` |
| Fresh manifest audits | `gate-a-post-save-audit.json`, `gate-b-post-save-audit.json` |
| Remaining valid Gate B authoring proposal | `gate-b-authoring-plan-final.json` |
| Content/source preservation and tested modules | `preservation-final.json`, `tested-binaries.json` |

## Changed files and preservation

- `Source/KatanaCombatEditor/Private/Commandlets/Operations/DefenseProofAuthoringOperation.cpp`: V5 recipe ownership, dependency binding, and current finisher warp expectations.
- `Source/KatanaCombatTest/Private/KatanaAssetMigrationTests.cpp`: current recipe contract plus authored-finisher preservation/dirty-dependency checks.
- `Tools/Codex/manifests/defense-gate-a.json` and `defense-gate-b.json`: current finisher mapping and explicit dependencies.
- `Content/ProjectFiles/Data/PDA/Attack/AttackData/Light/New/LightAttack_1.uasset`: restored reviewed counter reference; exactly one package saved/reloaded.
- This report, the health-resume report's follow-up link, `docs/guides/HEADLESS_ASSET_MIGRATIONS.md`, and the stabilization execution handoff: current instructions/results and remaining gates.

Of 55 protected Content paths, 54 retain their exact hash/existence state and only the intended `LightAttack_1` package differs. Content status entries match preflight; that asset was already modified before this repair. Of 365 captured source/configuration/tool files, four changed in this follow-up, exactly the recipe source, approval tests, and two manifests listed above; the other 361 match the prior verified state. The earlier eight-line Gate A test correction is unchanged by this follow-up. No gameplay C++ was edited.

The first final `git status` attempt could not write Git LFS's clean-filter cache under the restricted `.git` directory. A per-command `lfs.storage` override placed that cache under this evidence folder with `GIT_OPTIONAL_LOCKS=0`; status then completed successfully. No repository/global Git configuration was changed. `git diff --check` passed, both manifest JSON files parsed, and report links were checked.

## Proof limits and remaining work

- The pre-repair intermittent physical-contact failure did not recur in the focused or full post-repair Gate A runs. Its cause remains unproven; the evidence is retained in the health-resume folder. A future reproduction needs weapon-trace geometry, pose, and frame-time evidence rather than another unqualified retry.
- Headless PIE proves the named automated cases. A subsequent [rendered automation follow-up](AUTOMATED_ANIMATION_ACCEPTANCE_2026-09-09.md) passed Gate A and retained 47 images, while exposing capture-readiness and visibility gaps. Two-map Checkpoints 1C/1D and the full hold/input/interruption matrix remain incompletely covered; extend automated PIE acceptance for these behaviors. Manual play is not a prerequisite for closing them.
- Gate B's recipe also proposes refreshing nine attack variants and two matrix montages from the already edited source assets. These proposals are separate from the repaired counter/finisher dependency graph. Do not apply them as part of this one-package repair or call the Gate B authoring recipe `Unchanged` until independently reconciled.
- Review the existing source/test WIP into coherent slices alongside the remaining automated acceptance work. No commit was created by this repair.
