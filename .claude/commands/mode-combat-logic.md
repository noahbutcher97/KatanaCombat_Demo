# Combat Logic Mode - Focus on Combat Mechanics

This is a shortcut for `/mode combat-logic`.

Switch to combat-logic-focused context mode.

**Focus Areas**:
- CombatComponent state management
- Input handling and buffering
- Attack execution logic
- State machine transitions

**Included Files**:
- `Source/KatanaCombat/Public/Core/**`
- `Source/KatanaCombat/Private/Core/**`
- `Source/KatanaCombat/Public/CombatTypes.h`

**Excluded**:
- Animation implementation files
- Data asset configuration
- Editor-only code

**Action**: Switch to combat-logic mode.

**Implementation**:
1. **Load Mode Config**: Read `.claude/context-modes/combat-logic.json`
2. **Record Switch**:
   ```bash
   powershell.exe -ExecutionPolicy Bypass -File .claude/scripts/context-tracker.ps1 -Action switch -Mode "combat-logic" -Reason "Manual switch via /mode-combat-logic shortcut"
   ```
3. **Display Mode Info**

**Output**:
```markdown
# 📍 Context Mode: Combat Logic

⚔️ **Focus**: Core combat (state machines, input, components)

## 📂 Focused Files
- `Source/KatanaCombat/Public/Core/CombatComponent*.h`
- `Source/KatanaCombat/Private/Core/CombatComponent*.cpp`
- `Source/KatanaCombat/Public/CombatTypes.h`
- `Source/KatanaCombat/Public/Core/TargetingComponent.h`
- `Source/KatanaCombat/Public/Core/WeaponComponent.h`

## 📚 Relevant Docs
- `docs/ARCHITECTURE.md` - Combat architecture
- `docs/SYSTEM_PROMPT.md` - Core design principles
- `docs/ARCHITECTURE_QUICK.md` - Quick reference

## ⚡ Common Tasks
- Implementing combat state transitions
- Debugging input buffering
- Adding new combo logic
- Fixing hold detection
- Implementing parry mechanics

## 🎯 Key Principles
- Input ALWAYS buffered (combo window modifies WHEN, not WHETHER)
- Parry = contextual block (defender checks enemy's ParryWindow)
- Hold = button state check at window start (not duration tracking)
- Timer-based approach (not tick-based)
- Movement ≠ Attack input (context-aware sampling)

---

**Switch to other modes**:
- `/mode-animation` - Animation system
- `/mode-data-assets` - Attack data configuration
- `/mode-testing` - Test infrastructure
- `/mode-list` - See all modes
```