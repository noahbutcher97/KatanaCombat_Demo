# Paired animation publication checkpoint — 2026-09-13

The accumulated paired-animation runtime, capture integration and verification
work is grouped for publication on `investigate/finisher-source-pair`, based on
`50685e86dc2a284408f9f8fa6fbcedd37efed470`. Remote `main` matched that base during
preflight. No gameplay or asset changes were made during publication preparation.

Commit groups:

1. Bounded paired entry, facing policies and sync alignment ownership, with native
   regression coverage.
2. Pinned AnimationAnalysis asynchronous capture integration, shader installation
   integrity, comparison compatibility and consumer controls.
3. Transient facing, sync and entry capture settings, strict provenance and tests.
4. Specifications, plans and evidence reports, including unresolved authoring work.

## Verification and evidence limits

Fresh checks on September 13 passed: the editor build, 86 capture Python tests,
11 dependency Python tests, and Git whitespace validation. The current executable
source/config/tool fingerprint exactly matches the September 11 verified tree:

`282df9f180dbf8dd0bf06ae40d97ae2f86c1f7aba06d1b0f7bbc30f193585551`.

That earlier tree passed 799/799 headless automation tests and 12/12 rendered
lifecycle scenarios. Those Unreal tests were not repeated for this publication;
matching source and dependency bytes preserves the applicability of the recorded
results. Six contact/alignment evaluations completed with measured failures.
Preparation still visibly slides and turns, and lethal state is observed near
montage start. Publication does not approve the finisher's visual quality or
experimental authoring values. See the [bounded entry audit](BOUNDED_PAIRED_ENTRY_2026-09-11.md).

AnimationAnalysis remains pinned to
`3fd91eb70be340db63778a697b12465ab895cd8a`. Its generated plugin and detached source
checkout are verified against the pin. Later standalone mesh-reference work is a
separate consumer integration task.

Local publication commands, hook logs, source/file hashes and remote readback are
retained under `Saved/Logs/GitPublication-20260913-130123/`. Prior evidence remains
under the dated Saved paths in the individual audits. Saved artifacts and generated
plugin files are excluded from Git. The 1,665 PNGs from bounded-entry verification
were already hash-verified into their archive and pruned; this publication does not
generate new image captures. Content and existing source bytes are preserved.
