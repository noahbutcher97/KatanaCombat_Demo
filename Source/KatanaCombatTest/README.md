# KatanaCombat Test Suite

Automated tests for the KatanaCombat combat system.

For reusable PIE capture, motion/telemetry analysis, and frame review outside individual tests, see [Combat capture and analysis](../../docs/guides/COMBAT_CAPTURE_AND_ANALYSIS.md). The `KatanaCombat.Capture.*` suite exercises the same recorder used by the PIE console commands.

Capture engine code is owned by the separate [Animation Analysis dependency](../../Tools/AnimationAnalysis/README.md).
Run its setup command before building a fresh project checkout.
Project capture tests exercise its Katana compatibility adapter. The separate
`Tools/AnimationAnalysis/verify_unreal_host.py` verifier builds the copied plugin in
a minimal host and runs `AnimationAnalysis.Capture.Portability.*` controls without Katana
modules/assets. Those host tests are not part of the normal project test module.

`KatanaCombat.Capture.PIE.ThirdPersonAsyncAPI` verifies the opt-in asynchronous
recorder through Katana's C++ adapter with an explicitly configured diagnostic view.
Run it with D3D11 rendering; under NullRHI it records a deferral, not GPU evidence.
The existing console/API observation variants retain synchronous compatibility.

`python Tools/CombatCapture/run_scenario.py --map all --variant all --mode rendered` builds and runs completed/interrupted finisher input/recovery scenarios on both project maps, then publishes per-assertion evaluations. The driver uses public gameplay and Enhanced Input interfaces with active bystanders. Rendered evidence eligibility and calibrated displacement checks are separate from gameplay assertions. See the guide for repeated runs, fixed render resolution, source/asset identity, explicit references and capture-overhead controls.

## Module Configuration

Additional reusable scenarios and paired authoring checks:

```powershell
python Tools/CombatCapture/run_scenario.py --scenario Tools/CombatCapture/scenarios/hold-release-recovery.json --map all --variant all --mode rendered
python Tools/CombatCapture/evaluate_pair.py --profile Tools/CombatCapture/pairs/finisher-contact.json
```

`KatanaCombat.Capture.Scenarios.HoldReleaseRecovery.*` observes real hold notifies, all four directional releases, competing input, movement restoration and fresh attack ownership. `KatanaCombat.Editor.PairedEvaluation.*` validates contact intent, alignment budgets and shared preview/commandlet sampling. See [Paired animation evaluation](../../docs/guides/PAIRED_ANIMATION_EVALUATION.md) for capture comparison, profile format and unmeasured capabilities.

- **Type**: `UncookedOnly` - Excluded from shipping builds
- **Dependencies**: KatanaCombat, UnrealEd
- **Location**: `Source/KatanaCombatTest/`

## Test Coverage

The groups below describe the original core coverage; newer defense, AI, alignment, validation, and telemetry suites extend them. Use the baseline runner for current counts because Unreal's expanded automation result lines differ from hand-maintained totals.

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

#### 15. Action/Reaction Telemetry Tests (`ActionReactionTelemetryTests.cpp`)
- Verifies bounded component-owned retention and reset
- Correlates physical input, queue entries, action execution, holds, movement locks, and montage callbacks
- Verifies stable CSV fields, actor snapshots, and console controls without changing gameplay decisions

**Path**: `KatanaCombat.ActionReaction.Telemetry.*`

## Running Tests

### In Editor

1. Open **Session Frontend** (Window → Developer Tools → Session Frontend)
2. Go to **Automation** tab
3. Filter for "KatanaCombat"
4. Select tests to run
5. Click **Start Tests**

### Command Line

**Preferred Codex/agent baseline:**
```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File "Tools\Codex\run-agent-baseline.ps1"
```

This builds `KatanaCombatEditor Win64 Development`, runs all `KatanaCombat` automation tests with `;Quit`, writes timestamped logs under `Saved/Logs/`, and exits nonzero on build or automation failure.

**Run all tests:**
```powershell
"C:\Program Files\Epic Games\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "D:\UnrealProjects\5.6\KatanaCombat\KatanaCombat.uproject" -ExecCmds="Automation RunTests KatanaCombat;Quit" -NullRHI -NoSplash -Unattended -nopause -stdout
```

**Run specific test category:**
```powershell
"C:\Program Files\Epic Games\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "KatanaCombat.uproject" -ExecCmds="Automation RunTests KatanaCombat.DeathSystem" -NullRHI
```

**Check results in log:**
```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File ".agents\skills\katana-verify\scripts\summarize-automation-log.ps1"
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

**Test Suite Status**: GREEN command-line baseline on 2026-06-20: 368 completed automation result lines, 0 failures/errors.
