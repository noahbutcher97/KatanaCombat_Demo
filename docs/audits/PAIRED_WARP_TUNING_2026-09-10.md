# Paired warp tuning and collision ownership

The transient window/placement comparison exposed a runtime ownership defect. Opening finisher damage dispatched the victim's dying event. `UCombatComponent::OnCharacterDeath` preserved the attacker's sequence through partner notification, then cleared the victim's own partner list because the victim does not own the active montage sequence. Its subsequent collision notify acquired a lease with no attacker to ignore. CharacterMovement pushed the overlapping capsules apart.

No Content package was saved. The comparison does not select an animation correction or promote a quality reference. Existing contact criteria remain unchanged.

## Evidence and correction

Evidence root: `Saved/Logs/PairedWarpTuning-20260910-223032/`. The session began September 10 local time; some UTC artifact names are dated September 11.

- With victim warping delayed to 0.20–0.43 seconds, the large early step still occurred around montage 0.10–0.12 seconds while its modifier list was empty. Actor displacement disagreed with the movement velocity.
- Native `LogMovement Verbose` replay identified the victim capsule penetrating the player capsule by 17.421 cm and teleporting 17.546 cm along X. Later corrections followed as the attacker advanced. See `movement-diagnostic.log` and the referenced scenario's `automation.log`.
- A new actual-scenario assertion reproduced the asymmetric relationship: attacker ignores victim = true, victim ignores attacker = false; partner counts = 1/0. See `collision-check.log`.
- The focused public-API regression reproduces actual lethal damage and death callbacks before the victim collision notify begins. Its corrected fixture explicitly dispatches actor BeginPlay so the real combat death delegates are bound. `collision-lifecycle-red-tests.log` fails on the missing victim partner and movement-ignore relationship; `collision-green-tests.log` passes.
- `UPairedAnimationComponent::IsExpectedPairedVictimDeath` resolves the retained sequence and checks its existing lethal-damage/generation contract. Combat death cleanup preserves that sequence's victim partner link. Terminal legacy participation removes only the retiring owner's partner link and releases its leases. Unrelated death still cancels the sequence.

This qualifies the previous grounding audit's interpretation of the early spike. Native warp amplification was observed in that earlier configuration, but measured actor movement also included capsule depenetration. Changing the attacker translation policy exposed that additional contribution; window changes alone could not isolate it.

## Bounded screening

The new `paired-warp-tuning` experiment temporarily permits CharacterMovement on both participants and disables attacker translation warping to match the pair configuration. It varies only the victim's effective warp window and horizontal relative offset. It retains victim translation/rotation and attacker rotation. Request, scenario and capture provenance must describe exactly five overrides; original events, instanced notify objects and pair configuration are restored. No package-save path is used.

All eight ThirdPerson completed screening runs pass the existing gameplay checks; the four after the fix also pass the new reciprocal collision assertion. These runs render the world and record motion, with zero PNG export.

| Victim effective window (s) | Relative X (cm) | Early peak before fix (cm) | Early peak after fix (cm) | Blade proxy minimum after fix (cm) |
|---|---:|---:|---:|---:|
| 0.0001–0.872984, original | 50 | 19.532 | 3.389 | 38.804 |
| 0.0001–0.18, entry | 50 | 19.923 | 3.047 | 38.900 |
| 0.20–0.43, return | 50 | 19.703 | 2.683 | 40.271 |
| 0.0001–0.18, entry | 65 | 24.036 | 6.200 | 41.604 |

Early peaks are measured actor-position differences between captured observations after 0.05 seconds and before 0.30 seconds of victim montage time. The blade proxy uses the declared weapon segment and 18 cm torso sphere during attacker montage 0.30–1.50 seconds. These are sampled diagnostic measurements, not continuous-contact claims or artistic acceptance. Exact intervals and source identities are in `screen-results.json` and each capture bundle.

None of these settings justifies saved authoring changes. The original window has the smallest blade gap in this bounded set, but all exceed the existing provisional 8 cm allowance. Delaying the window reduces the early movement peak while worsening the gap; increasing X to 65 cm worsens both. The larger approximately 13.77 cm step later near montage 1.25 seconds is outside the warp windows and agrees with recorded movement velocity; its magnitude alone does not establish another warp defect.

## Changed surfaces

Runtime changes are limited to `CombatComponent.cpp`, `PairedAnimationComponent.cpp` and its public header. Editor/test/tool changes add strict tuning request validation, effective trigger-window handling, reversible fixture overrides, reciprocal collision checks and regression tests. No new private workflow names are introduced.

The effective-window regression also caught a harness error: Unreal computes notify end from start trigger time plus duration plus end trigger offset. The helper now preserves both authored offsets while setting the requested effective start/end; it no longer adds the start offset twice.

Reproduction and bounds are documented in [Paired animation evaluation](../guides/PAIRED_ANIMATION_EVALUATION.md). Source snapshots and the task-only diff are retained with the evidence. Temporary scripts stay in this evidence directory.

## Final verification

- Editor build: success (`collision-green-build.log`, 29 compile/link/metadata actions).
- Full `Automation RunTests KatanaCombat;Quit` with NullRHI: **776/776 success**, explicit process exit 0 (`full-automation.log`, `full-automation-summary.json`). This includes the new opening-death regression, effective-window/request tests and existing defense/legacy cleanup coverage.
- `python -m unittest discover -s Tools/CombatCapture -p 'test_*.py' -v`: **35/35 pass** (`final-offline-tests.log`).
- Final original-window tuning, completed/interrupted on ThirdPerson and DefenseMatrix: **4/4 rendered scenario passes**. All 455 submitted PNGs exported, with zero pending/rejected/failed frames before retention cleanup. Early victim steps are 3.389–3.399 cm, victim pitch and target-height variation are zero, and recorded movement acceleration is zero through the measured warp interval.
- Delayed-window native movement logging: **2/2 motion captures with rendered simulation pass**, one per map. Both have zero victim–attacker depenetration log entries; early steps are 2.683 cm and maximum steps through 0.95 seconds are 3.547 cm. These replays export no PNGs.
- Native paired evaluator: **8 valid measured comparisons**, covering all four screening settings plus original/delayed windows on both maps. All eight pass sampled entry separation and fail the unchanged blade-contact and release criteria. Original-window rendered blade gaps are approximately 38.802 cm on both maps; delayed-window gaps are approximately 40.271 cm. This is diagnostic evidence, not an accepted finisher presentation.
- Native malformed-provenance controls: mismatched requested window and omitted offset both produce `runtime_status=inconclusive`, with `Invalid paired warp tuning provenance` for every runtime contact case and no runtime observations. The authored lane still evaluates and fails, so the overall report remains `fail`; native process exit 0 means a report was produced. An initial local aggregation assertion incorrectly expected overall `inconclusive`; that assertion was corrected without changing the evaluator or repeating the valid comparisons.
- All **54 protected Content paths retain their original SHA-256 values or recorded absence**. The actual saved montages still disable movement and retain their previous translation flags; these experiments do not silently promote their temporary settings.

Exact run/capture/report paths are in `validation-stages.json`, `final-run-results.json` and `contact-results.json` under the evidence root.

## Retention and next action

After export and evaluation, 451 bulk PNGs were moved to the Windows Recycle Bin and four inspected review frames were retained across the two completed-map captures. The 11 earlier retained images were preserved; 15 PNG files remain under `Saved/CombatCaptures`. Each pruned capture has `retention.json`/`RETENTION.md`, and its generated HTML reports carry retention notices. Telemetry, pose ledgers, original export counts, criteria, identities and reports are preserved. Restoring the recycled frames or recapturing is required for a full image review or fresh image-integrity check; no reclaimed-disk-space claim is made.

The next authoring step is to reconcile the source pair's relative facing/placement and intended blade-contact region using the existing paired preview and native evaluator. The tested window/offset alternatives do not solve contact and should not be saved as corrections. Keep opening damage timing unchanged until contact intent and a concrete movement configuration are established. Confirm any selected configuration with fresh completed/interrupted gameplay captures on both maps before saving packages. Full AnimGraph parity, foot support, continuous contact and artistic feel remain outside these measurements; no plugin or learned scoring expansion is needed for this next step.
