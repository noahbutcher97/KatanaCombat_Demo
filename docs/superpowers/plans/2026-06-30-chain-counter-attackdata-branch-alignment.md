# Chain Counter And AttackData Branch Alignment Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make `feature/paired-animation-component` satisfy its corrected branch intent: production Chain Counter behavior, paired-animation runtime proof, and AttackData/montage migration tooling that validates the authored assets needed by that behavior.

**Architecture:** `UPairedAnimationComponent` owns finishers, counters, parry/counter chain state, paired partner state, and paired effects. `UCombatComponent` remains the input/action queue owner and delegates paired/counter actions to `UPairedAnimationComponent`. Editor-only AttackData migration stays in `Source/KatanaCombatEditor/` and must produce audit/plan/apply evidence without forcing package saves.

**Tech Stack:** Unreal Engine 5.6 C++, KatanaCombat runtime/editor/test modules, Unreal Automation tests, `KatanaAssetMigration` commandlet.

## Global Constraints

- Do not stage, revert, delete, rename, resave, or mass-add `Content/` assets unless the user explicitly authorizes asset mutation.
- Keep editor-only dependencies out of `Source/KatanaCombat/`.
- Preserve combat invariants: phases are exclusive; windows may overlap; input is always buffered; parry checks the attacker's parry window from defender-side logic; hold checks button state at the window boundary.
- Treat Chain Counter as canonical branch behavior, not experimental.
- Treat AttackData notify migration as in-scope for this branch because it prepares authored montages for the runtime behavior.
- Do not claim asset behavior without Editor, commandlet, UEMCP, or automation evidence.
- Run focused automation before broad automation; use `.agents/skills/katana-verify/scripts/summarize-automation-log.ps1` if command-line automation does not exit cleanly.

---

## Spec Reconciliation Checklist

Complete this checklist before starting Task 1. The purpose is to reconcile the intended behavior, source proof, asset proof, and documentation contract so implementation does not optimize around stale tests or aspirational docs.

- [ ] **Canonical runtime contract:** `docs/specs/PAIRED_ANIMATION_SPEC.md` states the shipped Chain path as Block press -> parryable attacker selection -> public `TryCounter()` -> active Chain context -> attack input -> counter step -> paired completion -> finisher or cancel.
- [ ] **Component ownership:** `docs/architecture/ARCHITECTURE_QUICK.md` states that `UCombatComponent` owns input capture, queue ownership, and attack-data resolution, while `UPairedAnimationComponent` owns Chain state, retained target/context, paired counter/finisher execution, and cleanup.
- [ ] **Active Chain context schema:** the spec names the runtime context fields before implementation: parried target, source attack metadata, selected counter `UAttackData`, resolved `CounterData`, resolved `FinisherData`, current Chain state, timeout handle, and paired-continuation flags.
- [ ] **Counter data resolution:** the spec defines the priority order as selected `UAttackData::CounterData`, then attacker notify `SpecificCounterData` only when explicitly allowed, then non-paired counter fallback.
- [ ] **Damage and lethality:** the spec states that Chain counter steps are nonlethal by default and the finisher step owns lethal damage; any `UPairedAnimationData::bIsLethal` exception for counter reaction types must be explicit.
- [ ] **Completion and cancel semantics:** the spec lists timeout, montage interruption, paired cancel, partner death, owner death, invalid target, failed montage start, and normal completion as context-clearing exits.
- [ ] **AttackData migration scope:** `docs/guides/HEADLESS_ASSET_MIGRATIONS.md` and the plan agree that parry/counter/paired support starts as readiness reporting, not broad automatic seeding or package saves.
- [ ] **Asset proof requirements:** the plan names the minimum proof set: one parryable enemy montage, one counter-capable player attack, valid `CounterData`, valid `FinisherData`, paired sync/collision notifies, and read-only commandlet/editor evidence.
- [ ] **Docs truth alignment:** `CLAUDE.md`, `docs/specs/PAIRED_ANIMATION_SPEC.md`, `docs/architecture/ARCHITECTURE_QUICK.md`, `docs/plans/gap-tracker.md`, and `docs/handoffs/2026-06-28-original-branch-intent-satisfaction-audit.md` use the same wording for Chain Counter scope, AttackData migration scope, and remaining asset proof.
- [ ] **Dirty WIP classification:** branch-critical runtime/editor/test/docs work, asset-proof candidates, unrelated imports/reorgs, and blocked merge-risk content changes are classified before any commit or package-save step.

---

## Corrected Acceptance Scope

This branch is complete only when all of these are true:

1. Block press during an enemy attacker-side `ParryWindow` starts Chain mode through the public input path and consumes the Block input on success.
2. `TryCounter()` can start Chain mode from an attacker-side `ParryWindow`.
3. Chain Counter uses a real state path: `ParryActive -> CounterWindow -> CounterActive -> FinisherReady -> paired finisher/counter completion or explicit cancel`.
4. Attack input during Chain `CounterWindow` advances the chain through `UPairedAnimationComponent`, not the normal attack queue.
5. Counter-specific paired animation data has one deterministic ownership rule: selected `UAttackData::CounterData` wins, attacker notify `SpecificCounterData` is a contextual fallback only when explicitly allowed, then the system falls back to non-paired counter behavior.
6. Counter-step damage is nonlethal by default; the finisher step is the lethal paired animation unless data explicitly says otherwise.
7. All Chain exits clear active context: timeout, montage interruption, paired cancel, partner death, owner death, invalid target, failed montage start, and normal completion.
8. Stale Chain tests that directly prove protected helper transitions are removed or rewritten after replacement public-flow tests exist; primitive tests that still defend valid contracts remain.
9. AttackData migration audits and plans the canonical notifies required by current runtime: phase transitions, hold start only when applicable, parry/counter windows when configured, and paired sync/collision requirements for paired assets.
10. Focused tests pass for `KatanaCombat.CounterSystem`, `KatanaCombat.PairedAnimation`, `KatanaCombat.Editor.AttackDataTools`, and `KatanaCombat.Editor.AssetMigration`.
11. Real assets are audited in read-only commandlet/editor mode before any package-save pass.

## File Structure

- Modify `Source/KatanaCombat/Public/Core/PairedAnimationComponent.h`
  - Add public Chain helpers used by `UCombatComponent`.
  - Add active chain context fields and accessors.
  - Add a helper to build counter context from parryable enemies.
- Modify `Source/KatanaCombat/Private/Core/PairedAnimationComponent.cpp`
  - Fix Chain target selection in `TryCounter()`.
  - Implement Chain context capture and explicit state transitions.
  - Replace immediate skip-over behavior with data-driven paired counter/finisher transitions.
- Modify `Source/KatanaCombat/Private/Core/CombatComponent.cpp`
  - Route successful Block press during Chain-eligible parry windows to `TryCounter()` before normal block/queue behavior.
  - Resolve Chain attack input through `GetAttackForInput()` and route it to `TryAdvanceChainCounter(UAttackData*)`.
  - Keep normal input queue behavior unchanged outside Chain windows.
- Modify `Source/KatanaCombatEditor/Public/AttackDataNotifyGenerationService.h`
  - Expand analysis/plan structs with branch-readiness fields for parry/counter windows and paired animation references.
- Modify `Source/KatanaCombatEditor/Private/AttackDataNotifyGenerationService.cpp`
  - Audit existing parry/counter notifies.
  - Report paired animation data readiness without mutating assets unless apply mode is selected.
- Modify `Source/KatanaCombatEditor/Private/Commandlets/Operations/AttackDataNotifyMigrationOperation.cpp`
  - Add new analysis fields to migration rows.
- Modify `Source/KatanaCombatTest/Private/CounterSystemTests.cpp`
  - Add failing-first tests for Block-input Chain entry, public `TryCounter()` Chain entrypoint, attack-input advance, target retention, paired completion handoff, nonlethal counter damage, and cancel cleanup.
  - Retire or rewrite stale Chain tests that directly call protected helper paths once replacement public-flow coverage exists.
- Modify `Source/KatanaCombatTest/Private/AttackDataEditorToolsTests.cpp`
  - Add tests for canonical notify analysis that preserves current parry/counter windows and reports paired data.
- Modify `Source/KatanaCombatTest/Private/KatanaAssetMigrationTests.cpp`
  - Add report-field tests for the expanded migration contract.
- Modify `CLAUDE.md`, `docs/specs/PAIRED_ANIMATION_SPEC.md`, `docs/architecture/ARCHITECTURE_QUICK.md`, `docs/guides/HEADLESS_ASSET_MIGRATIONS.md`, and `docs/plans/gap-tracker.md`
  - Align docs with Chain Counter as required branch behavior, AttackData migration as in-scope tooling, and remaining asset proof as an explicit blocker.
- Modify `docs/handoffs/2026-06-28-original-branch-intent-satisfaction-audit.md`
  - Replace the earlier "decide whether Chain is in scope" language with the corrected decision.
- Create `docs/handoffs/2026-06-30-branch-wip-lane-manifest.md`
  - Classify current dirty-tree work into branch-critical source/docs, asset-proof candidates, unrelated imports/reorgs, and merge-blocked content churn.

---

## Test Contract Policy

Do not keep tests that make a false production claim. Chain tests may call internals only for narrow primitive coverage and must be named as such. Any test whose purpose is "Chain Counter works" must enter through one of the real public paths:

- Block input through `UCombatComponent::OnInputEvent(EInputType::Block, EInputEventType::Press)`.
- Public `UPairedAnimationComponent::TryCounter()`.
- Attack input through `UCombatComponent::OnInputEvent(EInputType::LightAttack/HeavyAttack, EInputEventType::Press)` while Chain is waiting for attack input.
- Montage completion/cancel paths through `CompletePairedAnimation()` or `CancelPairedAnimation()` once paired counter data is active.

Keep primitive tests for parry window toggling, parryable enemy discovery, AC3 fallback, and context preservation. Replace or remove stale Chain tests that directly call `TryCounter_ChainMode()` or `ExecuteChainCounterAttack()` and then claim end-to-end Chain behavior.

### Task 0: Complete Spec Reconciliation Gate

**Files:**
- Modify: `CLAUDE.md`
- Modify: `docs/specs/PAIRED_ANIMATION_SPEC.md`
- Modify: `docs/architecture/ARCHITECTURE_QUICK.md`
- Modify: `docs/guides/HEADLESS_ASSET_MIGRATIONS.md`
- Modify: `docs/plans/gap-tracker.md`
- Modify: `docs/handoffs/2026-06-28-original-branch-intent-satisfaction-audit.md`
- Create: `docs/handoffs/2026-06-30-branch-wip-lane-manifest.md`

**Interfaces:**
- Consumes: `docs/handoffs/2026-06-30-chain-counter-attackdata-empirical-audit.md`, `docs/handoffs/2026-06-30-chain-counter-attackdata-design-audit.md`, current `git status --short --branch`.
- Produces: reconciled docs that state the same runtime contract, component ownership, migration scope, asset proof requirements, and dirty-lane boundary before source implementation begins.

- [ ] **Step 1: Update paired-animation spec with the canonical Chain contract**

In `docs/specs/PAIRED_ANIMATION_SPEC.md`, replace aspirational Chain/counter wording with this concrete section under Runtime Flow:

```markdown
### Chain Counter Runtime Contract

Chain Counter is required branch behavior. It is not experimental and is not accepted through protected helper calls.

Runtime flow:
1. Defender presses Block.
2. `UCombatComponent` delegates to `UPairedAnimationComponent::TryCounter()`.
3. Chain mode selects a parryable attacker from attacker-side `AnimNotifyState_ParryWindow`.
4. `UPairedAnimationComponent` stores an active Chain context containing parried target, source attack metadata, selected counter `UAttackData`, resolved `CounterData`, resolved `FinisherData`, current Chain state, timeout handle, and paired-continuation flags.
5. Light or Heavy input while Chain is waiting resolves `UAttackData` through `UCombatComponent::GetAttackForInput()` and calls `UPairedAnimationComponent::TryAdvanceChainCounter(UAttackData*)`.
6. The counter step uses selected `UAttackData::CounterData` first, attacker notify `SpecificCounterData` only when explicitly allowed, then non-paired counter fallback.
7. Counter paired steps are nonlethal by default. The finisher step owns lethal damage unless counter data explicitly opts into lethal behavior and validation reports that exception.
8. Paired counter completion either auto-continues to finisher with stored context or deliberately enters an unblocked `FinisherReady` state.
9. Timeout, montage interruption, paired cancel, partner death, owner death, invalid target, failed montage start, and normal completion clear Chain context.
```

- [ ] **Step 2: Update architecture ownership**

In `docs/architecture/ARCHITECTURE_QUICK.md`, add or update a Chain ownership section:

```markdown
### Chain Counter Ownership

- `UCombatComponent` owns input capture, queue ownership, and attack-data resolution.
- `UCombatComponent` must not enqueue successful Chain Block/attack inputs.
- `UPairedAnimationComponent` owns Chain state, retained target/context, paired counter execution, paired finisher execution, and cleanup.
- The public Chain advance API is `TryAdvanceChainCounter(UAttackData* SelectedAttackData)`. Low-level state helpers remain protected/internal unless a test explicitly names them as internal primitive coverage.
```

- [ ] **Step 3: Update CLAUDE current-status caveats**

In `CLAUDE.md`, revise the Chain Counter status language to:

```markdown
| Counter Chain Mode | Canonical but incomplete | Must be proven through public Block/attack input flow, active Chain context, paired completion handoff, and asset readiness. Protected helper tests are not enough. |
| SpecificCounterData Wiring | In scope | Resolve selected `UAttackData::CounterData` first, attacker notify `SpecificCounterData` only as an explicit fallback, then non-paired fallback. |
```

- [ ] **Step 4: Update migration guide scope**

In `docs/guides/HEADLESS_ASSET_MIGRATIONS.md`, add:

```markdown
## Counter And Paired Readiness Reporting

Parry, counter, and paired-animation support starts as readiness reporting. Do not auto-seed parry/counter windows or save packages from this operation until an audit report and reviewed target list identify exact assets and missing fields.

Readiness rows should report `CounterData`, `FinisherData`, parry/counter window presence, paired sync/collision notify presence, paired montage section validity, and lethal counter-data warnings.
```

- [ ] **Step 5: Update gap tracker and branch audit**

In `docs/plans/gap-tracker.md`, set gaps 26.1, 26.2, 26.3, 26.5, and 26.6 to "In progress - canonical branch scope" and list blockers:

```markdown
Blockers: public Block-input entry, attack-input Chain advance, active Chain context, paired counter completion handoff, nonlethal counter semantics, readiness reporting, and asset proof.
```

In `docs/handoffs/2026-06-28-original-branch-intent-satisfaction-audit.md`, replace any "decide whether Chain is in scope" wording with:

```markdown
Chain Counter is in scope for this branch. The branch is not ready until public runtime flow, paired completion handoff, readiness reporting, and asset proof are complete.
```

- [ ] **Step 6: Create dirty WIP lane manifest**

Create `docs/handoffs/2026-06-30-branch-wip-lane-manifest.md` with this structure:

```markdown
# Branch WIP Lane Manifest

Date: 2026-06-30
Branch: `feature/paired-animation-component`

## Branch-Critical Source And Docs

- Runtime Chain Counter source files.
- Editor migration/readiness reporting source files.
- Automation tests for public Chain flow and migration reports.
- Reconciled spec/docs listed in the implementation plan.

## Asset-Proof Candidates

- Counter-capable player AttackData assets.
- Parryable enemy attack montages.
- Paired counter and finisher data/montages.
- Maps or Blueprint defaults needed for one live proof path.

## Unrelated Imports Or Reorg Candidates

- Large third-party asset imports not required for Chain proof.
- Content folder renames/deletions not required for one proof path.

## Merge-Blocked Content Churn

- Any binary asset deletion, move, rename, or package save not tied to the reviewed proof path.

## Required Before Commit Or Save

- `git status --short --branch`
- `git diff --name-status`
- `git ls-files --others --exclude-standard`
- User approval for any package-save or mass asset lane.
```

- [ ] **Step 7: Run reconciliation sanity checks**

Run:

```powershell
rg -n "outside branch scope|decide whether Chain|protected helper tests are enough|auto-seed parry|auto-seed counter|Chain.*deferred|Chain.*not in scope|Chain.*optional|Chain.*prototype" CLAUDE.md docs/specs/PAIRED_ANIMATION_SPEC.md docs/architecture/ARCHITECTURE_QUICK.md docs/guides/HEADLESS_ASSET_MIGRATIONS.md docs/plans/gap-tracker.md docs/handoffs/2026-06-28-original-branch-intent-satisfaction-audit.md
```

Expected: no stale wording that contradicts canonical Chain scope, reporting-first migration scope, or public-flow proof requirements.

- [ ] **Step 8: Commit reconciled docs**

```powershell
git add CLAUDE.md docs/specs/PAIRED_ANIMATION_SPEC.md docs/architecture/ARCHITECTURE_QUICK.md docs/guides/HEADLESS_ASSET_MIGRATIONS.md docs/plans/gap-tracker.md docs/handoffs/2026-06-28-original-branch-intent-satisfaction-audit.md docs/handoffs/2026-06-30-branch-wip-lane-manifest.md
git commit -m "Reconcile Chain counter branch spec"
```

Do not start Task 1 until this task is complete.

---

### Task 1: Fix Public Chain Counter Entry Point

**Files:**
- Modify: `Source/KatanaCombat/Public/Core/PairedAnimationComponent.h`
- Modify: `Source/KatanaCombat/Private/Core/PairedAnimationComponent.cpp`
- Test: `Source/KatanaCombatTest/Private/CounterSystemTests.cpp`

**Interfaces:**
- Consumes: existing `FindParryableEnemy()`, `FindCounterableEnemy()`, `GetEnemyCounterContext(AActor*)`, `TryCounter_ChainMode(const FCounterContext&)`.
- Produces:
  - `FCounterContext GetEnemyParryContext(AActor* Enemy) const`
  - `EChainCounterState GetChainState() const`
  - `bool IsChainCounterWaitingForAttack() const`

- [ ] **Step 1: Add failing tests for public Chain targeting**

Add these tests to `Source/KatanaCombatTest/Private/CounterSystemTests.cpp` after `FCounter_FindParryableEnemy`:

```cpp
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCounter_TryCounterChainUsesParryWindow,
	"KatanaCombat.CounterSystem.TryCounter.ChainUsesParryWindow",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCounter_TryCounterChainUsesParryWindow::RunTest(const FString& Parameters)
{
	UWorld* World = FCombatTestHelpers::CreateTestWorld();
	UCombatComponent* PlayerCombat = nullptr;
	APlayerCharacter* Player = FCombatTestHelpers::CreateTestCharacterWithCombat(World, PlayerCombat);
	AEnemyCharacter* Enemy = FCombatTestHelpers::CreateTestEnemyCharacter(World, FVector(150.0f, 0.0f, 0.0f));

	if (!Player || !Player->PairedAnimationComponent || !Enemy || !Enemy->PairedAnimationComponent)
	{
		AddError(TEXT("Failed to create Chain counter test actors"));
		FCombatTestHelpers::DestroyTestWorld(World);
		return false;
	}

	Enemy->PairedAnimationComponent->SetParryWindowActive(true);

	const bool bStarted = Player->PairedAnimationComponent->TryCounter();

	TestTrue(TEXT("TryCounter should start Chain mode from an enemy parry window"), bStarted);
	TestEqual(TEXT("Chain should wait for attack input after parry"),
		static_cast<int32>(Player->PairedAnimationComponent->GetChainState()),
		static_cast<int32>(EChainCounterState::CounterWindow));

	Player->PairedAnimationComponent->CancelChainCounter();
	Enemy->PairedAnimationComponent->SetParryWindowActive(false);
	FCombatTestHelpers::DestroyTestWorld(World);
	return true;
}
```

- [ ] **Step 2: Run the new test to verify it fails**

Run:

```powershell
&"C:\Program Files\Epic Games\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "D:\UnrealProjects\5.6\KatanaCombat\KatanaCombat.uproject" -ExecCmds="Automation RunTests KatanaCombat.CounterSystem.TryCounter.ChainUsesParryWindow;Quit" -unattended -nopause -NullRHI -nosplash -stdout
```

Expected: fail because `GetChainState()` does not exist yet, and after adding it the current `TryCounter()` still selects `FindCounterableEnemy()`.

- [ ] **Step 3: Add Chain state accessors**

In `Source/KatanaCombat/Public/Core/PairedAnimationComponent.h`, add these methods in the Counter System API block:

```cpp
UFUNCTION(BlueprintPure, Category = "Combat|Counter")
EChainCounterState GetChainState() const { return ChainState; }

UFUNCTION(BlueprintPure, Category = "Combat|Counter")
bool IsChainCounterWaitingForAttack() const { return ChainState == EChainCounterState::CounterWindow; }
```

Move `CancelChainCounter()` from protected to public so `UCombatComponent` and tests can cancel a live chain intentionally:

```cpp
/** Cancel the chain counter mid-sequence (timeout, damage taken, etc.) */
UFUNCTION(BlueprintCallable, Category = "Combat|Counter")
void CancelChainCounter();
```

- [ ] **Step 4: Add parry context helper**

In `Source/KatanaCombat/Public/Core/PairedAnimationComponent.h`, add:

```cpp
UFUNCTION(BlueprintPure, Category = "Combat|Counter")
FCounterContext GetEnemyParryContext(AActor* Enemy) const;
```

In `Source/KatanaCombat/Private/Core/PairedAnimationComponent.cpp`, implement it after `GetEnemyCounterContext`:

```cpp
FCounterContext UPairedAnimationComponent::GetEnemyParryContext(AActor* Enemy) const
{
	FCounterContext Context;
	if (!Enemy)
	{
		return Context;
	}

	const UPairedAnimationComponent* EnemyPaired = Enemy->FindComponentByClass<UPairedAnimationComponent>();
	const UCombatComponent* EnemyCombat = Enemy->FindComponentByClass<UCombatComponent>();
	const bool bEnemyInParryWindow = EnemyPaired
		? EnemyPaired->IsInParryWindow()
		: (EnemyCombat && EnemyCombat->IsInParryWindow());
	if (!bEnemyInParryWindow)
	{
		return Context;
	}

	Context.Attacker = Enemy;

	if (EnemyCombat)
	{
		if (const UAttackData* EnemyAttack = EnemyCombat->GetCurrentAttack())
		{
			Context.AttackType = EnemyAttack->AttackType;
			Context.SpecificCounterData = EnemyAttack->CounterData;
		}
	}

	return Context;
}
```

- [ ] **Step 5: Fix `TryCounter()` target/context selection**

Replace the target/context section in `UPairedAnimationComponent::TryCounter()` with:

```cpp
const bool bUseChainMode = CounterMode == ECounterSystemMode::Chain;
AActor* Target = bUseChainMode ? FindParryableEnemy() : FindCounterableEnemy();
if (!Target)
{
	UE_LOG(LogPairedAnim, Verbose, TEXT("[COUNTER] TryCounter failed: No valid target found"));
	return false;
}

FCounterContext Context = bUseChainMode ? GetEnemyParryContext(Target) : GetEnemyCounterContext(Target);
if (!Context.Attacker)
{
	UE_LOG(LogPairedAnim, Warning, TEXT("[COUNTER] TryCounter failed: Invalid counter context"));
	return false;
}
```

Keep the existing `switch (CounterMode)` block.

- [ ] **Step 6: Run focused test**

Run:

```powershell
&"C:\Program Files\Epic Games\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "D:\UnrealProjects\5.6\KatanaCombat\KatanaCombat.uproject" -ExecCmds="Automation RunTests KatanaCombat.CounterSystem.TryCounter.ChainUsesParryWindow;Quit" -unattended -nopause -NullRHI -nosplash -stdout
```

Expected: pass.

- [ ] **Step 7: Commit**

```powershell
git add Source/KatanaCombat/Public/Core/PairedAnimationComponent.h Source/KatanaCombat/Private/Core/PairedAnimationComponent.cpp Source/KatanaCombatTest/Private/CounterSystemTests.cpp
git commit -m "Fix Chain counter public entrypoint"
```

---

### Task 2: Make Chain Counter Start And Advance From Input

**Files:**
- Modify: `Source/KatanaCombat/Public/Core/PairedAnimationComponent.h`
- Modify: `Source/KatanaCombat/Private/Core/PairedAnimationComponent.cpp`
- Modify: `Source/KatanaCombat/Private/Core/CombatComponent.cpp`
- Test: `Source/KatanaCombatTest/Private/CounterSystemTests.cpp`

**Interfaces:**
- Consumes: `TryCounter()`, `IsChainCounterWaitingForAttack()`, `GetAttackForInput(EInputType)`, `EInputType`.
- Produces:
  - `bool TryAdvanceChainCounter(UAttackData* SelectedAttackData)`
  - Block input starts Chain parry when an enemy is in an attacker-side parry window.
  - Attack input during Chain `CounterWindow` resolves selected `UAttackData` in `UCombatComponent`, calls `TryAdvanceChainCounter(UAttackData*)`, and does not enqueue a normal attack when that call succeeds.

- [ ] **Step 1: Add failing test for Block-input Chain start**

Add this test after `FCounter_TryCounterChainUsesParryWindow`:

```cpp
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCounter_BlockInputStartsChainParry,
	"KatanaCombat.CounterSystem.Input.BlockStartsChainParry",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCounter_BlockInputStartsChainParry::RunTest(const FString& Parameters)
{
	UWorld* World = FCombatTestHelpers::CreateTestWorld();
	UCombatComponent* PlayerCombat = nullptr;
	APlayerCharacter* Player = FCombatTestHelpers::CreateTestCharacterWithCombat(World, PlayerCombat);
	AEnemyCharacter* Enemy = FCombatTestHelpers::CreateTestEnemyCharacter(World, FVector(150.0f, 0.0f, 0.0f));

	if (!PlayerCombat || !Player || !Player->PairedAnimationComponent || !Enemy || !Enemy->PairedAnimationComponent)
	{
		AddError(TEXT("Failed to create Block input Chain test actors"));
		FCombatTestHelpers::DestroyTestWorld(World);
		return false;
	}

	Enemy->PairedAnimationComponent->SetParryWindowActive(true);

	PlayerCombat->OnInputEvent(EInputType::Block, EInputEventType::Press);

	TestEqual(TEXT("Block press should start Chain parry and enter CounterWindow"),
		static_cast<int32>(Player->PairedAnimationComponent->GetChainState()),
		static_cast<int32>(EChainCounterState::CounterWindow));
	TestEqual(TEXT("Successful Chain parry should consume Block input without queueing"),
		PlayerCombat->GetPendingActionCount(),
		0);

	Player->PairedAnimationComponent->CancelChainCounter();
	Enemy->PairedAnimationComponent->SetParryWindowActive(false);
	FCombatTestHelpers::DestroyTestWorld(World);
	return true;
}
```

- [ ] **Step 2: Run the Block-input test to verify it fails**

Run:

```powershell
&"C:\Program Files\Epic Games\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "D:\UnrealProjects\5.6\KatanaCombat\KatanaCombat.uproject" -ExecCmds="Automation RunTests KatanaCombat.CounterSystem.Input.BlockStartsChainParry;Quit" -unattended -nopause -NullRHI -nosplash -stdout
```

Expected: fail because Block input does not route to `TryCounter()`.

- [ ] **Step 3: Route successful Block press to Chain parry**

In `UCombatComponent::OnInputEvent`, immediately after the `CanProcessInput` check and before directional sampling, add:

```cpp
if (CachedPairedAnimComp &&
	EventType == EInputEventType::Press &&
	InputType == EInputType::Block &&
	CachedPairedAnimComp->TryCounter())
{
	return;
}
```

This preserves normal block behavior when no parryable enemy exists.

- [ ] **Step 4: Run the Block-input test**

Run:

```powershell
&"C:\Program Files\Epic Games\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "D:\UnrealProjects\5.6\KatanaCombat\KatanaCombat.uproject" -ExecCmds="Automation RunTests KatanaCombat.CounterSystem.Input.BlockStartsChainParry;Quit" -unattended -nopause -NullRHI -nosplash -stdout
```

Expected: pass.

- [ ] **Step 5: Add failing test for input-driven chain advance**

Add this test after `FCounter_ChainCounterAttack`:

```cpp
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCounter_ChainAttackInputAdvancesCounter,
	"KatanaCombat.CounterSystem.ChainAttackInputAdvancesCounter",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCounter_ChainAttackInputAdvancesCounter::RunTest(const FString& Parameters)
{
	UWorld* World = FCombatTestHelpers::CreateTestWorld();
	UCombatComponent* PlayerCombat = nullptr;
	APlayerCharacter* Player = FCombatTestHelpers::CreateTestCharacterWithCombat(World, PlayerCombat);
	AEnemyCharacter* Enemy = FCombatTestHelpers::CreateTestEnemyCharacter(World, FVector(150.0f, 0.0f, 0.0f));

	if (!PlayerCombat || !Player || !Player->PairedAnimationComponent || !Enemy || !Enemy->PairedAnimationComponent)
	{
		AddError(TEXT("Failed to create Chain input test actors"));
		FCombatTestHelpers::DestroyTestWorld(World);
		return false;
	}

	UAttackConfiguration* AttackConfig = Player->CombatSettings ? Player->CombatSettings->GetAttackConfiguration() : nullptr;
	if (!AttackConfig)
	{
		AddError(TEXT("Failed to create attack configuration for Chain input test"));
		FCombatTestHelpers::DestroyTestWorld(World);
		return false;
	}

	UAttackData* LightAttack = FCombatTestHelpers::CreateTestAttack(EAttackType::Light);
	AttackConfig->DefaultLightAttack = LightAttack;

	Enemy->PairedAnimationComponent->SetParryWindowActive(true);
	PlayerCombat->OnInputEvent(EInputType::Block, EInputEventType::Press);
	TestTrue(TEXT("Chain should wait for attack input"), Player->PairedAnimationComponent->IsChainCounterWaitingForAttack());

	PlayerCombat->OnInputEvent(EInputType::LightAttack, EInputEventType::Press);

	TestFalse(TEXT("Attack input should leave the waiting state"),
		Player->PairedAnimationComponent->IsChainCounterWaitingForAttack());
	TestEqual(TEXT("Successful Chain attack input should not queue a normal attack"),
		PlayerCombat->GetPendingActionCount(),
		0);

	Player->PairedAnimationComponent->CancelChainCounter();
	Enemy->PairedAnimationComponent->SetParryWindowActive(false);
	FCombatTestHelpers::DestroyTestWorld(World);
	return true;
}
```

- [ ] **Step 6: Run the new attack-input test to verify it fails**

Run:

```powershell
&"C:\Program Files\Epic Games\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "D:\UnrealProjects\5.6\KatanaCombat\KatanaCombat.uproject" -ExecCmds="Automation RunTests KatanaCombat.CounterSystem.ChainAttackInputAdvancesCounter;Quit" -unattended -nopause -NullRHI -nosplash -stdout
```

Expected: fail because attack input follows the normal queue and does not call `TryAdvanceChainCounter(UAttackData*)`.

- [ ] **Step 7: Add public Chain advance API without exposing the low-level executor**

In `Source/KatanaCombat/Public/Core/PairedAnimationComponent.h`, add this public method in the Counter System API block:

```cpp
UFUNCTION(BlueprintCallable, Category = "Combat|Counter")
bool TryAdvanceChainCounter(UAttackData* SelectedAttackData);
```

Keep `ExecuteChainCounterAttack()` protected/internal. Add this temporary implementation in `Source/KatanaCombat/Private/Core/PairedAnimationComponent.cpp`; Task 3 will replace it with selected attack-data handling:

```cpp
bool UPairedAnimationComponent::TryAdvanceChainCounter(UAttackData* SelectedAttackData)
{
	if (ChainState != EChainCounterState::CounterWindow)
	{
		return false;
	}

	if (!SelectedAttackData)
	{
		UE_LOG(LogPairedAnim, Verbose, TEXT("[COUNTER-CHAIN] Advancing without selected attack data; Task 3 will make this a hard validation path"));
	}

	return ExecuteChainCounterAttack();
}
```

- [ ] **Step 8: Route attack presses during Chain CounterWindow**

In `UCombatComponent::OnInputEvent`, immediately after the `CanProcessInput` check and before directional sampling, add:

```cpp
if (CachedPairedAnimComp &&
	EventType == EInputEventType::Press &&
	(InputType == EInputType::LightAttack || InputType == EInputType::HeavyAttack) &&
	CachedPairedAnimComp->IsChainCounterWaitingForAttack())
{
	UAttackData* ChainAttackData = GetAttackForInput(InputType);
	if (CachedPairedAnimComp->TryAdvanceChainCounter(ChainAttackData))
	{
		return;
	}
}
```

- [ ] **Step 9: Run focused input tests**

Run:

```powershell
&"C:\Program Files\Epic Games\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "D:\UnrealProjects\5.6\KatanaCombat\KatanaCombat.uproject" -ExecCmds="Automation RunTests KatanaCombat.CounterSystem.Input;Quit" -unattended -nopause -NullRHI -nosplash -stdout
```

Expected: pass.

- [ ] **Step 10: Retire stale direct Chain tests**

After the Block-input and attack-input public-flow tests pass, rewrite or remove Chain tests that directly call `TryCounter_ChainMode()` or `ExecuteChainCounterAttack()` for end-to-end claims. Keep only primitive tests that explicitly name the internal state they prove, for example:

```cpp
// Keep as primitive coverage only if renamed to make the scope explicit:
"KatanaCombat.CounterSystem.Internal.ChainParryTransition"
"KatanaCombat.CounterSystem.Internal.ExecuteChainCounterAttackRequiresCounterWindow"
```

Remove or rewrite tests that assert "Chain counter completes" while the test bypasses public input or treats "finisher failed" as success.

- [ ] **Step 11: Commit**

```powershell
git add Source/KatanaCombat/Public/Core/PairedAnimationComponent.h Source/KatanaCombat/Private/Core/CombatComponent.cpp Source/KatanaCombatTest/Private/CounterSystemTests.cpp
git commit -m "Route Chain counter input"
```

---

### Task 3: Make Chain Counter Preserve Target And Continue Through Authored Counter Data

**Files:**
- Modify: `Source/KatanaCombat/Public/Core/PairedAnimationComponent.h`
- Modify: `Source/KatanaCombat/Private/Core/PairedAnimationComponent.cpp`
- Modify: `Source/KatanaCombat/Private/Core/CombatComponent.cpp`
- Test: `Source/KatanaCombatTest/Private/CounterSystemTests.cpp`

**Interfaces:**
- Consumes: `FCounterContext`, `UAttackData::CounterData`, `UAttackData::FinisherData`, `TryAdvanceChainCounter(UAttackData*)`, `TryStartPairedAnimationWithTarget`.
- Produces:
  - `FCounterContext ActiveChainContext`
  - `TWeakObjectPtr<AActor> ActiveChainTarget`
  - `TObjectPtr<UAttackData> ActiveChainAttackData`
  - `bool bContinueChainAfterCounterPairedAnimation`
  - `bool bAllowNotifyCounterDataFallback`
  - `bool HasActiveChainTarget() const`

- [ ] **Step 1: Add failing target-retention test**

Add:

```cpp
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCounter_ChainStoresParriedTarget,
	"KatanaCombat.CounterSystem.ChainStoresParriedTarget",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCounter_ChainStoresParriedTarget::RunTest(const FString& Parameters)
{
	UWorld* World = FCombatTestHelpers::CreateTestWorld();
	UCombatComponent* PlayerCombat = nullptr;
	APlayerCharacter* Player = FCombatTestHelpers::CreateTestCharacterWithCombat(World, PlayerCombat);
	AEnemyCharacter* Enemy = FCombatTestHelpers::CreateTestEnemyCharacter(World, FVector(150.0f, 0.0f, 0.0f));

	if (!PlayerCombat || !Player || !Player->PairedAnimationComponent || !Enemy || !Enemy->PairedAnimationComponent)
	{
		AddError(TEXT("Failed to create Chain target test actors"));
		FCombatTestHelpers::DestroyTestWorld(World);
		return false;
	}

	Enemy->PairedAnimationComponent->SetParryWindowActive(true);
	PlayerCombat->OnInputEvent(EInputType::Block, EInputEventType::Press);
	TestTrue(TEXT("Chain should retain the parried target"),
		Player->PairedAnimationComponent->HasActiveChainTarget());

	Player->PairedAnimationComponent->CancelChainCounter();
	TestFalse(TEXT("Cancel should clear retained target"),
		Player->PairedAnimationComponent->HasActiveChainTarget());

	Enemy->PairedAnimationComponent->SetParryWindowActive(false);
	FCombatTestHelpers::DestroyTestWorld(World);
	return true;
}
```

- [ ] **Step 2: Add state fields and accessor**

In `Source/KatanaCombat/Public/Core/PairedAnimationComponent.h`, add public accessor:

```cpp
UFUNCTION(BlueprintPure, Category = "Combat|Counter")
bool HasActiveChainTarget() const { return ActiveChainTarget.IsValid(); }

UFUNCTION(BlueprintPure, Category = "Combat|Paired Animation")
bool ShouldTreatPairedAnimationAsLethal(EPairedReactionType ReactionType, const UPairedAnimationData* PairedAnimData) const;
```

Add protected state near `ChainState`:

```cpp
void ClearChainContext();

UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat|Counter")
FCounterContext ActiveChainContext;

UPROPERTY()
TWeakObjectPtr<AActor> ActiveChainTarget;

UPROPERTY()
TObjectPtr<UAttackData> ActiveChainAttackData = nullptr;

bool bContinueChainAfterCounterPairedAnimation = false;

UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Counter")
bool bAllowNotifyCounterDataFallback = false;

UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Counter")
bool bAllowLethalCounterPairedData = false;
```

- [ ] **Step 3: Store and clear chain context**

At the start of successful `TryCounter_ChainMode`, after validating `Owner` and `Context.Attacker`, add:

```cpp
ActiveChainContext = Context;
ActiveChainTarget = Context.Attacker;
```

In `CancelChainCounter()`, after `ChainState = EChainCounterState::None;`, add:

```cpp
ActiveChainContext.Reset();
ActiveChainTarget.Reset();
ActiveChainAttackData = nullptr;
bContinueChainAfterCounterPairedAnimation = false;
```

In `ExecuteChainFinisher()`, after `ChainState = EChainCounterState::None;`, add the same reset.

- [ ] **Step 4: Thread selected attack data through the public Chain advance API**

In `Source/KatanaCombat/Public/Core/PairedAnimationComponent.h`, keep this method public:

```cpp
UFUNCTION(BlueprintCallable, Category = "Combat|Counter")
bool TryAdvanceChainCounter(UAttackData* SelectedAttackData);
```

Change the low-level executor declaration to protected/internal:

```cpp
bool ExecuteChainCounterAttack(UAttackData* ChainAttackData);
```

In `Source/KatanaCombatTest/Private/CounterSystemTests.cpp`, update any remaining direct end-to-end calls so they enter through public flow. Primitive tests may call `TryAdvanceChainCounter(nullptr)` only when their name explicitly says they cover invalid selected attack-data behavior.

In `Source/KatanaCombat/Private/Core/CombatComponent.cpp`, keep the Chain input route from Task 2 passing the resolved attack and only consuming input when advancement succeeds:

```cpp
if (CachedPairedAnimComp &&
	EventType == EInputEventType::Press &&
	(InputType == EInputType::LightAttack || InputType == EInputType::HeavyAttack) &&
	CachedPairedAnimComp->IsChainCounterWaitingForAttack())
{
	UAttackData* ChainAttackData = GetAttackForInput(InputType);
	if (CachedPairedAnimComp->TryAdvanceChainCounter(ChainAttackData))
	{
		return;
	}
}
```

- [ ] **Step 5: Make counter attack use authored paired counter data when present**

Replace the temporary `TryAdvanceChainCounter(UAttackData*)` from Task 2 and the old `ExecuteChainCounterAttack()` with:

```cpp
bool UPairedAnimationComponent::TryAdvanceChainCounter(UAttackData* SelectedAttackData)
{
	if (ChainState != EChainCounterState::CounterWindow)
	{
		return false;
	}

	if (!SelectedAttackData)
	{
		UE_LOG(LogPairedAnim, Warning, TEXT("[COUNTER-CHAIN] Cannot advance: selected attack data is null"));
		return false;
	}

	return ExecuteChainCounterAttack(SelectedAttackData);
}

bool UPairedAnimationComponent::ExecuteChainCounterAttack(UAttackData* ChainAttackData)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ChainTimeoutHandle);
	}

	ActiveChainAttackData = ChainAttackData;
	ChainState = EChainCounterState::CounterActive;

	UPairedAnimationData* CounterPairedData = ChainAttackData ? ChainAttackData->CounterData : nullptr;
	if (!CounterPairedData && bAllowNotifyCounterDataFallback)
	{
		CounterPairedData = ActiveChainContext.SpecificCounterData;
	}

	if (CounterPairedData && ActiveChainTarget.IsValid())
	{
		bContinueChainAfterCounterPairedAnimation = true;
		if (TryStartPairedAnimationWithTarget(ActiveChainTarget.Get(), CounterPairedData, EPairedReactionType::Counter))
		{
			return true;
		}

		bContinueChainAfterCounterPairedAnimation = false;
		UE_LOG(LogPairedAnim, Warning, TEXT("[COUNTER-CHAIN] Authored counter paired animation failed to start; continuing to finisher readiness"));
	}

	ChainState = EChainCounterState::FinisherReady;
	UE_LOG(LogPairedAnim, Log, TEXT("[COUNTER-CHAIN] Counter attack executed. Finisher is ready."));
	return ExecuteChainFinisher();
}
```

- [ ] **Step 6: Continue from paired counter completion into Chain finisher**

At the top of `CompletePairedAnimation()`, immediately after `bCompletingPairedAnimation = true;`, add:

```cpp
const bool bShouldContinueChainAfterCounter =
	bContinueChainAfterCounterPairedAnimation &&
	ChainState == EChainCounterState::CounterActive &&
	ActivePairedReactionType == EPairedReactionType::Counter;
```

Near the end of `CompletePairedAnimation()`, immediately after `bCompletingPairedAnimation = false;`, add:

```cpp
if (bShouldContinueChainAfterCounter)
{
	bContinueChainAfterCounterPairedAnimation = false;
	ChainState = EChainCounterState::FinisherReady;
	ExecuteChainFinisher();
	return;
}
```

- [ ] **Step 7: Make Chain finisher use stored target and attack data**

Replace the first part of `ExecuteChainFinisher()` with:

```cpp
if (ChainState != EChainCounterState::FinisherReady)
{
	return false;
}

UAttackData* ChainAttack = ActiveChainAttackData ? ActiveChainAttackData.Get() : nullptr;

bool bSuccess = false;
if (ChainAttack && ChainAttack->FinisherData && ActiveChainTarget.IsValid())
{
	bSuccess = TryStartPairedAnimationWithTarget(ActiveChainTarget.Get(), ChainAttack->FinisherData, EPairedReactionType::Finisher);
}
else
{
	bSuccess = TryExecuteFinisher(ChainAttack);
}

ChainState = EChainCounterState::None;
ActiveChainContext.Reset();
ActiveChainTarget.Reset();
ActiveChainAttackData = nullptr;
bContinueChainAfterCounterPairedAnimation = false;
```

Keep the existing success/failure logging after this block.

- [ ] **Step 8: Add tests for nonlethal counter policy and paired-cancel cleanup**

Add these tests after `FCounter_ChainStoresParriedTarget`:

```cpp
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCounter_ChainCounterDamagePolicyNonLethalByDefault,
	"KatanaCombat.CounterSystem.ChainCounterDamagePolicyNonLethalByDefault",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCounter_ChainCounterDamagePolicyNonLethalByDefault::RunTest(const FString& Parameters)
{
	UWorld* World = FCombatTestHelpers::CreateTestWorld();
	UCombatComponent* PlayerCombat = nullptr;
	APlayerCharacter* Player = FCombatTestHelpers::CreateTestCharacterWithCombat(World, PlayerCombat);

	if (!Player || !Player->PairedAnimationComponent)
	{
		AddError(TEXT("Failed to create Chain damage policy test actor"));
		FCombatTestHelpers::DestroyTestWorld(World);
		return false;
	}

	UPairedAnimationData* PairedData = NewObject<UPairedAnimationData>(Player);
	PairedData->bIsLethal = true;

	TestFalse(TEXT("Counter paired animations should be nonlethal by default even when data is lethal"),
		Player->PairedAnimationComponent->ShouldTreatPairedAnimationAsLethal(EPairedReactionType::Counter, PairedData));
	TestTrue(TEXT("Finisher paired animations should preserve lethal data"),
		Player->PairedAnimationComponent->ShouldTreatPairedAnimationAsLethal(EPairedReactionType::Finisher, PairedData));

	FCombatTestHelpers::DestroyTestWorld(World);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCounter_ChainPairedCancelClearsContext,
	"KatanaCombat.CounterSystem.ChainPairedCancelClearsContext",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCounter_ChainPairedCancelClearsContext::RunTest(const FString& Parameters)
{
	UWorld* World = FCombatTestHelpers::CreateTestWorld();
	UCombatComponent* PlayerCombat = nullptr;
	APlayerCharacter* Player = FCombatTestHelpers::CreateTestCharacterWithCombat(World, PlayerCombat);
	AEnemyCharacter* Enemy = FCombatTestHelpers::CreateTestEnemyCharacter(World, FVector(150.0f, 0.0f, 0.0f));

	if (!PlayerCombat || !Player || !Player->PairedAnimationComponent || !Enemy || !Enemy->PairedAnimationComponent)
	{
		AddError(TEXT("Failed to create Chain paired-cancel cleanup test actors"));
		FCombatTestHelpers::DestroyTestWorld(World);
		return false;
	}

	Enemy->PairedAnimationComponent->SetParryWindowActive(true);
	PlayerCombat->OnInputEvent(EInputType::Block, EInputEventType::Press);

	TestTrue(TEXT("Chain should retain target before paired cancel"),
		Player->PairedAnimationComponent->HasActiveChainTarget());

	Player->PairedAnimationComponent->CancelPairedAnimation();

	TestEqual(TEXT("Paired cancel should clear Chain state"),
		static_cast<int32>(Player->PairedAnimationComponent->GetChainState()),
		static_cast<int32>(EChainCounterState::None));
	TestFalse(TEXT("Paired cancel should clear retained target"),
		Player->PairedAnimationComponent->HasActiveChainTarget());

	Enemy->PairedAnimationComponent->SetParryWindowActive(false);
	FCombatTestHelpers::DestroyTestWorld(World);
	return true;
}
```

- [ ] **Step 9: Implement lethal policy and shared Chain cleanup**

In `Source/KatanaCombat/Private/Core/PairedAnimationComponent.cpp`, add:

```cpp
bool UPairedAnimationComponent::ShouldTreatPairedAnimationAsLethal(
	EPairedReactionType ReactionType,
	const UPairedAnimationData* PairedAnimData) const
{
	if (!PairedAnimData)
	{
		return false;
	}

	if (ReactionType == EPairedReactionType::Counter && !bAllowLethalCounterPairedData)
	{
		return false;
	}

	return PairedAnimData->bIsLethal;
}

void UPairedAnimationComponent::ClearChainContext()
{
	ChainState = EChainCounterState::None;
	ActiveChainContext.Reset();
	ActiveChainTarget.Reset();
	ActiveChainAttackData = nullptr;
	bContinueChainAfterCounterPairedAnimation = false;
}
```

In `TryStartPairedAnimationWithTarget`, compute the lethal policy before `EnterPairedAnimationState`:

```cpp
const bool bTreatAsLethal = ShouldTreatPairedAnimationAsLethal(ReactionType, PairedAnimData);
if (ReactionType == EPairedReactionType::Counter && PairedAnimData->bIsLethal && !bTreatAsLethal)
{
	UE_LOG(LogPairedAnim, Warning, TEXT("[COUNTER-CHAIN] Counter paired data is authored lethal but runtime policy treats counter steps as nonlethal"));
}

TargetHitReaction->EnterPairedAnimationState(
	PairedAnimData->VictimMontage,
	PairedAnimData->VictimDeathOutcome,
	PairedAnimData->RagdollBlendTime,
	bTreatAsLethal,
	GetOwner());
```

Replace the manual Chain reset blocks in `CancelChainCounter()` and `ExecuteChainFinisher()` with:

```cpp
ClearChainContext();
```

In `CancelPairedAnimation()`, before the final debug log, add:

```cpp
if (ChainState != EChainCounterState::None)
{
	ClearChainContext();
}
```

In `CompletePairedAnimation()`, compute lethal policy once and use it for logging and damage:

```cpp
const bool bTreatAsLethal = ShouldTreatPairedAnimationAsLethal(ActivePairedReactionType, ActivePairedAnimData);
```

Replace `ActivePairedAnimData->bIsLethal` checks in the damage block with `bTreatAsLethal`.

- [ ] **Step 10: Run focused tests**

Run:

```powershell
&"C:\Program Files\Epic Games\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "D:\UnrealProjects\5.6\KatanaCombat\KatanaCombat.uproject" -ExecCmds="Automation RunTests KatanaCombat.CounterSystem.ChainStoresParriedTarget;Quit" -unattended -nopause -NullRHI -nosplash -stdout
```

Then:

```powershell
&"C:\Program Files\Epic Games\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "D:\UnrealProjects\5.6\KatanaCombat\KatanaCombat.uproject" -ExecCmds="Automation RunTests KatanaCombat.CounterSystem.ChainCounterDamagePolicyNonLethalByDefault;Quit" -unattended -nopause -NullRHI -nosplash -stdout
```

Then:

```powershell
&"C:\Program Files\Epic Games\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "D:\UnrealProjects\5.6\KatanaCombat\KatanaCombat.uproject" -ExecCmds="Automation RunTests KatanaCombat.CounterSystem.ChainPairedCancelClearsContext;Quit" -unattended -nopause -NullRHI -nosplash -stdout
```

Then:

```powershell
&"C:\Program Files\Epic Games\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "D:\UnrealProjects\5.6\KatanaCombat\KatanaCombat.uproject" -ExecCmds="Automation RunTests KatanaCombat.CounterSystem;Quit" -unattended -nopause -NullRHI -nosplash -stdout
```

Expected: all targeted tests and the full CounterSystem suite pass.

- [ ] **Step 11: Commit**

```powershell
git add Source/KatanaCombat/Public/Core/PairedAnimationComponent.h Source/KatanaCombat/Private/Core/PairedAnimationComponent.cpp Source/KatanaCombatTest/Private/CounterSystemTests.cpp
git commit -m "Retain Chain counter target context"
```

---

### Task 4: Expand AttackData Notify Analysis For Counter And Paired Readiness

**Files:**
- Modify: `Source/KatanaCombatEditor/Public/AttackDataNotifyGenerationService.h`
- Modify: `Source/KatanaCombatEditor/Private/AttackDataNotifyGenerationService.cpp`
- Test: `Source/KatanaCombatTest/Private/AttackDataEditorToolsTests.cpp`

**Interfaces:**
- Consumes: `UAttackData::bHasCounterVariant`, `UAttackData::CounterData`, `UAttackData::FinisherData`, `UAnimNotifyState_CounterWindow`, `UAnimNotifyState_ParryWindow`.
- Produces analysis fields:
  - `bool bHasParryWindow`
  - `bool bHasCounterWindow`
  - `bool bCounterVariantHasData`
  - `bool bFinisherHasData`
  - `TArray<FString> BranchReadinessWarnings`

- [ ] **Step 1: Add failing tests for readiness analysis**

In `Source/KatanaCombatTest/Private/AttackDataEditorToolsTests.cpp`, add includes:

```cpp
#include "Animation/AnimNotifyState_CounterWindow.h"
#include "Animation/AnimNotifyState_ParryWindow.h"
#include "AttackDataNotifyGenerationService.h"
#include "Data/PairedAnimationData.h"
```

Add this test before `#endif`:

```cpp
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAttackDataNotifyAnalysisReportsCounterReadinessTest,
	"KatanaCombat.Editor.AttackDataTools.Analysis.CounterReadiness",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FAttackDataNotifyAnalysisReportsCounterReadinessTest::RunTest(const FString& Parameters)
{
	UAnimMontage* Montage = CreateTransientMontageWithSections();
	UAttackData* AttackData = CreateValidLightAttackData(Montage);
	AttackData->bHasCounterVariant = true;
	AttackData->CounterData = NewObject<UPairedAnimationData>(AttackData);

	AddStateNotify<UAnimNotifyState_ParryWindow>(Montage, 0.10f, 0.20f);
	AddStateNotify<UAnimNotifyState_CounterWindow>(Montage, 0.15f, 0.25f);

	const FAttackDataNotifyAnalysis Analysis = FAttackDataNotifyGenerationService::AnalyzeAttackDataNotifies(AttackData);

	TestTrue(TEXT("Analysis should detect parry window"), Analysis.bHasParryWindow);
	TestTrue(TEXT("Analysis should detect counter window"), Analysis.bHasCounterWindow);
	TestTrue(TEXT("Analysis should detect counter variant data"), Analysis.bCounterVariantHasData);
	TestEqual(TEXT("Valid counter setup should have no branch readiness warnings"), Analysis.BranchReadinessWarnings.Num(), 0);
	return true;
}
```

Add a warning-path test:

```cpp
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAttackDataNotifyAnalysisWarnsMissingCounterDataTest,
	"KatanaCombat.Editor.AttackDataTools.Analysis.MissingCounterDataWarning",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FAttackDataNotifyAnalysisWarnsMissingCounterDataTest::RunTest(const FString& Parameters)
{
	UAnimMontage* Montage = CreateTransientMontageWithSections();
	UAttackData* AttackData = CreateValidLightAttackData(Montage);
	AttackData->bHasCounterVariant = true;
	AttackData->CounterData = nullptr;

	const FAttackDataNotifyAnalysis Analysis = FAttackDataNotifyGenerationService::AnalyzeAttackDataNotifies(AttackData);

	TestTrue(TEXT("Analysis should warn when counter variant lacks CounterData"),
		Analysis.BranchReadinessWarnings.Contains(TEXT("Counter variant is enabled but CounterData is null")));
	return true;
}
```

Add a lethal counter-data warning test:

```cpp
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAttackDataNotifyAnalysisWarnsLethalCounterDataTest,
	"KatanaCombat.Editor.AttackDataTools.Analysis.LethalCounterDataWarning",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FAttackDataNotifyAnalysisWarnsLethalCounterDataTest::RunTest(const FString& Parameters)
{
	UAnimMontage* Montage = CreateTransientMontageWithSections();
	UAttackData* AttackData = CreateValidLightAttackData(Montage);
	AttackData->bHasCounterVariant = true;
	AttackData->CounterData = NewObject<UPairedAnimationData>(AttackData);
	AttackData->CounterData->bIsLethal = true;

	const FAttackDataNotifyAnalysis Analysis = FAttackDataNotifyGenerationService::AnalyzeAttackDataNotifies(AttackData);

	TestTrue(TEXT("Analysis should warn when CounterData is authored lethal"),
		Analysis.BranchReadinessWarnings.Contains(TEXT("CounterData is lethal; Chain counter steps are nonlethal by default unless runtime policy explicitly allows lethal counter data")));
	return true;
}
```

- [ ] **Step 2: Run tests to verify they fail**

Run:

```powershell
&"C:\Program Files\Epic Games\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "D:\UnrealProjects\5.6\KatanaCombat\KatanaCombat.uproject" -ExecCmds="Automation RunTests KatanaCombat.Editor.AttackDataTools.Analysis;Quit" -unattended -nopause -NullRHI -nosplash -stdout
```

Expected: compile or test failure because analysis fields do not exist.

- [ ] **Step 3: Add analysis fields**

In `FAttackDataNotifyAnalysis`, add:

```cpp
bool bHasParryWindow = false;
bool bHasCounterWindow = false;
bool bCounterVariantHasData = false;
bool bFinisherHasData = false;
TArray<FString> BranchReadinessWarnings;
```

- [ ] **Step 4: Detect parry/counter windows and paired data**

In `AttackDataNotifyGenerationService.cpp`, add includes:

```cpp
#include "Animation/AnimNotifyState_CounterWindow.h"
#include "Animation/AnimNotifyState_ParryWindow.h"
#include "Data/PairedAnimationData.h"
```

Inside the notify loop in `AnalyzeAttackDataNotifies`, add:

```cpp
if (Event.NotifyStateClass && Event.NotifyStateClass->IsA(UAnimNotifyState_ParryWindow::StaticClass()))
{
	Analysis.bHasParryWindow = true;
}
else if (Event.NotifyStateClass && Event.NotifyStateClass->IsA(UAnimNotifyState_CounterWindow::StaticClass()))
{
	Analysis.bHasCounterWindow = true;
}
```

After canonical missing checks, add:

```cpp
Analysis.bCounterVariantHasData = AttackData->bHasCounterVariant && AttackData->CounterData != nullptr;
Analysis.bFinisherHasData = AttackData->bCanTriggerFinisher && AttackData->FinisherData != nullptr;

if (AttackData->bHasCounterVariant && !AttackData->CounterData)
{
	Analysis.BranchReadinessWarnings.Add(TEXT("Counter variant is enabled but CounterData is null"));
}
if (AttackData->bCanTriggerFinisher && !AttackData->FinisherData)
{
	Analysis.BranchReadinessWarnings.Add(TEXT("Finisher trigger is enabled but FinisherData is null"));
}
if (AttackData->CounterData && !Analysis.bHasCounterWindow && !Analysis.bHasParryWindow)
{
	Analysis.BranchReadinessWarnings.Add(TEXT("CounterData is set but montage section has no parry or counter window"));
}
if (AttackData->CounterData && AttackData->CounterData->bIsLethal)
{
	Analysis.BranchReadinessWarnings.Add(TEXT("CounterData is lethal; Chain counter steps are nonlethal by default unless runtime policy explicitly allows lethal counter data"));
}
```

- [ ] **Step 5: Run focused analysis tests**

Run:

```powershell
&"C:\Program Files\Epic Games\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "D:\UnrealProjects\5.6\KatanaCombat\KatanaCombat.uproject" -ExecCmds="Automation RunTests KatanaCombat.Editor.AttackDataTools.Analysis;Quit" -unattended -nopause -NullRHI -nosplash -stdout
```

Expected: pass.

- [ ] **Step 6: Commit**

```powershell
git add Source/KatanaCombatEditor/Public/AttackDataNotifyGenerationService.h Source/KatanaCombatEditor/Private/AttackDataNotifyGenerationService.cpp Source/KatanaCombatTest/Private/AttackDataEditorToolsTests.cpp
git commit -m "Expand AttackData branch readiness analysis"
```

---

### Task 5: Surface Expanded Migration Evidence In Commandlet Reports

**Files:**
- Modify: `Source/KatanaCombatEditor/Public/Commandlets/KatanaAssetMigrationTypes.h`
- Modify: `Source/KatanaCombatEditor/Private/Commandlets/Operations/AttackDataNotifyMigrationOperation.cpp`
- Modify: `Source/KatanaCombatEditor/Private/Commandlets/KatanaAssetMigrationRunner.cpp`
- Test: `Source/KatanaCombatTest/Private/KatanaAssetMigrationTests.cpp`

**Interfaces:**
- Consumes: `FAttackDataNotifyAnalysis::BranchReadinessWarnings`.
- Produces report rows with `branch_readiness_warnings`, `has_parry_window`, `has_counter_window`, `counter_variant_has_data`, `finisher_has_data`.

- [ ] **Step 1: Add failing report-field test**

In `FKatanaAssetMigrationReportJsonFieldsTest`, after the existing `stale_canonical_notifies_found` assertion, add:

```cpp
TestTrue(TEXT("row should include branch readiness warnings"),
	(*Rows)[0]->AsObject()->HasTypedField<EJson::Array>(TEXT("branch_readiness_warnings")));
TestTrue(TEXT("row should include has_parry_window"),
	(*Rows)[0]->AsObject()->HasField(TEXT("has_parry_window")));
TestTrue(TEXT("row should include has_counter_window"),
	(*Rows)[0]->AsObject()->HasField(TEXT("has_counter_window")));
```

- [ ] **Step 2: Run the report test to verify it fails**

Run:

```powershell
&"C:\Program Files\Epic Games\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "D:\UnrealProjects\5.6\KatanaCombat\KatanaCombat.uproject" -ExecCmds="Automation RunTests KatanaCombat.Editor.AssetMigration.Runner.ReportJsonFields;Quit" -unattended -nopause -NullRHI -nosplash -stdout
```

Expected: fail because row fields are absent.

- [ ] **Step 3: Add row fields**

In `FKatanaAssetMigrationRow` in `KatanaAssetMigrationTypes.h`, add:

```cpp
TArray<FString> BranchReadinessWarnings;
bool bHasParryWindow = false;
bool bHasCounterWindow = false;
bool bCounterVariantHasData = false;
bool bFinisherHasData = false;
```

- [ ] **Step 4: Populate row fields**

In `FAttackDataNotifyMigrationOperation::Run`, after canonical fields are copied, add:

```cpp
OutRow.BranchReadinessWarnings = Analysis.BranchReadinessWarnings;
OutRow.bHasParryWindow = Analysis.bHasParryWindow;
OutRow.bHasCounterWindow = Analysis.bHasCounterWindow;
OutRow.bCounterVariantHasData = Analysis.bCounterVariantHasData;
OutRow.bFinisherHasData = Analysis.bFinisherHasData;
```

- [ ] **Step 5: Serialize row fields**

In the JSON row writer in `FKatanaAssetMigrationRunner::WriteReport`, add fields:

```cpp
RowObject->SetArrayField(TEXT("branch_readiness_warnings"), ToJsonStringArray(Row.BranchReadinessWarnings));
RowObject->SetBoolField(TEXT("has_parry_window"), Row.bHasParryWindow);
RowObject->SetBoolField(TEXT("has_counter_window"), Row.bHasCounterWindow);
RowObject->SetBoolField(TEXT("counter_variant_has_data"), Row.bCounterVariantHasData);
RowObject->SetBoolField(TEXT("finisher_has_data"), Row.bFinisherHasData);
```

Use the existing string-array helper in that file. If no helper exists, add:

```cpp
static TArray<TSharedPtr<FJsonValue>> ToJsonStringArray(const TArray<FString>& Values)
{
	TArray<TSharedPtr<FJsonValue>> JsonValues;
	for (const FString& Value : Values)
	{
		JsonValues.Add(MakeShared<FJsonValueString>(Value));
	}
	return JsonValues;
}
```

- [ ] **Step 6: Run asset migration tests**

Run:

```powershell
&"C:\Program Files\Epic Games\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "D:\UnrealProjects\5.6\KatanaCombat\KatanaCombat.uproject" -ExecCmds="Automation RunTests KatanaCombat.Editor.AssetMigration;Quit" -unattended -nopause -NullRHI -nosplash -stdout
```

Expected: pass.

- [ ] **Step 7: Commit**

```powershell
git add Source/KatanaCombatEditor/Public/Commandlets/KatanaAssetMigrationTypes.h Source/KatanaCombatEditor/Private/Commandlets/Operations/AttackDataNotifyMigrationOperation.cpp Source/KatanaCombatEditor/Private/Commandlets/KatanaAssetMigrationRunner.cpp Source/KatanaCombatTest/Private/KatanaAssetMigrationTests.cpp
git commit -m "Report AttackData branch readiness"
```

---

### Task 6: Read-Only Asset Proof Pass

**Files:**
- Create evidence under: `Saved/Logs/Commandlets/KatanaAssetMigration/`
- Modify docs only after proof: `docs/handoffs/2026-06-28-original-branch-intent-satisfaction-audit.md`

**Interfaces:**
- Consumes: `KatanaAssetMigration` commandlet.
- Produces: JSON report with changed-package candidates and branch-readiness warnings.

- [ ] **Step 1: Run global audit without package save**

Run:

```powershell
&"C:\Program Files\Epic Games\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "D:\UnrealProjects\5.6\KatanaCombat\KatanaCombat.uproject" -run=KatanaAssetMigration -Operation=AttackDataNotifyMigration -Mode=Audit -AllowGlobalScan -ReportPath="Saved/Logs/Commandlets/KatanaAssetMigration/attackdata-notify-audit.json" -unattended -nopause -NullRHI -nosplash -stdout
```

Expected: commandlet exits successfully or reports row-level failures without modifying packages.

- [ ] **Step 2: Inspect audit report**

Run:

```powershell
Get-Content Saved\Logs\Commandlets\KatanaAssetMigration\attackdata-notify-audit.json -Raw | ConvertFrom-Json | Select-Object operation, mode, total_rows, failed_rows, would_change_rows
```

Expected: JSON parses and shows row counts.

- [ ] **Step 3: Create reviewed target list for plan mode**

Create `Config/AssetMigrations/AttackDataNotifyTargets.txt` only after reviewing the audit report. Include one object path per line, for example:

```text
/Game/ProjectFiles/Data/PDA/Attack/AttackData/Light/New/LightAttack_1.LightAttack_1
/Game/ProjectFiles/Data/PDA/Attack/AttackData/Heavy/New/HeavyAttack_1.HeavyAttack_1
```

- [ ] **Step 4: Run plan mode on reviewed targets**

Run:

```powershell
&"C:\Program Files\Epic Games\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "D:\UnrealProjects\5.6\KatanaCombat\KatanaCombat.uproject" -run=KatanaAssetMigration -Operation=AttackDataNotifyMigration -Mode=Plan -TargetsFile="Config/AssetMigrations/AttackDataNotifyTargets.txt" -ReportPath="Saved/Logs/Commandlets/KatanaAssetMigration/attackdata-notify-plan.json" -unattended -nopause -NullRHI -nosplash -stdout
```

Expected: no package mutation; report lists planned additions/removals and readiness warnings.

- [ ] **Step 5: Document proof boundary**

Append a new section to `docs/handoffs/2026-06-28-original-branch-intent-satisfaction-audit.md`:

```markdown
## 2026-06-30 Read-Only Asset Proof Update

- `AttackDataNotifyMigration` audit report: `Saved/Logs/Commandlets/KatanaAssetMigration/attackdata-notify-audit.json`
- Plan report: `Saved/Logs/Commandlets/KatanaAssetMigration/attackdata-notify-plan.json`
- No package-save pass was run.
- Remaining blocker: package mutation requires explicit reviewed targets and `-AllowPackageSave`.
```

- [ ] **Step 6: Commit docs and target list only if intentionally part of branch**

```powershell
git add Config/AssetMigrations/AttackDataNotifyTargets.txt docs/handoffs/2026-06-28-original-branch-intent-satisfaction-audit.md
git commit -m "Document AttackData migration proof"
```

---

### Task 7: Post-Implementation Documentation And Evidence Alignment

**Files:**
- Modify: `docs/specs/PAIRED_ANIMATION_SPEC.md`
- Modify: `docs/architecture/ARCHITECTURE_QUICK.md`
- Modify: `docs/plans/gap-tracker.md`
- Modify: `docs/handoffs/2026-06-28-original-branch-intent-satisfaction-audit.md`

**Interfaces:**
- Consumes: completed source/test results from Tasks 1-6.
- Produces: docs that preserve the Task 0 reconciled spec and add implementation/evidence status without changing the accepted runtime contract.

- [ ] **Step 1: Verify paired spec still matches implemented Chain behavior**

In `docs/specs/PAIRED_ANIMATION_SPEC.md`, keep the Task 0 `Chain Counter Runtime Contract` as the source of truth. Add this evidence note below it only after Tasks 1-3 pass:

```markdown
### Chain Counter Implementation Evidence

- Public Block-input entry is covered by `KatanaCombat.CounterSystem.Input.BlockStartsChainParry`.
- Public attack-input advance is covered by `KatanaCombat.CounterSystem.ChainAttackInputAdvancesCounter`.
- Attack input resolves selected `UAttackData` in `UCombatComponent` and advances through `UPairedAnimationComponent::TryAdvanceChainCounter(UAttackData*)`.
- Counter data resolution remains selected `UAttackData::CounterData`, explicit notify fallback, then non-paired fallback.
- Asset-backed montage proof remains separate from source-level automation until Task 6 evidence is captured.
```

- [ ] **Step 2: Update architecture quick reference with evidence notes**

In `docs/architecture/ARCHITECTURE_QUICK.md`, keep the Task 0 `Chain Counter Ownership` section. Under AnimNotify Requirements, add:

```markdown
### Chain Counter Requirements
- Attacker montages that can be parried require `AnimNotifyState_ParryWindow`.
- Attacker montages that can be directly countered require `AnimNotifyState_CounterWindow`.
- Counter-capable `UAttackData` should set `bHasCounterVariant` and `CounterData` when a paired counter animation exists.
- Paired counter/finisher montages require paired sync/collision notifies when they depend on impact timing or partner collision suppression.
- Successful Chain Block and attack inputs are consumed only when `UPairedAnimationComponent` returns success.
- `UCombatComponent` resolves selected attack data before crossing into Chain advance.
```

- [ ] **Step 3: Update gap tracker**

In `docs/plans/gap-tracker.md`, update gaps 26.1, 26.2, 26.3, and 26.5 from partial/pending wording to the current implementation status and list any asset-proof blockers found by Task 6.

- [ ] **Step 4: Update branch audit**

In `docs/handoffs/2026-06-28-original-branch-intent-satisfaction-audit.md`, replace the Chain row with:

```markdown
| Chain counter mode | Required branch behavior | Public `TryCounter()` starts from attacker-side parry windows, attack input advances the chain, and authored paired counter data is used when present. | Asset-backed proof remains required for concrete montages and maps. |
```

Replace the AttackData row with:

```markdown
| AttackData notify migration | In scope | Migration tooling audits/plans canonical phase, hold-start, parry/counter, and paired-readiness evidence for AttackData-driven montages. | Package-save passes require reviewed target lists and explicit approval. |
```

- [ ] **Step 5: Run Markdown/search sanity checks**

Run:

```powershell
rg -n "outside branch scope|Chain.*deferred|Chain.*not in scope|Chain.*optional|calls `ExecuteChainCounterAttack\\(\\)`|SpecificCounterData.*first" docs/specs/PAIRED_ANIMATION_SPEC.md docs/architecture/ARCHITECTURE_QUICK.md docs/plans/gap-tracker.md docs/handoffs/2026-06-28-original-branch-intent-satisfaction-audit.md
```

Expected: no stale references that describe Chain Counter as outside branch scope, deferred, optional, executor-driven from `UCombatComponent`, or notify-data-first.

- [ ] **Step 6: Commit docs**

```powershell
git add docs/specs/PAIRED_ANIMATION_SPEC.md docs/architecture/ARCHITECTURE_QUICK.md docs/plans/gap-tracker.md docs/handoffs/2026-06-28-original-branch-intent-satisfaction-audit.md
git commit -m "Align Chain counter and migration docs"
```

---

### Task 8: Final Verification Gate

**Files:**
- Evidence only under `Saved/Logs/`
- No source changes unless verification finds a defect.

**Interfaces:**
- Consumes: all previous tasks.
- Produces: final pass/fail evidence for branch readiness.

- [ ] **Step 1: Build editor target**

Run:

```powershell
&"C:\Program Files\Epic Games\UE_5.6\Engine\Build\BatchFiles\Build.bat" KatanaCombatEditor Win64 Development -Project="D:\UnrealProjects\5.6\KatanaCombat\KatanaCombat.uproject" -Progress -NoHotReload
```

Expected: exit code `0`.

- [ ] **Step 2: Run focused runtime tests**

Run:

```powershell
&"C:\Program Files\Epic Games\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "D:\UnrealProjects\5.6\KatanaCombat\KatanaCombat.uproject" -ExecCmds="Automation RunTests KatanaCombat.CounterSystem;Quit" -unattended -nopause -NullRHI -nosplash -stdout
```

Then:

```powershell
&"C:\Program Files\Epic Games\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "D:\UnrealProjects\5.6\KatanaCombat\KatanaCombat.uproject" -ExecCmds="Automation RunTests KatanaCombat.PairedAnimation;Quit" -unattended -nopause -NullRHI -nosplash -stdout
```

Expected: both suites pass.

- [ ] **Step 3: Run focused editor tests**

Run:

```powershell
&"C:\Program Files\Epic Games\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "D:\UnrealProjects\5.6\KatanaCombat\KatanaCombat.uproject" -ExecCmds="Automation RunTests KatanaCombat.Editor.AttackDataTools;Quit" -unattended -nopause -NullRHI -nosplash -stdout
```

Then:

```powershell
&"C:\Program Files\Epic Games\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "D:\UnrealProjects\5.6\KatanaCombat\KatanaCombat.uproject" -ExecCmds="Automation RunTests KatanaCombat.Editor.AssetMigration;Quit" -unattended -nopause -NullRHI -nosplash -stdout
```

Expected: both suites pass.

- [ ] **Step 4: Run broad suite**

Run:

```powershell
&"C:\Program Files\Epic Games\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "D:\UnrealProjects\5.6\KatanaCombat\KatanaCombat.uproject" -ExecCmds="Automation RunTests KatanaCombat;Quit" -unattended -nopause -NullRHI -nosplash -stdout
```

If the process does not exit cleanly, run:

```powershell
powershell -ExecutionPolicy Bypass -File ".agents/skills/katana-verify/scripts/summarize-automation-log.ps1"
```

Expected: all relevant `Test Completed` rows report success, or failures are captured with exact paths.

- [ ] **Step 5: Final dirty-lane review**

Run:

```powershell
git status --short --branch
git diff --name-status
git ls-files --others --exclude-standard
```

Expected: branch changes are classified into runtime, editor tooling, tests, docs, and explicitly approved asset evidence. Large `Content/` deletions/imports remain blocked unless separately authorized.

- [ ] **Step 6: Final commit if needed**

If verification required doc-only evidence updates:

```powershell
git add docs/handoffs/2026-06-28-original-branch-intent-satisfaction-audit.md
git commit -m "Record branch readiness evidence"
```

---

## Self-Review

### Spec Coverage

- Spec reconciliation is a pre-implementation gate: the Spec Reconciliation Checklist requires runtime contract, component ownership, context schema, counter-data priority, damage semantics, cleanup semantics, migration scope, asset proof, docs alignment, and dirty WIP classification before Task 1 starts.
- Chain Counter is required: Task 0 reconciles the spec before source implementation, public `TryCounter()` coverage is in Task 1, Block/attack input coverage is in Task 2, active context and paired continuation coverage is in Task 3, evidence docs are in Task 7, and verification is in Task 8.
- Stale Chain test replacement is explicit: the Test Contract Policy and Task 2 Step 10 require removal or rewrite of direct-helper tests that claim end-to-end Chain behavior.
- Counter data ownership, nonlethal counter damage, and full Chain cleanup are acceptance requirements covered by Task 3 tests, Task 4 readiness warnings, Task 7 evidence docs, and Task 8 verification.
- AttackData migration is in scope: covered by Tasks 4, 5, 6, 7, and 8.
- Asset behavior must not be overclaimed: covered by Task 6 and Global Constraints.
- Runtime paired/counter tests are required: covered by Task 8.
- Docs must match proof: covered by Task 7.

### Placeholder Scan

The plan intentionally avoids unresolved placeholders. Any implementation detail that depends on real assets is handled through read-only audit/plan reports and explicit package-save gates.

### Type Consistency

- `GetChainState()`, `IsChainCounterWaitingForAttack()`, `HasActiveChainTarget()`, `ShouldTreatPairedAnimationAsLethal(EPairedReactionType, const UPairedAnimationData*)`, and `GetEnemyParryContext(AActor*)` are introduced before later tasks use them.
- `TryAdvanceChainCounter(UAttackData* SelectedAttackData)` is the only public `UCombatComponent` handoff for attack-input Chain advance. `ExecuteChainCounterAttack(UAttackData* ChainAttackData)` remains protected/internal.
- `BranchReadinessWarnings` and related booleans are introduced in Task 4 before commandlet report serialization in Task 5.
