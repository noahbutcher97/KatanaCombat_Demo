# Data Assets Mode - Focus on Data Configuration

This is a shortcut for `/mode data-assets`.

Switch to data-assets-focused context mode.

**Focus Areas**:
- AttackData configuration
- CombatSettings tuning
- PrimaryDataAsset setup
- Property validation

**Included Files**:
- `Source/KatanaCombat/Public/Data/**`
- `Source/KatanaCombat/Private/Data/**`
- `Content/ProjectFiles/Data/**` (asset files)

**Excluded**:
- Combat logic implementation
- Animation implementation
- Test files

**Action**: Switch to data-assets mode.

**Implementation**:
1. **Load Mode Config**: Read `.claude/context-modes/data-assets.json`
2. **Record Switch**:
   ```bash
   powershell.exe -ExecutionPolicy Bypass -File .claude/scripts/context-tracker.ps1 -Action switch -Mode "data-assets" -Reason "Manual switch via /mode-data-assets shortcut"
   ```
3. **Display Mode Info**

**Output**:
```markdown
# 📍 Context Mode: Data Assets

📊 **Focus**: Data-driven config (AttackData, settings)

## 📂 Focused Files
- `Source/KatanaCombat/Public/Data/AttackData.h`
- `Source/KatanaCombat/Public/Data/CombatSettings.h`
- `Source/KatanaCombat/Public/Data/AttackConfiguration.h`
- `Content/ProjectFiles/Data/PDA/**`

## 📚 Relevant Docs
- `docs/ATTACK_CREATION.md` - Creating attacks
- `docs/ARCHITECTURE_QUICK.md` - Default values
- `docs/ARCHITECTURE.md` - Data asset architecture

## ⚡ Common Tasks
- Creating new attack data assets
- Tuning attack properties
- Setting up combo chains
- Configuring directional follow-ups
- Adding validation rules
- Adjusting default values

## 🎯 Key Principles
- ComboInputWindow: 0.6s default
- ParryWindow: 0.3s default
- ComboBlendOut/In: 0.1s default
- LightDamage: 25.0f, HeavyDamage: 50.0f
- MaxPosture: 100.0f
- All UPROPERTY must have Category and Tooltip
- Validation in AttackData::Validate()

## Default Values Quick Reference
| Property | Default | Range |
|----------|---------|-------|
| ComboInputWindow | 0.6s | 0.0-2.0s |
| ParryWindow | 0.3s | 0.0-1.0s |
| ComboBlendOut | 0.1s | 0.0-1.0s |
| LightDamage | 25.0f | 0.0-999.0f |
| HeavyDamage | 50.0f | 0.0-999.0f |

---

**Switch to other modes**:
- `/mode-animation` - Animation system
- `/mode-combat-logic` - Combat mechanics
- `/mode-testing` - Test infrastructure
- `/mode-list` - See all modes
```