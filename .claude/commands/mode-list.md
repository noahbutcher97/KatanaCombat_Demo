# Mode List - Show All Available Modes

This is a shortcut for `/mode list`.

Shows all available context modes with descriptions.

**Action**: Display all mode options from `.claude/context-modes/` directory.

**Implementation**:
1. Read all JSON files from `.claude/context-modes/` directory
2. Extract name, description, and key metadata from each mode
3. Format as organized list

**Output**:
```markdown
# 📚 Available Context Modes

## Specialized Modes

### 🎬 animation
**Description**: Animation system (montages, notifies, phase transitions)
**Focus**: Animation files, AnimNotify classes, montage utilities
**Best for**: Working with combat animations and phase systems
**Switch**: `/mode animation`

### ⚔️ combat-logic
**Description**: Core combat (state machines, input, components)
**Focus**: CombatComponent, state management, input handling
**Best for**: Combat mechanics and state transitions
**Switch**: `/mode combat-logic`

### 📊 data-assets
**Description**: Data-driven config (AttackData, settings)
**Focus**: AttackData, CombatSettings, PrimaryDataAssets
**Best for**: Tuning attack properties and combat parameters
**Switch**: `/mode data-assets`

### 🖥️ editor-ui
**Description**: Editor tooling (custom panels, asset editors, UI)
**Focus**: Slate, customization, editor-only code
**Best for**: Building editor tools and custom UI
**Switch**: `/mode editor-ui`

### 🧪 testing
**Description**: Test infrastructure (unit tests, integration tests)
**Focus**: Test files, automation, assertions
**Best for**: Writing and debugging tests
**Switch**: `/mode testing`

### 📝 documentation
**Description**: Documentation work (writing, updating docs)
**Focus**: Markdown files, API docs, guides
**Best for**: Documentation maintenance and creation
**Switch**: `/mode documentation`

## General Mode

### 🌐 full
**Description**: Full context (no filtering, default)
**Focus**: All project files
**Best for**: Multi-system work, exploration, general development
**Switch**: `/mode full`

---

## Quick Actions

- **Switch mode**: `/mode [mode-name]`
- **Get suggestion**: `/mode-suggest`
- **Current status**: `/mode-status`
- **Enable auto**: `/mode-auto-enable`

---

💡 **Tip**: Use `/mode-suggest` to get AI-recommended mode based on your current work.
```