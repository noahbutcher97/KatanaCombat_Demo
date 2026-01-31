# Validation Warning Quick Fix Guide

**Problem:** Getting excessive validation warnings when saving AttackData or adding CombatWarp notifies?

**Status:** ✅ FIXED in this PR

---

## AttackData: "Invalid dataasset" or "Circular reference" Warnings

### What Was Happening

You'd save one AttackData and get the same error repeated multiple times:
```
[Error] DA_LightAttack_1: Circular reference detected in combo chain!
[Error] DA_LightAttack_2: Circular reference detected in combo chain!
[Error] DA_LightAttack_3: Circular reference detected in combo chain!
```

### What's Fixed

Now you'll only see ONE error for the actual asset that creates the cycle:
```
[Error] DA_LightAttack_3: Circular reference detected! This attack is part of a combo cycle.
        Review NextComboAttack, HeavyComboAttack, and DirectionalFollowUps to break the cycle.
```

### How to Fix Circular References

If you see a circular reference error:

1. Open the asset mentioned in the error
2. Check these properties:
   - `NextComboAttack`
   - `HeavyComboAttack`
   - `DirectionalFollowUps`
   - `HeavyDirectionalFollowUps`
3. Look for a chain that loops back to itself
4. Example bad chain: A → B → C → A (loops!)
5. Fix by breaking one reference (e.g., set C's NextComboAttack to None or a different attack)

**Note:** Valid branching is allowed! A→C and B→C is perfectly fine.

---

## AnimNotifyState_CombatWarp: "Warp target name is not set" Warning

### What Was Happening

Adding the CombatWarp notify to your montages would produce:
```
[Warning] WarpTargetName is not set for Combat Warp notify
```

Even though the notify works perfectly fine at runtime!

### What's Fixed

The warning is now suppressed. The notify validates properly that you have valid target names configured.

### How to Use CombatWarp (Reminder)

1. **Add to Montage:**
   - Open your attack montage
   - Add `AnimNotifyState_CombatWarp` notify
   
2. **Configure (optional - defaults work fine):**
   - `TargetWarpName`: Name for translation+rotation warp (default: "AttackTarget")
   - `RotationWarpName`: Name for rotation-only warp (default: "RotationTarget")
   
3. **How It Works:**
   - At runtime, `CombatComponent::SetupAttackWarp()` sets up ONE target
   - If enemy nearby: Uses TargetWarpName (moves toward enemy)
   - If no enemy: Uses RotationWarpName (just rotates)
   - The notify automatically detects which exists and configures itself

**No warnings should appear now!**

---

## Validation Best Practices

### AttackData

✅ **DO:**
- Use branching combos (A→C, B→C)
- Chain combos linearly (A→B→C→D)
- Leave NextComboAttack as None for terminal attacks

❌ **DON'T:**
- Create cycles (A→B→C→A)
- Self-reference (A.NextComboAttack = A)
- Create indirect cycles through directional follow-ups

### CombatWarp Notify

✅ **DO:**
- Keep default names unless you have a specific need
- Use one notify per montage (replaces two MotionWarping notifies)
- Place during the movement phase of your attack

❌ **DON'T:**
- Set both TargetWarpName and RotationWarpName to None (validation will set defaults)
- Mix with regular MotionWarping notifies (pick one approach)

---

## Testing Your Combos

### Quick Validation Test

1. Open your AttackData asset
2. Click "Validate" button in the details panel
3. Should see: ✓ "Montage section is valid!"

### If You Still See Errors

**Circular Reference Error:**
- Follow the asset name in the error message
- Check that asset's combo references
- Break the cycle

**Missing Montage/Section:**
- Assign an AttackMontage
- Select a valid MontageSection (or leave as None for full montage)

**Missing Notifies (if using AnimNotify timing):**
- Click "Generate AnimNotifies" button
- Or add `AnimNotifyState_AttackPhase` notifies manually

---

## Need Help?

If you encounter validation warnings not covered here:

1. Read the full error message - it should be more specific now
2. Check the VALIDATION_AUDIT_REPORT.md for detailed explanations
3. Look at the test cases in the audit report
4. File an issue with the specific warning message

---

**Last Updated:** 2026-01-31  
**Fix Version:** Current PR
