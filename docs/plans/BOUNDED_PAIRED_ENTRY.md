# Bounded paired entry

Add opt-in preparation before legacy paired montage playback. Preserve current
assets and retained defense behavior. The prepared pose is explicitly authored as
a victim transform relative to the initiating character; it is separate from a
montage's warp endpoint. The initiator holds its current transform, while the
victim approaches the requested relative transform. This slice supplies movement
and lifecycle correctness, not an authored approach animation or contact approval.

1. Add actor-independent alignment limits, step calculation and outcomes in
   `CombatTypes.h` and `Utilities/AlignmentMotionLibrary.*`. Bound translation,
   yaw, cumulative travel/turn and elapsed simulation time; reject nonfinite data.
2. Extend `TargetingComponent` with a scoped bounded-movement executor, public
   outcome query, swept movement and explicit blocked/exhausted/invalid outcomes.
   Existing rotation-only and motion-warp executors retain their contracts.
3. Add `FPairedEntryConfig` to paired data. In `PairedAnimationComponent`, reserve
   existing paired ownership, collision/movement leases and scoped alignment;
   defer montage playback and sync damage until entry succeeds. Cancellation,
   target loss, competing ownership or a missed deadline releases only this
   generation's resources. Keep implementation in a dedicated entry translation
   unit and extract the existing legacy montage-start body without changing it.
4. Exercise public APIs across frame cadences, moving targets, obstacles,
   unreachable geometry, interruption, reentry and retained defense. Add explicit
   transient capture configuration and observations for successful/rejected entry
   using production finisher assets; do not save experimental authoring.
5. Build and run focused checks, adjacent/full regression as warranted, rendered
   comparisons and visual review. Record residual montage/contact limitations.
   Preserve WIP/Content, then archive/hash-verify generated PNGs before removal.

Acceptance: preparation cannot damage or play paired montages early; each applied
entry step obeys the declared limits, and success requires both participants to
meet their live target tolerances. Failure does not teleport back or proceed to
impact. Deadlines are in world simulation time and do not advance during pause.
Entry limits do not claim to bound subsequent authored/root-warp motion.

Implemented and verified: [bounded entry audit](../audits/BOUNDED_PAIRED_ENTRY_2026-09-11.md).
The final 799-test baseline and 12 rendered scenarios pass. The audit separates
mechanical acceptance from the remaining approach-animation, contact and impact
timing work; no asset settings have been promoted.
