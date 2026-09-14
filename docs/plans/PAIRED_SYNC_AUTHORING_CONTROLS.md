# Paired sync authoring controls

The facing comparison identified early damage and a large entry turn. Source and
retained runtime logs now show that the primary sync notify also nudges the partner
from approximately 141 cm to 80 cm at montage entry. Moving the event without
isolating that nudge would change both timing and the entry trajectory.

This slice adds optional primary-sync timing and nudge controls to the existing
transient paired evaluation workflow. It does not save assets, change gameplay
defaults, adopt the pending historical entry-approach design, or change contact
thresholds. The runtime entry trajectory is evaluated before deciding its next
implementation. The controls identify primary damage notifies by semantics, not
asset names, and operate on both roles with original event/object restoration.

1. Add a small editor `PairedSyncTuning` settings/provenance helper. Extend native
   `PairedWarpTuning` and Python validation with an optional nonempty `primary_sync`
   object: finite `time_s` within 0..5 and/or boolean `nudge_enabled`. Omission
   preserves the existing field. Require exactly one primary damage notify per
   role and reject missing/duplicate/inconsistent overrides.
2. Extend the scenario's transient notify copies and CLI arguments; retain notify
   duration and trigger offsets, reject an event extending beyond its montage,
   record exact before/after settings and restore original events/objects.
3. Add native and Python controls for independent fields, bounds, preservation,
   unknown settings, mismatched provenance and missing participant overrides.
4. Build and run affected capture/paired automation. Compare the current matching
   control, nudge-disabled entry, and nudge-disabled delayed sync. Use 0.6 seconds
   as a diagnostic hypothesis from the source strike interval, not a selected
   artist-approved impact time. Check completed/interrupted outcomes on both maps.
5. Review rendered entry/strike evidence and measured trajectories. Record the
   scenario's 1-health setup separately from damage observations. Retain geometric
   failures and unresolved contact/entry limits, then archive/hash-verify/remove
   generated PNG files.

Files: `Source/KatanaCombatEditor/Public/Analysis/PairedSyncTuning.h`,
`PairedWarpTuning.h`, project scenario/tuning tests, `Tools/CombatCapture` CLI and
validation/tests, and the evaluation guide/audit. Shared-plugin source and runtime
combat implementation are outside this diagnostic slice.
