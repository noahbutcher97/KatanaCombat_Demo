# Defense Gate B Acceptance

## Decision

**Status: Accepted for the scoped single-player defense interaction.** The dedicated fixture loads without manual wiring, every manifest proof case has headless and/or rendered PIE evidence, the final migration audit is clean, and the UBA-disabled baseline is green. No in-scope contract below remains `Partial` or `Not Implemented`.

This decision does not accept multiplayer ordering/replication, production enemy tactics, catalog-wide animation polish, a Chooser backend, guard-break resources, or UI integration.

## Review Strategy

Do not review this branch as one flat aggregate diff. The implementation is organized into 27 dependency-ordered commits before this acceptance-only documentation commit:

| Review pass | Commits | Area and gate |
|---|---|---|
| 1 | `bc2a59ac` through `871106eb` | Accepted design, implementation plan, and feasibility record. |
| 2 | `0c263254` through `ec79c4c4` | Typed runtime contracts, idempotent contact authority, alignment, consumption, and retained Chain ownership. |
| 3 | `c2647241` through `1b3170b8` | Telemetry, validation workflow, deterministic asset authoring, and Gate A acceptance. |
| 4 | `4e18fe9b` | Runtime trajectory, contact, alignment, AI recovery, and focused regressions. |
| 5 | `e39f4ba3`, then `4acb3e88` | Migration/authoring safety first, then the reviewed Gate B binary assets. |
| 6 | `97384859` through `420b36f2` | Fixture, catalog, 54-case matrix, two-threat arbitration, semantic proof, and isolated unity-build fixes. |
| 7 | `37e09083`, then `2949d3ff` | Coherent reviewed-prediction test seam and adjacent-frame Gate A handoff proof. |

Review each pass against its tests and evidence below before advancing. The final regenerated baseline then checks the integrated stack, including committed unity-file participation.

## Evidence

- Authority: `Tools/Codex/manifests/defense-gate-b.json` and `Config/AssetMigrations/DefenseGateBTargets.txt`.
- V11 authoring: `defense-gate-b-v11-bone-height-plan.json`, `defense-gate-b-v11-bone-height-save.json`, and `defense-gate-b-v11-bone-height-post-save-audit.json`; approved fingerprint `5AD2313E317B118CE2148AD8257C9C59E0A8237B`.
- Closed dependency audit: `defense-gate-b-final-audit-green.json`, 82/82 unchanged with zero failures, mutations, or saves.
- Matrix proof: `Saved/DefenseProof/GateB/{Headless,Rendered}/defense-gate-b-evidence.json`; final rendered log `Saved/Logs/Defense-GateB-PIEProof-Rendered-CurrentSource-20260718.log`.
- Threat proof: `Saved/DefenseProof/GateB/Threat/{Headless,Rendered}/defense-gate-b-threat-evidence.json`; final rendered log `Saved/Logs/Defense-GateB-ThreatPIEProof-Rendered-CurrentSource-20260718.log`.
- Semantic proof: `Saved/DefenseProof/GateB/Semantic/{Headless,Rendered}/defense-gate-b-semantic-evidence.json`; final rendered log `Saved/Logs/Defense-GateB-SemanticPIEProof-Rendered-CrispFinal-20260718.log`.
- Gate A continuity regression: `Saved/DefenseProof/GateA/{Headless,Rendered}/defense-gate-a-evidence.json`; rendered log `Saved/Logs/Defense-GateA-PIEProof-Rendered-Handoff-20260718.log`.
- Full baseline: `Saved/Logs/Codex-Agent-Baseline-20260718-151218-*`; no-UBA build passed and 645/645 tests completed with zero failures/errors and an explicit success marker.

The rendered matrix passed 54/54 variants and 108/108 decoded, nontrivial frames. It covers all nine height/lane cells at montage rates `0.5`, `1.0`, and `2.0`, with world dilation `1.0` and `0.5`. All frames contain both full capsules, none overlap, and minimum actor separation is `55.35 px`. Every variant recorded one resolution, presentation, VFX, and audio invocation, plus fixture restoration. Responses split into 18 recoil and 36 continue cases. The maximum target-bone vertical delta is `29.28 cm` against `30 cm`.

Threat PIE passed seven deterministic tie/deadline/lock/switch/stale/invalidation scenarios with one enumeration each and restored the one-attacker default. Semantic PIE proved a physical `UnblockableMiddleCenter` hit for exactly 25 damage and a physical `PerfectParryGateARegression` with one consumption, token release, attack end, `ParryActive`, and `CounterWindow` transition. Gate A recorded all three adjacent-frame Chain handoffs; rendered maximum pelvis discontinuity was `4.11 cm` against `15 cm`. Gate B's `max_alignment_pelvis_frame_delta_cm` remains diagnostic and is not a stage-handoff assertion.

## Contract Traceability

| Spec section/contract | Owning slice | Implementation files/assets | Automation evidence | Commandlet/static evidence | PIE/telemetry evidence | Status | Residual risk |
|---|---|---|---|---|---|---|---|
| Target semantics: held guard is stateful and perfect parry is edge-triggered | 3, 4 | `CombatComponent`, player input route | `KatanaCombat.Defense.Input`, `.Parry` | Input/source architecture gates | Gate A plus semantic `PerfectParryGateARegression` | Proven | Controller and animation feel remain tuning. |
| Typed identity and snapshots are immutable; resolver performs no mutable second read | 1, 2 | `CombatTypes.h`, `DefenseResolver`, `CombatComponent` | `.Resolver`, `.Contact` reentry and cache cases | Architecture source tests | Matrix and semantic rows retain one interaction/attack identity | Proven | Network serialization is out of scope. |
| Physical contact is target-authoritative, first commit wins, and hit budget follows the receipt | 2, 7 | `BaseCombatCharacter`, `WeaponComponent` | `.Contact`, `Weapon.HitDetection` | Rich-path source gate | 54 physical block variants plus physical semantic contacts | Proven | Network ordering is out of scope. |
| Friendly, neutral, invulnerable, dead, i-frame, stale, and consumed preconditions are explicit | 2 | `BaseCombatCharacter`, `DefenseResolver`, `WeaponComponent` | `.Contact`, `.Resolver`, weapon-budget cases | Gameplay-tag and rich-path source gates | Matrix preserves player health; semantic cleanup is exact | Proven | Neutral compatibility remains policy-driven. |
| Blockable contact inside the defense cone resolves zero-damage normal block | 2, 3, 7 | Resolver, defense configuration, nine Gate B AttackData assets | `.Resolver`, `.Contact`, `.Presentation` | Manifest matrix invariant; 82-target final audit | 54/54 matrix variants at six rate/dilation combinations | Proven | Pose polish remains content tuning. |
| `Attack.Property.Unblockable` bypasses held guard | 2, 7 | Resolver, `LightAttack_11` | `.Resolver`, `.Contact` | Manifest semantic case; final audit | `UnblockableMiddleCenter`: exactly 25 damage | Proven | One reviewed unblockable attack is accepted. |
| Perfect parry requires a fresh press, parryable tag, live source window, reachable threat, and high-confidence timing | 4, 6, 7 | `CombatComponent`, `LightAttack_1`, Gate A bridge assets | `.Parry`, `.Input`, `.Chain` | Gate A manifest and post-audit | Semantic perfect parry plus Gate A Chain proof | Proven | Catalog breadth is not implied. |
| Block-interruptible attacks recoil; other normal blocks continue | 2, 3, 7 | Presentation selector, Gate B configuration, `AM_Recoil_Generic` | `.Presentation`, `.Contact` | Manifest response rows; final audit | 18 `RecoilGeneric`, 36 `ContinueGeneric` | Proven | Reviewed matrix assets only. |
| Source bearing drives cone eligibility; measured weapon/path trajectory drives lane | 1, 2, 7 | `CombatTypes.h`, `WeaponComponent`, `DefenseResolver` | `Weapon.HitDetection`, `.Resolver` trajectory cases | Manifest provenance invariant | All matrix rows report physical trajectory provenance | Proven | Authored fallback remains low confidence by design. |
| Height, lane, swing, source socket, target bone, and hit metadata remain independent | 1, 7 | Typed contact records, AttackData profiles, Gate B assets | `.Resolver`, `.AssetValidation` | V11 maps `spine_01` to Middle; 82-target audit | Nine cells preserve expected independent fields and <=30 cm target delta | Proven | IK consumption is not separately captured. |
| Configuration precedence and deterministic sparse fallback do not change gameplay outcomes | 1, 3, 7 | `DefenseConfiguration`, presentation selector, combat settings | `.Presentation`, `.Alignment`, configuration precedence cases | Ambiguity/fallback validation; manifest closure | Matrix selects exact or declared fallback rows | Proven | Chooser backend is out of scope. |
| Defender/attacker presentation and VFX/audio consume one immutable committed resolution | 2, 3, 7 | `HitReactionComponent`, cinematic FX utility, contact receipts | `.Presentation`, `.Contact` reentry cases | Manifest dependency closure | Every matrix variant records one resolution, presentation, VFX, and audio call | Proven | Network replay is out of scope. |
| Threat ranking is deterministic, hysteretic, age-aware, and enumerates candidates once | 3, 7 | `CombatComponent`, stable combatant IDs | `.Threat`, `.Input` | Stable-ID and ranking source gates | Seven two-active-threat scenarios, one enumeration each | Proven | More than two live threats remain automation-only. |
| Defense alignment is owner-scoped, rate/turn bounded, and uses copied Motion Warping without direct runtime snaps | 3, 6, 7 | `TargetingComponent`, combat warp notify, proof settings | `.Alignment`, `CombatWarp`, `.Targeting` | No production defense transform write; template-isolation gate | Gate A/B yaw and displacement ledgers stay within limits | Proven | Smoothing remains tuning. |
| Clock domains, callback reentry, tombstones, and terminal cleanup preserve first-commit ordering | 2, 4, 5 | `CombatComponent`, paired component, combat-effects subsystem | `.Contact`, `.Parry`, `.Chain`, `.CombatEffects` | Lifecycle/clock source gates | Semantic exact-once counts and Gate A terminal cleanup | Proven | Multiplayer races are out of scope. |
| Perfect parry atomically consumes the source generation and prevents later damage | 4, 7 | `CombatComponent`, `WeaponComponent`, enemy AI | `.Parry`, `.Contact`, `.Chain` | Consumed-window source gates | Semantic records one consume, token release, and attack end | Proven | Multiplayer races are out of scope. |
| AI interruption, cancel, recovery, and death release token and warp exactly once; same-team damage is rejected | 2, 4, 7 | `EnemyCombatAIComponent`, contact policy | `EnemyCombatAI`, `.Contact` | Fixture/AI source gates | Semantic and fixture restoration telemetry | Proven | Production tactics and perception are out of scope. |
| Counter bridge retains context, leases, identity, and pose through counter-to-finisher | 5, 6 | `PairedAnimationComponent`, Chain stage notify, Gate A assets | `.Chain`, `.PairedAnimation`, `.CombatEffects` | Gate A dependency audit | Three adjacent-frame handoffs; max pelvis discontinuity 4.11 cm | Proven | One canonical paired sequence is accepted. |
| Failure, cancellation, destruction, and timeout paths restore owned state without cross-owner release | 2, 4, 5 | Contact fallback, targeting leases, paired leases, AI termination | Reentry/destruction, `.Chain`, `.CombatEffects`, `EnemyCombatAI` | Ownership source gates | Gate A cleanup and every Gate B case restore fixture state | Proven | Extreme external actor teardown remains engine-lifecycle dependent. |
| Asset writes are explicit, approved, atomic, reload-audited, and limited to the reviewed proof fixture | 6, 7 | Approval service, authoring/migration operations, target ledger | `KatanaCombat.Editor.AssetMigration`, `.AssetValidation` | V11 approved fingerprint; 82/82 unchanged post-audit | Load-and-play matrix, threat, and semantic fixtures | Proven | Fingerprints are recipe-specific and non-reusable. |
| Proof is repeatable and gameplay remains decoupled from UI | 6, 7 | Proof director, Gate A/B maps, structured evidence writers | Gate A/B headless and rendered roots; 645/645 baseline | Branch classification contains no UI implementation | 49 Gate A and 108 Gate B rendered frames plus semantic/threat captures | Proven | UI remains intentionally deferred. |

## Change Classification

The final branch classification accounts for 164 paths relative to `main`: 59 runtime source/header files, 16 editor workflow files, 33 automation files, 37 reviewed binary packages, 12 canonical/workflow docs, and 7 config/manifest/tool files. The binary set is 30 additions and 7 modifications; all 37 packages have the LFS filter and `git lfs fsck` passes. No path is unexplained or unrelated to the accepted branch intent.

- **Required runtime:** contact trajectory aggregation, threat ownership, AI recovery, immutable presentation telemetry, and scoped alignment/target cleanup.
- **Required editor workflow:** manifest validation, deterministic authoring approval, exact save/reload ledgers, and map/Blueprint persistence checks.
- **Supporting proof:** proof director, Gate A/B PIE tests, catalog/migration tests, test weapon sockets, and evidence-only effect counters.
- **Required content:** Gate A assets plus nine lane-specific Gate B AttackData variants, Gate B montages, defense/proof settings, fixture Blueprints, and `Lvl_DefenseMatrix`.
- **Approved existing content:** `LightAttack_7`, `LightAttack_2`, and `LightAttack_11` were changed with exact migration provenance. `LightAttack_1` remains the reviewed Gate A dependency. No unrelated package is accepted here.

## Follow-On Work

Future commits may expand animation quality/catalog coverage, production AI, multiplayer replication, UI, and selector storage. Those are separate scopes and do not weaken this acceptance decision.
