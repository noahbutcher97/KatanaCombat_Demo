# Micro-Plan 07: Validation And Visible Proof

**Spec:** `docs/superpowers/specs/2026-07-18-combat-action-reaction-stabilization/07-validation-and-visible-proof.md`

**Goal:** Add durable telemetry/proof fixtures, run the full verification ladder, reconcile docs, and produce a merge-ready evidence record without saving user maps.

## Task 1: Add Telemetry Contracts First

**Create:**

- `Source/KatanaCombat/Public/Debug/ActionReactionTelemetry.h`
- `Source/KatanaCombat/Private/Debug/ActionReactionTelemetry.cpp`
- `Source/KatanaCombatTest/Private/ActionReactionTelemetryTests.cpp`

- [ ] Define bounded structured records for input capture/decision/application, hold ownership, action decisions, reactions, AI movement/tokens, alignment, animation lane/fallback, and socket drift.
- [ ] Include actor, attack/primary generation, montage instance/source, queue entry, hold generation, and reason codes so stale, duplicate, and concurrent events are observable.
- [ ] Add tests for bounded retention, reset, stable serialization fields, and no gameplay mutation.

## Task 2: Add A Transient PIE Proof Director

**Create:**

- `Source/KatanaCombat/Public/Debug/ActionReactionProofDirector.h`
- `Source/KatanaCombat/Private/Debug/ActionReactionProofDirector.cpp`
- `Source/KatanaCombatTest/Private/ActionReactionPIEProofTests.cpp`

- [ ] Spawn the director transiently through a test/console entry point; do not place or save it in either map.
- [ ] Implement the eleven named scenarios from Micro-Spec 07 with deterministic setup, timeout, teardown, and restoration.
- [ ] In the alignment scenario, assert actual actor yaw at first contact and warp end for isolated targetless 45/90/135/exact-180-degree attacks, terminal-zero/no-input facing preservation, invalid-target fallback, and captured facing across queue delay; published targets/modifiers alone do not pass.
- [ ] Exercise targeted and targetless live steering by holding, reversing, and releasing input, plus a late pre-contact change. Prove bounded actor-yaw response, stable target/translation identity, reachable clamping, exact-180 tie behavior, and zero contribution to AI/paired/defense alignment.
- [ ] Emit one pass/fail record per contract and distinguish headless-capable assertions from visible-review requirements.
- [ ] Prove repeated runs leave no actors, timers, delegates, tokens, focus, speed overrides, or modified disk state.

## Task 3: Add Architecture And Asset Guardrails

**Create/Modify:**

- `Source/KatanaCombatTest/Private/ActionReactionArchitectureSourceTests.cpp`
- `Source/KatanaCombatTest/Private/DefenseAssetValidationTests.cpp`
- `Source/KatanaCombatTest/Private/GameplayTagContractTests.cpp`

- [ ] Reject direct attack/defense rotation writes, orbit positional-boolean MoveTo, gameplay decisions in AnimInstance/StateTree, raw-damage AI interruption, and overlay root motion.
- [ ] Verify no new semantic tag exists without runtime and validator consumers.
- [ ] Verify maps/external actors are excluded from migration manifests.

## Task 4: Run The Full Evidence Ladder

- [ ] Run focused proof roots after a clean editor build.
- [ ] Run `Tools/Codex/run-agent-baseline.ps1` and preserve its timestamped logs/summary.
- [ ] Run data validation and both action-reaction commandlets in audit/no-save mode.
- [ ] Run all eleven PIE scenarios in `Lvl_DefenseMatrix` and `Lvl_ThirdPerson1` without saving.
- [ ] Capture short video for targeted and targetless static/live-steered attack alignment, orbit, moving guard, parry distinction, additive trade, full-body interrupt, and 3x3 response matrix.
- [ ] Restart Editor and repeat crash, orbit, guard, trade, interrupt, and death scenarios.
- [ ] Classify each claim by evidence tier; do not use source-only evidence for visible quality.

## Task 5: Adversarial Closure

- [ ] Re-run every challenge in `ADVERSARIAL_AUDIT.md` against actual code, assets, tests, and telemetry.
- [ ] Record finding severity, evidence, disposition, and regression proof in the execution handoff.
- [ ] Fix all high/medium findings. Any justified low deferral gets an explicit follow-up issue and cannot contradict in-scope acceptance.
- [ ] Run an original-intent audit against the crash report and every gameplay note that initiated this design.

## Task 6: Reconcile Documentation

**Modify only to proven state:**

- `CLAUDE.md`
- `docs/architecture/ARCHITECTURE_QUICK.md`
- `Source/KatanaCombatTest/README.md`
- `docs/superpowers/specs/2026-07-18-combat-action-reaction-stabilization-design.md`
- `docs/handoffs/2026-07-18-combat-action-reaction-stabilization-execution.md`

- [ ] Mark each target contract `Proven`, `Partial`, `Not Implemented`, or `Out Of Scope` with direct evidence.
- [ ] Remove stale claims that CancelWindow is future-only and ordinary stun is finisher eligibility.
- [ ] Document deferred EQS/avoidance and flinch-driven trace/damage as non-current behavior.

## Task 7: Final Diff And Commits

- [ ] Run `git diff --check`, inspect all commits/diffs, verify LFS pointers, and compare user map WIP hashes/status to the preflight.
- [ ] Commit source proof: `Add action reaction proof coverage`.
- [ ] Commit docs/evidence: `Document action reaction verification`.
- [ ] Run the baseline once more from final HEAD.

**Gate:** Merge is blocked by any crash, unexplained input lockout, unauthorized committed-hold replacement, frozen pose, token imbalance, path churn, unaligned accepted attack, stale/unbounded live steering, steering leakage into non-regular alignment, guard sliding, indistinct parry, cancel escape before minimum lock, ordinary-stun finisher, unintended asset save, or missing current evidence.
