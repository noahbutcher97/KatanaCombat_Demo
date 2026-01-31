# Validation Patterns Guide - Similar Issues Across the Codebase

**Date:** 2026-01-31  
**Related PR:** copilot/audit-attack-combo-validation  
**Purpose:** Identify and document similar validation issues across all custom editor tools and data assets

---

## Executive Summary

After auditing the validation system that fixed **AttackData** cascading errors and **AnimNotifyState_CombatWarp** false warnings, we identified similar patterns and potential issues across other data assets and custom classes in the project.

### Assets with Validation Logic

The project contains **4 primary data asset types** with validation:

1. ✅ **AttackData** - FIXED (cascading error issue resolved)
2. ⚠️ **HitReactionData** - Similar pattern, potential for same issue
3. ⚠️ **PairedAnimationData** - Simpler validation, lower risk
4. ✅ **WeaponData** - No validation issues (only PostEditChangeProperty)

### AnimNotify Classes

The project has **multiple AnimNotify classes** - none show the same validation issue as CombatWarp because they don't extend classes with parent validation conflicts.

---

## Detailed Analysis by Asset Type

### 1. HitReactionData - Similar Risk to AttackData ⚠️

**Location:** `Source/KatanaCombat/Private/Data/HitReactionData.cpp` (lines 93-158)

#### Current Validation Logic

```cpp
EDataValidationResult UHitReactionData::IsDataValid(FDataValidationContext& Context) const
{
    EDataValidationResult Result = EDataValidationResult::Valid;

    // Validate montage exists
    if (!ReactionMontage)
    {
        Context.AddError(FText::FromString(FString::Printf(
            TEXT("%s: No ReactionMontage assigned"), *GetName())));
        Result = EDataValidationResult::Invalid;
    }

    // Validate section exists if specified
    if (ReactionMontage && MontageSection != NAME_None)
    {
        const int32 SectionIndex = ReactionMontage->GetSectionIndex(MontageSection);
        if (SectionIndex == INDEX_NONE)
        {
            Context.AddError(FText::FromString(FString::Printf(
                TEXT("%s: MontageSection '%s' not found in montage '%s'"),
                *GetName(), *MontageSection.ToString(), *ReactionMontage->GetName())));
            Result = EDataValidationResult::Invalid;
        }
    }

    // ... more validation ...

    return CombineDataValidationResults(Result, Super::IsDataValid(Context));
}
```

#### Assessment

**✅ GOOD:** All error messages include `*GetName()` prefix, so errors are properly attributed to the asset.

**⚠️ POTENTIAL ISSUE:** If HitReactionData assets reference each other through `PairedReactionData` or similar properties (if such references exist), the same cascading error pattern from AttackData could occur.

#### Current Status

**NO IMMEDIATE ACTION REQUIRED** - HitReactionData does not appear to have recursive references like AttackData's combo chains. However, if future features add reactions that reference other reactions, apply the same filtering pattern as AttackData.

#### Monitoring Recommendation

Watch for:
- Addition of "NextReaction" or similar chaining properties
- User reports of duplicate validation errors on HitReactionData
- Any recursive traversal added to validation

---

### 2. PairedAnimationData - Low Risk ✅

**Location:** `Source/KatanaCombat/Private/Data/PairedAnimationData.cpp` (lines 68-131)

#### Current Validation Logic

```cpp
EDataValidationResult UPairedAnimationData::IsDataValid(FDataValidationContext& Context) const
{
    EDataValidationResult Result = Super::IsDataValid(Context);

    // Check attacker montage
    if (!AttackerMontage)
    {
        Context.AddError(FText::FromString(TEXT("AttackerMontage is required")));
        Result = EDataValidationResult::Invalid;
    }

    // Check victim montage
    if (!VictimMontage)
    {
        Context.AddError(FText::FromString(TEXT("VictimMontage is required for paired animations")));
        Result = EDataValidationResult::Invalid;
    }

    // ... more validation ...

    return Result;
}
```

#### Assessment

**⚠️ ISSUE FOUND:** Error messages **DO NOT** include asset name prefix!

**Impact:** When validation errors occur, users won't immediately know which PairedAnimationData asset has the problem if multiple are validated in a batch.

**Risk Level:** LOW - PairedAnimationData doesn't have recursive references, so no cascading errors. Only an issue with error message clarity.

#### Recommended Fix

Add asset name to error messages for consistency:

```cpp
// BEFORE
Context.AddError(FText::FromString(TEXT("AttackerMontage is required")));

// AFTER
Context.AddError(FText::FromString(FString::Printf(
    TEXT("%s: AttackerMontage is required"), *GetName())));
```

#### Action Items

- [ ] Update all error/warning messages in `PairedAnimationData::IsDataValid()` to include asset name
- [ ] Follow the pattern established in `HitReactionData` and fixed `AttackData`
- [ ] Ensures consistency across all data asset validation

---

### 3. WeaponData - No Issues ✅

**Location:** `Source/KatanaCombat/Private/Data/WeaponData.cpp` (lines 32-44)

#### Current Implementation

```cpp
void UWeaponData::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
    Super::PostEditChangeProperty(PropertyChangedEvent);

    // Validate damage multiplier
    DamageMultiplier = FMath::Clamp(DamageMultiplier, 0.1f, 5.0f);

    // Validate trace radius
    TraceRadius = FMath::Clamp(TraceRadius, 1.0f, 50.0f);

    // Validate weapon reach
    WeaponReach = FMath::Clamp(WeaponReach, 50.0f, 500.0f);
}
```

#### Assessment

**✅ NO ISSUES:**
- Uses `PostEditChangeProperty` for validation (auto-corrects values)
- No `IsDataValid` implementation = no validation errors reported
- No recursive references to other WeaponData assets
- Auto-clamping prevents invalid values silently

#### Best Practice

This is actually a good pattern for certain types of validation - instead of reporting errors, automatically correct values to valid ranges. Users never see errors because invalid inputs are immediately fixed.

**When to use this pattern:**
- Numeric ranges that can be auto-corrected
- Boolean flags with clear fallbacks
- Properties where "fixing" is unambiguous

**When NOT to use this pattern:**
- Required object references (can't auto-fix null)
- Complex relationships (can't auto-resolve)
- Ambiguous corrections (multiple valid fixes)

---

## AnimNotify Validation Patterns

### Classes Checked

All AnimNotify classes in the project implement `CanBePlaced()` but **none** have the same validation issue as `AnimNotifyState_CombatWarp` because:

1. They don't extend classes with built-in editor validation
2. They don't dynamically set parent properties at runtime
3. They use simple boolean `CanBePlaced()` checks, not complex validation

### Example Pattern (Common Across All)

```cpp
virtual bool CanBePlaced(UAnimSequenceBase* Animation) const override 
{ 
    return true; 
}
```

**Assessment:** ✅ NO ISSUES - Simple, straightforward, no conflicts.

---

## Validation Best Practices Summary

Based on the audit, here are the established patterns for the project:

### 1. Error Message Format ✅

**ALWAYS** include asset name in validation messages:

```cpp
// GOOD
Context.AddError(FText::FromString(FString::Printf(
    TEXT("%s: Error description here"), *GetName())));

// BAD
Context.AddError(FText::FromString(TEXT("Error description here")));
```

**Why:** Makes it immediately clear which asset has the problem, especially in batch validation.

---

### 2. Recursive Validation ⚠️

**IF** your asset references other assets of the same type:

```cpp
EDataValidationResult UMyAsset::IsDataValid(FDataValidationContext& Context) const
{
    TArray<FText> ValidationErrors;
    
    // Do recursive validation...
    ValidateChildren(Visited, ValidationErrors);
    
    // CRITICAL: Filter errors to only report THIS asset's issues
    const FString ThisAssetName = GetName();
    for (const FText& Error : ValidationErrors)
    {
        if (Error.ToString().StartsWith(ThisAssetName + TEXT(":")))
        {
            Context.AddError(Error);
        }
    }
}
```

**Why:** Prevents cascading duplicate errors through reference chains.

**Applies to:**
- AttackData (combo chains) ✅ FIXED
- Any future asset with recursive references

---

### 3. Parent Class Validation Conflicts ⚠️

**IF** your class extends a parent with editor validation:

```cpp
#if WITH_EDITOR
void UMyNotify::ValidateAssociatedAssets()
{
    // Implement custom validation
    // DON'T call Super::ValidateAssociatedAssets() if it conflicts
}
#endif
```

**Why:** Parent validation may not understand your dynamic runtime behavior.

**Applies to:**
- AnimNotifyState_CombatWarp ✅ FIXED
- Any future notify extending MotionWarping or similar

---

### 4. Auto-Correction Pattern ✅

**FOR** simple numeric ranges:

```cpp
void UMyAsset::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
    Super::PostEditChangeProperty(PropertyChangedEvent);
    
    // Auto-correct to valid range
    MyValue = FMath::Clamp(MyValue, MinValue, MaxValue);
}
```

**Why:** Silent fixes are better UX than error messages for obvious corrections.

**Applies to:**
- WeaponData ✅ Already uses this
- Any numeric property with clear valid range

---

## Recommended Actions

### Immediate (Before Next Release)

1. **PairedAnimationData Error Messages** 🔴 HIGH PRIORITY
   - Add asset name prefix to all error/warning messages
   - Follows established pattern from AttackData/HitReactionData
   - Low risk, high consistency benefit

### Future Monitoring

2. **HitReactionData Recursive References** 🟡 MEDIUM PRIORITY
   - Watch for addition of reaction chaining features
   - If added, apply AttackData filtering pattern preemptively
   - Currently no issue, but pattern could emerge

3. **New AnimNotify Classes** 🟢 LOW PRIORITY
   - When creating new notifies extending parent classes with validation
   - Check for validation conflicts like CombatWarp
   - Document parent validation behavior

### Documentation

4. **Update Architecture Docs** 🟢 LOW PRIORITY
   - Add validation patterns section to ARCHITECTURE.md
   - Link to this guide from troubleshooting
   - Include in onboarding docs for new developers

---

## Testing Validation Changes

When modifying validation logic, test:

### 1. Single Asset Save
```
- Open one asset
- Make it invalid (e.g., remove required montage)
- Save
- Should see ONE error for THIS asset
```

### 2. Batch Validation
```
- Run "Validate Assets" on Content folder
- Check that each invalid asset reports its own errors
- No duplicate errors across assets
```

### 3. Valid Asset
```
- Configure asset correctly
- Save
- Should see NO errors
```

### 4. Cascading References (if applicable)
```
- Create chain: A→B→C with C invalid
- Save A
- Should see error ONLY about C, not repeated for A and B
```

---

## Code Review Checklist

When reviewing validation changes:

- [ ] Error messages include asset name (`*GetName()`)
- [ ] Recursive validation filters errors by asset name
- [ ] Parent class validation conflicts are overridden
- [ ] Auto-correction used where appropriate (numeric ranges)
- [ ] No hardcoded asset names in error messages
- [ ] Warnings vs Errors used appropriately
- [ ] Validation doesn't break on null references
- [ ] Tests cover both valid and invalid cases

---

## Related Issues & PRs

- **Original Issue:** "Invalid dataasset" warnings and CombatWarp warnings
- **Fixed PR:** copilot/audit-attack-combo-validation
- **Fixed Assets:** AttackData, AnimNotifyState_CombatWarp
- **This Document:** Guidance for similar patterns project-wide

---

## Appendix: Quick Reference

### Assets with IsDataValid()

| Asset | File | Status | Notes |
|-------|------|--------|-------|
| AttackData | AttackData.cpp | ✅ Fixed | Cascading errors resolved |
| HitReactionData | HitReactionData.cpp | ✅ Good | Proper error formatting |
| PairedAnimationData | PairedAnimationData.cpp | ⚠️ Needs Update | Missing asset names in errors |
| WeaponData | WeaponData.cpp | ✅ Good | No IsDataValid (uses auto-correct) |

### AnimNotify Classes

All AnimNotify classes use simple `CanBePlaced()` pattern. Only CombatWarp needed special handling due to parent class conflict (✅ Fixed).

---

**Last Updated:** 2026-01-31  
**Maintained By:** Development Team  
**Review Frequency:** Quarterly or when adding new data asset types
