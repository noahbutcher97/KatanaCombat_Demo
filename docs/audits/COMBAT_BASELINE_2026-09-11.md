# Katana combat baseline after shared-suite handoff

The current editor build and full headless automation baseline pass. All 778
discovered tests produce unique successful results, with zero automation
failures/errors, the explicit success marker and process exit 0. One rendered-only
surface control explicitly defers its GPU check under `NullRHI`; these results do
not establish visual quality or continuous surface contact.

## Scope and reproduction

The shared-suite developer now owns AnimationAnalysis development. This run verifies
Katana independently against its existing pin,
`ba13149d3318f80d3098958cd1d2cd52bba3e5d1`.
Project branch: `codex/combat-action-reaction-stabilization`; HEAD:
`6e170503b5958d84d4c065cfaf6e18704546df7a` plus existing WIP.
Source/config/dependency identity:
`8c10258cdf3b6a1a34a2936d2f999b8532e9215e400177b38c1f643a976319b8`.

Executed from the project root:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File Tools/Codex/run-agent-baseline.ps1
```

The standard runner built `KatanaCombatEditor Win64 Development` and ran
`Automation RunTests KatanaCombat;Quit` with `NullRHI`. It required no additional
DDC override in this run. Both processes exited 0.

Evidence: `Saved/Logs/CombatResume-20260911-120723/verification.json`, source and
asset-preservation records, complete parsed test results and warning groups.
Original runner logs use the prefix
`Saved/Logs/Codex-Agent-Baseline-20260911-080726`; the verification record hashes
each build/test log and the runner's summary.

The 481 automation warning records remain visible in the evidence. Common messages
concern empty trace windows, Niagara spawn failures, missing navigation in fixtures
and dead-target rejection. They are not automation assertion failures; this pass
does not classify every warning as harmless. The specific rendered surface deferral
is retained separately from ordinary messages about skipped gameplay operations.

## Disposition

There are no fresh failing tests to repair from this baseline. The original July
approval-contract and defense-continuity failures were repaired in subsequent work;
the current full run includes successful authoring/approval and defense PIE results.
The initial health report remains historical evidence, not the current pass state.

The next bounded Katana task is the finisher source-pair review recorded in
[the visual contact investigation](FINISHER_VISUAL_CONTACT_2026-09-10.md):

1. Inspect the unwarped attacker/victim source choreography using the existing
   paired preview, intended meshes and equipped weapon attachment.
2. Establish intended relative facing, placement, grip and anatomical contact region
   from the source motion and visible evidence. Current proxy names and damage events
   do not establish those facts.
3. Prepare a concrete placement/contact proposal, then compare it with the existing
   native evaluator and raw/projected gameplay views. Keep any provisional criterion
   revision explicit and justified independently of whether it produces a pass.
4. Verify selected changes with completed/interrupted captures on both maps before
   promoting saved authoring or a quality reference.

Earlier transient warp settings and the quarter-turn grid result were diagnostic
controls, not selected corrections. The saved montages still retain their authored
movement-disabling flags. Opening damage timing and contact authoring remain open.
The new shared agent's asynchronous readback and moving-surface work can proceed
in parallel; the source-pair review uses existing project capabilities.

## Preservation and retention

Source/configuration/dependency manifests match before and after this run. All
7,948 Content file size/time records and 54 protected asset hash/missing records
are unchanged. This task adds this audit and refreshes the current-state handoff;
it makes no gameplay, asset, dependency or shared-repository implementation change.

Complete newly generated capture/test bundles were archived and each archive entry
hash checked before removing all six generated PNGs. The archive is
`Saved/Logs/CombatResume-20260911-120723/capture-evidence.zip`, 10,740,033 bytes,
SHA-256 `cad80899ac7790cd4c253f3b1d21419beb3c2842ba465a7e96fd95888362001d`.
Restore archive entries before replaying image-dependent analysis of those bundles.
Existing WIP and prior retained evidence are preserved; nothing was staged or committed.
