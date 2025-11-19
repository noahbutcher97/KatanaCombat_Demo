# Testing Mode - Focus on Test Infrastructure

This is a shortcut for `/mode testing`.

Switch to testing-focused context mode.

**Focus Areas**:
- Unit tests
- Integration tests
- Automation tests
- Test utilities

**Included Files**:
- `Source/KatanaCombatTest/**`
- Test-related utilities
- Mock objects

**Excluded**:
- Production code (unless testing it)
- Editor-only code
- Data assets

**Action**: Switch to testing mode.

**Implementation**:
1. **Load Mode Config**: Read `.claude/context-modes/testing.json`
2. **Record Switch**:
   ```bash
   powershell.exe -ExecutionPolicy Bypass -File .claude/scripts/context-tracker.ps1 -Action switch -Mode "testing" -Reason "Manual switch via /mode-testing shortcut"
   ```
3. **Display Mode Info**

**Output**:
```markdown
# 📍 Context Mode: Testing

🧪 **Focus**: Test infrastructure (unit tests, integration tests)

## 📂 Focused Files
- `Source/KatanaCombatTest/**/*.cpp`
- `Source/KatanaCombatTest/README.md`

## 📚 Relevant Docs
- `Source/KatanaCombatTest/README.md` - Test suite documentation
- `docs/ARCHITECTURE.md` - Testing section
- `docs/TROUBLESHOOTING.md` - Debugging tests

## ⚡ Common Tasks
- Writing unit tests
- Creating integration tests
- Running automation tests
- Debugging test failures
- Adding test coverage
- Creating mock objects

## 🎯 Test Categories
- **Unit Tests**: CombatComponentTest, AttackDataTest, etc.
- **Integration Tests**: Full workflow tests
- **State Machine Tests**: Transition validation
- **Input Tests**: Buffering and handling
- **Combo Tests**: Chain execution
- **Hold Tests**: Detection and resolution

## Test Patterns
```cpp
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FTestName,
    "KatanaCombat.Category.SubCategory",
    EAutomationTestFlags::ApplicationContextMask |
    EAutomationTestFlags::ProductFilter
)

bool FTestName::RunTest(const FString& Parameters)
{
    // Arrange
    // Act
    // Assert
    return true;
}
```

## Running Tests
```bash
# Run all tests
UnrealEditor.exe [Project].uproject -ExecCmds="Automation RunTests KatanaCombat"

# Run specific test
UnrealEditor.exe [Project].uproject -ExecCmds="Automation RunTests KatanaCombat.CombatComponent"
```

---

**Switch to other modes**:
- `/mode-combat-logic` - Combat mechanics
- `/mode-animation` - Animation system
- `/mode-data-assets` - Attack configuration
- `/mode-list` - See all modes
```