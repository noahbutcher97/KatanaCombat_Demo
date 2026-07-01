# KatanaCombat WIP Evidence Annex - 2026-06-27

## Scope

This annex backs `docs/handoffs/2026-06-27-wip-classification.md` with reproducible Git evidence and exact lane manifests.
It is additive documentation only. No source, assets, maps, or config files were staged, reverted, deleted, renamed, or resaved.

## Evidence Directory

Raw evidence lives under:

- `docs/handoffs/evidence/2026-06-27-wip/git-status-short-branch.txt`
- `docs/handoffs/evidence/2026-06-27-wip/git-status-after-proof.txt`
- `docs/handoffs/evidence/2026-06-27-wip/git-diff-name-status.txt`
- `docs/handoffs/evidence/2026-06-27-wip/git-untracked-files.txt`
- `docs/handoffs/evidence/2026-06-27-wip/lane-counts.tsv`
- `docs/handoffs/evidence/2026-06-27-wip/lane-manifest.tsv`
- `docs/handoffs/evidence/2026-06-27-wip/build-katana-editor-development-stdout.txt`
- `docs/handoffs/evidence/2026-06-27-wip/automation-asset-migration-stdout.txt`
- `docs/handoffs/evidence/2026-06-27-wip/automation-attack-data-tools-stdout.txt`
- `docs/handoffs/evidence/2026-06-27-wip/verification-summary.json`

Per-lane path lists:

- `docs/handoffs/evidence/2026-06-27-wip/lane-current-feature-candidate.txt`
- `docs/handoffs/evidence/2026-06-27-wip/lane-runtime-combat-wip.txt`
- `docs/handoffs/evidence/2026-06-27-wip/lane-tracked-asset-modifications.txt`
- `docs/handoffs/evidence/2026-06-27-wip/lane-tracked-asset-deletions.txt`
- `docs/handoffs/evidence/2026-06-27-wip/lane-untracked-content-imports-and-replacements.txt`
- `docs/handoffs/evidence/2026-06-27-wip/lane-local-tooling-config-wip.txt`
- `docs/handoffs/evidence/2026-06-27-wip/lane-historical-audit-docs.txt`

## Lane Counts

| Lane | Git State | Count |
| --- | --- | ---: |
| current-feature-candidate | M | 5 |
| current-feature-candidate | ?? | 15 |
| historical-audit-docs | M | 4 |
| historical-audit-docs | ?? | 135 |
| local-tooling-config-wip | M | 3 |
| local-tooling-config-wip | ?? | 3 |
| runtime-combat-wip | M | 11 |
| tracked-asset-deletions | D | 160 |
| tracked-asset-modifications | M | 4 |
| untracked-content-imports-and-replacements | ?? | 6,305 |

## Proof Boundaries

The Git evidence proves path classification and current dirty-worktree shape. It does not prove source correctness, runtime behavior, asset referencer health, map loadability, Blueprint defaults, montage notify contents, package save status, or LFS readiness.

Because this checkout contains unrelated runtime and asset WIP, any build or automation run performed directly in this checkout proves the current checkout state. It does not isolate the current-feature lane from runtime-combat WIP. Isolated proof requires a clean worktree or branch with only `lane-current-feature-candidate.txt` applied.

## Verification Plan

1. Validate generated evidence exists and lane counts match Git.
2. Run static syntax checks for JSON and PowerShell files that are in the local/tooling lane.
3. Build `KatanaCombatEditor Win64 Development` with `-NoLiveCoding`.
4. If the build succeeds, run focused automation:
   - `KatanaCombat.Editor.AssetMigration`
   - `KatanaCombat.Editor.AttackDataTools`
5. Record command exit codes and log-summary evidence below.

## Verification Results

Completed in the dirty checkout. These results prove the current checkout state, not an isolated branch containing only the current-feature lane.

| Check | Result | Evidence |
| --- | --- | --- |
| `.mcp.json` JSON parse | Passed | `ConvertFrom-Json` completed with no exception. |
| `Config/Automation/Presets/Katana.json` JSON parse | Passed | `ConvertFrom-Json` completed with no exception. |
| Focused test discovery | Passed | Source contains `KatanaCombat.Editor.AssetMigration` and `KatanaCombat.Editor.AttackDataTools` tests. |
| Build: `KatanaCombatEditor Win64 Development -NoLiveCoding` | Passed | Exit code `0`; output reports `Target is up to date` and `Result: Succeeded`. |
| Automation: `KatanaCombat.Editor.AssetMigration` | Passed | Exit code `0`; found 13 tests; 13 success completions; test complete exit code `0`. |
| Automation: `KatanaCombat.Editor.AttackDataTools` | Passed | Exit code `0`; found 7 tests; 7 success completions; test complete exit code `0`. |

`KatanaCombat.Editor.AttackDataTools` emitted 3 expected warning-log entries while exercising invalid timing and missing-section rejection cases. They did not fail the automation run.

## Remaining Proof Gaps

- This was not an isolated-lane proof because the checkout also contains runtime-combat WIP and extensive asset/content WIP.
- The build was up to date; it proves UBT accepted the current target state, but it was not a from-clean rebuild.
- No asset referencer scan, map load audit, Blueprint default inspection, montage notify inspection on real assets, package save test, LFS audit, or cook/package run was performed.
- `Content/` deletions and imports remain blocked pending explicit asset intent and editor-level verification.
