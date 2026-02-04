# Plan: Hit Reaction Variations with N-2 Randomization

## Overview

Add animation variety to hit reactions by supporting multiple montages per direction and implementing n-2 randomization to prevent flip-flopping (the same 1-2 animations repeating).

**Scope:**
- Extend `FHitReactionEntry` to support array of montages
- Implement n-2 selection: exclude last 2 played, random from remaining
- Track reaction history per direction in HitReactionComponent
- Maintain backwards compatibility with single-montage configs

---

## Selection Algorithm

| Array Size | Behavior |
|------------|----------|
| **1 montage** | Always play it (no exclusion, no history) |
| **2 montages** | Alternate back and forth (exclude only last 1) |
| **3+ montages** | N-2 pattern (exclude last 2, random from remaining) |

---

## Current Architecture

```
FHitReactionEntry
  └── UAnimMontage* ReactionMontage  // SINGLE montage

FDirectionalReactionSet
  ├── FHitReactionEntry Front
  ├── FHitReactionEntry Back
  ├── FHitReactionEntry Left
  └── FHitReactionEntry Right

HitReactionSettings
  └── TMap<EHitIntensity, FDirectionalReactionSet> DirectionalReactions

Selection Flow:
  PlayHitReaction() → GetDirectionalReaction() → PlayReactionFromEntry()
  (deterministic - always same animation for direction+intensity)
```

---

## Files to Modify

| File | Changes |
|------|---------|
| `Public/CombatTypes.h` | Add `ReactionMontages` array to `FHitReactionEntry`, add `FReactionHistory` struct |
| `Public/Core/HitReactionComponent.h` | Add history map, add `SelectMontageWithVariety()` |
| `Private/Core/HitReactionComponent.cpp` | Implement variety selection, update `PlayReactionFromEntry()` |

---

## Implementation Phases

### Phase 1: Add FReactionHistory Struct

**File: `Public/CombatTypes.h`**

Add after `FHitReactionEntry`:

```cpp
/**
 * History of played reaction indices for n-2 randomization
 * Tracks last N played montages to exclude from selection
 */
USTRUCT()
struct FReactionHistory
{
    GENERATED_BODY()

    /** Recently played montage indices (most recent at end) */
    TArray<int32> RecentIndices;

    /** Maximum history entries to keep */
    static constexpr int32 MaxHistory = 2;

    /** Record that a montage index was played */
    void RecordPlayed(int32 Index)
    {
        RecentIndices.Add(Index);
        while (RecentIndices.Num() > MaxHistory)
        {
            RecentIndices.RemoveAt(0);
        }
    }

    /** Get the most recently played index (-1 if none) */
    int32 GetLastPlayed() const
    {
        return RecentIndices.Num() > 0 ? RecentIndices.Last() : -1;
    }

    /** Clear history (e.g., on death/respawn) */
    void Clear() { RecentIndices.Empty(); }
};
```

### Phase 2: Extend FHitReactionEntry

**File: `Public/CombatTypes.h`**

Add array field to `FHitReactionEntry` Animation section:

```cpp
/** Array of montages for variety (use this for multiple animations) */
UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animation")
TArray<TObjectPtr<UAnimMontage>> ReactionMontages;

/** Get all available montages (combines single + array for compatibility) */
TArray<UAnimMontage*> GetAllMontages() const
{
    TArray<UAnimMontage*> Result;

    // Add array montages first (preferred)
    for (UAnimMontage* Montage : ReactionMontages)
    {
        if (Montage)
        {
            Result.Add(Montage);
        }
    }

    // Add single montage if array is empty (backwards compat)
    if (Result.Num() == 0 && ReactionMontage)
    {
        Result.Add(ReactionMontage);
    }

    return Result;
}

/** Get count of available montages */
int32 GetMontageCount() const
{
    int32 Count = 0;
    for (UAnimMontage* Montage : ReactionMontages)
    {
        if (Montage) Count++;
    }
    if (Count == 0 && ReactionMontage)
    {
        Count = 1;
    }
    return Count;
}
```

### Phase 3: Add History Tracking to HitReactionComponent

**File: `Public/Core/HitReactionComponent.h`**

Add in private section:

```cpp
// ============================================================================
// REACTION VARIETY (n-2 randomization)
// ============================================================================

/** History of played reactions: Intensity → Direction → History */
TMap<EHitIntensity, TMap<EAttackDirection, FReactionHistory>> ReactionHistoryMap;

/**
 * Select montage with variety (n-2 for 3+, alternation for 2, direct for 1)
 */
UAnimMontage* SelectMontageWithVariety(
    const FHitReactionEntry& Entry,
    EHitIntensity Intensity,
    EAttackDirection Direction);

/** Record that a montage was played */
void RecordMontagePlay(int32 MontageIndex, EHitIntensity Intensity, EAttackDirection Direction);
```

Add in public section:

```cpp
/** Clear reaction history (call on death/respawn) */
UFUNCTION(BlueprintCallable, Category = "Hit Reaction")
void ClearReactionHistory();
```

### Phase 4: Implement Variety Selection

**File: `Private/Core/HitReactionComponent.cpp`**

```cpp
UAnimMontage* UHitReactionComponent::SelectMontageWithVariety(
    const FHitReactionEntry& Entry,
    EHitIntensity Intensity,
    EAttackDirection Direction)
{
    TArray<UAnimMontage*> AllMontages = Entry.GetAllMontages();

    if (AllMontages.Num() == 0)
    {
        return nullptr;
    }

    // === 1 MONTAGE: Always play it ===
    if (AllMontages.Num() == 1)
    {
        // No history tracking needed - always play the only option
        return AllMontages[0];
    }

    // Get history for this intensity+direction
    FReactionHistory& History = ReactionHistoryMap.FindOrAdd(Intensity).FindOrAdd(Direction);

    // === 2 MONTAGES: Simple alternation ===
    if (AllMontages.Num() == 2)
    {
        const int32 LastPlayed = History.GetLastPlayed();
        // Play the other one (or 0 if no history)
        const int32 SelectedIndex = (LastPlayed == 0) ? 1 : 0;
        RecordMontagePlay(SelectedIndex, Intensity, Direction);
        return AllMontages[SelectedIndex];
    }

    // === 3+ MONTAGES: N-2 randomization ===
    // Build list of valid indices (exclude last 2)
    TArray<int32> ValidIndices;
    for (int32 i = 0; i < AllMontages.Num(); ++i)
    {
        bool bExcluded = false;
        for (int32 RecentIdx : History.RecentIndices)
        {
            if (RecentIdx == i)
            {
                bExcluded = true;
                break;
            }
        }
        if (!bExcluded)
        {
            ValidIndices.Add(i);
        }
    }

    // Fallback: if somehow all excluded, pick any (shouldn't happen with 3+)
    if (ValidIndices.Num() == 0)
    {
        ValidIndices.Add(FMath::RandRange(0, AllMontages.Num() - 1));
    }

    // Random select from valid indices
    const int32 SelectedIndex = ValidIndices[FMath::RandRange(0, ValidIndices.Num() - 1)];
    RecordMontagePlay(SelectedIndex, Intensity, Direction);

    return AllMontages[SelectedIndex];
}

void UHitReactionComponent::RecordMontagePlay(
    int32 MontageIndex,
    EHitIntensity Intensity,
    EAttackDirection Direction)
{
    FReactionHistory& History = ReactionHistoryMap.FindOrAdd(Intensity).FindOrAdd(Direction);
    History.RecordPlayed(MontageIndex);
}

void UHitReactionComponent::ClearReactionHistory()
{
    ReactionHistoryMap.Empty();
}
```

### Phase 5: Update PlayReactionFromEntry

**File: `Private/Core/HitReactionComponent.cpp`**

Add new overload with intensity parameter:

```cpp
bool UHitReactionComponent::PlayReactionFromEntry(
    const FHitReactionEntry& ReactionEntry,
    EAttackDirection Direction,
    bool bIsHeavy,
    EHitIntensity Intensity)
{
    if (!AnimInstance)
    {
        return false;
    }

    // Select montage with variety
    UAnimMontage* SelectedMontage = SelectMontageWithVariety(ReactionEntry, Intensity, Direction);
    if (!SelectedMontage)
    {
        return false;
    }

    // Reset and setup i-frame tracking
    CurrentReactionTime = 0.0f;
    bCurrentReactionHasIFrames = ReactionEntry.bHasIFrames;
    CurrentIFrameStart = ReactionEntry.IFrameStart;
    CurrentIFrameEnd = ReactionEntry.IFrameEnd;

    if (bCurrentReactionHasIFrames)
    {
        SetComponentTickEnabled(true);
    }

    // Play the SELECTED montage (not ReactionEntry.ReactionMontage)
    const float Duration = AnimInstance->Montage_Play(SelectedMontage, ReactionEntry.PlayRate);

    if (Duration <= 0.0f)
    {
        bCurrentReactionHasIFrames = false;
        return false;
    }

    // Handle section selection (same as before)
    if (ReactionEntry.MontageSection != NAME_None)
    {
        if (ReactionEntry.bJumpToSectionStart)
        {
            AnimInstance->Montage_JumpToSection(ReactionEntry.MontageSection, SelectedMontage);
        }
        if (ReactionEntry.bUseSectionOnly)
        {
            AnimInstance->Montage_SetNextSection(ReactionEntry.MontageSection, NAME_None, SelectedMontage);
        }
    }

    OnHitReactionStarted.Broadcast(Direction, bIsHeavy);
    return true;
}
```

Update original overload to call new one:

```cpp
bool UHitReactionComponent::PlayReactionFromEntry(
    const FHitReactionEntry& ReactionEntry,
    EAttackDirection Direction,
    bool bIsHeavy)
{
    const EHitIntensity Intensity = bIsHeavy ? EHitIntensity::Heavy : EHitIntensity::Light;
    return PlayReactionFromEntry(ReactionEntry, Direction, bIsHeavy, Intensity);
}
```

### Phase 6: Update PlayHitReaction Call Site

**File: `Private/Core/HitReactionComponent.cpp`**

In `PlayHitReaction()` around line 231, pass intensity:

```cpp
// Change from:
if (PlayReactionFromEntry(*ReactionEntry, RelativeDir, bIsHeavy))

// To:
if (PlayReactionFromEntry(*ReactionEntry, RelativeDir, bIsHeavy, Intensity))
```

### Phase 7: Clear History on Death

**File: `Private/Core/HitReactionComponent.cpp`**

In `PlayDeathReaction()`, clear history:

```cpp
bool UHitReactionComponent::PlayDeathReaction(EAttackDirection Direction)
{
    // Clear reaction history on death (fresh start if revived)
    ClearReactionHistory();

    // ... rest of existing implementation
}
```

---

## Verification Checklist

- [ ] Project compiles without errors
- [ ] Single-montage configs still work (backwards compat)
- [ ] 1 montage: always plays that montage
- [ ] 2 montages: alternates back and forth
- [ ] 3+ montages: no repeat within last 2 played
- [ ] History clears on character death
- [ ] Existing 126 tests still pass

---

## Test Scenarios

### Manual Testing
1. Configure 4 light front hit montages in HitReactionSettings
2. Hit enemy repeatedly from front with light attacks
3. Verify: Same animation never plays 3 times in a row
4. Verify: Good variety across the 4 animations

### Edge Cases
1. **1 montage configured**: Always plays it, no errors
2. **2 montages configured**: Alternates A→B→A→B
3. **3 montages configured**: Never same twice in a row
4. **4+ montages configured**: Full n-2 variety

---

## Data Asset Updates (Post-Implementation)

Update `DA_HitReactionSettings_Default`:

```
DirectionalReactions[Light].Front.ReactionMontages = [
    AM_HitReact_Light_Front_01,
    AM_HitReact_Light_Front_02,
    AM_HitReact_Light_Front_03,
    AM_HitReact_Light_Front_04
]
```

Can keep single `ReactionMontage` for directions with only one animation.
