# Combat capture hardening - 2026-09-09

## Scope

Repair the existing rendered combat proof so it captures the active PIE game viewport after drawing, waits for render resources, and separates simulation deadlines from loading delays. Improve paired-contact visibility using a temporary side camera. This is test-harness work; gameplay behavior, authored assets, and model-based feel scoring are outside this slice.

Evidence directory: `Saved/Logs/CaptureHardening-20260909-144740/`. The source snapshot and all modified/untracked Content hashes were recorded before edits. Existing source and asset WIP remain in place.

## Failure and repair

The preceding [animation acceptance audit](AUTOMATED_ANIMATION_ACCEPTANCE_2026-09-09.md) retained a passing run whose first screenshot showed a PlayerStart icon in an editor viewport. The proof used a global screenshot request. Unreal's viewport draw path processes screenshots for editor and game viewports; the projected actor centers came from the player controller, independently of whichever viewport supplied the image.

The updated proof listens to `UGameViewportClient::OnViewportRendered`, checks the exact viewport client and PIE world, and reads that viewport's pixels after the canvas flush. It records the actual draw index, world, capture source, request timestamp, draw timestamp, requested stage, and stage at draw. Pending captures complete before the next scenario update. The fixture removes its delegate during cleanup.

The new regression `KatanaCombat.Automation.RenderCapture.ViewportIdentity` rejects editor worlds, a different client of the same world, another PIE world, a client that reverted to an editor world, and a missing client. The existing PNG validation remains in place. The overall evidence result now includes frame count, pixel validation, projected-center checks, and capture provenance; gameplay success alone cannot mark an incomplete rendered proof successful.

The proof waits for shader preparation before starting PIE and for two game viewport draws with no pending shader/asset compilation before initializing observation. Stage deadlines use simulation time. A separate 60-second wall-clock watchdog handles stalled stages, and final cleanup uses wall time so it can finish after a lost or paused world. The latent command's startup clock begins when it first executes, after preceding load commands.

The controlled scenario uses a temporary side camera, restored and destroyed during cleanup. Projection checks retain their actual meaning: centers inside the viewport. They do not claim pixel-level visibility or complete mesh/contact correctness.

## Naming guidance

The user directed that repository names describe their purpose without requiring private workflow, machine, or AI-tooling knowledge. This rule is recorded in root `AGENTS.md`. New helper/test names follow it. Existing opaque proof names have consumers in automation commands, manifests, assets, and documentation; their migration belongs in a scoped naming change that updates those consumers together.

## Verification

- Initial build passed. An initial implementation passed all three then-selected rendered tests using an empty local DDC, with 48/48 captures from the PIE game viewport. Inspection confirmed the stale editor image was gone, but the first mesh still showed material warmup. This result is retained in `rendered-cold-evidence/`; it predates the final resource-readiness and side-camera changes and the new test path.
- The final build passed (`build-final.out.log`, exit 0).
- Final rendered verification with a fresh local DDC passed **2/2 tests**, exit 0, with all 12 gameplay cases and **48/48** captures. `rendered-final-summary.json` records discovery/completion agreement, the explicit success marker, no automation errors, and seven warnings. `rendered-final-evidence/` retains all images, telemetry, and schema-4 evidence. Two images use the gameplay camera and 46 use the controlled side camera.
- Inspected final images 1, 35, 40, and 48: the first image now shows the shaded gameplay character; the side images show both participants at finisher entry, during the action, and at cleanup. `rendered-provenance-check.json` independently verifies unique increasing draw indices, the PIE capture world/source, and draw timestamps at or after request timestamps for all 48 images. The maximum handoff actor displacement was 0 cm and pelvis displacement was 5.1407 cm across three observed handoffs. Counter damage, finisher damage, and finisher completed cleanup each occurred once.
- Full headless regression passed **740/740**, exit 0 (`full-headless-summary.json`), with matching discovery/completion, an explicit success marker, no automation errors, and 369 warnings. The new `KatanaCombat.Automation.RenderCapture.ViewportIdentity` regression passed under its descriptive final name. The existing rendered-file validation and real defense PIE proof both passed.
- All **59** protected Content paths retain their pre-edit hash or absence state (`content-preservation.json`). No gameplay source or asset was changed. Scope: the existing defense PIE test source, root naming guidance, this report, and the linked audit/handoff updates. No commit was created.

## Remaining acceptance

The subsequent [reusable capture and analysis layer](COMBAT_CAPTURE_AND_ANALYSIS_2026-09-09.md) adds a shared editor API, ordinary PIE console controls, synchronized motion/telemetry exports, and offline reports. The earlier changes above remain a specialized proof repair; new capture work should use the shared API documented in the [capture guide](../guides/COMBAT_CAPTURE_AND_ANALYSIS.md).

After this capture slice, continue the real-montage input/recovery and active-bystander lifecycle scenarios on both project maps. Preserve the existing passing component-level tests while filling the integration gaps identified in the earlier audit. Neither a perceptual scoring platform nor plugin packaging is required for that work.

Rendered image sequences remain sampled evidence, not a calibrated smoothness or feel score. Actual viewport resolution is recorded; fixed-resolution comparisons, dense bone/contact measurements, and any vision-based scoring require their own demonstrated need and verification.
