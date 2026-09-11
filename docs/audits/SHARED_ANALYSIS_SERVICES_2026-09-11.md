# Shared animation analysis services

The existing analyzers and runners now consume portable integrity, identity,
publication, numeric and temporal services in [animation-analysis 0.2.0](../../Tools/AnimationAnalysis/README.md).
The migration also closes a provenance gap: runner source snapshots previously
omitted the portable package, and analyzer/evaluator identities hashed only their
entrypoint script. Changes to shared code now invalidate those identities.

## Implementation and compatibility

- `integrity` and `errors` own strict JSON/JSONL, finite numbers, contained paths and
  existing RGB/RGBA PNG CRC/scanline validation. The project `CaptureError` is a
  compatibility alias for `EvidenceError`. JSON exponent overflow now rejects along
  with NaN/Infinity; malformed deflate is reported as an evidence error.
- `artifacts` owns SHA256, canonical identity, caller-selected relative manifests
  and atomic UTF-8/JSON replacement. It rejects escaping and duplicate manifest
  paths. Failed replacement preserves the last complete file and removes its
  temporary file. Atomicity is per file, not a report/sidecar transaction.
- `metrics` owns numeric summaries/deltas and explicit status precedence. Boolean
  flags are excluded from numeric deltas; non-finite observations reject. A pass
  applies to executed cases only; unexecuted cases stay visible in reports.
- `temporal` takes declared clocks and caller-translated records. It requires unique
  endpoint events and complete ordered bracketing observations. Clock mismatches,
  missing coverage and negative padding reject. Duplicate acquisition times remain
  available to consumer-specific pose/cadence validation.
- Project `capture_format.py` selects legacy bundle files, translates simulation
  timestamps and combines package/project implementation identities. It does not
  select a character, contact region, skeleton or gameplay outcome in the core.
- Existing analyzer/evaluator APIs remain importable. The scenario runner, paired
  runner and summary tool use shared artifact services. All four packaged visual
  jobs use shared atomic replacement. Combat assertions, reference eligibility,
  legacy stream interpretation and launch defaults remain project integration.

Offline implementation identity conservatively includes all package Python source
and the relevant project entrypoint, format adapter, analyzer and bootstrap. It
does not identify external decoders/models; those retain their separate provenance.
Source snapshots also include package tests and packaging metadata. Execution-source
identity excludes offline analysis code while full source identity retains it.
Legacy single-script references intentionally become incompatible; none was
automatically regenerated or relabelled as valid.

## Verification

Evidence: `Saved/Logs/AnalysisServices-20260911-104924/`.

| Check | Result |
|---|---|
| Project Python suite | 76 passed |
| Installed wheel, no image dependency | 24 passed; one image-only test explicitly skipped |
| Installed wheel with optional image dependency | 25 passed |
| Installed commands | Four help entrypoints succeeded outside the checkout |
| Isolation | Python isolated mode; package from temporary site-packages, no project import or Pillow for the core check |
| Calibration replay | 90 cases, zero mismatches; full result unchanged |
| Segment/surface replay | 16 segment measurements/assessments and five surface observations unchanged |
| Capture stream/evaluator replay | Four captures, 1,131 samples; analysis, event bracketing and scenario outputs unchanged apart from the intended evaluator identity |
| Preserved evidence | 117 retained input files byte-identical after replay |

The four stream captures already lack bulk PNGs following verified evidence
retention. Both old and new analyzers report them incomplete. Replay establishes
telemetry/behavior parity and continued rejection of incomplete visual coverage,
not fresh rendered success. Pixel replay uses retained embedded images and surface
buffers; no new capture or PNG export was needed.

Regression controls cover missing/ambiguous events, clock mismatch, missing interval
coverage, changed implementation dependencies, stale references, path escape,
failed publication and corrupt PNGs without Pillow. The initial migration test run
caught a leftover zlib exception reference; it was fixed before the passing run.
A test's UTF-8 HTML read was also made explicit for Windows system encodings.

The isolated build/install temporary directory was removed. No C++, config or
assets were edited; all 7,948 Content file sizes/timestamps and the 54 previously
protected asset hash/missing states were preserved. No new PNGs remain in the
evidence folder. The retained wheel and verification logs are the deliverables;
no package was published externally.

## Remaining migration

This completes a shared-service slice of the [suite-wide plan](../plans/VISUAL_ANALYSIS_LIBRARY.md).
Legacy stream/motion analysis, report assembly, process orchestration and reusable
retention still have project coupling. The next concrete implementation boundary is
the native Unreal recorder/observation module with Katana discovery and gameplay
telemetry supplied by adapters. Native build isolation, async readback performance,
continuous moving skeletal sampling, preview/commandlet migration, support/region
surfaces and mesh penetration remain open. This Python verification establishes
none of those runtime or animation-quality claims.
