# Paired entry movement presentation

`FPairedEntryConfig` optionally prepares a relative pair pose before either paired
montage starts. `MovingRole` selects the participant that moves. `Victim` preserves
the original behavior; `Initiator` holds the victim at its accepted world pose and
resolves the initiator goal with `VictimRelativeTransform.Inverse() * VictimWorld`.
The relative transform always describes the victim relative to the initiator.

Both roles retain scoped movement/input ownership and bounded swept alignment.
Environment obstruction, exhausted limits or lost ownership cancels entry before
paired playback and damage. This executor is direct upright alignment: it does not
plan a navigation path or follow terrain.

`MovementAnimation` supplies optional single-cycle in-place presentation for the
moving participant. The actual animation blueprint plays a transient montage on
`MovementSlot`; the movement component remains disabled and reports zero velocity.
The clip is supervised by animation instance, montage instance ID and entry
generation. Replacement or early stop cancels preparation; entry cleanup stops
only its accepted instance. Already-ready entry skips movement presentation.

Preflight requires the exact mesh skeleton, a registered slot, composition support,
root motion disabled, valid positive rates, and sufficient clip duration for the
entry deadline plus blend-out, accounting for actor time dilation. Authors must
also inspect the raw root track, gameplay notifies, actual AnimGraph slot coverage
and directional suitability. Disabling a root-motion flag alone does not establish
an in-place clip. A registered skeleton slot alone does not prove live graph coverage.

The capture runner accepts the original complete ten-field entry JSON, or all six
additional presentation fields. Partial/unknown fields fail validation. An example
for the reviewed straight rear approach is:

```json
{
  "enabled": true,
  "victim_offset_cm": [100, 0, 0],
  "victim_yaw_deg": 0,
  "duration_s": 0.65,
  "translation_speed_cm_s": 122,
  "travel_budget_cm": 150,
  "turn_rate_deg_s": 540,
  "turn_budget_deg": 180,
  "position_tolerance_cm": 2,
  "yaw_tolerance_deg": 3,
  "moving_role": "initiator",
  "movement_animation": "/Game/Assets/Animations/KatanaAnimset/InPlace/WalkForward_InPlace.WalkForward_InPlace",
  "movement_slot": "DefaultSlot",
  "movement_play_rate": 1,
  "movement_blend_in_s": 0.1,
  "movement_blend_out_s": 0.25
}
```

Pass its path with `run_scenario.py --entry-config` during `paired-warp-tuning`.
Animation paths must use exact object identity. The fixture records requested and
actual overrides and includes the selected animation in asset dependency hashes.
No source asset is saved by this experiment.

The 0.25-second blend-out matches the reviewed finisher blend-in. It is an authoring
candidate, not a gameplay default or a guarantee of smooth motion. Gait phase,
approach start/stop, oblique heading changes and floor support remain separate
authoring decisions. See the [qualification audit](../audits/INITIATOR_FINISHER_APPROACH_2026-09-13.md).

Run native controls with `Automation RunTests KatanaCombat.PairedAnimation.Entry`
and Python controls with `python -m unittest discover -s Tools/CombatCapture -p test_entry_presentation.py`.
For read-only raw animation inventory, set `KATANA_ENTRY_INVENTORY` to a folder
containing `inventory-input.json` with `reference_mesh` and explicit `animations`,
then run `inspect_entry_animations.py` through Unreal's Python commandlet. Inventory
output explicitly records unavailable reflection fields; native inspection is
required to close those gaps.
