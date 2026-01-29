# Plan: Death System with Directional Animations & Ragdoll

**Status**: COMPLETED (2025-01-29)
**Commits**: 6d21571 (HitReactionComponent lazy init), c4823be (Documentation audit)

---

## Overview

Implement a proper death handling system for combat characters. When an enemy's health reaches zero, they should play a directional death animation based on where the killing blow came from, then transition to ragdoll physics.

**MVP Scope (COMPLETED)**:
- [x] `bIsDead` flag to block further damage/reactions
- [x] Directional death animations (Front/Back/Left/Right)
- [x] Ragdoll transition after animation completes
- [x] Skip dead actors in weapon hit detection
- [x] Debug HUD shows enemy health/alive state

**Deferred (Future Iterations)**:
- [ ] Animation variety (array per direction with rotation) - Next: Hit Reaction Polish
- [ ] Reaction chains for repeated hits
- [ ] AnimNotifyState_IFrames (keep data-driven for now)
- [ ] Knockdown/get-up system

---

## Implementation Summary

### Files Modified

| File | Changes |
|------|---------|
| `Public/CombatTypes.h` | Added `EReactionOutcome` enum, extended `FHitReactionEntry` |
| `Public/Data/HitReactionSettings.h` | Added `DeathReactions` map |
| `Private/Data/HitReactionSettings.cpp` | Added `GetDeathReaction()` method |
| `Public/Characters/BaseCombatCharacter.h` | Added `bIsDead` flag |
| `Private/Characters/BaseCombatCharacter.cpp` | Enhanced `HandleDeath`, skip dead in `OnWeaponHitTarget` |
| `Public/Core/HitReactionComponent.h` | Added `PlayDeathReaction()`, ragdoll handling |
| `Private/Core/HitReactionComponent.cpp` | Death reaction + ragdoll transition + lazy init fix |
| `Public/Debug/CombatDebugHUD.h` | Added enemy health data to `FCombatDebugData` |
| `Private/Debug/CombatDebugHUD.cpp` | Display enemy health/alive state |

### Test Coverage Added

- **DeathSystemTests** (11 tests) - Death flag lifecycle, damage blocking, death events
- **HitReactionTests** - Directional hit detection (Front/Back/Left/Right)
- All 126 tests passing

### Key Implementation Details

1. **EReactionOutcome Enum**: StandardRecovery, Death, Ragdoll
2. **Lazy Initialization**: `GetOwnerCharacterCached()` for test environment compatibility
3. **Directional Fallback**: Falls back to Forward if specific direction not configured
4. **Ragdoll Transition**: Montage end delegate triggers physics simulation

---

## Verification (All Passed)

- [x] Project compiles without errors
- [x] Enemy health reaches 0 → `bIsDead` is set
- [x] Death animation plays based on killing blow direction
- [x] Ragdoll activates after death animation completes
- [x] Dead enemies ignore further weapon hits
- [x] Dead enemies don't play hit reactions
- [x] Debug HUD shows target health and state
- [x] All 126 tests passing

---

## Next Steps (From Deferred Items)

1. **Hit Reaction Polish** - Cycled animation arrays with n-2 randomization
2. **Paired Animation System** - Synced finisher/counter animations
