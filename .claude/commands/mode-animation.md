# Animation Mode - Focus on Animation System

This is a shortcut for `/mode animation`.

Switch to animation-focused context mode.

**Focus Areas**:
- Animation montages and notifies
- Phase transition system
- AnimNotify classes
- Blending and timing

**Included Files**:
- `Source/KatanaCombat/Public/Animation/**`
- `Source/KatanaCombat/Private/Animation/**`
- Animation-related utilities

**Excluded**:
- Combat logic files
- Data assets (unless animation-specific)
- Test files

**Action**: Switch to animation mode.

**Implementation**:
1. **Load Mode Config**: Read `.claude/context-modes/animation.json`
2. **Record Switch**:
   ```bash
   powershell.exe -ExecutionPolicy Bypass -File .claude/scripts/context-tracker.ps1 -Action switch -Mode "animation" -Reason "Manual switch via /mode-animation shortcut"
   ```
3. **Display Mode Info**

**Output**:
```markdown
# 📍 Context Mode: Animation

🎬 **Focus**: Animation system (montages, notifies, phase transitions)

## 📂 Focused Files
- `Source/KatanaCombat/Public/Animation/**/*.h`
- `Source/KatanaCombat/Private/Animation/**/*.cpp`
- `Source/KatanaCombat/Public/Utilities/MontageUtilityLibrary.h`

## 📚 Relevant Docs
- `docs/PHASE_SYSTEM_MIGRATION.md` - Phase transition setup
- `docs/ARCHITECTURE.md` - Animation architecture
- `docs/API_REFERENCE.md` - Montage utilities

## ⚡ Common Tasks
- Creating new AnimNotify classes
- Setting up phase transitions in montages
- Debugging blend timing
- Implementing hold mechanics
- Configuring combo windows

## 🎯 Key Principles
- Phases are exclusive: Windup → Active → Recovery
- Windows can overlap phases (ComboWindow, ParryWindow, HoldWindow)
- Use AnimNotify_AttackPhaseTransition (not deprecated AnimNotifyState_AttackPhase)
- Automatic hit detection during Active phase (no manual toggle needed)

---

**Switch to other modes**:
- `/mode-combat-logic` - Combat mechanics
- `/mode-data-assets` - Attack data configuration
- `/mode-testing` - Test infrastructure
- `/mode-list` - See all modes
```