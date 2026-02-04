# Finisher Testing Guide

This guide explains how to set up, trigger, and debug finisher animations in the KatanaCombat paired animation system.

## Quick Start

### Prerequisites
1. A character with `UCombatComponent` and `UHitReactionComponent`
2. A target enemy with `UHitReactionComponent`
3. At least one `UPairedAnimationData` asset configured
4. Debug visualization enabled (see Debug Visualization section)

### Triggering a Finisher

Currently, finishers can only be triggered via the **Low Health** condition (below 25% health). The other trigger conditions (Guard Break, Stun) require systems that are not yet implemented.

**In PIE (Play In Editor):**

1. Enable debug visualization:
   ```
   Combat.Debug.PairedAnim 1
   ```

2. Damage the target below 25% health:
   - Use `ApplyDamage()` in Blueprint/code
   - Or use console command: `ke * HitReactionComponent ApplyDamage 80`

3. The debug HUD will show "Target Vulnerable: YES" with "Reason: LowHealth"

4. Call `TryExecuteFinisher()` on the attacker's CombatComponent with the target as parameter

---

## System Status

### Currently Implemented
| Feature | Status | Notes |
|---------|--------|-------|
| Low Health Trigger | Working | Below 25% health threshold |
| Victim Warp | Working | Continuous position tracking |
| Attacker Warp | Working | Motion warping toward victim |
| Input Blocking | Working | Combat input blocked during finisher |
| Slow Motion | Working | Via CinematicEffectsUtilityLibrary |
| Hitstop | Working | Sakurai-style freeze at sync point |
| Camera Shake | Working | Triggered at sync point |
| Debug Visualization | Working | HUD + 3D visuals |

### NOT Yet Implemented
| Feature | Status | Required For |
|---------|--------|--------------|
| Posture System | Not Implemented | Guard Break trigger |
| Block/Guard | Not Implemented | Parry system |
| Parry Detection | Not Implemented | Counter attacks |
| Counter Attacks | Not Implemented | Parry->Counter flow |
| Guard Break Trigger | Not Implemented | Finisher on posture depletion |
| Stun Trigger | Not Implemented | Finisher on heavy attack stun |
| AI Token System | Pending | Coordinated enemy attacks |

---

## Debug Visualization

### Enable Debug CVars

Open the console (`~` key) and enter:

```
// Enable all paired animation debug
Combat.Debug.PairedAnim 1

// Or enable specific categories:
Combat.Debug.PairedAnim.Warp 1           // Warp target visualization
Combat.Debug.PairedAnim.Partners 1       // Partner connection lines
Combat.Debug.PairedAnim.Sync 1           // Sync point visualization
Combat.Debug.PairedAnim.Vulnerability 1  // Finisher vulnerability indicators
```

### What Each Visualization Shows

#### 3D Visualizations (World Space)

| Element | Color | Description |
|---------|-------|-------------|
| Warp Target Crosshair | Cyan | Where warp is targeting |
| Partner Connection | Yellow Dashed | Line between paired characters |
| Sync Point | Magenta Pulse | Location where sync happens |
| Vulnerability Indicator | Green/Red | Whether target can receive finisher |
| Range Circle | Orange | Max finisher distance from attacker |
| Offset Arrow | Cyan | Shows warp offset direction |
| Alignment Line | Green/Red | Shows sync point alignment quality |

#### HUD Panel (Top-Right Corner)

The HUD panel shows:
- **State**: Current paired animation state (IDLE, EXECUTING_FINISHER, RECEIVING_FINISHER)
- **Role**: ATTACKER, VICTIM, or NONE
- **Partner**: Name of paired partner(s)
- **Warp Tracking**: Active warp status and distances
- **Vulnerability**: Whether current target is vulnerable and why
- **Sync Point**: Current sync progress and alignment
- **Effects Active**: Slow-mo, hitstop, camera shake status

---

## Setting Up a Finisher Animation

### Step 1: Create PairedAnimationData Asset

1. Right-click in Content Browser → Miscellaneous → Data Asset
2. Select `PairedAnimationData` as the class
3. Configure:
   ```
   AttackerMontage: [Your attacker animation montage]
   VictimMontage: [Your victim animation montage]
   SyncPointTime: 0.8 (when damage/effects trigger, in seconds)
   VictimOffset: (X=100, Y=0, Z=0) (relative to attacker)
   bVictimFacesAttacker: true
   ```

### Step 2: Configure Warp Settings

In the PairedAnimationData asset:
```
AttackerWarpConfig:
  WarpTargetName: "FinisherWarp"
  MaxWarpDistance: 300.0
  bAdjustToTerrain: true

VictimWarpConfig:
  WarpTargetName: "VictimWarp"
  MaxWarpDistance: 100.0
  bAdjustToTerrain: true
```

### Step 3: Add AnimNotifyStates to Montages

**Attacker Montage:**
- Add `AnimNotifyState_PairedAnimationSync` at sync point
- Add `AnimNotifyState_PairedAnimationCollision` for collision handling

**Victim Montage:**
- Add `AnimNotifyState_PairedAnimationCollision` for collision handling

### Step 4: Link to AttackData (Optional)

If the finisher should be triggered from a specific attack:
```cpp
// In your AttackData asset
FinisherData = YourPairedAnimationData;
```

---

## Testing Workflow

### Basic Finisher Test

1. **Setup Scene**:
   - Place player character and enemy in level
   - Ensure both have combat components

2. **Enable Debug**:
   ```
   Combat.Debug.PairedAnim 1
   Combat.Debug.All 1
   ```

3. **Damage Enemy**:
   - Attack enemy until health < 25%
   - Or use: `ke * HitReactionComponent ApplyDamage 80`

4. **Check Vulnerability**:
   - HUD should show "Target Vulnerable: YES"
   - Reason should be "LowHealth"

5. **Execute Finisher**:
   - Call `TryExecuteFinisher(Enemy)` on player's CombatComponent

6. **Observe**:
   - Warp targets appear (cyan crosshairs)
   - Partner connection (yellow line)
   - Sync point (magenta sphere at impact)
   - Slow-mo and hitstop at sync point

### Warp Testing

Test warp behavior on different terrain:

| Test | Expected Result |
|------|-----------------|
| Flat Ground | Both characters warp to offset positions |
| Uphill | Z-adjusted to terrain, no floating |
| Downhill | Same as uphill |
| Near Wall | Validation should reject if obstructed |
| Moving Target | Continuous tracking updates position |

### Edge Case Testing

| Scenario | Expected Behavior |
|----------|-------------------|
| Attacker dies mid-finisher | Victim animation cancels |
| Spam finisher button | Only one execution (input blocked) |
| Target already in finisher | Rejected (bIsFinisherTarget mutex) |
| Out of range target | Rejected (distance validation) |
| Obstructed path | Rejected (obstruction detection) |

---

## Troubleshooting

### Finisher Won't Trigger

1. **Check vulnerability**:
   - Is target below 25% health?
   - HUD should show "Target Vulnerable: YES"

2. **Check distance**:
   - Is target within `MaxWarpDistance`?
   - HUD shows current distance and max

3. **Check mutex**:
   - Is `bIsFinisherTarget` already true on target?
   - Another finisher may be in progress

4. **Check components**:
   - Does attacker have `UCombatComponent`?
   - Does target have `UHitReactionComponent`?

### Characters Not Aligning

1. **Check warp targets**:
   - Enable `Combat.Debug.PairedAnim.Warp 1`
   - Cyan crosshairs show target positions

2. **Check Motion Warping**:
   - Both characters need `UMotionWarpingComponent`
   - Warp target name must match montage's MotionWarping notify

3. **Check terrain adjustment**:
   - Is `bAdjustToTerrain` enabled in warp config?
   - Ground sampling should prevent floating

### No Effects Playing

1. **Check PairedAnimationData**:
   - Is `bApplySlowMotion` enabled?
   - Is `ImpactCameraShake` assigned?
   - Is `bApplyHitPause` enabled?

2. **Check sync point**:
   - Is `AnimNotifyState_PairedAnimationSync` in montage?
   - Is `SyncPointTime` correct?

3. **Check delegate binding**:
   - Effects are wired via `OnPairedAnimationSyncPoint` delegate
   - CombatComponent should auto-bind these

### Debug Visualization Not Showing

1. **Check CVar**:
   - `Combat.Debug.PairedAnim 1` must be set
   - Or master: `Combat.Debug.All 1`

2. **Check HUD class**:
   - Is `ACombatDebugHUD` set as HUD class?
   - Check GameMode or PlayerController settings

3. **Check character**:
   - Debug data only generated for `ABaseCombatCharacter`
   - Player must be controlling a combat character

---

## Console Commands Reference

```
// Master debug toggles
Combat.Debug.All 1                      // Enable all debug
Combat.Debug.PairedAnim 1               // Enable paired animation debug

// Paired animation categories
Combat.Debug.PairedAnim.Warp 1          // Warp targets
Combat.Debug.PairedAnim.Partners 1      // Partner connections
Combat.Debug.PairedAnim.Sync 1          // Sync points
Combat.Debug.PairedAnim.Vulnerability 1 // Vulnerability indicators

// Debug shape duration
Combat.Debug.DrawDuration 2.0           // How long shapes persist (seconds)

// Other useful debug
Combat.Debug.Targeting 1                // Targeting visualization
Combat.Debug.Phase 1                    // Attack phase indicators
```

---

## Known Limitations

1. **Only LowHealth trigger works**: Guard Break and Stun triggers require unimplemented systems
2. **No parry->counter flow**: Parry system not implemented
3. **Single victim only**: Multi-victim finishers are Phase 6
4. **No environmental finishers**: Architecture exists but not implemented
5. **Debug HUD shows "(N/I)" for unimplemented triggers**: These will activate when systems are added

---

## Next Steps (Roadmap)

See `docs/ROADMAP.md` and the active plan file for implementation timeline:

1. **Phase 5b-5: AI Coordination** - Attack token system for enemy coordination
2. **Phase 5b-6: Audio/VFX** - Sound and visual effects at sync points
3. **Phase 6: Parry/Counter** - Parry detection and counter attack flow
4. **Phase 6: Posture System** - Guard break vulnerability trigger
