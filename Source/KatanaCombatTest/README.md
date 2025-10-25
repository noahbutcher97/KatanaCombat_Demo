# KatanaCombat Test Suite

Automated tests for the KatanaCombat combat system.

## Module Configuration

- **Type**: `UncookedOnly` - Excluded from shipping builds
- **Dependencies**: KatanaCombat, UnrealEd
- **Location**: `Source/KatanaCombatTest/`

## Test Coverage

### 1. State Transition Tests (`StateTransitionTests.cpp`)
- Validates all combat state transitions
- Verifies terminal states (Dead)
- Tests invalid transition rejection

**Path**: `KatanaCombat.CombatComponent.StateTransitions`

### 2. Input Buffering Tests (`InputBufferingTests.cpp`)
- Verifies hybrid responsive + snappy input system
- Tests combo window affects TIMING, not WHETHER input buffers
- Validates snappy path vs responsive path

**Path**: `KatanaCombat.CombatComponent.InputBuffering`

### 3. Hold Window Tests (`HoldWindowTests.cpp`)
- Verifies button state detection at window start (NOT duration tracking)
- Tests hold with correct/wrong button
- Validates bCanHold requirement

**Path**: `KatanaCombat.CombatComponent.HoldWindow`

### 4. Parry Detection Tests (`ParryDetectionTests.cpp`)
- Verifies defender-side parry detection
- Tests attacker's IsInParryWindow() state
- Validates window independence between characters

**Path**: `KatanaCombat.CombatComponent.ParryDetection`

### 5. Attack Execution Tests (`AttackExecutionTests.cpp`)
- Validates ExecuteAttack() only works from Idle
- Tests ExecuteComboAttack() works from Attacking
- Verifies null protection

**Path**: `KatanaCombat.CombatComponent.AttackExecution`

### 6. Memory Safety Tests (`MemorySafetyTests.cpp`)
- Tests null CurrentAttackData handling
- Verifies no crashes on edge cases
- Validates graceful degradation

**Path**: `KatanaCombat.CombatComponent.MemorySafety`

### 7. Phases vs Windows Tests (`PhasesVsWindowsTests.cpp`)
- Verifies phases are mutually exclusive (only 1 active)
- Tests windows can overlap (multiple active)
- Validates architectural separation

**Path**: `KatanaCombat.CombatComponent.PhasesVsWindows`

## Running Tests

### In Editor

1. Open **Session Frontend** (Window → Developer Tools → Session Frontend)
2. Go to **Automation** tab
3. Filter for "KatanaCombat"
4. Select tests to run
5. Click **Start Tests**

### Command Line

**Run all tests:**
```bash
UnrealEditor.exe "KatanaCombat.uproject" -ExecCmds="Automation RunTests KatanaCombat" -unattended -nopause -testexit="Automation Test Queue Empty"
```

**Run specific test category:**
```bash
UnrealEditor.exe "KatanaCombat.uproject" -ExecCmds="Automation RunTests KatanaCombat.CombatComponent" -unattended -nopause
```

**Run single test:**
```bash
UnrealEditor.exe "KatanaCombat.uproject" -ExecCmds="Automation RunTests KatanaCombat.CombatComponent.StateTransitions" -unattended -nopause
```

## Adding New Tests

### 1. Create test file in `Private/`

```cpp
// Private/MyNewTests.cpp

#include "CombatTestHelpers.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FMyNewTest,
    "KatanaCombat.Category.TestName",
    EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FMyNewTest::RunTest(const FString& Parameters)
{
    // Setup
    UWorld* World = FCombatTestHelpers::CreateTestWorld();
    UCombatComponent* CombatComp = nullptr;
    ACharacter* Character = FCombatTestHelpers::CreateTestCharacterWithCombat(World, CombatComp);

    // Your tests here
    TestTrue("Description", SomeCondition);
    TestEqual("Description", ActualValue, ExpectedValue);

    // Cleanup
    World->DestroyActor(Character);
    FCombatTestHelpers::DestroyTestWorld(World);

    return true;
}
```

### 2. Add friend declaration to CombatComponent.h (if needed)

```cpp
#if WITH_AUTOMATION_TESTS
    friend class FMyNewTest;
#endif
```

### 3. Recompile and run

## Test Helpers

`CombatTestHelpers.h` provides utilities:

- `CreateTestWorld()` - Create minimal test world
- `DestroyTestWorld(World)` - Clean up test world
- `CreateTestCharacterWithCombat(World, OutCombat)` - Spawn character with combat component
- `CreateTestCharacterWithCombatAndTargeting(World, OutCombat, OutTargeting)` - With targeting too
- `CreateTestAttack(Type)` - Create attack data asset
- `CreateTestComboChain(Length, Type)` - Create combo chain

## Notes

- Tests use friend declarations to access private members of CombatComponent
- All tests are excluded from shipping builds (UncookedOnly module type)
- Tests verify design principles from audit documentation
- Each test is independent and cleans up after itself

## Related Documentation

- `docs/SYSTEM_PROMPT.md` - Core design principles tested
- `docs/ARCHITECTURE_QUICK.md` - Default values validated by tests
- `docs/TROUBLESHOOTING.md` - Common issues tests catch

---

**Generated from Full Audit Report** - All tests based on comprehensive system validation.