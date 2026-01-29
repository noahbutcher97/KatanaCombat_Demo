# KatanaCombat Test Suite

Automated tests for the KatanaCombat combat system.

## Module Configuration

- **Type**: `UncookedOnly` - Excluded from shipping builds
- **Dependencies**: KatanaCombat, UnrealEd
- **Location**: `Source/KatanaCombatTest/`

## Test Coverage (14 Suites, 126 Tests)

### Core Combat Tests

#### 1. State Transition Tests (`StateTransitionTests.cpp`)
- Validates all combat state transitions
- Verifies terminal states (Dead)
- Tests invalid transition rejection

**Path**: `KatanaCombat.CombatComponent.StateTransitions`

#### 2. Input Buffering Tests (`InputBufferingTests.cpp`)
- Verifies hybrid responsive + snappy input system
- Tests combo window affects TIMING, not WHETHER input buffers
- Validates snappy path vs responsive path

**Path**: `KatanaCombat.CombatComponent.InputBuffering`

#### 3. Hold Window Tests (`HoldWindowTests.cpp`)
- Verifies button state detection at window start (NOT duration tracking)
- Tests hold with correct/wrong button
- Validates bCanHold requirement

**Path**: `KatanaCombat.CombatComponent.HoldWindow`

#### 4. Parry Detection Tests (`ParryDetectionTests.cpp`)
- Verifies defender-side parry detection
- Tests attacker's IsInParryWindow() state
- Validates window independence between characters

**Path**: `KatanaCombat.CombatComponent.ParryDetection`

#### 5. Attack Execution Tests (`AttackExecutionTests.cpp`)
- Validates ExecuteAttack() only works from Idle
- Tests ExecuteComboAttack() works from Attacking
- Verifies null protection

**Path**: `KatanaCombat.CombatComponent.AttackExecution`

#### 6. Phases vs Windows Tests (`PhasesVsWindowsTests.cpp`)
- Verifies phases are mutually exclusive (only 1 active)
- Tests windows can overlap (multiple active)
- Validates architectural separation

**Path**: `KatanaCombat.CombatComponent.PhasesVsWindows`

### Component Tests

#### 7. Targeting Component Tests (`TargetingComponentTests.cpp`)
- Soft-lock targeting acquisition
- Direction conversion (world ↔ local)
- Target filtering and prioritization

**Path**: `KatanaCombat.Targeting.*`

#### 8. Weapon Component Tests (`WeaponComponentTests.cpp`)
- Hit detection enable/disable
- Equip/holster state changes
- Hit actor tracking and reset
- Socket configuration

**Path**: `KatanaCombat.Weapon.*`

#### 9. Hit Reaction Tests (`HitReactionTests.cpp`)
- Damage application and resistance
- Directional hit calculation (Front/Back/Left/Right)
- Stun state management
- I-frame blocking
- Death pose snapshot

**Path**: `KatanaCombat.HitReaction.*`

### System Tests

#### 10. Damage Application Tests (`DamageApplicationTests.cpp`)
- Damage flow through interfaces
- Resistance multipliers
- Super armor and invulnerability

**Path**: `KatanaCombat.Damage.*`

#### 11. Death System Tests (`DeathSystemTests.cpp`)
- Death flag (bIsDead) lifecycle
- Damage blocking after death
- Death event firing
- Multiple enemy independence
- Edge cases (exact lethal, overkill)

**Path**: `KatanaCombat.DeathSystem.*`

#### 12. Combat Integration Tests (`CombatIntegrationTests.cpp`)
- Full damage flow (player → weapon → enemy → death)
- Multi-component coordination
- Team-based damage filtering

**Path**: `KatanaCombat.Integration.*`

#### 13. Debug Visualization Tests (`DebugVisualizationTests.cpp`)
- CVar-based debug system
- Debug HUD data collection
- Visual state reporting

**Path**: `KatanaCombat.Debug.*`

#### 14. Memory Safety Tests (`MemorySafetyTests.cpp`)
- Null CurrentAttackData handling
- Null component graceful degradation
- Edge case crash prevention

**Path**: `KatanaCombat.CombatComponent.MemorySafety`

## Running Tests

### In Editor

1. Open **Session Frontend** (Window → Developer Tools → Session Frontend)
2. Go to **Automation** tab
3. Filter for "KatanaCombat"
4. Select tests to run
5. Click **Start Tests**

### Command Line

**Run all tests:**
```powershell
"C:\Program Files\Epic Games\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "D:\UnrealProjects\5.6\KatanaCombat\KatanaCombat.uproject" -ExecCmds="Automation RunTests KatanaCombat;Quit" -NullRHI -NoSplash -Unattended
```

**Run specific test category:**
```powershell
"C:\Program Files\Epic Games\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "KatanaCombat.uproject" -ExecCmds="Automation RunTests KatanaCombat.DeathSystem" -NullRHI
```

**Check results in log:**
```powershell
grep "Result=" D:/UnrealProjects/5.6/KatanaCombat/Saved/Logs/KatanaCombat.log | grep -c "Success"
```

## Adding New Tests

### 1. Create test file in `Private/`

```cpp
// Private/MyNewTests.cpp

#include "CombatTestHelpers.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FMyNewTest,
    "KatanaCombat.Category.TestName",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FMyNewTest::RunTest(const FString& Parameters)
{
    // Setup
    UWorld* World = FCombatTestHelpers::CreateTestWorld();
    AEnemyCharacter* Enemy = FCombatTestHelpers::CreateTestEnemyCharacter(World);

    // Your tests here
    TestTrue("Description", SomeCondition);
    TestEqual("Description", ActualValue, ExpectedValue);
    TestNotNull("Should exist", Enemy->HitReactionComponent.Get());

    // Cleanup
    World->DestroyActor(Enemy);
    FCombatTestHelpers::DestroyTestWorld(World);

    return true;
}
```

### 2. Use Test Helpers

`CombatTestHelpers.h` provides utilities:

**World Management:**
- `CreateTestWorld()` - Create minimal test world
- `DestroyTestWorld(World)` - Clean up test world

**Character Creation:**
- `CreateTestPlayerCharacter(World, Location)` - Spawn player
- `CreateTestEnemyCharacter(World, Location)` - Spawn enemy
- `CreateCombatScenario(World, OutPlayer, OutEnemies, Count, Distance)` - Full scenario

**Combat Data:**
- `CreateTestAttack(Type)` - Create attack data asset
- `CreateTestComboChain(Length, Type)` - Create combo chain
- `CreateTestHitInfo(Attacker, Damage, Direction, AttackData)` - Create hit info

**Utilities:**
- `DealLethalDamage(Target, Attacker)` - Kill a character
- `SetCharacterHealth(Character, Health)` - Set health directly

### 3. Recompile and run

## Notes

- All tests are independent and clean up after themselves
- Tests use `TObjectPtr<>.Get()` for `TestNotNull` calls
- Tests excluded from shipping builds (UncookedOnly module type)
- Each test verifies specific design principles from architecture docs

## Related Documentation

- `docs/SYSTEM_PROMPT.md` - Core design principles tested
- `docs/ARCHITECTURE_QUICK.md` - Default values validated by tests
- `docs/TROUBLESHOOTING.md` - Common issues tests catch

---

**Test Suite Status**: ✅ All 126 tests passing (as of 2025-01-29)
