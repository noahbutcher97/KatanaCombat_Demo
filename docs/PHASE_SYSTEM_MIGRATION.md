# Phase System Migration Guide

**From**: NotifyState-based phase system (AnimNotifyState_AttackPhase + AnimNotify_ToggleHitDetection)
**To**: Event-based phase transition system (AnimNotify_AttackPhaseTransition)

---

## Quick Summary

The phase system has been simplified from **NotifyStates with ranges** to **single-event transitions**:

### Old System (6 notifies per attack)
```
[AnimNotifyState: Windup Phase]
  [AnimNotifyState: Active Phase]
    [AnimNotify: Enable Hit Detection]
    [AnimNotify: Disable Hit Detection]
  [AnimNotifyState: Recovery Phase]
```

### New System (2 notifies per attack)
```
[AnimNotify: Phase Transition → Active]
[AnimNotify: Phase Transition → Recovery]
```

**Why?** Phases are contiguous by definition - they don't need explicit ranges. Hit detection is automatic with the Active phase.

---

## Migration Steps

### For Each Attack Montage:

#### 1. Remove Old Notifies
Delete the following from your attack montages:
- All `AnimNotifyState_AttackPhase` instances (Windup, Active, Recovery)
- All `AnimNotify_ToggleHitDetection` instances (Enable/Disable)

#### 2. Add Phase Transition Events

Add **exactly 2** `AnimNotify_AttackPhaseTransition` notifies:

**First Transition** (Windup → Active):
- Position: At the END of the windup animation (where attack becomes damaging)
- Property: `TransitionToPhase = Active`
- Example: 0.3s into a 1.0s attack (30% mark)

**Second Transition** (Active → Recovery):
- Position: At the END of the active phase (where attack stops dealing damage)
- Property: `TransitionToPhase = Recovery`
- Example: 0.5s into a 1.0s attack (50% mark)

#### 3. Verify Implicit Boundaries

The system now uses implicit phase boundaries:
- **Windup Start**: Automatic when montage begins
- **Recovery End**: Automatic when montage ends
- No explicit notifies needed for these!

---

## Visual Guide

### Attack Timeline

```
Montage Start                                    Montage End
    ↓                                                ↓
    [──────────────────────────────────────────────]

    ╔═══════════╗╔══════╗╔════════════════╗
    ║  WINDUP   ║║ACTIVE║║   RECOVERY     ║
    ╚═══════════╝╚══════╝╚════════════════╝
                 ▲        ▲
                 │        └─ AnimNotify_AttackPhaseTransition(Recovery)
                 └────────── AnimNotify_AttackPhaseTransition(Active)
```

### Hit Detection Timing

Hit detection is **automatic** based on the Active phase:

```
    [──────────────────────────────────────────────]

    ╔═══════════╗╔══════╗╔════════════════╗
    ║  WINDUP   ║║ACTIVE║║   RECOVERY     ║
    ╚═══════════╝╚══════╝╚════════════════╝
                 ┌──────┐
                 │ HIT  │ ← Automatic from phase
                 │ DET  │    No manual toggles!
                 └──────┘
```

---

## Migration Examples

### Example 1: Light Attack (1.0s duration)

#### Old System
```
0.0s: [Montage Start]
0.0s: AnimNotifyState_AttackPhase BEGIN (Windup)
0.3s: AnimNotifyState_AttackPhase END (Windup)
0.3s: AnimNotifyState_AttackPhase BEGIN (Active)
0.3s: AnimNotify_ToggleHitDetection (Enable)
0.5s: AnimNotify_ToggleHitDetection (Disable)
0.5s: AnimNotifyState_AttackPhase END (Active)
0.5s: AnimNotifyState_AttackPhase BEGIN (Recovery)
1.0s: AnimNotifyState_AttackPhase END (Recovery)
1.0s: [Montage End]
```
**Total**: 6 notify events + 4 notify state markers = 10 elements

#### New System
```
0.0s: [Montage Start] ← Implicit Windup begins
0.3s: AnimNotify_AttackPhaseTransition(Active) ← Transition + auto-enable hit detection
0.5s: AnimNotify_AttackPhaseTransition(Recovery) ← Transition + auto-disable hit detection
1.0s: [Montage End] ← Implicit Recovery ends
```
**Total**: 2 notify events

### Example 2: Heavy Attack with Hold (2.0s duration)

#### Old System
```
0.0s: [Montage Start]
0.0s: AnimNotifyState_AttackPhase BEGIN (Windup)
0.5s: AnimNotifyState_AttackPhase END (Windup)
0.5s: AnimNotifyState_AttackPhase BEGIN (Active)
0.5s: AnimNotify_ToggleHitDetection (Enable)
1.0s: AnimNotify_ToggleHitDetection (Disable)
1.0s: AnimNotifyState_AttackPhase END (Active)
1.0s: AnimNotifyState_AttackPhase BEGIN (Recovery)
2.0s: AnimNotifyState_AttackPhase END (Recovery)
2.0s: [Montage End]

PLUS: AnimNotifyState_HoldWindow (0.2s - 0.4s) ← Keep this!
```

#### New System
```
0.0s: [Montage Start] ← Implicit Windup begins
0.5s: AnimNotify_AttackPhaseTransition(Active)
1.0s: AnimNotify_AttackPhaseTransition(Recovery)
2.0s: [Montage End] ← Implicit Recovery ends

PLUS: AnimNotifyState_HoldWindow (0.2s - 0.4s) ← Keep this!
```

**Note**: Windows (Hold, Combo, Parry, Cancel, Counter) are UNCHANGED. Only phase notifies are affected.

---

## What to Keep (Don't Delete These!)

The following AnimNotifyStates are **NOT deprecated** and should remain in your montages:

- ✅ `AnimNotifyState_HoldWindow` - For heavy attack charge detection
- ✅ `AnimNotifyState_ComboWindow` - For combo input timing
- ✅ `AnimNotifyState_ParryWindow` - For parry detection
- ✅ `AnimNotifyState_CancelWindow` - For attack canceling (if implemented)
- ✅ `AnimNotifyState_CounterWindow` - For counter opportunities (if implemented)

**These are Windows, not Phases!** Windows can overlap and are non-contiguous.

---

## Common Mistakes to Avoid

### ❌ Mistake 1: Adding Phase Transition at Start
```
0.0s: AnimNotify_AttackPhaseTransition(Windup) ← WRONG!
```
**Why wrong?** Windup starts implicitly at montage start. No notify needed.

### ❌ Mistake 2: Adding Phase Transition at End
```
1.0s: AnimNotify_AttackPhaseTransition(None) ← WRONG!
```
**Why wrong?** Recovery ends implicitly at montage end. No notify needed.

### ❌ Mistake 3: Adding 3 Transitions
```
0.3s: AnimNotify_AttackPhaseTransition(Active)
0.5s: AnimNotify_AttackPhaseTransition(Recovery)
1.0s: AnimNotify_AttackPhaseTransition(None) ← WRONG!
```
**Why wrong?** You only need transitions to Active and Recovery. Start/end are implicit.

### ❌ Mistake 4: Keeping Hit Detection Toggles
```
0.3s: AnimNotify_AttackPhaseTransition(Active)
0.3s: AnimNotify_ToggleHitDetection(Enable) ← WRONG! Redundant!
```
**Why wrong?** Hit detection is now automatic. The transition to Active already enables it.

### ❌ Mistake 5: Removing Window NotifyStates
```
Deleted AnimNotifyState_ComboWindow ← WRONG!
```
**Why wrong?** Windows are separate from phases. Only delete phase and hit detection notifies.

---

## Verification Checklist

After migrating a montage, verify:

- [ ] Exactly 2 `AnimNotify_AttackPhaseTransition` events present
- [ ] First transition is set to `Active`
- [ ] Second transition is set to `Recovery`
- [ ] No `AnimNotifyState_AttackPhase` instances remain
- [ ] No `AnimNotify_ToggleHitDetection` instances remain
- [ ] All window notifies (Combo, Hold, Parry, etc.) are still present
- [ ] First transition is positioned at end of windup (where damage should start)
- [ ] Second transition is positioned at end of active (where damage should stop)
- [ ] No transitions at montage start (0.0s)
- [ ] No transitions at montage end

---

## Testing Your Migration

### In-Game Testing

1. **Play the migrated attack in-game**
2. **Enable debug visualization**: `CombatComponent->bDebugDraw = true`
3. **Watch the log output for**:
   ```
   [PHASE] Montage started → Windup phase (implicit)
   [PHASE] Transitioning from Windup to Active
   [HIT DETECTION] Enabled (automatic from Active phase)
   [PHASE] Transitioning from Active to Recovery
   [HIT DETECTION] Disabled (automatic from Recovery phase)
   [PHASE] Montage ended → None phase (implicit Recovery end)
   ```

4. **Verify**:
   - Attack deals damage during Active phase only
   - Combo inputs are accepted during combo window
   - Hold detection works (heavy attacks)
   - No deprecation warnings in log

### Expected Behavior

**Phase Timing**:
- Windup: Should start immediately when attack begins
- Active: Should start at first transition notify
- Recovery: Should start at second transition notify
- End: Should happen when montage completes

**Hit Detection**:
- Should enable automatically when Active phase begins
- Should disable automatically when Active phase ends
- No manual enable/disable needed

**Combo System**:
- Should work identically to before
- Combo window timing unchanged

---

## Automated Migration (Optional)

For projects with many montages, consider using the automated migration tools:

### Option 1: Blueprint Editor Utility Widget
1. Open `Content/EditorUtilities/EUW_PhaseNotifyMigrator`
2. Select montages to migrate
3. Click "Migrate to Event System"
4. Review changes before saving

### Option 2: Command-Line Tool
```bash
UnrealEditor-Cmd.exe "KatanaCombat.uproject" -run=MigratePhaseNotifies -montages="Content/Animations/Attacks/*"
```

### Option 3: Python Script (if available)
```python
# Run in Unreal Editor Python console
import unreal
unreal.EditorUtilityLibrary.run_editor_utility("/Game/EditorUtilities/MigratePhaseNotifies")
```

---

## Rollback (If Needed)

If you need to revert to the old system:

1. **Keep both old and new notifies** temporarily during transition
2. **Test thoroughly** with new system first
3. **Only delete old notifies** after confirming new system works
4. **If issues arise**, old notifies still function (with deprecation warnings)

The old system is **deprecated but not removed** - it will continue to work with warning logs.

---

## Benefits of New System

### Simplicity
- **6 notifies → 2 notifies** per attack
- **No manual hit detection toggles**
- **No gaps between phases**

### Correctness
- **Impossible to have phase gaps** (implicit boundaries)
- **No desync between hit detection and Active phase**
- **Clear visual timeline** in animation editor

### Maintainability
- **Fewer things to configure** per attack
- **Fewer opportunities for error**
- **Easier to teach new team members**

### Performance
- Minimal (phase transitions are infrequent)
- Automatic hit detection is same cost as manual toggles

---

## FAQ

### Q: Do I need to migrate immediately?
**A**: No. The old system is deprecated but still functional. Migrate when convenient.

### Q: What about existing attacks in production?
**A**: They'll work with deprecation warnings logged once per session. Migrate during your next content update.

### Q: Can I use both systems in the same project?
**A**: Yes, during transition. However, don't mix within a single montage - use one system per attack.

### Q: What if I have custom phase logic?
**A**: The phase transition events route to `ICombatInterface::OnAttackPhaseTransition()`. Hook into this for custom behavior.

### Q: How do I migrate attacks with complex phase timing?
**A**: Place the transition notifies at the exact frame where the phase should change. The Active phase determines hit detection window.

### Q: What about finishers, counters, or special attacks?
**A**: Same migration applies. Phases are universal - only transitions matter.

---

## Support

If you encounter issues during migration:

1. **Check the log** for deprecation warnings and phase transition events
2. **Enable debug draw** to visualize phase timing
3. **Refer to TROUBLESHOOTING.md** for common issues
4. **Review ARCHITECTURE.md** for phase system details
5. **Check example montages** in `Content/ProjectFiles/Core/Animation/` (already migrated)

---

## Summary Checklist

Before considering migration complete:

- [ ] Read this guide completely
- [ ] Understand phases vs windows distinction
- [ ] Migrated all attack montages
- [ ] Deleted old AnimNotifyState_AttackPhase instances
- [ ] Deleted old AnimNotify_ToggleHitDetection instances
- [ ] Kept all window notifies (Combo, Hold, Parry, etc.)
- [ ] Added exactly 2 phase transitions per attack
- [ ] Tested attacks in-game with debug draw enabled
- [ ] Verified hit detection timing matches Active phase
- [ ] Confirmed no deprecation warnings in log
- [ ] Updated any custom animation authoring documentation

---

**Migration Status**: Once all montages are migrated, the old notify classes can be safely removed from the codebase in a future update.
