# Combo Validation & Motion Warp Audit Report

**Date:** 2026-01-31
**Auditor:** Claude Code
**Branch:** `claude/audit-combo-validation-58IST`

---

## Executive Summary

This audit investigates three reported issues:
1. "Invalid DataAsset" warnings flooding the editor on save
2. "Warp target name not set" warnings when saving montages with `AnimNotifyState_CombatWarp`
3. General audit of the circular combo dependency detection system

All three issues have been traced to their root causes with solutions provided below.

---

## Issue 1: "Invalid DataAsset" Warnings on Save

### Symptom
Massive volume of "Invalid DataAsset" warnings appear whenever attack data assets are saved in the Unreal Editor.

### Root Cause Trace

**Entry Point:** Unreal's Asset Validation Framework automatically calls `IsDataValid()` on data assets during save operations.

**Validation Chain:**
```
Asset Save Triggered
    │
    ▼
UAttackData::IsDataValid() ─────────────────► AttackData.cpp:289-313
    │
    ├── DetectCycles()                       ─► AttackData.cpp:315-378
    │   └── Reports: "Circular reference detected in combo chain!"
    │
    ├── ValidateDirectionalFollowUps()       ─► AttackData.cpp:380-416
    │   └── Reports: "Has 'Attack.Capability.CanDirectional' tag but no DirectionalFollowUps"
    │   └── Reports: "Has DirectionalFollowUps but missing 'Attack.Capability.CanDirectional' tag"
    │
    └── ValidateTerminalTag()                ─► AttackData.cpp:418-466
        └── Reports: "Has 'Attack.Capability.Terminal' tag but NextComboAttack is set"
        └── Reports: "Has 'Attack.Capability.Terminal' tag but HeavyComboAttack is set"
        └── Reports: "Has 'Attack.Capability.Terminal' tag but DirectionalFollowUps are set"
```

**Key Code Location:** `Source/KatanaCombat/Private/Data/AttackData.cpp:289-313`

```cpp
EDataValidationResult UAttackData::IsDataValid(FDataValidationContext& Context) const
{
    EDataValidationResult Result = EDataValidationResult::Valid;
    TArray<FText> ValidationErrors;

    // Run all validation checks
    TSet<const UAttackData*> Visited;
    const bool bHasCycles = DetectCycles(Visited, ValidationErrors);           // Line 296
    const bool bDirectionalValid = ValidateDirectionalFollowUps(ValidationErrors);  // Line 297
    const bool bTerminalValid = ValidateTerminalTag(ValidationErrors);         // Line 298

    // Report all accumulated errors
    for (const FText& Error : ValidationErrors)
    {
        Context.AddError(Error);  // ◄── THIS produces the warnings
    }
    ...
}
```

### Common Causes of Validation Failures

| Validation | Error Trigger | Fix |
|------------|---------------|-----|
| `DetectCycles` | Attack A → Attack B → Attack A | Break the cycle in combo chain |
| `ValidateDirectionalFollowUps` | Has `Attack.Capability.CanDirectional` tag but empty `DirectionalFollowUps` map | Add directional attacks OR remove tag |
| `ValidateDirectionalFollowUps` | Has entries in `DirectionalFollowUps` but missing tag | Add `Attack.Capability.CanDirectional` tag |
| `ValidateTerminalTag` | Has `Attack.Capability.Terminal` tag but `NextComboAttack` is set | Remove follow-up OR remove Terminal tag |

### Solution Recommendations

1. **Batch Fix Tool:** Run the editor's batch validation to identify all problematic assets:
   ```cpp
   // In AttackDataTools.cpp:691-708
   UAttackDataTools::BatchValidate(AllAttacks, ValidAssets, InvalidAssets);
   ```

2. **Per-Asset Fix:** Open each flagged AttackData asset and either:
   - Remove contradictory tags (Terminal vs having follow-ups)
   - Add missing tags (CanDirectional when directional follow-ups exist)
   - Break circular references in combo chains

3. **Systematic Approach:**
   - Export all `UAttackData` assets to a spreadsheet
   - Cross-reference `AttackTags` with actual combo chain configuration
   - Ensure consistency between tags and linked attacks

---

## Issue 2: "Warp Target Name Not Set" Warning on Montage Save

### Symptom
When saving any montage containing `AnimNotifyState_CombatWarp`, a warning appears stating the warp target name is not set.

### Root Cause Trace

**The warning does NOT come from project code.** It originates from **Unreal Engine's built-in MotionWarping plugin**.

**Inheritance Chain:**
```
UAnimNotifyState_CombatWarp (Project Code)
    │
    ▼ inherits from
UAnimNotifyState_MotionWarping (Unreal Engine Plugin)
    │
    ▼ has property
RootMotionModifier::WarpTargetName  ◄── Validated by engine
```

**Why This Happens:**

The `UAnimNotifyState_CombatWarp` class (at `AnimNotifyState_CombatWarp.h:34-87`) is designed to **dynamically** choose between two warp targets at runtime:

```cpp
// AnimNotifyState_CombatWarp.h:50-58
UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat Warp")
FName TargetWarpName = "AttackTarget";      // Used when enemy exists

UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat Warp")
FName RotationWarpName = "RotationTarget";  // Used when no enemy
```

**The Problem:** The parent class `UAnimNotifyState_MotionWarping` performs editor-time validation on its `RootMotionModifier` template's `WarpTargetName` property. Since the Combat Warp system sets this dynamically at runtime (in `AddRootMotionModifier_Implementation`), the editor doesn't know which name will be used and validates against the **default/unset** value.

**Key Runtime Code:** `AnimNotifyState_CombatWarp.cpp:39-77`
```cpp
// Determine which target exists and configure accordingly
FName ActiveTargetName = NAME_None;

if (HasWarpTarget(MotionWarpingComp, TargetWarpName))
{
    ActiveTargetName = TargetWarpName;          // Set at RUNTIME
    ...
}
else if (HasWarpTarget(MotionWarpingComp, RotationWarpName))
{
    ActiveTargetName = RotationWarpName;        // Set at RUNTIME
    ...
}

// Temporarily modify the template's properties for this warp
WarpModifierTemplate->WarpTargetName = ActiveTargetName;  // Line 76
```

### Solution Recommendations

**Option A: Override Parent Validation (Recommended)**

Add a custom `IsDataValid()` override to `UAnimNotifyState_CombatWarp` that bypasses the parent's WarpTargetName check:

```cpp
// Add to AnimNotifyState_CombatWarp.h
#if WITH_EDITOR
virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif

// Add to AnimNotifyState_CombatWarp.cpp
#if WITH_EDITOR
#include "Misc/DataValidation.h"

EDataValidationResult UAnimNotifyState_CombatWarp::IsDataValid(FDataValidationContext& Context) const
{
    EDataValidationResult Result = EDataValidationResult::Valid;

    // Validate our own properties instead of parent's WarpTargetName
    if (TargetWarpName.IsNone() && RotationWarpName.IsNone())
    {
        Context.AddError(FText::FromString(
            TEXT("Combat Warp: Both TargetWarpName and RotationWarpName are None. At least one must be set.")));
        Result = EDataValidationResult::Invalid;
    }

    // Skip parent validation (which checks RootMotionModifier->WarpTargetName)
    // Go directly to grandparent
    return CombineDataValidationResults(Result, UAnimNotifyState::IsDataValid(Context));
}
#endif
```

**Option B: Pre-populate WarpTargetName**

Set a default value in the constructor to suppress the warning (less elegant):

```cpp
UAnimNotifyState_CombatWarp::UAnimNotifyState_CombatWarp(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    // Pre-populate to suppress validation warning
    if (RootMotionModifier)
    {
        if (URootMotionModifier_Warp* WarpMod = Cast<URootMotionModifier_Warp>(RootMotionModifier))
        {
            WarpMod->WarpTargetName = FName("AttackTarget");  // Default
        }
    }
}
```

**Option C: Suppress via Editor Settings**

Disable asset validation for AnimNotifyState assets in Project Settings (not recommended as it hides legitimate issues).

---

## Issue 3: Circular Combo Dependency Detection Audit

### Algorithm Analysis

**Location:** `Source/KatanaCombat/Private/Data/AttackData.cpp:315-378`

**Algorithm:** Depth-First Search (DFS) with visited set tracking

```cpp
bool UAttackData::DetectCycles(TSet<const UAttackData*>& Visited, TArray<FText>& Errors) const
{
    // Check if we've already visited this attack (cycle detected!)
    if (Visited.Contains(this))
    {
        Errors.Add(...);
        return true;
    }

    // Add this attack to visited set
    Visited.Add(this);

    bool bFoundCycle = false;

    // Check all combo links
    if (NextComboAttack)
        if (NextComboAttack->DetectCycles(Visited, Errors))
            bFoundCycle = true;

    if (HeavyComboAttack)
        if (HeavyComboAttack->DetectCycles(Visited, Errors))
            bFoundCycle = true;

    for (const auto& Pair : DirectionalFollowUps)
        if (Pair.Value && Pair.Value->DetectCycles(Visited, Errors))
            bFoundCycle = true;

    for (const auto& Pair : HeavyDirectionalFollowUps)
        if (Pair.Value && Pair.Value->DetectCycles(Visited, Errors))
            bFoundCycle = true;

    // Remove from visited set (allow branching paths)  ◄── CRITICAL LINE
    Visited.Remove(this);

    return bFoundCycle;
}
```

### Correctness Assessment

**The algorithm is CORRECT for its intended purpose.**

The `Visited.Remove(this)` on line 375 is intentional and correct. Here's why:

**Without removal (incorrect):** Would flag legal diamond patterns as cycles
```
    A
   / \
  B   C
   \ /
    D    ◄── Would be flagged as "already visited" when reached via C after B
```

**With removal (correct):** Only detects actual back-edges (true cycles)
```
    A ────┐
    │     │
    B     │
    │     │
    C ────┘    ◄── Correctly detected: C → A forms a cycle
```

### Edge Cases Handled

| Scenario | Detection | Result |
|----------|-----------|--------|
| Direct self-reference (A→A) | Immediate | ✓ Detected |
| Simple cycle (A→B→A) | 2 steps | ✓ Detected |
| Diamond pattern (A→B→D, A→C→D) | No cycle | ✓ Allowed |
| Complex cycle (A→B→C→A) | 3 steps | ✓ Detected |
| Multiple cycles | Per-branch | ✓ All detected |

### Potential Enhancement

The current algorithm doesn't report the **path** of the cycle, only that one exists. For better debugging:

```cpp
// Enhanced version (suggested)
bool UAttackData::DetectCycles(
    TSet<const UAttackData*>& Visited,
    TArray<const UAttackData*>& Path,  // NEW: Track path for error reporting
    TArray<FText>& Errors) const
{
    if (Visited.Contains(this))
    {
        // Build path string for better error message
        FString PathStr;
        for (const UAttackData* Attack : Path)
            PathStr += Attack->GetName() + TEXT(" → ");
        PathStr += GetName();

        Errors.Add(FText::FromString(FString::Printf(
            TEXT("Circular combo chain: %s"), *PathStr)));
        return true;
    }

    Visited.Add(this);
    Path.Add(this);  // Track path

    // ... check children ...

    Path.Pop();  // Remove from path
    Visited.Remove(this);
    return bFoundCycle;
}
```

---

## Additional Issues Discovered

### 1. Deprecated AnimNotifyState_AttackPhase Still in Use

**Location:** `AttackData.cpp:119-131`

```cpp
// DEPRECATED: Also check for old AnimNotifyState_AttackPhase for backward compatibility
if (const UAnimNotifyState_AttackPhase* PhaseNotify = ...)
{
    static bool bDeprecationWarningLogged = false;
    if (!bDeprecationWarningLogged)
    {
        UE_LOG(LogTemp, Warning, TEXT("[AttackData] Found deprecated AnimNotifyState_AttackPhase..."));
        bDeprecationWarningLogged = true;
    }
    return true; // Old system found - consider valid for now
}
```

**Issue:** The deprecation warning only logs once per session (static bool), so users may not realize they have deprecated notifies in their montages.

**Recommendation:** Add editor-time validation to flag assets still using the deprecated system.

### 2. Missing Warp Target Name Editor Validation

**Location:** No validation exists for checking if configured warp target names match actual montage notifies.

**Issue:** If `WarpConfig.TargetWarpName` doesn't match any motion warping notify in the montage, warping silently fails at runtime.

**Recommendation:** Add validation in `UAttackData::IsDataValid()`:
```cpp
// Check WarpConfig target names exist in montage notifies
if (AttackMontage && WarpConfig.bEnableWarp)
{
    // Search montage for matching MotionWarping notifies
    bool bFoundTargetWarp = false;
    for (const FAnimNotifyEvent& NotifyEvent : AttackMontage->Notifies)
    {
        if (auto* WarpNotify = Cast<UAnimNotifyState_MotionWarping>(NotifyEvent.NotifyStateClass))
        {
            // Check if notify's target name matches config
            // ...
        }
    }
}
```

---

## Summary of Fixes

| Issue | Priority | Fix Type | Effort |
|-------|----------|----------|--------|
| Invalid DataAsset warnings | High | Data fix + tag consistency | Medium |
| Warp target name warning | High | Code change (override validation) | Low |
| Cycle detection path reporting | Low | Code enhancement | Low |
| Deprecated notify detection | Medium | Code change (add validation) | Low |
| Warp target name mismatch | Medium | Code change (add validation) | Medium |

---

## Files Modified by This Audit

None - this is a read-only audit report.

## Files That Require Modification

1. `Source/KatanaCombat/Public/Animation/AnimNotifyState_CombatWarp.h` - Add `IsDataValid()` override declaration
2. `Source/KatanaCombat/Private/Animation/AnimNotifyState_CombatWarp.cpp` - Add `IsDataValid()` implementation
3. (Optional) `Source/KatanaCombat/Private/Data/AttackData.cpp` - Enhance cycle path reporting
4. (Optional) `Source/KatanaCombat/Private/Data/AttackData.cpp` - Add warp target name validation
