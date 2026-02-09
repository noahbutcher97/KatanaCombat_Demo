# Pooled Impact FX System (UCombatFXData) - ARCHIVED

> **Status**: COMPLETED - Committed as 150cd3a
> **Archived**: 2026-02-05

## Overview

Replace per-attack FX tedium with pooled FX arrays organized by attack type. New `UCombatFXData` data asset lives **on WeaponData** so each weapon has its own impact character. Random selection from pools prevents repetitive audio. Surface FX scaffolded but not wired.

**U-15 (hit audio) is already committed.** This plan builds on top of it.

## Resolution Chain (New 4-Tier)

```
1. AttackData.ImpactAudioConfig.ImpactSound   ← per-attack override (special attacks)
2. WeaponData.CombatFXData pool[AttackType]    ← NEW: random from pool (most attacks)
3. WeaponData.HitSound                         ← simple weapon fallback (legacy)
4. silent
```

Same pattern for VFX (scaffold — not wired until U-16).

**Backward compatible**: If CombatFXData is null on WeaponData, Tier 2 is skipped entirely. Existing behavior unchanged.

## Implementation Summary

- UCombatFXData data asset with TMap<EAttackType, FImpactFXPool>
- FImpactFXPool with TArray<FImpactSoundEntry> and TArray<FImpactVFXEntry>
- ResolveAndPlayImpactSound() for 4-tier audio resolution
- ResolveAndSpawnImpactVFX() for 4-tier VFX resolution
- 18 unit tests for pool functionality

## Commits

- **150cd3a**: Implement pooled impact FX system (UCombatFXData) - 8 files, 18 new tests
- **3038b21**: Implement impact VFX system (U-16) - SpawnImpactVFX(), ResolveAndSpawnImpactVFX()
