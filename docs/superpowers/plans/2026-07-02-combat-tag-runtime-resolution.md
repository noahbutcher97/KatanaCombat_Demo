# Combat Tag Runtime Resolution Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make existing gameplay tags authoritative for contextual attack eligibility and unblockable attack defense behavior.

**Architecture:** Keep the accepted ownership split. Enums remain runtime states/results, booleans remain local latches/readiness gates, gameplay tags gate authored semantics, and data references own payloads. This plan wires the two currently decorative tag contracts into the existing resolver and block path without adding new tags or asset mutations. Context handling in this slice is candidate filtering for already-linked attacks, not global discovery of arbitrary counter or finisher assets.

**Tech Stack:** Unreal Engine 5.6 C++, KatanaCombat runtime/editor/test modules, GameplayTags, Unreal Automation tests.

## Global Constraints

- Do not touch binary assets or package-save content.
- Do not add new gameplay tag families in this slice.
- Preserve combat invariants: phases are exclusive; windows may overlap; input is always buffered; parry checks the attacker's parry window; hold checks button state at the window boundary.
- Keep editor-only dependencies out of `Source/KatanaCombat/`.
- Stage by explicit path only; do not use `git add .`.
- This branch has pre-existing WIP in several planned source files. Before each task, capture `git diff -- <task files>` as the baseline, then review the post-task delta before staging anything.
- Do not use plain `git add <file>` on files that already had WIP. If a commit is requested, use `git add -p`, inspect `git diff --staged`, and confirm the staged diff excludes pre-existing work.
- Treat context mutators as low-level API only. Gameplay producers and lifecycle clear points for `Context.ParryCounter`, finisher context, or future defense context must be specified before wiring those tags into Chain Counter or finisher flow.
- Do not chain multiple `Automation RunTests ...` commands in one `-ExecCmds` string. Use one test root per editor-cmd invocation with `;Quit`, or run a broad common root such as `Automation RunTests KatanaCombat`.

---

## File Structure

- Modify `Source/KatanaCombat/Public/Core/CombatComponent.h`
  - Add context-tag mutators.
  - Add `CanBlockHit(const FHitReactionInfo& HitInfo) const`.
- Modify `Source/KatanaCombat/Private/Core/CombatComponent.cpp`
  - Implement context-tag mutators.
  - Implement tag-aware block resolution while preserving facing checks.
- Modify `Source/KatanaCombat/Private/Characters/BaseCombatCharacter.cpp`
  - Use `CanBlockHit` for damage, impact audio, VFX, and hitstop blocked-state classification.
- Modify `Source/KatanaCombat/Private/Utilities/MontageUtilityLibrary.cpp`
  - Filter existing attack candidates by `RequiredContextTags`.
- Modify `Source/KatanaCombat/Public/Utilities/MontageUtilityLibrary.h`
  - Clarify that this slice filters existing candidates and does not perform global context-attack discovery.
- Add `Source/KatanaCombat/Public/Utilities/CombatGameplayTags.h`
  - Declare shared accessors for semantic combat gameplay tags.
- Add `Source/KatanaCombat/Private/Utilities/CombatGameplayTags.cpp`
  - Resolve semantic combat gameplay tags from one runtime-owned source.
- Add `Source/KatanaCombatTest/Private/AttackResolutionContextTests.cpp`
  - Prove contextual attack candidates require matching active context tags.
- Add `Source/KatanaCombatTest/Private/GameplayTagContractTests.cpp`
  - Prove semantic combat gameplay tags are registered and accessor-backed.
- Modify `Source/KatanaCombatTest/Private/CounterSystemTests.cpp`
  - Prove `Attack.Property.Unblockable` bypasses sustained block.
- Modify `Source/KatanaCombatEditor/Public/AttackDataNotifyGenerationService.h`
  - Add semantic tag report fields.
- Modify `Source/KatanaCombatEditor/Private/AttackDataNotifyGenerationService.cpp`
  - Populate semantic tag report fields.
- Modify `Source/KatanaCombatEditor/Public/Commandlets/KatanaAssetMigrationTypes.h`
  - Add row fields for attack/context tags.
- Modify `Source/KatanaCombatEditor/Private/Commandlets/Operations/AttackDataNotifyMigrationOperation.cpp`
  - Copy semantic tag fields to report rows.
- Modify `Source/KatanaCombatEditor/Private/Commandlets/Operations/AttackDataTimingMigrationOperation.cpp`
  - Copy semantic tag fields to timing rows.
- Modify `Source/KatanaCombatEditor/Private/Commandlets/Operations/ContentReadinessAuditOperation.cpp`
  - Copy semantic tag fields to readiness rows.
- Modify `Source/KatanaCombatEditor/Private/Commandlets/Operations/CounterChainProofMigrationOperation.cpp`
  - Copy semantic tag fields to counter-chain proof rows.
- Modify `Source/KatanaCombatEditor/Private/Commandlets/KatanaAssetMigrationRunner.cpp`
  - Serialize semantic tag fields to JSON.
- Modify `Source/KatanaCombatTest/Private/AttackDataEditorToolsTests.cpp`
  - Prove notify analysis extracts authored semantic tags.
- Modify `Source/KatanaCombatTest/Private/KatanaAssetMigrationTests.cpp`
  - Prove semantic tag report fields exist and operation rows receive them.

---

### Task 1: Runtime Context Tag Gating

**Files:**
- Modify: `Source/KatanaCombat/Public/Core/CombatComponent.h`
- Modify: `Source/KatanaCombat/Private/Core/CombatComponent.cpp`
- Modify: `Source/KatanaCombat/Private/Utilities/MontageUtilityLibrary.cpp`
- Modify: `Source/KatanaCombat/Public/Utilities/MontageUtilityLibrary.h`
- Add: `Source/KatanaCombat/Public/Utilities/CombatGameplayTags.h`
- Add: `Source/KatanaCombat/Private/Utilities/CombatGameplayTags.cpp`
- Add: `Source/KatanaCombatTest/Private/AttackResolutionContextTests.cpp`
- Add: `Source/KatanaCombatTest/Private/GameplayTagContractTests.cpp`

**Interfaces:**
- Consumes: `UAttackData::RequiredContextTags`, `UCombatComponent::ActiveContextTags`, `UMontageUtilityLibrary::ResolveNextAttackContextual(...)`.
- Produces:
  - `void UCombatComponent::AddActiveContextTag(FGameplayTag ContextTag)`
  - `void UCombatComponent::RemoveActiveContextTag(FGameplayTag ContextTag)`
  - `void UCombatComponent::ClearActiveContextTags()`
  - `bool UCombatComponent::HasActiveContextTag(FGameplayTag ContextTag) const`
  - `FGameplayTag KatanaCombatGameplayTags::AttackPropertyUnblockable()`
  - `FGameplayTag KatanaCombatGameplayTags::ContextParryCounter()`
  - `FGameplayTag KatanaCombatGameplayTags::ContextLowHealthFinisher()`

- [ ] **Step 1: Add shared semantic tag accessors**

Create `Source/KatanaCombat/Public/Utilities/CombatGameplayTags.h`:

```cpp
#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"

namespace KatanaCombatGameplayTags
{
	KATANACOMBAT_API FGameplayTag AttackPropertyUnblockable();
	KATANACOMBAT_API FGameplayTag ContextParryCounter();
	KATANACOMBAT_API FGameplayTag ContextLowHealthFinisher();
}
```

Create `Source/KatanaCombat/Private/Utilities/CombatGameplayTags.cpp`:

```cpp
#include "Utilities/CombatGameplayTags.h"

namespace
{
FGameplayTag RequestCombatSemanticTag(const TCHAR* TagName)
{
	return FGameplayTag::RequestGameplayTag(FName(TagName), false);
}
}

FGameplayTag KatanaCombatGameplayTags::AttackPropertyUnblockable()
{
	static const FGameplayTag Tag = RequestCombatSemanticTag(TEXT("Attack.Property.Unblockable"));
	return Tag;
}

FGameplayTag KatanaCombatGameplayTags::ContextParryCounter()
{
	static const FGameplayTag Tag = RequestCombatSemanticTag(TEXT("Context.ParryCounter"));
	return Tag;
}

FGameplayTag KatanaCombatGameplayTags::ContextLowHealthFinisher()
{
	static const FGameplayTag Tag = RequestCombatSemanticTag(TEXT("Context.LowHealthFinisher"));
	return Tag;
}
```

- [ ] **Step 2: Add failing tag-registration and context-resolution tests**

Create `Source/KatanaCombatTest/Private/GameplayTagContractTests.cpp`:

```cpp
#include "Misc/AutomationTest.h"
#include "Utilities/CombatGameplayTags.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FKatanaSemanticGameplayTagsRegisteredTest,
	"KatanaCombat.GameplayTags.SemanticTagsRegistered",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FKatanaSemanticGameplayTagsRegisteredTest::RunTest(const FString& Parameters)
{
	TestTrue(TEXT("Attack.Property.Unblockable should be registered"),
		KatanaCombatGameplayTags::AttackPropertyUnblockable().IsValid());
	TestTrue(TEXT("Context.ParryCounter should be registered"),
		KatanaCombatGameplayTags::ContextParryCounter().IsValid());
	TestTrue(TEXT("Context.LowHealthFinisher should be registered"),
		KatanaCombatGameplayTags::ContextLowHealthFinisher().IsValid());
	return true;
}
```

Create `Source/KatanaCombatTest/Private/AttackResolutionContextTests.cpp`:

```cpp
#include "Misc/AutomationTest.h"
#include "CombatTestHelpers.h"
#include "Core/CombatComponent.h"
#include "Data/AttackData.h"
#include "Utilities/CombatGameplayTags.h"
#include "Utilities/MontageUtilityLibrary.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAttackResolutionRejectsMissingRequiredContextTest,
	"KatanaCombat.AttackResolution.Context.RequiredTagsRejectMissingContext",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FAttackResolutionRejectsMissingRequiredContextTest::RunTest(const FString& Parameters)
{
	UAttackData* CurrentAttack = FCombatTestHelpers::CreateTestAttack(EAttackType::Light);
	UAttackData* ContextAttack = FCombatTestHelpers::CreateTestAttack(EAttackType::Light);
	UAttackData* DefaultLight = FCombatTestHelpers::CreateTestAttack(EAttackType::Light);
	UAttackData* DefaultHeavy = FCombatTestHelpers::CreateTestAttack(EAttackType::Heavy);

	CurrentAttack->NextComboAttack = ContextAttack;
	ContextAttack->RequiredContextTags.AddTag(KatanaCombatGameplayTags::ContextParryCounter());

	FHoldState HoldState;
	FGameplayTagContainer ActiveContext;
	TSet<UAttackData*> Visited;

	const FAttackResolutionResult Result = UMontageUtilityLibrary::ResolveNextAttackContextual(
		CurrentAttack,
		EInputType::LightAttack,
		EAttackDirection::None,
		HoldState,
		true,
		DefaultLight,
		DefaultHeavy,
		ActiveContext,
		Visited);

	TestEqual(TEXT("Missing required context should reject combo candidate and fall back to default"),
		Result.Attack.Get(),
		DefaultLight);
	TestEqual(TEXT("Fallback path should remain Default"),
		static_cast<int32>(Result.Path),
		static_cast<int32>(EResolutionPath::Default));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAttackResolutionAllowsMatchingRequiredContextTest,
	"KatanaCombat.AttackResolution.Context.RequiredTagsAllowMatchingContext",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FAttackResolutionAllowsMatchingRequiredContextTest::RunTest(const FString& Parameters)
{
	UAttackData* CurrentAttack = FCombatTestHelpers::CreateTestAttack(EAttackType::Light);
	UAttackData* ContextAttack = FCombatTestHelpers::CreateTestAttack(EAttackType::Light);
	UAttackData* DefaultLight = FCombatTestHelpers::CreateTestAttack(EAttackType::Light);
	UAttackData* DefaultHeavy = FCombatTestHelpers::CreateTestAttack(EAttackType::Heavy);

	const FGameplayTag ParryCounterTag = KatanaCombatGameplayTags::ContextParryCounter();
	CurrentAttack->NextComboAttack = ContextAttack;
	ContextAttack->RequiredContextTags.AddTag(ParryCounterTag);

	FHoldState HoldState;
	FGameplayTagContainer ActiveContext;
	ActiveContext.AddTag(ParryCounterTag);
	TSet<UAttackData*> Visited;

	const FAttackResolutionResult Result = UMontageUtilityLibrary::ResolveNextAttackContextual(
		CurrentAttack,
		EInputType::LightAttack,
		EAttackDirection::None,
		HoldState,
		true,
		DefaultLight,
		DefaultHeavy,
		ActiveContext,
		Visited);

	TestEqual(TEXT("Matching required context should allow context candidate"),
		Result.Attack.Get(),
		ContextAttack);
	TestEqual(TEXT("Context gating should not change structural combo path"),
		static_cast<int32>(Result.Path),
		static_cast<int32>(EResolutionPath::NormalCombo));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatComponentActiveContextMutatorsTest,
	"KatanaCombat.CombatComponent.Context.ActiveContextTagMutators",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FCombatComponentActiveContextMutatorsTest::RunTest(const FString& Parameters)
{
	UWorld* World = FCombatTestHelpers::CreateTestWorld();
	UCombatComponent* Combat = nullptr;
	APlayerCharacter* Player = FCombatTestHelpers::CreateTestCharacterWithCombat(World, Combat);
	const FGameplayTag ParryCounterTag = KatanaCombatGameplayTags::ContextParryCounter();

	TestFalse(TEXT("Context tag should start inactive"), Combat->HasActiveContextTag(ParryCounterTag));
	Combat->AddActiveContextTag(ParryCounterTag);
	TestTrue(TEXT("AddActiveContextTag should activate tag"), Combat->HasActiveContextTag(ParryCounterTag));
	Combat->RemoveActiveContextTag(ParryCounterTag);
	TestFalse(TEXT("RemoveActiveContextTag should remove tag"), Combat->HasActiveContextTag(ParryCounterTag));
	Combat->AddActiveContextTag(ParryCounterTag);
	Combat->ClearActiveContextTags();
	TestFalse(TEXT("ClearActiveContextTags should remove all active context"), Combat->HasActiveContextTag(ParryCounterTag));

	World->DestroyActor(Player);
	FCombatTestHelpers::DestroyTestWorld(World);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAttackResolutionEmergencyFallbackRejectsMissingRequiredContextTest,
	"KatanaCombat.AttackResolution.Context.EmergencyFallbackRejectsMissingContext",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FAttackResolutionEmergencyFallbackRejectsMissingRequiredContextTest::RunTest(const FString& Parameters)
{
	UAttackData* CurrentAttack = FCombatTestHelpers::CreateTestAttack(EAttackType::Light);
	const FGameplayTag ParryCounterTag = KatanaCombatGameplayTags::ContextParryCounter();
	CurrentAttack->RequiredContextTags.AddTag(ParryCounterTag);

	FHoldState HoldState;
	FGameplayTagContainer ActiveContext;
	TSet<UAttackData*> Visited;

	const FAttackResolutionResult Result = UMontageUtilityLibrary::ResolveNextAttackContextual(
		CurrentAttack,
		EInputType::LightAttack,
		EAttackDirection::None,
		HoldState,
		false,
		nullptr,
		nullptr,
		ActiveContext,
		Visited);

	TestNull(TEXT("Emergency repeat fallback must not bypass RequiredContextTags on the original attack"),
		Result.Attack.Get());
	return true;
}
```

- [ ] **Step 3: Run the focused tests to verify failure**

Run:

```powershell
&"C:\Program Files\Epic Games\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "D:\UnrealProjects\5.6\KatanaCombat\KatanaCombat.uproject" -ExecCmds="Automation RunTests KatanaCombat.GameplayTags.SemanticTagsRegistered;Quit" -unattended -nopause -NullRHI -nosplash -stdout
&"C:\Program Files\Epic Games\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "D:\UnrealProjects\5.6\KatanaCombat\KatanaCombat.uproject" -ExecCmds="Automation RunTests KatanaCombat.AttackResolution.Context;Quit" -unattended -nopause -NullRHI -nosplash -stdout
```

Expected: compile failure because the context mutator API does not exist, or test failure because `RequiredContextTags` is ignored.

- [ ] **Step 4: Add context mutators to `CombatComponent.h`**

Add after `CanBlockAttackFrom`:

```cpp
	/** Add a runtime context tag used by context-aware attack resolution. C++ only until gameplay lifecycle ownership is specified. */
	void AddActiveContextTag(FGameplayTag ContextTag);

	/** Remove a runtime context tag used by context-aware attack resolution. C++ only until gameplay lifecycle ownership is specified. */
	void RemoveActiveContextTag(FGameplayTag ContextTag);

	/** Clear all runtime context tags. C++ only until gameplay lifecycle ownership is specified. */
	void ClearActiveContextTags();

	/** True when the runtime context currently contains this tag. */
	UFUNCTION(BlueprintPure, Category = "Combat|Context")
	bool HasActiveContextTag(FGameplayTag ContextTag) const;
```

- [ ] **Step 5: Implement context mutators in `CombatComponent.cpp`**

Add after `CanBlockAttackFrom`:

```cpp
void UCombatComponent::AddActiveContextTag(FGameplayTag ContextTag)
{
	if (ContextTag.IsValid())
	{
		ActiveContextTags.AddTag(ContextTag);
	}
}

void UCombatComponent::RemoveActiveContextTag(FGameplayTag ContextTag)
{
	if (ContextTag.IsValid())
	{
		ActiveContextTags.RemoveTag(ContextTag);
	}
}

void UCombatComponent::ClearActiveContextTags()
{
	ActiveContextTags.Reset();
}

bool UCombatComponent::HasActiveContextTag(FGameplayTag ContextTag) const
{
	return ContextTag.IsValid() && ActiveContextTags.HasTag(ContextTag);
}
```

- [ ] **Step 6: Clarify resolver docs and gate attack candidates by required context**

In `Source/KatanaCombat/Public/Utilities/MontageUtilityLibrary.h`, update the `ResolveNextAttackContextual` comment so the priority list describes this slice accurately:

```cpp
	 * Resolution Priority:
	 * 1. Filter each resolved candidate by RequiredContextTags before it can be returned
	 * 2. Directional follow-ups (if HoldState.IsHoldCompleted() + Direction != None)
	 * 3. Normal combo chain (if bComboWindowActive)
	 * 4. Default attacks (fallback)
	 *
	 * Note: this function filters already-linked directional/combo/default candidates.
	 * It does not scan a global catalog for arbitrary context-sensitive attacks.
	 * Path remains structural telemetry; required-context tags do not convert a normal combo into a counter or finisher outcome.
```

In `Source/KatanaCombat/Private/Utilities/MontageUtilityLibrary.cpp`, add helper functions above `ResolveNextAttackContextual`:

```cpp
namespace
{
bool DoesAttackMeetRequiredContext(const UAttackData* AttackData, const FGameplayTagContainer& ActiveContext)
{
	return !AttackData ||
		AttackData->RequiredContextTags.IsEmpty() ||
		ActiveContext.HasAll(AttackData->RequiredContextTags);
}

}
```

Then update the directional, combo, default, and emergency fallback branches. Every candidate that can be returned must pass `DoesAttackMeetRequiredContext(...)`; do not rely on a single flag that only protects one path.

```cpp
		if (DirectionalAttack && !DoesAttackMeetRequiredContext(DirectionalAttack, ActiveContext))
		{
			UE_LOG(LogCombat, Log, TEXT("[RESOLVE] Directional candidate '%s' rejected by RequiredContextTags"), *DirectionalAttack->GetName());
			DirectionalAttack = nullptr;
		}
```

```cpp
		if (ComboAttack && !DoesAttackMeetRequiredContext(ComboAttack, ActiveContext))
		{
			UE_LOG(LogCombat, Log, TEXT("[RESOLVE] Combo candidate '%s' rejected by RequiredContextTags"), *ComboAttack->GetName());
			ComboAttack = nullptr;
		}
		if (ComboAttack)
		{
			Result.Attack = ComboAttack;
			Result.Path = EResolutionPath::NormalCombo;
			UE_LOG(LogCombat, Log, TEXT("[RESOLVE] Resolved to combo/context candidate: '%s'"), *ComboAttack->GetName());
			return Result;
		}
```

```cpp
	if (DefaultAttack && !DoesAttackMeetRequiredContext(DefaultAttack, ActiveContext))
	{
		UE_LOG(LogCombat, Warning, TEXT("[RESOLVE] Default candidate '%s' rejected by RequiredContextTags"), *DefaultAttack->GetName());
		DefaultAttack = nullptr;
	}
```

When the emergency fallback tries to repeat the original attack, guard it explicitly:

```cpp
			if (OriginalAttack && DoesAttackMeetRequiredContext(OriginalAttack, ActiveContext))
			{
				Result.Attack = OriginalAttack;
				Result.Path = EResolutionPath::Default;
				UE_LOG(LogCombat, Warning, TEXT("[RESOLVE] Emergency fallback: Repeating original attack '%s'"),
					*OriginalAttack->GetName());
			}
			else if (OriginalAttack)
			{
				UE_LOG(LogCombat, Warning, TEXT("[RESOLVE] Emergency fallback rejected original attack '%s' by RequiredContextTags"),
					*OriginalAttack->GetName());
			}
```

The actual safety check is `DoesAttackMeetRequiredContext(OriginalAttack, ActiveContext)`. Do not use a weaker guard such as "default was blocked by context"; that still lets a context-gated original attack repeat when defaults are missing.

- [ ] **Step 7: Run focused context tests**

Run:

```powershell
&"C:\Program Files\Epic Games\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "D:\UnrealProjects\5.6\KatanaCombat\KatanaCombat.uproject" -ExecCmds="Automation RunTests KatanaCombat.GameplayTags.SemanticTagsRegistered;Quit" -unattended -nopause -NullRHI -nosplash -stdout
&"C:\Program Files\Epic Games\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "D:\UnrealProjects\5.6\KatanaCombat\KatanaCombat.uproject" -ExecCmds="Automation RunTests KatanaCombat.AttackResolution.Context;Quit" -unattended -nopause -NullRHI -nosplash -stdout
```

Expected: semantic tag registration and all `KatanaCombat.AttackResolution.Context` tests pass.

- [ ] **Step 8: Review task diff before any staging**

```powershell
git diff -- Source/KatanaCombat/Public/Core/CombatComponent.h Source/KatanaCombat/Private/Core/CombatComponent.cpp Source/KatanaCombat/Private/Utilities/MontageUtilityLibrary.cpp Source/KatanaCombat/Public/Utilities/MontageUtilityLibrary.h Source/KatanaCombat/Public/Utilities/CombatGameplayTags.h Source/KatanaCombat/Private/Utilities/CombatGameplayTags.cpp Source/KatanaCombatTest/Private/AttackResolutionContextTests.cpp Source/KatanaCombatTest/Private/GameplayTagContractTests.cpp
git diff --check -- Source/KatanaCombat/Public/Core/CombatComponent.h Source/KatanaCombat/Private/Core/CombatComponent.cpp Source/KatanaCombat/Private/Utilities/MontageUtilityLibrary.cpp Source/KatanaCombat/Public/Utilities/MontageUtilityLibrary.h Source/KatanaCombat/Public/Utilities/CombatGameplayTags.h Source/KatanaCombat/Private/Utilities/CombatGameplayTags.cpp Source/KatanaCombatTest/Private/AttackResolutionContextTests.cpp Source/KatanaCombatTest/Private/GameplayTagContractTests.cpp
```

Expected: diff only contains Task 1 changes, and whitespace check reports no errors. If a commit is requested, stage with `git add -p` and inspect `git diff --staged` before committing.

---

### Task 2: Unblockable Tag In Block Resolution

**Files:**
- Modify: `Source/KatanaCombat/Public/Core/CombatComponent.h`
- Modify: `Source/KatanaCombat/Private/Core/CombatComponent.cpp`
- Modify: `Source/KatanaCombat/Private/Characters/BaseCombatCharacter.cpp`
- Modify: `Source/KatanaCombatTest/Private/CounterSystemTests.cpp`

**Interfaces:**
- Consumes: `UAttackData::AttackTags`, `KatanaCombatGameplayTags::AttackPropertyUnblockable()`, `FHitReactionInfo::AttackData`, `CanBlockAttackFrom(AActor*)`.
- Produces: `bool UCombatComponent::CanBlockHit(const FHitReactionInfo& HitInfo) const`.

- [ ] **Step 1: Add failing unblockable block test**

In `Source/KatanaCombatTest/Private/CounterSystemTests.cpp`, add `#include "Utilities/CombatGameplayTags.h"` near the other includes, then add after `FCounter_BlockInputStartsNormalBlockWhenNoParryTarget`:

```cpp
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCounter_NullAttackDataPreservesNormalBlock,
	"KatanaCombat.CounterSystem.Block.NullAttackDataPreservesBlock",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCounter_NullAttackDataPreservesNormalBlock::RunTest(const FString& Parameters)
{
	UWorld* World = FCombatTestHelpers::CreateTestWorld();
	UCombatComponent* PlayerCombat = nullptr;
	APlayerCharacter* Player = FCombatTestHelpers::CreateTestCharacterWithCombat(World, PlayerCombat);
	AEnemyCharacter* FrontEnemy = FCombatTestHelpers::CreateTestEnemyCharacter(World, FVector(150.0f, 0.0f, 0.0f));

	if (!Player || !PlayerCombat || !FrontEnemy)
	{
		AddError(TEXT("Failed to create null attack data block test actors"));
		FCombatTestHelpers::DestroyTestWorld(World);
		return false;
	}

	PlayerCombat->OnInputEvent(EInputType::Block, EInputEventType::Press);

	const float HealthBeforeHit = Player->CurrentHealth;
	FHitReactionInfo LegacyHit = FCombatTestHelpers::CreateTestHitInfo(
		FrontEnemy,
		25.0f,
		FVector::ForwardVector,
		nullptr);

	TestTrue(TEXT("Null AttackData should preserve normal block behavior"),
		PlayerCombat->CanBlockHit(LegacyHit));

	IDamageableInterface::Execute_ApplyDamage(Player, LegacyHit);
	TestEqual(TEXT("Null AttackData blocked hit should not damage"),
		Player->CurrentHealth,
		HealthBeforeHit);

	PlayerCombat->OnInputEvent(EInputType::Block, EInputEventType::Release);
	FCombatTestHelpers::DestroyTestWorld(World);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCounter_UnblockableTagBypassesNormalBlock,
	"KatanaCombat.CounterSystem.Block.UnblockableTagBypassesBlock",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCounter_UnblockableTagBypassesNormalBlock::RunTest(const FString& Parameters)
{
	UWorld* World = FCombatTestHelpers::CreateTestWorld();
	UCombatComponent* PlayerCombat = nullptr;
	APlayerCharacter* Player = FCombatTestHelpers::CreateTestCharacterWithCombat(World, PlayerCombat);
	AEnemyCharacter* FrontEnemy = FCombatTestHelpers::CreateTestEnemyCharacter(World, FVector(150.0f, 0.0f, 0.0f));

	if (!Player || !PlayerCombat || !FrontEnemy)
	{
		AddError(TEXT("Failed to create unblockable block test actors"));
		FCombatTestHelpers::DestroyTestWorld(World);
		return false;
	}

	PlayerCombat->OnInputEvent(EInputType::Block, EInputEventType::Press);
	TestTrue(TEXT("Front enemy should be blockable before attack tags are considered"),
		PlayerCombat->CanBlockAttackFrom(FrontEnemy));

	UAttackData* UnblockableAttack = FCombatTestHelpers::CreateTestAttack(EAttackType::Heavy);
	UnblockableAttack->AttackTags.AddTag(KatanaCombatGameplayTags::AttackPropertyUnblockable());

	const float HealthBeforeHit = Player->CurrentHealth;
	FHitReactionInfo Hit = FCombatTestHelpers::CreateTestHitInfo(
		FrontEnemy,
		25.0f,
		FVector::ForwardVector,
		UnblockableAttack);

	TestFalse(TEXT("CanBlockHit should reject attacks tagged unblockable"),
		PlayerCombat->CanBlockHit(Hit));

	IDamageableInterface::Execute_ApplyDamage(Player, Hit);
	TestEqual(TEXT("Unblockable tagged hit should damage through normal block"),
		Player->CurrentHealth,
		HealthBeforeHit - 25.0f);

	PlayerCombat->OnInputEvent(EInputType::Block, EInputEventType::Release);
	FCombatTestHelpers::DestroyTestWorld(World);
	return true;
}
```

- [ ] **Step 2: Run the focused test to verify failure**

Run:

```powershell
&"C:\Program Files\Epic Games\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "D:\UnrealProjects\5.6\KatanaCombat\KatanaCombat.uproject" -ExecCmds="Automation RunTests KatanaCombat.CounterSystem.Block.NullAttackDataPreservesBlock;Quit" -unattended -nopause -NullRHI -nosplash -stdout
&"C:\Program Files\Epic Games\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "D:\UnrealProjects\5.6\KatanaCombat\KatanaCombat.uproject" -ExecCmds="Automation RunTests KatanaCombat.CounterSystem.Block.UnblockableTagBypassesBlock;Quit" -unattended -nopause -NullRHI -nosplash -stdout
```

Expected: compile failure because `CanBlockHit` does not exist.

- [ ] **Step 3: Add `CanBlockHit` declaration**

In `CombatComponent.h`, add after `CanBlockAttackFrom`:

```cpp
	/** True when the held block should mitigate this concrete incoming hit. */
	UFUNCTION(BlueprintPure, Category = "Combat|Block")
	bool CanBlockHit(const FHitReactionInfo& HitInfo) const;
```

- [ ] **Step 4: Implement tag-aware block resolution**

In `CombatComponent.cpp`, add near the existing anonymous helper area or above `CanBlockAttackFrom`:

```cpp
#include "Utilities/CombatGameplayTags.h"

namespace
{
bool IsAttackTaggedUnblockable(const UAttackData* AttackData)
{
	if (!AttackData)
	{
		return false;
	}

	const FGameplayTag UnblockableTag = KatanaCombatGameplayTags::AttackPropertyUnblockable();
	return UnblockableTag.IsValid() && AttackData->AttackTags.HasTag(UnblockableTag);
}
}
```

Then add after `CanBlockAttackFrom`:

```cpp
bool UCombatComponent::CanBlockHit(const FHitReactionInfo& HitInfo) const
{
	if (!CanBlockAttackFrom(HitInfo.Attacker))
	{
		return false;
	}

	if (IsAttackTaggedUnblockable(HitInfo.AttackData))
	{
		return false;
	}

	return true;
}
```

- [ ] **Step 5: Use `CanBlockHit` in damage and impact classification**

In `ABaseCombatCharacter::ApplyDamage_Implementation`, replace:

```cpp
	if (CombatComponent && CombatComponent->CanBlockAttackFrom(HitInfo.Attacker))
```

with:

```cpp
	if (CombatComponent && CombatComponent->CanBlockHit(HitInfo))
```

In `ABaseCombatCharacter::OnWeaponHitTarget`, replace:

```cpp
			bWasBlocked = HitCombatCharacter->CombatComponent &&
				HitCombatCharacter->CombatComponent->CanBlockAttackFrom(this);
```

with:

```cpp
			bWasBlocked = HitCombatCharacter->CombatComponent &&
				HitCombatCharacter->CombatComponent->CanBlockHit(HitInfo);
```

This task only makes `ABaseCombatCharacter` damage and hit-impact classification hit-aware. Generic `IDamageableInterface` implementers still expose only `IsBlocking()` and cannot inspect `FHitReactionInfo::AttackData`; a separate hit-aware defense interface is required before unblockable behavior is claimed for non-`ABaseCombatCharacter` targets.

- [ ] **Step 6: Run focused block tests**

Run:

```powershell
&"C:\Program Files\Epic Games\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "D:\UnrealProjects\5.6\KatanaCombat\KatanaCombat.uproject" -ExecCmds="Automation RunTests KatanaCombat.CounterSystem.Block;Quit" -unattended -nopause -NullRHI -nosplash -stdout
```

Expected: normal block tests and unblockable tag test pass.

- [ ] **Step 7: Review task diff before any staging**

```powershell
git diff -- Source/KatanaCombat/Public/Core/CombatComponent.h Source/KatanaCombat/Private/Core/CombatComponent.cpp Source/KatanaCombat/Private/Characters/BaseCombatCharacter.cpp Source/KatanaCombatTest/Private/CounterSystemTests.cpp
git diff --check -- Source/KatanaCombat/Public/Core/CombatComponent.h Source/KatanaCombat/Private/Core/CombatComponent.cpp Source/KatanaCombat/Private/Characters/BaseCombatCharacter.cpp Source/KatanaCombatTest/Private/CounterSystemTests.cpp
```

Expected: diff only contains Task 2 changes, and whitespace check reports no errors. If a commit is requested, stage with `git add -p` and inspect `git diff --staged` before committing.

---

### Task 3: Semantic Tag Visibility In Readiness Reports

**Files:**
- Modify: `Source/KatanaCombatEditor/Public/AttackDataNotifyGenerationService.h`
- Modify: `Source/KatanaCombatEditor/Private/AttackDataNotifyGenerationService.cpp`
- Modify: `Source/KatanaCombatEditor/Public/Commandlets/KatanaAssetMigrationTypes.h`
- Modify: `Source/KatanaCombatEditor/Private/Commandlets/Operations/AttackDataNotifyMigrationOperation.cpp`
- Modify: `Source/KatanaCombatEditor/Private/Commandlets/Operations/AttackDataTimingMigrationOperation.cpp`
- Modify: `Source/KatanaCombatEditor/Private/Commandlets/Operations/ContentReadinessAuditOperation.cpp`
- Modify: `Source/KatanaCombatEditor/Private/Commandlets/Operations/CounterChainProofMigrationOperation.cpp`
- Modify: `Source/KatanaCombatEditor/Private/Commandlets/KatanaAssetMigrationRunner.cpp`
- Modify: `Source/KatanaCombatTest/Private/AttackDataEditorToolsTests.cpp`
- Modify: `Source/KatanaCombatTest/Private/KatanaAssetMigrationTests.cpp`

**Interfaces:**
- Consumes: `UAttackData::AttackTags`, `UAttackData::RequiredContextTags`, `KatanaCombatGameplayTags::AttackPropertyUnblockable()`, `KatanaCombatGameplayTags::ContextParryCounter()`.
- Produces report fields:
  - `attack_tags`
  - `required_context_tags`
  - `has_required_context_tags`
  - `has_unblockable_tag`

- [ ] **Step 1: Add failing analysis, row, and report-field assertions**

In `Source/KatanaCombatTest/Private/AttackDataEditorToolsTests.cpp`, add `#include "Utilities/CombatGameplayTags.h"` and append these tests near the existing `AttackDataTools.Analysis.*` tests:

```cpp
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAttackDataNotifyAnalysisReportsSemanticTagsTest,
	"KatanaCombat.Editor.AttackDataTools.Analysis.SemanticTags",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FAttackDataNotifyAnalysisReportsSemanticTagsTest::RunTest(const FString& Parameters)
{
	UAnimMontage* Montage = CreateTransientMontageWithSections();
	UAttackData* AttackData = CreateValidLightAttackData(Montage);

	const FGameplayTag UnblockableTag = KatanaCombatGameplayTags::AttackPropertyUnblockable();
	const FGameplayTag ParryCounterTag = KatanaCombatGameplayTags::ContextParryCounter();
	AttackData->AttackTags.AddTag(UnblockableTag);
	AttackData->RequiredContextTags.AddTag(ParryCounterTag);

	const FAttackDataNotifyAnalysis Analysis = FAttackDataNotifyGenerationService::AnalyzeAttackDataNotifies(AttackData);

	TestTrue(TEXT("Analysis should include authored attack tag"),
		Analysis.AttackTags.Contains(TEXT("Attack.Property.Unblockable")));
	TestTrue(TEXT("Analysis should include authored required context tag"),
		Analysis.RequiredContextTags.Contains(TEXT("Context.ParryCounter")));
	TestTrue(TEXT("Analysis should flag required context tags"), Analysis.bHasRequiredContextTags);
	TestTrue(TEXT("Analysis should flag unblockable tag"), Analysis.bHasUnblockableTag);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAttackDataNotifyAnalysisReportsSemanticTagsForInvalidMontageTest,
	"KatanaCombat.Editor.AttackDataTools.Analysis.SemanticTagsInvalidMontage",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FAttackDataNotifyAnalysisReportsSemanticTagsForInvalidMontageTest::RunTest(const FString& Parameters)
{
	UAttackData* AttackData = NewObject<UAttackData>(GetTransientPackage());
	AttackData->AttackTags.AddTag(KatanaCombatGameplayTags::AttackPropertyUnblockable());
	AttackData->RequiredContextTags.AddTag(KatanaCombatGameplayTags::ContextParryCounter());

	const FAttackDataNotifyAnalysis Analysis = FAttackDataNotifyGenerationService::AnalyzeAttackDataNotifies(AttackData);

	TestFalse(TEXT("Analysis should remain invalid without montage"), Analysis.bValid);
	TestTrue(TEXT("Invalid-montage analysis should still include authored attack tag"),
		Analysis.AttackTags.Contains(TEXT("Attack.Property.Unblockable")));
	TestTrue(TEXT("Invalid-montage analysis should still include authored required context tag"),
		Analysis.RequiredContextTags.Contains(TEXT("Context.ParryCounter")));
	TestTrue(TEXT("Invalid-montage analysis should flag required context tags"), Analysis.bHasRequiredContextTags);
	TestTrue(TEXT("Invalid-montage analysis should flag unblockable tag"), Analysis.bHasUnblockableTag);
	return true;
}
```

In `Source/KatanaCombatTest/Private/KatanaAssetMigrationTests.cpp`, add `#include "Utilities/CombatGameplayTags.h"` near the other includes and append this helper in `namespace KatanaAssetMigrationTest`:

```cpp
	void AddSemanticTags(UAttackData* AttackData)
	{
		AttackData->AttackTags.AddTag(KatanaCombatGameplayTags::AttackPropertyUnblockable());
		AttackData->RequiredContextTags.AddTag(KatanaCombatGameplayTags::ContextParryCounter());
	}

	void ExpectSemanticRowFields(FAutomationTestBase& Test, const FKatanaAssetMigrationRow& Row, const TCHAR* Label)
	{
		Test.TestTrue(FString::Printf(TEXT("%s row should include unblockable attack tag"), Label),
			Row.AttackTags.Contains(TEXT("Attack.Property.Unblockable")));
		Test.TestTrue(FString::Printf(TEXT("%s row should include parry counter context tag"), Label),
			Row.RequiredContextTags.Contains(TEXT("Context.ParryCounter")));
		Test.TestTrue(FString::Printf(TEXT("%s row should flag required context tags"), Label),
			Row.bHasRequiredContextTags);
		Test.TestTrue(FString::Printf(TEXT("%s row should flag unblockable tag"), Label),
			Row.bHasUnblockableTag);
	}
```

Then add this operation-level propagation test:

```cpp
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FKatanaAssetMigrationRowsReportSemanticTagsTest,
	"KatanaCombat.Editor.AssetMigration.Rows.SemanticTags",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FKatanaAssetMigrationRowsReportSemanticTagsTest::RunTest(const FString& Parameters)
{
	UAnimMontage* Montage = KatanaAssetMigrationTest::CreateMontage();
	UAttackData* AttackData = KatanaAssetMigrationTest::CreateAttackData(Montage);
	KatanaAssetMigrationTest::AddSemanticTags(AttackData);

	FKatanaAssetMigrationRow NotifyRow;
	const FAttackDataNotifyMigrationOperation NotifyOperation;
	TestTrue(TEXT("Notify operation should run"), NotifyOperation.Run(AttackData, EKatanaAssetMigrationMode::Audit, NotifyRow));
	KatanaAssetMigrationTest::ExpectSemanticRowFields(*this, NotifyRow, TEXT("Notify"));

	FKatanaAssetMigrationRow TimingRow;
	const FAttackDataTimingMigrationOperation TimingOperation;
	TestTrue(TEXT("Timing operation should run"), TimingOperation.Run(AttackData, EKatanaAssetMigrationMode::Plan, TimingRow));
	KatanaAssetMigrationTest::ExpectSemanticRowFields(*this, TimingRow, TEXT("Timing"));

	FKatanaAssetMigrationRow ReadinessRow;
	const FContentReadinessAuditOperation ReadinessOperation;
	TestTrue(TEXT("Readiness operation should run"),
		ReadinessOperation.RunLoadedObject(TEXT("/Game/Test/DA_Attack.DA_Attack"), AttackData, false, ReadinessRow));
	KatanaAssetMigrationTest::ExpectSemanticRowFields(*this, ReadinessRow, TEXT("Readiness"));

	return true;
}
```

Also update `FKatanaCounterChainProofApplySeedsCounterDataAndWindowTest` in `Source/KatanaCombatTest/Private/KatanaAssetMigrationTests.cpp`: after creating `AttackData`, call `KatanaAssetMigrationTest::AddSemanticTags(AttackData);`, and after the existing row assertions add:

```cpp
	KatanaAssetMigrationTest::ExpectSemanticRowFields(*this, Row, TEXT("CounterChainProof"));
```

In `Source/KatanaCombatTest/Private/KatanaAssetMigrationTests.cpp`, extend the existing JSON/report field test that asserts `branch_readiness_warnings`, `has_parry_window`, and `has_counter_window` with:

```cpp
TestTrue(TEXT("row should include attack_tags"),
	(*Rows)[0]->AsObject()->HasTypedField<EJson::Array>(TEXT("attack_tags")));
TestTrue(TEXT("row should include required_context_tags"),
	(*Rows)[0]->AsObject()->HasTypedField<EJson::Array>(TEXT("required_context_tags")));
TestTrue(TEXT("row should include has_required_context_tags"),
	(*Rows)[0]->AsObject()->HasField(TEXT("has_required_context_tags")));
TestTrue(TEXT("row should include has_unblockable_tag"),
	(*Rows)[0]->AsObject()->HasField(TEXT("has_unblockable_tag")));
```

- [ ] **Step 2: Run the report test to verify failure**

Run:

```powershell
&"C:\Program Files\Epic Games\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "D:\UnrealProjects\5.6\KatanaCombat\KatanaCombat.uproject" -ExecCmds="Automation RunTests KatanaCombat.Editor.AttackDataTools.Analysis.SemanticTags;Quit" -unattended -nopause -NullRHI -nosplash -stdout
&"C:\Program Files\Epic Games\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "D:\UnrealProjects\5.6\KatanaCombat\KatanaCombat.uproject" -ExecCmds="Automation RunTests KatanaCombat.Editor.AttackDataTools.Analysis.SemanticTagsInvalidMontage;Quit" -unattended -nopause -NullRHI -nosplash -stdout
&"C:\Program Files\Epic Games\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "D:\UnrealProjects\5.6\KatanaCombat\KatanaCombat.uproject" -ExecCmds="Automation RunTests KatanaCombat.Editor.AssetMigration.Rows.SemanticTags;Quit" -unattended -nopause -NullRHI -nosplash -stdout
&"C:\Program Files\Epic Games\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "D:\UnrealProjects\5.6\KatanaCombat\KatanaCombat.uproject" -ExecCmds="Automation RunTests KatanaCombat.Editor.AssetMigration.Runner.ReportJsonFields;Quit" -unattended -nopause -NullRHI -nosplash -stdout
```

Expected: tests fail because semantic tag analysis fields, row fields, or JSON fields are absent.

- [ ] **Step 3: Add analysis and row fields**

In `FAttackDataNotifyAnalysis`, add:

```cpp
TArray<FString> AttackTags;
TArray<FString> RequiredContextTags;
bool bHasRequiredContextTags = false;
bool bHasUnblockableTag = false;
```

In `FKatanaAssetMigrationRow`, add the same fields.

- [ ] **Step 4: Populate analysis fields**

In `AttackDataNotifyGenerationService.cpp`, add `#include "Utilities/CombatGameplayTags.h"` and a local helper:

```cpp
static TArray<FString> GameplayTagContainerToStrings(const FGameplayTagContainer& Tags)
{
	TArray<FString> Values;
	TArray<FGameplayTag> TagArray;
	Tags.GetGameplayTagArray(TagArray);
	for (const FGameplayTag& Tag : TagArray)
	{
		Values.Add(Tag.ToString());
	}
	Values.Sort();
	return Values;
}
```

Immediately after the `if (!AttackData)` early return and before reading `AttackData->AttackMontage`, add:

```cpp
Analysis.AttackTags = GameplayTagContainerToStrings(AttackData->AttackTags);
Analysis.RequiredContextTags = GameplayTagContainerToStrings(AttackData->RequiredContextTags);
Analysis.bHasRequiredContextTags = !AttackData->RequiredContextTags.IsEmpty();

const FGameplayTag UnblockableTag = KatanaCombatGameplayTags::AttackPropertyUnblockable();
Analysis.bHasUnblockableTag = UnblockableTag.IsValid() && AttackData->AttackTags.HasTag(UnblockableTag);
```

Do not place this block near the end of `AnalyzeAttackDataNotifies()`. Missing montage, invalid section, and invalid timing all return early, and reports still need to show authored semantic tags for those invalid assets.

- [ ] **Step 5: Copy and serialize row fields**

In every operation that copies `FAttackDataNotifyAnalysis` into `FKatanaAssetMigrationRow`, copy the new fields. This exact list must be updated:

- `Source/KatanaCombatEditor/Private/Commandlets/Operations/AttackDataNotifyMigrationOperation.cpp`
- `Source/KatanaCombatEditor/Private/Commandlets/Operations/AttackDataTimingMigrationOperation.cpp`
- `Source/KatanaCombatEditor/Private/Commandlets/Operations/ContentReadinessAuditOperation.cpp`
- `Source/KatanaCombatEditor/Private/Commandlets/Operations/CounterChainProofMigrationOperation.cpp`

```cpp
OutRow.AttackTags = Analysis.AttackTags;
OutRow.RequiredContextTags = Analysis.RequiredContextTags;
OutRow.bHasRequiredContextTags = Analysis.bHasRequiredContextTags;
OutRow.bHasUnblockableTag = Analysis.bHasUnblockableTag;
```

In `KatanaAssetMigrationRunner.cpp`, serialize:

```cpp
RowObject->SetArrayField(TEXT("attack_tags"), ToJsonArray(Row.AttackTags));
RowObject->SetArrayField(TEXT("required_context_tags"), ToJsonArray(Row.RequiredContextTags));
RowObject->SetBoolField(TEXT("has_required_context_tags"), Row.bHasRequiredContextTags);
RowObject->SetBoolField(TEXT("has_unblockable_tag"), Row.bHasUnblockableTag);
```

- [ ] **Step 6: Run focused editor report tests**

Run:

```powershell
&"C:\Program Files\Epic Games\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "D:\UnrealProjects\5.6\KatanaCombat\KatanaCombat.uproject" -ExecCmds="Automation RunTests KatanaCombat.Editor.AttackDataTools.Analysis.SemanticTags;Quit" -unattended -nopause -NullRHI -nosplash -stdout
&"C:\Program Files\Epic Games\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "D:\UnrealProjects\5.6\KatanaCombat\KatanaCombat.uproject" -ExecCmds="Automation RunTests KatanaCombat.Editor.AttackDataTools.Analysis.SemanticTagsInvalidMontage;Quit" -unattended -nopause -NullRHI -nosplash -stdout
&"C:\Program Files\Epic Games\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "D:\UnrealProjects\5.6\KatanaCombat\KatanaCombat.uproject" -ExecCmds="Automation RunTests KatanaCombat.Editor.AssetMigration.Rows.SemanticTags;Quit" -unattended -nopause -NullRHI -nosplash -stdout
&"C:\Program Files\Epic Games\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "D:\UnrealProjects\5.6\KatanaCombat\KatanaCombat.uproject" -ExecCmds="Automation RunTests KatanaCombat.Editor.AssetMigration.Runner.ReportJsonFields;Quit" -unattended -nopause -NullRHI -nosplash -stdout
```

Expected: semantic analysis, operation row propagation, and JSON field tests pass.

- [ ] **Step 7: Review task diff before any staging**

```powershell
git diff -- Source/KatanaCombatEditor/Public/AttackDataNotifyGenerationService.h Source/KatanaCombatEditor/Private/AttackDataNotifyGenerationService.cpp Source/KatanaCombatEditor/Public/Commandlets/KatanaAssetMigrationTypes.h Source/KatanaCombatEditor/Private/Commandlets/Operations/AttackDataNotifyMigrationOperation.cpp Source/KatanaCombatEditor/Private/Commandlets/Operations/AttackDataTimingMigrationOperation.cpp Source/KatanaCombatEditor/Private/Commandlets/Operations/ContentReadinessAuditOperation.cpp Source/KatanaCombatEditor/Private/Commandlets/Operations/CounterChainProofMigrationOperation.cpp Source/KatanaCombatEditor/Private/Commandlets/KatanaAssetMigrationRunner.cpp Source/KatanaCombatTest/Private/AttackDataEditorToolsTests.cpp Source/KatanaCombatTest/Private/KatanaAssetMigrationTests.cpp
git diff --check -- Source/KatanaCombatEditor/Public/AttackDataNotifyGenerationService.h Source/KatanaCombatEditor/Private/AttackDataNotifyGenerationService.cpp Source/KatanaCombatEditor/Public/Commandlets/KatanaAssetMigrationTypes.h Source/KatanaCombatEditor/Private/Commandlets/Operations/AttackDataNotifyMigrationOperation.cpp Source/KatanaCombatEditor/Private/Commandlets/Operations/AttackDataTimingMigrationOperation.cpp Source/KatanaCombatEditor/Private/Commandlets/Operations/ContentReadinessAuditOperation.cpp Source/KatanaCombatEditor/Private/Commandlets/Operations/CounterChainProofMigrationOperation.cpp Source/KatanaCombatEditor/Private/Commandlets/KatanaAssetMigrationRunner.cpp Source/KatanaCombatTest/Private/AttackDataEditorToolsTests.cpp Source/KatanaCombatTest/Private/KatanaAssetMigrationTests.cpp
```

Expected: diff only contains Task 3 changes, and whitespace check reports no errors. If a commit is requested, stage with `git add -p` and inspect `git diff --staged` before committing.

---

### Task 4: Verification And Documentation Closeout

**Files:**
- Modify: `docs/handoffs/2026-07-02-combat-semantic-runtime-audit.md`
- Modify: `docs/superpowers/specs/2026-07-02-combat-semantics-ownership-design.md`

**Interfaces:**
- Consumes: completed Tasks 1-3 and test evidence.
- Produces: updated proof boundary stating which tags are now runtime-authoritative.

- [ ] **Step 1: Run focused runtime and editor tests**

Run:

```powershell
&"C:\Program Files\Epic Games\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "D:\UnrealProjects\5.6\KatanaCombat\KatanaCombat.uproject" -ExecCmds="Automation RunTests KatanaCombat.GameplayTags.SemanticTagsRegistered;Quit" -unattended -nopause -NullRHI -nosplash -stdout
&"C:\Program Files\Epic Games\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "D:\UnrealProjects\5.6\KatanaCombat\KatanaCombat.uproject" -ExecCmds="Automation RunTests KatanaCombat.AttackResolution.Context;Quit" -unattended -nopause -NullRHI -nosplash -stdout
&"C:\Program Files\Epic Games\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "D:\UnrealProjects\5.6\KatanaCombat\KatanaCombat.uproject" -ExecCmds="Automation RunTests KatanaCombat.CounterSystem.Block;Quit" -unattended -nopause -NullRHI -nosplash -stdout
&"C:\Program Files\Epic Games\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "D:\UnrealProjects\5.6\KatanaCombat\KatanaCombat.uproject" -ExecCmds="Automation RunTests KatanaCombat.Editor.AttackDataTools.Analysis.SemanticTags;Quit" -unattended -nopause -NullRHI -nosplash -stdout
&"C:\Program Files\Epic Games\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "D:\UnrealProjects\5.6\KatanaCombat\KatanaCombat.uproject" -ExecCmds="Automation RunTests KatanaCombat.Editor.AttackDataTools.Analysis.SemanticTagsInvalidMontage;Quit" -unattended -nopause -NullRHI -nosplash -stdout
&"C:\Program Files\Epic Games\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "D:\UnrealProjects\5.6\KatanaCombat\KatanaCombat.uproject" -ExecCmds="Automation RunTests KatanaCombat.Editor.AssetMigration.Rows.SemanticTags;Quit" -unattended -nopause -NullRHI -nosplash -stdout
&"C:\Program Files\Epic Games\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "D:\UnrealProjects\5.6\KatanaCombat\KatanaCombat.uproject" -ExecCmds="Automation RunTests KatanaCombat.Editor.AssetMigration.Runner.ReportJsonFields;Quit" -unattended -nopause -NullRHI -nosplash -stdout
```

Expected: all focused tests pass.

- [ ] **Step 2: Build editor target**

Run:

```powershell
&"C:\Program Files\Epic Games\UE_5.6\Engine\Build\BatchFiles\Build.bat" KatanaCombatEditor Win64 Development -Project="D:\UnrealProjects\5.6\KatanaCombat\KatanaCombat.uproject" -Progress -NoHotReload
```

Expected: exit code `0`.

- [ ] **Step 3: Update proof docs**

Append this to `docs/handoffs/2026-07-02-combat-semantic-runtime-audit.md`:

```markdown
## Implementation Proof Update

- `RequiredContextTags` is now consumed by attack resolution for existing candidates.
- `Attack.Property.Unblockable` is now consumed by concrete hit/block resolution.
- Semantic combat tag names now route through shared runtime accessors.
- Readiness reports now surface `AttackTags` and `RequiredContextTags`, including invalid non-null `AttackData` rows that return early for missing montage, invalid section, or invalid timing.
- Null `FHitReactionInfo::AttackData` still preserves normal block behavior.
- Unblockable behavior is proven for `ABaseCombatCharacter` damage and impact classification only; generic `IDamageableInterface` targets still need a hit-aware defense API.
- Remaining tag contracts outside this slice, including hit-reaction context tags and future block/recoil tags, still require separate runtime proof.
```

In `docs/superpowers/specs/2026-07-02-combat-semantics-ownership-design.md`, update the current reconciliation target bullets for `RequiredContextTags` and `Attack.Property.Unblockable` from gap wording to implemented wording, while keeping hit-reaction tags listed as future work.

- [ ] **Step 4: Run doc sanity checks**

Run:

```powershell
rg -n "RequiredContextTags.*must be evaluated|Attack\\.Property\\.Unblockable.*must inspect|decorative" docs/superpowers/specs/2026-07-02-combat-semantics-ownership-design.md docs/handoffs/2026-07-02-combat-semantic-runtime-audit.md
git diff --check -- docs/handoffs/2026-07-02-combat-semantic-runtime-audit.md docs/superpowers/specs/2026-07-02-combat-semantics-ownership-design.md
```

Expected: no stale gap wording for implemented tags and no whitespace errors.

- [ ] **Step 5: Review docs diff before any staging**

```powershell
git diff -- docs/handoffs/2026-07-02-combat-semantic-runtime-audit.md docs/superpowers/specs/2026-07-02-combat-semantics-ownership-design.md docs/superpowers/plans/2026-07-02-combat-tag-runtime-resolution.md
git diff --check -- docs/handoffs/2026-07-02-combat-semantic-runtime-audit.md docs/superpowers/specs/2026-07-02-combat-semantics-ownership-design.md docs/superpowers/plans/2026-07-02-combat-tag-runtime-resolution.md
```

Expected: docs reflect the verified proof boundary, and whitespace check reports no errors. If a commit is requested, stage with `git add -p` and inspect `git diff --staged` before committing.

---

## Self-Review

### Spec Coverage

- Shared semantic gameplay tag accessors and registration proof are covered by Task 1.
- `RequiredContextTags` runtime consumption for existing resolver candidates is covered by Task 1.
- Structural `EResolutionPath` behavior is preserved by Task 1: context gates eligibility but does not relabel normal combo resolution as counter or finisher resolution.
- `Attack.Property.Unblockable` runtime consumption is covered by Task 2, with null `AttackData` preserving legacy block behavior.
- The `ABaseCombatCharacter` scope boundary for hit-aware block resolution is documented in Task 2.
- Report visibility for authored semantic tags is covered by Task 3 across notify migration, timing migration, content readiness, and counter-chain proof rows, including invalid non-null `AttackData` early-return cases.
- Documentation proof boundary is covered by Task 4.

### Scope Control

This plan does not add new gameplay tags, does not mutate assets, does not redesign hit reactions, does not implement attacker recoil, does not implement block alignment, and does not convert runtime state machines to tags.

### Remaining Design Gaps

- `ActiveContextTags` still needs gameplay-event producers beyond the explicit mutator API. Chain Counter can add/remove `Context.ParryCounter` only after a behavior-specific task defines add/remove ownership for parry success, counter advance, counter fail, paired cancel, owner death, partner death, timeout, and normal completion.
- This slice proves normal block can reject unblockable attacks. It does not define the full block matrix for normal block, perfect parry, counter start, guard break, attacker recoil, VFX/audio selection, or motion-warped block alignment.
- Generic `IDamageableInterface` targets remain outside hit-aware unblockable resolution until a defense outcome API accepts `FHitReactionInfo`.
- `UHitReactionData::ReactionTags` and `RequiredContextTags` remain unproven until hit-reaction selection is redesigned.
- Future tags such as block-interruptible, parryable, recoil type, and armor-breaking should be added only after a centralized defense outcome resolver is planned.
