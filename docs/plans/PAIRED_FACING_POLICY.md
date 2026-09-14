# Explicit paired facing policy

Implemented and verified: [results and remaining authoring work](../audits/PAIRED_FACING_POLICY_2026-09-11.md).

Approved direction: the [source-pair review](../audits/FINISHER_SOURCE_PAIR_REVIEW_2026-09-11.md)
identifies rear-approach choreography, while current paired warps face the partner.
The [AnimationAnalysis integration](../audits/ANIMATION_ANALYSIS_INTEGRATION_2026-09-11.md)
is verified before this gameplay change begins.

## Contract

- `EPairedFacingPolicy` expresses face partner, face away from partner, or match
  partner heading. Existing assets default to face partner. Rotation disabled keeps
  existing behavior; no policy enables movement or overrides montage modifier flags.
- Resolve enabled facing with yaw only, ignoring participant pitch/roll and height.
  Coincident horizontal positions preserve the caller's fallback heading for
  positional facing; matching uses the partner heading even at coincidence.
- One actor-independent resolver feeds direct paired targets, retained alignment
  requests and their preflight turn-budget checks. The source of a positional facing
  vector remains unchanged in each existing consumer.
- Invalid policy values are rejected before acquiring or replacing ownership.
  Missing/destroyed partners retain existing rejection and cleanup behavior.
- Policy belongs to the paired role config and is carried into retained alignment
  requests. Legacy unwired victim-facing fields remain unused.

## Implementation and verification

1. Add the enum and alignment request field in `CombatTypes.h`, the authored field
   in `PairedAnimationTypes.h`, and the pure facing resolver in
   `PairedAnimationUtilityLibrary`.
2. Route direct setup/pre-update and retained alignment through the resolver in
   `TargetingComponent.cpp`. Preserve default facing, disabled rotation, target
   ownership and the existing antipodal turn tie-break.
3. Validate and carry the policy through paired bridge/stage preflight and alignment
   construction in `PairedAnimationComponent.cpp`. Update any corresponding initial
   bridge alignment consumer discovered during implementation.
4. Test both roles, all policies, changed heading, coincidence, disabled rotation,
   invalid values, missing/destroyed partners and retained alignment through public
   APIs. Build and run affected targeting, paired and defense tests; broaden to the
   combat baseline because shared alignment is on the defense path.
5. Add an explicit transient facing control to the existing finisher evaluation
   workflow if needed for the gameplay comparison, preserving its original default
   control and recording the requested policy. Compare completed/interrupted cases
   on both maps before proposing saved authoring. Keep source initial placement
   distinct from runtime warp endpoints and keep contact criteria unchanged.

No asset resave, criterion relaxation, dependency update or shared-plugin edit is
part of this facing implementation. Exact visible contact and impact timing remain
separate evidence questions; a correctly oriented warp target is not a quality pass.
