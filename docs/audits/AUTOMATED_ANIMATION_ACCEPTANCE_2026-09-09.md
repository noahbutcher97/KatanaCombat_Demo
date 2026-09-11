# Automated animation acceptance - 2026-09-09

Latest follow-up: [reusable scenario evaluation](COMBAT_SCENARIO_EVALUATION_2026-09-09.md) adds completed/interrupted finisher input recovery with active bystanders on both maps, explicit pose/frame eligibility, fixed 960×540 rendering, run identity and repeated mechanical reference measurements. Its full headless baseline passes 749/749. The initial findings below are preserved as historical evidence; see the newer report for what is now verified and the remaining contact, foot-support and perceptual-quality limits.

Automation can measure animation defects and produce rendered evidence. The earlier recommendation that visual animation quality or the remaining lifecycle checkpoints necessarily require manual play was too broad. Behavioral acceptance should use the existing automation and extend it where coverage is missing. Artistic intent still needs a reference or explicit criteria; passing a numerical threshold alone does not establish that an animation looks good.

## Fresh rendered verification

Evidence directory: `Saved/Logs/RenderedAnimation-20260909-142416/`. No gameplay source, test source, or Content was edited in this verification follow-up. The three loaded module hashes match the recorded post-repair build. The previous rendered evidence directory was copied before the fixture replaced its generated output.

Ran `Automation RunTests KatanaCombat.Defense.GateA.PIEProof;Quit` through `UnrealEditor-Cmd.exe`, with rendering enabled using `-RenderOffScreen`, without `-NullRHI`. Full arguments and logs are retained per attempt.

| Attempt | Result | Evidence and interpretation |
|---|---|---|
| 1 | Startup failed, exit 3 | Shader working-directory writes failed outside the permitted workspace. No automation result. |
| 2 | 1 test failed, exit 255 | Workspace shader/cache paths fixed startup. Asset preparation overlapped the hands-off observation window. The fixture's 10-second wall-clock deadline expired before an attack generation; captured simulation times advanced from 0.4 to 0.8 seconds. Two images were saved. |
| 3 | 1/1 test passed, exit 0 | Same assertions and assets, with the cache populated. All 12 proof cases passed; 47/47 requested PNGs decoded and passed the existing pixel/framing checks. Four automation warnings remain in the log. |

Attempt 2 is preserved in `attempt-2-evidence/`; attempt 3 in `attempt-3-evidence/`. `summary-3.json` confirms discovery/completion agreement, no automation errors, and the explicit successful exit marker. Attempt 3 does not resolve the cold-start readiness problem exposed by attempt 2.

The successful launch uses `-ShaderWorkingDir=<evidence>/ShaderWork`, `-DDC=InstalledNoZenLocalFallback`, and the process-local `UE-LocalDataCachePath=<project>/Saved/Automation/RenderedDDC`. These avoid changing machine or project cache settings. The requested outer window size was 1280x720; the actual PIE images are **759x483**. A future comparison must control the actual game viewport resolution.

## Measurements already asserted

From `attempt-3-evidence/defense-gate-a-evidence.json`:

| Measurement | Rendered result | Existing assertion |
|---|---:|---:|
| Stage handoffs observed | 3 | Required transition ledger complete, valid identities and adjacent samples |
| Maximum actor displacement at handoff | 0 cm | At most 10 cm |
| Maximum pelvis displacement at handoff | 5.2043 cm | At most 15 cm |
| Maximum unexpected alignment displacement | 0 cm | At most 10 cm |
| Maximum yaw above rate times simulation delta | 0.0000093 degrees | At most 0.1 degrees |
| Evaluated alignment frames | 46 | Nonempty |
| Counter damage / finisher damage / finisher completed cleanup | 1 / 1 / 1 | Exactly once |

`DefenseGateAPIEProofTests.cpp` samples actor and pelvis positions on adjacent updates and checks them during evidence finalization. The pelvis measurement is displacement, not a full-pose discontinuity score: it includes legitimate animation motion, covers one bone per participant, and uses a coarse existing threshold. Passing it cannot rule out wrist/foot/head pops or sliding. Maximum alignment pelvis frame delta was 6.0034 cm; that value is recorded but has no separate assertion here.

The preceding full headless run remains 739/739, as recorded in [the repair report](DEFENSE_PROOF_REPAIR_2026-09-09.md). This follow-up ran one focused rendered test, not the full suite again.

## Capture weaknesses found by inspection

Inspected attempt-3 frames 1, 4, 27, 34, 39, and 47, plus attempt-2 frame 2. Later frames show actual guard, paired combat, and finisher cleanup. Frame 1 in the passing run still shows an editor view with a PlayerStart icon. Thus the current nontrivial-pixel and projected-center checks can accept a stale editor frame. The all-frames flags do not establish that every image contains the intended gameplay view.

The rear camera also obscures much of the opponent in several paired frames. An actor center projected inside the viewport does not prove the actor's limbs or contact points are visible. These captures establish that rendered automation runs here; they do not establish complete visual acceptance. No reference-image comparison, dense temporal quality score, foot-slip measurement, or penetration assertion was executed.

## Next automated acceptance work

1. Harden capture readiness: wait for loaded render resources and a drawn PIE game viewport, bind each image to that viewport and its evaluated frame, and reject stale/editor captures. Preserve a separate bounded wall-clock watchdog while measuring gameplay deadlines in simulation time. Add a side view for paired contact inspection and control actual viewport resolution.
2. Extend real-montage PIE input/lifecycle coverage on both `Lvl_ThirdPerson1` and `Lvl_DefenseMatrix`: submit movement and attack input during paired ownership, exercise release/repress after completion, and observe active/queued bystanders through takeover, lethal completion, and nonlethal recovery. Existing component tests already cover many rules; Gate A's controlled paired phase disables controller logic and cannot substitute for the full active-bystander scenario.
3. Sample evaluated root and relevant bone transforms throughout transitions, including before/after windows. Measure translation and angular velocity changes with explicit sample intervals, participant identity, and expected root motion. Retain peaks and failure frames; reject missing/stale pose samples instead of counting zeros as good quality.
4. Measure foot motion only during authored or validated planted intervals; measure weapon/hand contact distance and timing against the intended partner region. Add penetration checks with explicit exceptions for intentional weapon contact. Calibrate thresholds against reviewed acceptable and deliberately defective examples before making them quality gates.
5. Add temporal rendered comparisons with fixed camera, phase alignment, warm resources, and tolerances for renderer variation. A reference comparison detects change; the reference itself must have an established acceptance basis. Sparse stage screenshots alone cannot prove frame-to-frame smoothness.

The first priority is trustworthy capture and sampling, then additional quality assertions. Manual input is not a prerequisite for these automated checks. Visual review can use the automatically captured evidence for questions of timing, readability, and style that have not yet been expressed as acceptance criteria.
