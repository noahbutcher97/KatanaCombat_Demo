# Claude Code - KatanaCombat Project

**Quick onboarding for AI assistants working on the KatanaCombat combat system.**

---

## Instant Context (Read This First)

**Project**: Ghost of Tsushima-inspired melee combat system for Unreal Engine 5.6 (C++)

**Architecture**: 4 components (~20 files), pragmatic design, data-driven tuning

**Core Identity**:
- Phases vs Windows (DISTINCT systems - phases exclusive, windows overlap)
- Input ALWAYS buffered (combo window modifies WHEN, not WHETHER)
- Parry is contextual block (defender-side detection)
- Hold detection checks button state at window start (NOT duration tracking)
- Delegates centralized in `CombatTypes.h` (system-wide events declared ONCE)

---

## Essential Reading Order

### 1. **START HERE** → `docs/SYSTEM_PROMPT.md` (25 KB, ~10 min)
**Read this completely before doing anything.**

Contains:
- Critical design corrections (phases vs windows, input buffering, parry system)
- Component responsibilities and interaction patterns
- Core systems deep dive (attack phases, windows, hold mechanics)
- Data structures and interfaces
- Design principles and common mistakes

**Why read it**: Prevents incorrect assumptions about system design. The system has specific architectural decisions that differ from typical implementations.

### 2. **Quick Reference** → `docs/ARCHITECTURE_QUICK.md` (8 KB, ~3 min)
**Keep open while coding.**

Contains:
- Condensed technical reference
- Default tuning values
- Component structure diagram
- Common mistakes checklist
- File locations

**Why use it**: Fast lookups for default values, state transitions, component APIs.

### 3. **Deep Dive** → `docs/ARCHITECTURE.md` (52 KB, as needed)
**Reference when implementing complex features.**

Contains:
- Complete technical documentation
- State machine with all transition rules
- Attack system implementation details
- Delegate architecture (CRITICAL section)
- Complete data flow diagrams

**Why use it**: Comprehensive understanding for complex implementations.

---

## Documentation Map

```
docs/
├── SYSTEM_PROMPT.md          [MUST READ] Core AI understanding
├── ARCHITECTURE_QUICK.md     [REFERENCE] Quick technical lookup
├── ARCHITECTURE.md           [DEEP DIVE] Complete implementation details
├── README.md                 [USER DOCS] Project overview
├── GETTING_STARTED.md        [USER DOCS] Setup guide
├── ATTACK_CREATION.md        [USER DOCS] Attack authoring workflow
├── API_REFERENCE.md          [REFERENCE] Complete function signatures
└── TROUBLESHOOTING.md        [DEBUGGING] Common issues & solutions

Source/KatanaCombatTest/
└── README.md                 [TESTING] C++ unit test suite (7 tests, 45+ assertions)
```

---

## Critical Design Patterns (Memorize These)

### 1. Phases vs Windows

**Phases** (Mutually Exclusive):
```
Windup → Active → Recovery
(Only ONE active at a time)
```

**Windows** (Independent, Can Overlap):
```
ParryWindow    [────]
CancelWindow   [────────]
ComboWindow         [────────]
HoldWindow              [──]
```

**KEY**: Hold and ParryWindow are NOT phases, they're windows!

### 2. Input Buffering

```
Input During Attack:
  ↓
ALWAYS buffered (stored)
  ↓
Combo Window Active?
├─ YES → Execute at Active end (early, "snappy")
└─ NO → Execute at Recovery end (normal, "responsive")
```

**KEY**: Input is NEVER gated by combo window, only timing is modified.

### 3. Parry Detection

```
Defender Presses Block:
  ↓
Check enemy->IsInParryWindow()
├─ TRUE → PARRY ACTION (no damage, counter window)
└─ FALSE → BLOCK ACTION (posture damage)
```

**KEY**: Parry window is on ATTACKER's montage, defender checks enemy state.

### 4. Hold Mechanics

```
HoldWindow Starts:
  ↓
Is bLightAttackPressed STILL true?
├─ YES → Begin hold behavior (slowdown, freeze)
└─ NO → Continue normal combo
```

**KEY**: Check button state at window start, NOT tracking duration.

### 5. Delegate Architecture

```
CombatTypes.h:
  DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnCombatStateChanged, ...)

CombatComponent.h:
  UPROPERTY(BlueprintAssignable, Category = "Combat")
  FOnCombatStateChanged OnCombatStateChanged;  // ONLY UPROPERTY
```

**KEY**: System-wide delegates declared ONCE in `CombatTypes.h`, components use `UPROPERTY` only.

---

## Component Structure

```
Character
├── CombatComponent (~1000 lines)
│   ├── State machine, attacks, posture, combos, parry/counters
│   └── WHY LARGE: Combat flow logic is tightly coupled
│
├── TargetingComponent (~300 lines)
│   └── Cone-based targeting, motion warp setup
│
├── WeaponComponent (~200 lines)
│   └── Socket-based hit detection
│
└── HitReactionComponent (~300 lines)
    └── Damage reception, hit reactions
```

**WHY 4 COMPONENTS**: Only separate when distinct, reusable responsibility. CombatComponent is intentionally consolidated.

---

## File Locations (Quick Reference)

### Core Systems
```
Source/KatanaCombat/Public/
├── CombatTypes.h                    ← ALL enums, structs, DELEGATES
├── Core/
│   ├── CombatComponent.h            ← Main combat hub
│   ├── TargetingComponent.h
│   ├── WeaponComponent.h
│   └── HitReactionComponent.h
├── Data/
│   ├── AttackData.h                 ← Attack configuration
│   └── CombatSettings.h             ← Global tuning
├── Animation/
│   ├── AnimNotify_AttackPhaseTransition.h          ← Phase transitions (NEW)
│   ├── AnimNotifyState_ParryWindow.h               ← Parry detection window
│   ├── AnimNotifyState_HoldWindow.h                ← Hold detection window
│   ├── AnimNotifyState_ComboWindow.h               ← Combo input window
│   ├── AnimNotifyState_AttackPhase.h               ← DEPRECATED - Old phase system
│   └── AnimNotify_ToggleHitDetection.h             ← DEPRECATED - Now automatic
└── Interfaces/
    ├── CombatInterface.h
    └── DamageableInterface.h
```

---

## Common Tasks - Where to Look

| Task | Documentation |
|------|--------------|
| Understanding system architecture | `docs/SYSTEM_PROMPT.md` |
| Quick value lookup | `docs/ARCHITECTURE_QUICK.md` |
| Implementing complex feature | `docs/ARCHITECTURE.md` |
| Adding new attack | `docs/ATTACK_CREATION.md` |
| Migrating from old phase system | `docs/PHASE_SYSTEM_MIGRATION.md` |
| Function signatures | `docs/API_REFERENCE.md` |
| Debugging issue | `docs/TROUBLESHOOTING.md` |
| Setting up project | `docs/GETTING_STARTED.md` |

---

## Default Values (Quick Lookup)

```cpp
// Timing
ComboInputWindow:             0.6s
ParryWindow:                  0.3s
CounterWindowDuration:        1.5s

// Posture
MaxPosture:                   100.0f
PostureRegenRate_Attacking:   50.0f  // Fastest (rewards aggression)
PostureRegenRate_Idle:        20.0f
GuardBreakStunDuration:       2.0f

// Damage
LightBaseDamage:              25.0f
HeavyBaseDamage:              50.0f
CounterDamageMultiplier:      1.5f

// Motion Warping
MaxWarpDistance:              400.0f
DirectionalConeHalfAngle:     60.0f  // 120° total cone
```

---

## Common Mistakes (Avoid These)

❌ Making Hold or ParryWindow an attack phase
❌ Using combo window to gate input buffering
❌ Tracking hold duration instead of button state
❌ Putting parry window on defender's animation
❌ Declaring system delegates in component headers (use `CombatTypes.h`)
❌ Using TArray for cancel inputs (use bitmask)
❌ Splitting CombatComponent into artificial fragments
❌ Assuming input isn't buffered outside combo window

---

## Debug Visualization

Enable debug draws:
```cpp
CombatComponent->bDebugDraw = true;      // State, phases, windows
TargetingComponent->bDebugDraw = true;   // Cones, targets, distances
WeaponComponent->bDebugDraw = true;      // Traces, hit points
```

Console commands:
```
showdebug animation    // See current state, montage
stat fps               // Performance
slomo 0.3              // Slow motion for timing verification
```

---

## Communication Style

When explaining to user:
- Use `file:line` references (e.g., `CombatComponent.cpp:245`)
- Show ASCII timelines for phase/window diagrams
- Explain design decisions (why phases vs windows, why pragmatic consolidation)
- Reference Ghost of Tsushima inspirations when relevant
- Keep technically accurate but concise

When modifying code:
- Maintain component separation (don't consolidate into character)
- Preserve Blueprint exposure (`UFUNCTION(BlueprintCallable)`)
- Follow naming conventions (see system prompt)
- Update AnimInstance variables if adding state
- Test state transitions with `CanTransitionTo()`

---

## Quick Troubleshooting

**Attacks not executing**:
1. Check `CombatComponent->GetCombatState()` == `Idle`
2. Verify `DefaultLightAttack` is assigned
3. Check `AnimInstance` is valid
4. Look at `CombatComponent->CanAttack()` return value

**Combos not chaining**:
1. Verify `AnimNotifyState_ComboWindow` in montage
2. Check `NextComboAttack` is set in AttackData
3. Enable debug draw to see combo window state
4. Check combo isn't resetting too quickly

**Hits not detecting**:
1. Verify weapon sockets exist (`WeaponStart`, `WeaponEnd`)
2. Check `AnimNotify_AttackPhaseTransition(Active)` is present (hit detection automatic)
3. Verify Active phase timing matches expected hit window
4. Verify trace channel matches target collision
5. Enable weapon debug draw to see traces

**Note**: Hit detection is now automatic with Active phase. Old `AnimNotify_ToggleHitDetection` is deprecated.

**Parry not working**:
1. Ensure `AnimNotifyState_ParryWindow` on ATTACKER's montage
2. Check defender is calling `IsInParryWindow()` on enemy
3. Verify defender is pressing block during window
4. Check enemy is in range and defender is facing them

---

## Session Start Checklist

Before coding:
- [ ] Read `docs/SYSTEM_PROMPT.md` completely (10 min)
- [ ] Skim `docs/ARCHITECTURE_QUICK.md` (3 min)
- [ ] Understand phases vs windows distinction
- [ ] Know that input is always buffered
- [ ] Understand parry is defender-side detection
- [ ] Remember delegates in `CombatTypes.h` only

During coding:
- [ ] Reference `ARCHITECTURE_QUICK.md` for quick lookups
- [ ] Check `API_REFERENCE.md` for function signatures
- [ ] Use `TROUBLESHOOTING.md` when debugging
- [ ] Enable debug visualization for testing

When explaining:
- [ ] Use file:line references
- [ ] Show ASCII diagrams for timelines
- [ ] Explain design decisions
- [ ] Keep concise but accurate

---

## System Quality Assurance

**Code Health**: 96% compliant with design specifications (validated via comprehensive audit)

**Test Coverage**: 7 C++ unit test files with 45+ assertions validating:
- ✓ State machine transitions
- ✓ Input buffering (always-on, responsive + snappy)
- ✓ Hold window detection (button state, not duration)
- ✓ Parry system (defender-side detection)
- ✓ Attack execution separation
- ✓ Memory safety (null handling)
- ✓ Phases vs windows architecture

**Running Tests**:
```bash
# In Editor: Window → Developer Tools → Session Frontend → Automation → Filter: "KatanaCombat"
# Command Line:
UnrealEditor.exe "KatanaCombat.uproject" -ExecCmds="Automation RunTests KatanaCombat"
```

**See**: `Source/KatanaCombatTest/README.md` for complete test documentation.

---

## You're Ready!

**Next Step**: Open `docs/SYSTEM_PROMPT.md` and read it completely. This will give you the foundational understanding needed to work effectively on KatanaCombat.

**Time Investment**: ~15 minutes of reading = Hours saved in debugging and refactoring

**Remember**: This system has specific architectural decisions (phases vs windows, input buffering, parry detection, delegate centralization) that differ from typical implementations. Understanding these upfront prevents incorrect assumptions.

---

## GPU Crash Fix (RTX 5090 + UE 5.6)

**Issue**: NVIDIA RTX 5090 driver 581.57 (Game Ready) causes GPU crashes with DirectX 12 in UE 5.6 editor during:
- Batch operations on large animation sets (1000+ assets)
- Opening multiple animation previews
- Skeleton replacement operations

**Symptoms**:
```
LogD3D12RHI: Error: GPU crash detected: DXGI_ERROR_DEVICE_REMOVED
LogNvidiaAftermath: Warning: Timed out while waiting for Aftermath to start the GPU crash dump
```

**Temporary Fix Applied**: Switched to DirectX 11 (2025-10-24)

**Config File**: `Config/DefaultEngine.ini`
**Changed Line**: Line 47

---

### How to Revert to DirectX 12

**Option A: When you have stable drivers**

1. Open: `D:\UnrealProjects\5.6\KatanaCombat\Config\DefaultEngine.ini`
2. Find line 47: `DefaultGraphicsRHI=DefaultGraphicsRHI_DX11`
3. Change to: `DefaultGraphicsRHI=DefaultGraphicsRHI_DX12`
4. Save file
5. Restart Unreal Editor

**Option B: Install NVIDIA Studio Driver 580.97** (Recommended for long-term stability)

1. Download: https://www.nvidia.com/en-us/drivers/details/252609/
2. Use DDU (Display Driver Uninstaller) to clean current driver:
   - Download DDU from: https://www.guru3d.com/files-details/display-driver-uninstaller-download.html
   - Boot into Safe Mode
   - Run DDU, select "Clean and Restart"
3. Install Studio Driver 580.97
4. Revert to DX12 using Option A steps above

**Why Studio Driver 580.97?**
- August 2025 release with proven stability for RTX 5090
- Optimized for content creation apps (Unreal Engine, Blender, etc.)
- User reports show 581.xx series has known RTX 5090 stability issues

**Performance Impact of DX11**:
- ~5-10% lower editor viewport performance (barely noticeable)
- Packaged games still use DX12 (only editor affected)
- No impact on final game performance

---

**Crash Logs Location**: `Saved/Crashes/` (for reference if issues persist)

---

**Happy coding!** 🗡️
