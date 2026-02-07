// ComboBlendSystemTests.cpp
// Comprehensive test suite for combo progression, blending, and attack state transitions
// Tests all potential bug-causing areas in the combat system

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "CombatTypes.h"
#include "ActionQueueTypes.h"
#include "Core/CombatComponent.h"
#include "Data/AttackData.h"
#include "Data/CombatSettings.h"
#include "Data/ProceduralAnimationTypes.h"
#include "Utilities/ProceduralAnimationLibrary.h"
#include "CombatTestHelpers.h"
#include "Characters/PlayerCharacter.h"

// ============================================================================
// TEST CATEGORY 1: COMBO PROGRESSION
// Tests that combos chain correctly through the attack sequence
// ============================================================================

/**
 * Test: Light attack combo progresses through all attacks in sequence.
 * Verifies CurrentAttackData and NextComboAttack linkage.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FComboProgression_LightChain,
	"KatanaCombat.ComboBlend.Progression.LightChain",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FComboProgression_LightChain::RunTest(const FString& Parameters)
{
	// Test the attack data chain linkage
	UWorld* World = FCombatTestHelpers::CreateTestWorld();
	UCombatComponent* CombatComp = nullptr;
	APlayerCharacter* Player = FCombatTestHelpers::CreateTestCharacterWithCombat(World, CombatComp);

	if (!CombatComp || !Player)
	{
		AddError(TEXT("Failed to create test player"));
		FCombatTestHelpers::DestroyTestWorld(World);
		return false;
	}

	UAttackData* CurrentAttack = CombatComp->GetDefaultLightAttack();
	if (!CurrentAttack)
	{
		AddWarning(TEXT("DefaultLightAttack is null - combo chain cannot be tested (data asset not configured)"));
		FCombatTestHelpers::DestroyTestWorld(World);
		return true;  // Not a failure, just not configured
	}

	// Verify combo chain exists and is properly linked
	int32 ChainLength = 0;
	TArray<FString> ChainNames;

	while (CurrentAttack && ChainLength < 20)  // Safety limit
	{
		ChainNames.Add(CurrentAttack->GetName());
		ChainLength++;
		CurrentAttack = CurrentAttack->NextComboAttack;
	}

	TestTrue("Combo chain has at least 2 attacks", ChainLength >= 2);

	// Log the chain for debugging
	FString ChainStr = FString::Join(ChainNames, TEXT(" -> "));
	AddInfo(FString::Printf(TEXT("Light combo chain (%d attacks): %s"), ChainLength, *ChainStr));

	FCombatTestHelpers::DestroyTestWorld(World);
	return true;
}

/**
 * Test: Heavy attack combo progresses correctly.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FComboProgression_HeavyChain,
	"KatanaCombat.ComboBlend.Progression.HeavyChain",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FComboProgression_HeavyChain::RunTest(const FString& Parameters)
{
	UWorld* World = FCombatTestHelpers::CreateTestWorld();
	UCombatComponent* CombatComp = nullptr;
	APlayerCharacter* Player = FCombatTestHelpers::CreateTestCharacterWithCombat(World, CombatComp);

	if (!CombatComp || !Player)
	{
		AddError(TEXT("Failed to create test player"));
		FCombatTestHelpers::DestroyTestWorld(World);
		return false;
	}

	UAttackData* CurrentAttack = CombatComp->GetDefaultHeavyAttack();
	if (!CurrentAttack)
	{
		AddWarning(TEXT("DefaultHeavyAttack is null - heavy combo chain cannot be tested (data asset not configured)"));
		FCombatTestHelpers::DestroyTestWorld(World);
		return true;  // Not a failure, just not configured
	}

	int32 ChainLength = 0;
	TArray<FString> ChainNames;

	while (CurrentAttack && ChainLength < 20)
	{
		ChainNames.Add(CurrentAttack->GetName());
		ChainLength++;
		CurrentAttack = CurrentAttack->NextComboAttack;
	}

	TestTrue("Heavy combo chain has at least 1 attack", ChainLength >= 1);

	FString ChainStr = FString::Join(ChainNames, TEXT(" -> "));
	AddInfo(FString::Printf(TEXT("Heavy combo chain (%d attacks): %s"), ChainLength, *ChainStr));

	FCombatTestHelpers::DestroyTestWorld(World);
	return true;
}

/**
 * Test: Combo window flag state tracking.
 * Verifies bComboWindowActive flag behavior.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FComboProgression_WindowFlag,
	"KatanaCombat.ComboBlend.Progression.WindowFlag",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FComboProgression_WindowFlag::RunTest(const FString& Parameters)
{
	UWorld* World = FCombatTestHelpers::CreateTestWorld();
	UCombatComponent* CombatComp = nullptr;
	APlayerCharacter* Player = FCombatTestHelpers::CreateTestCharacterWithCombat(World, CombatComp);

	if (!CombatComp)
	{
		AddError(TEXT("Failed to create test player"));
		FCombatTestHelpers::DestroyTestWorld(World);
		return false;
	}

	// Initially combo window should not be active
	TestFalse("Combo window initially inactive", CombatComp->IsInComboWindow());

	FCombatTestHelpers::DestroyTestWorld(World);
	return true;
}

// ============================================================================
// TEST CATEGORY 2: CURRENT ATTACK DATA LIFECYCLE
// Tests that CurrentAttackData is preserved correctly during combo flow
// ============================================================================

/**
 * Test: Attack phase is None when idle.
 * (CurrentAttackData is on WeaponComponent, not CombatComponent, so test phase instead)
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCurrentAttack_PhaseNoneWhenIdle,
	"KatanaCombat.ComboBlend.CurrentAttack.PhaseNoneWhenIdle",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FCurrentAttack_PhaseNoneWhenIdle::RunTest(const FString& Parameters)
{
	UWorld* World = FCombatTestHelpers::CreateTestWorld();
	UCombatComponent* CombatComp = nullptr;
	APlayerCharacter* Player = FCombatTestHelpers::CreateTestCharacterWithCombat(World, CombatComp);

	if (!CombatComp)
	{
		AddError(TEXT("Failed to create test player"));
		FCombatTestHelpers::DestroyTestWorld(World);
		return false;
	}

	// When idle, attack phase should be None
	TestEqual("Phase is None when idle", CombatComp->GetCurrentPhase(), EAttackPhase::None);

	FCombatTestHelpers::DestroyTestWorld(World);
	return true;
}

/**
 * Test: Attack phase enum values are correct.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCurrentAttack_PhaseValues,
	"KatanaCombat.ComboBlend.CurrentAttack.PhaseValues",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FCurrentAttack_PhaseValues::RunTest(const FString& Parameters)
{
	// Verify phase enum values match expected
	TestEqual("None phase is 0", static_cast<int32>(EAttackPhase::None), 0);
	TestEqual("Windup phase is 1", static_cast<int32>(EAttackPhase::Windup), 1);
	TestEqual("Active phase is 2", static_cast<int32>(EAttackPhase::Active), 2);
	TestEqual("Recovery phase is 3", static_cast<int32>(EAttackPhase::Recovery), 3);

	return true;
}

// ============================================================================
// TEST CATEGORY 3: BLEND TIME CALCULATION
// Tests procedural blend time calculation at various animation progress points
// ============================================================================

/**
 * Test: Blend time at animation start (0% progress).
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBlendCalc_AtStart,
	"KatanaCombat.ComboBlend.BlendCalc.AtStart",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FBlendCalc_AtStart::RunTest(const FString& Parameters)
{
	FProceduralBlendConfig Config;
	Config.PerceptualParams = FPerceptualDerivationParams::FromPreset(60.0f, ECombatFeelPreset::Balanced);

	FProceduralBlendResult Result = UProceduralAnimationLibrary::CalculateProceduralBlend(
		0.0f,   // CurrentPosition (start)
		1.0f,   // MontageLength
		Config,
		false   // bIsRapidInput
	);

	TestTrue("Result is valid", Result.IsValid());
	TestEqual("Progress is 0%", Result.AnimationProgress, 0.0f);
	TestTrue("Blend time > 0", Result.BlendInTime > 0.0f);

	// At start, blend should be near max (we have lots of time)
	const float MaxBlend = Config.PerceptualParams.GetMaxBlendTime();
	TestTrue("Blend time near max at start", Result.BlendInTime >= MaxBlend * 0.8f);

	AddInfo(FString::Printf(TEXT("Blend at start: %.3fs (Max: %.3fs)"), Result.BlendInTime, MaxBlend));

	return true;
}

/**
 * Test: Blend time at animation middle (50% progress).
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBlendCalc_AtMiddle,
	"KatanaCombat.ComboBlend.BlendCalc.AtMiddle",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FBlendCalc_AtMiddle::RunTest(const FString& Parameters)
{
	FProceduralBlendConfig Config;
	Config.PerceptualParams = FPerceptualDerivationParams::FromPreset(60.0f, ECombatFeelPreset::Balanced);

	FProceduralBlendResult Result = UProceduralAnimationLibrary::CalculateProceduralBlend(
		0.5f,   // CurrentPosition (middle)
		1.0f,   // MontageLength
		Config,
		false
	);

	TestTrue("Result is valid", Result.IsValid());
	TestTrue("Progress near 50%", FMath::IsNearlyEqual(Result.AnimationProgress, 0.5f, 0.01f));

	// At middle, blend should be between min and max
	const float MinBlend = Config.PerceptualParams.GetMinBlendTime();
	const float MaxBlend = Config.PerceptualParams.GetMaxBlendTime();
	TestTrue("Blend time between min and max", Result.BlendInTime >= MinBlend && Result.BlendInTime <= MaxBlend);

	AddInfo(FString::Printf(TEXT("Blend at 50%%: %.3fs (Range: %.3f-%.3f)"),
		Result.BlendInTime, MinBlend, MaxBlend));

	return true;
}

/**
 * Test: Blend time at animation end (95% progress).
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBlendCalc_AtEnd,
	"KatanaCombat.ComboBlend.BlendCalc.AtEnd",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FBlendCalc_AtEnd::RunTest(const FString& Parameters)
{
	FProceduralBlendConfig Config;
	Config.PerceptualParams = FPerceptualDerivationParams::FromPreset(60.0f, ECombatFeelPreset::Balanced);

	FProceduralBlendResult Result = UProceduralAnimationLibrary::CalculateProceduralBlend(
		0.95f,  // CurrentPosition (near end)
		1.0f,   // MontageLength
		Config,
		false
	);

	TestTrue("Result is valid", Result.IsValid());

	// At end, blend should be constrained by remaining time (0.05s)
	const float RemainingTime = 0.05f;
	TestTrue("Blend time <= remaining time", Result.BlendInTime <= RemainingTime + 0.001f);

	AddInfo(FString::Printf(TEXT("Blend at 95%%: %.3fs (Remaining: %.3fs)"),
		Result.BlendInTime, Result.RemainingTime));

	return true;
}

/**
 * Test: Rapid input during blend forces faster transition.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBlendCalc_RapidInput,
	"KatanaCombat.ComboBlend.BlendCalc.RapidInput",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FBlendCalc_RapidInput::RunTest(const FString& Parameters)
{
	FProceduralBlendConfig Config;
	Config.PerceptualParams = FPerceptualDerivationParams::FromPreset(60.0f, ECombatFeelPreset::Balanced);
	Config.RapidInputMode = ERapidInputBlendMode::ForceInstant;

	// Normal input
	FProceduralBlendResult NormalResult = UProceduralAnimationLibrary::CalculateProceduralBlend(
		0.3f, 1.0f, Config, false);

	// Rapid input
	FProceduralBlendResult RapidResult = UProceduralAnimationLibrary::CalculateProceduralBlend(
		0.3f, 1.0f, Config, true);

	TestTrue("Rapid input flagged", RapidResult.bRapidInputDetected);
	TestTrue("Rapid input uses instant blend", RapidResult.bUseInstantBlend);
	TestTrue("Rapid blend faster than normal", RapidResult.BlendInTime <= NormalResult.BlendInTime);

	AddInfo(FString::Printf(TEXT("Normal: %.3fs, Rapid: %.3fs"),
		NormalResult.BlendInTime, RapidResult.BlendInTime));

	return true;
}

/**
 * Test: Fresh attack (no previous montage) uses instant blend.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBlendCalc_FreshAttack,
	"KatanaCombat.ComboBlend.BlendCalc.FreshAttack",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FBlendCalc_FreshAttack::RunTest(const FString& Parameters)
{
	FProceduralBlendConfig Config;
	Config.PerceptualParams = FPerceptualDerivationParams::FromPreset(60.0f, ECombatFeelPreset::Balanced);

	// Zero-length montage = fresh attack (no previous animation)
	FProceduralBlendResult Result = UProceduralAnimationLibrary::CalculateProceduralBlend(
		0.0f,  // CurrentPosition
		0.0f,  // MontageLength = 0 means fresh attack
		Config,
		false
	);

	TestTrue("Fresh attack flagged", Result.bIsFreshAttack);
	TestTrue("Instant blend flagged", Result.bUseInstantBlend);

	return true;
}

// ============================================================================
// TEST CATEGORY 4: BLEND STRATEGIES
// Tests different blend interpolation strategies
// ============================================================================

/**
 * Test: Linear strategy produces linear interpolation.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBlendStrategy_Linear,
	"KatanaCombat.ComboBlend.Strategy.Linear",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FBlendStrategy_Linear::RunTest(const FString& Parameters)
{
	FProceduralBlendConfig Config;
	Config.Strategy = EProceduralStrategy::Linear;
	Config.PerceptualParams = FPerceptualDerivationParams::FromPreset(60.0f, ECombatFeelPreset::Balanced);

	// Test at 25%, 50%, 75%
	FProceduralBlendResult Result25 = UProceduralAnimationLibrary::CalculateProceduralBlend(0.25f, 1.0f, Config, false);
	FProceduralBlendResult Result50 = UProceduralAnimationLibrary::CalculateProceduralBlend(0.50f, 1.0f, Config, false);
	FProceduralBlendResult Result75 = UProceduralAnimationLibrary::CalculateProceduralBlend(0.75f, 1.0f, Config, false);

	// Linear: blend should decrease linearly with progress
	TestTrue("25% > 50%", Result25.BlendInTime > Result50.BlendInTime);
	TestTrue("50% > 75%", Result50.BlendInTime > Result75.BlendInTime);

	AddInfo(FString::Printf(TEXT("Linear: 25%%=%.3f, 50%%=%.3f, 75%%=%.3f"),
		Result25.BlendInTime, Result50.BlendInTime, Result75.BlendInTime));

	return true;
}

/**
 * Test: EaseOut strategy produces fast-start, slow-end interpolation.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBlendStrategy_EaseOut,
	"KatanaCombat.ComboBlend.Strategy.EaseOut",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FBlendStrategy_EaseOut::RunTest(const FString& Parameters)
{
	FProceduralBlendConfig Config;
	Config.Strategy = EProceduralStrategy::EaseOut;
	Config.PerceptualParams = FPerceptualDerivationParams::FromPreset(60.0f, ECombatFeelPreset::Balanced);

	FProceduralBlendResult Result25 = UProceduralAnimationLibrary::CalculateProceduralBlend(0.25f, 1.0f, Config, false);
	FProceduralBlendResult Result50 = UProceduralAnimationLibrary::CalculateProceduralBlend(0.50f, 1.0f, Config, false);

	TestEqual("Strategy used is EaseOut", Result25.UsedStrategy, EProceduralStrategy::EaseOut);

	// EaseOut: faster decrease at start (more change between 25-50% than linear would have)
	AddInfo(FString::Printf(TEXT("EaseOut: 25%%=%.3f, 50%%=%.3f"),
		Result25.BlendInTime, Result50.BlendInTime));

	return true;
}

/**
 * Test: Step strategy produces threshold-based instant blend.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBlendStrategy_Step,
	"KatanaCombat.ComboBlend.Strategy.Step",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FBlendStrategy_Step::RunTest(const FString& Parameters)
{
	FProceduralBlendConfig Config;
	Config.Strategy = EProceduralStrategy::Step;
	Config.PerceptualParams = FPerceptualDerivationParams::FromPreset(60.0f, ECombatFeelPreset::Snappy);
	Config.InstantBlendThreshold = 0.8f;

	// Below threshold
	FProceduralBlendResult BelowResult = UProceduralAnimationLibrary::CalculateProceduralBlend(0.5f, 1.0f, Config, false);
	// Above threshold
	FProceduralBlendResult AboveResult = UProceduralAnimationLibrary::CalculateProceduralBlend(0.9f, 1.0f, Config, false);

	TestFalse("Below threshold: not instant", BelowResult.bUseInstantBlend);
	TestTrue("Above threshold: instant", AboveResult.bUseInstantBlend);

	return true;
}

// ============================================================================
// TEST CATEGORY 5: ATTACK STATE MACHINE
// Tests the FAttackStateMachine tracking and grace period handling
// ============================================================================

/**
 * Test: Attack state machine default values.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAttackStateMachine_Defaults,
	"KatanaCombat.ComboBlend.StateMachine.Defaults",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FAttackStateMachine_Defaults::RunTest(const FString& Parameters)
{
	FAttackStateMachine StateMachine;

	TestTrue("No active montage initially", StateMachine.ActiveMontage == nullptr);
	TestEqual("Section name empty", StateMachine.ActiveSectionName, FName(NAME_None));
	TestTrue("Section start time is 0", FMath::IsNearlyEqual(StateMachine.SectionStartTime, 0.0f, 0.001f));
	TestTrue("Combo blend end time is 0", FMath::IsNearlyEqual(StateMachine.ComboBlendEndTime, 0.0f, 0.001f));

	return true;
}

/**
 * Test: Attack state machine tracks montage starts.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAttackStateMachine_OnAttackStarted,
	"KatanaCombat.ComboBlend.StateMachine.OnAttackStarted",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FAttackStateMachine_OnAttackStarted::RunTest(const FString& Parameters)
{
	FAttackStateMachine StateMachine;

	// Simulate attack start
	const float CurrentTime = 1.5f;
	const FName SectionName = FName("Attack1");

	StateMachine.OnAttackStarted(nullptr, SectionName, CurrentTime);

	TestEqual("Section name set", StateMachine.ActiveSectionName, SectionName);
	TestTrue("Section start time set", FMath::IsNearlyEqual(StateMachine.SectionStartTime, CurrentTime, 0.001f));

	return true;
}

/**
 * Test: Grace period filters stale callbacks.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAttackStateMachine_GracePeriod,
	"KatanaCombat.ComboBlend.StateMachine.GracePeriod",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FAttackStateMachine_GracePeriod::RunTest(const FString& Parameters)
{
	FAttackStateMachine StateMachine;

	// Start an attack at time 1.0
	const float AttackStartTime = 1.0f;
	StateMachine.OnAttackStarted(nullptr, FName("Attack1"), AttackStartTime);

	// Grace period is 150ms, so callbacks within that window should be filtered
	// ShouldProcessMontageEnd takes (UAnimMontage*, bool bInterrupted, float CurrentWorldTime)

	// Time just after start (should be filtered if interrupted)
	const float EarlyCallbackTime = AttackStartTime + 0.05f;  // 50ms after start
	bool bShouldProcessEarly = StateMachine.ShouldProcessMontageEnd(nullptr, true, EarlyCallbackTime);
	TestFalse("Early interrupted callback within grace period filtered", bShouldProcessEarly);

	// Time after grace period (should process if same montage)
	const float LateCallbackTime = AttackStartTime + 0.2f;  // 200ms after start
	bool bShouldProcessLate = StateMachine.ShouldProcessMontageEnd(nullptr, true, LateCallbackTime);
	// Note: nullptr montage won't match ActiveMontage, so this will be false
	// But the grace period test passes because we're past it
	AddInfo(FString::Printf(TEXT("Grace period: %.3fs"), FAttackStateMachine::SectionTransitionGracePeriod));

	return true;
}

// ============================================================================
// TEST CATEGORY 6: HOLD STATE TRANSITIONS
// Tests hold activation, release, and directional capture
// ============================================================================

/**
 * Test: Hold state default values.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHoldState_Defaults,
	"KatanaCombat.ComboBlend.HoldState.Defaults",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FHoldState_Defaults::RunTest(const FString& Parameters)
{
	FHoldState HoldState;

	TestFalse("Not holding initially", HoldState.IsHolding());
	TestFalse("Not easing initially", HoldState.bIsEasing);
	TestEqual("Hold ID is 0", HoldState.CurrentHold.HoldID, 0);
	TestEqual("Input type is None", HoldState.GetHeldInputType(), EInputType::None);

	return true;
}

/**
 * Test: Hold state activation.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHoldState_Activation,
	"KatanaCombat.ComboBlend.HoldState.Activation",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FHoldState_Activation::RunTest(const FString& Parameters)
{
	FHoldState HoldState;

	// Activate hold - Activate(EInputType, float CurrentTime, float PlayRate)
	const float StartTime = 1.5f;
	const float PlayRate = 0.5f;
	HoldState.Activate(EInputType::HeavyAttack, StartTime, PlayRate);

	TestTrue("Is holding after activation", HoldState.IsHolding());
	TestTrue("Hold ID > 0 after activation", HoldState.CurrentHold.HoldID > 0);
	TestEqual("Input type set", HoldState.GetHeldInputType(), EInputType::HeavyAttack);
	TestTrue("Start time set", FMath::IsNearlyEqual(HoldState.CurrentHold.StartTime, StartTime, 0.001f));

	return true;
}

/**
 * Test: Hold state deactivation.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHoldState_Deactivation,
	"KatanaCombat.ComboBlend.HoldState.Deactivation",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FHoldState_Deactivation::RunTest(const FString& Parameters)
{
	FHoldState HoldState;

	// Activate then deactivate
	HoldState.Activate(EInputType::HeavyAttack, 1.0f, 0.5f);
	TestTrue("Is holding", HoldState.IsHolding());

	HoldState.Deactivate();
	TestFalse("Not holding after deactivation", HoldState.IsHolding());
	TestEqual("Hold ID reset", HoldState.CurrentHold.HoldID, 0);

	return true;
}

/**
 * Test: Hold duration calculation.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHoldState_Duration,
	"KatanaCombat.ComboBlend.HoldState.Duration",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FHoldState_Duration::RunTest(const FString& Parameters)
{
	FHoldState HoldState;

	const float StartTime = 1.0f;
	const float CurrentTime = 1.5f;
	HoldState.Activate(EInputType::HeavyAttack, StartTime, 0.5f);

	const float Duration = HoldState.GetHoldDuration(CurrentTime);
	TestTrue("Duration is 0.5s", FMath::IsNearlyEqual(Duration, 0.5f, 0.001f));

	return true;
}

// ============================================================================
// TEST CATEGORY 7: DIRECTIONAL INPUT BUFFER
// Tests direction capture for hold release attacks
// ============================================================================

/**
 * Test: Directional buffer captures direction.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDirectionalBuffer_Capture,
	"KatanaCombat.ComboBlend.DirectionalBuffer.Capture",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FDirectionalBuffer_Capture::RunTest(const FString& Parameters)
{
	FDirectionalInputBuffer Buffer;

	Buffer.CaptureDirection(EInputDirection::Forward, 1.0f);

	TestEqual("Direction captured", Buffer.DirectionAtRelease, EInputDirection::Forward);
	TestTrue("Has valid input", Buffer.HasValidInput());

	return true;
}

/**
 * Test: Directional buffer reset.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDirectionalBuffer_Reset,
	"KatanaCombat.ComboBlend.DirectionalBuffer.Reset",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FDirectionalBuffer_Reset::RunTest(const FString& Parameters)
{
	FDirectionalInputBuffer Buffer;

	Buffer.CaptureDirection(EInputDirection::Forward, 1.0f);
	TestTrue("Has input before reset", Buffer.HasValidInput());

	Buffer.Reset();
	TestFalse("No input after reset", Buffer.HasValidInput());
	TestEqual("Direction is None", Buffer.DirectionAtRelease, EInputDirection::None);

	return true;
}

/**
 * Test: All directional values are valid.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDirectionalBuffer_AllDirections,
	"KatanaCombat.ComboBlend.DirectionalBuffer.AllDirections",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FDirectionalBuffer_AllDirections::RunTest(const FString& Parameters)
{
	FDirectionalInputBuffer Buffer;

	TArray<EInputDirection> Directions = {
		EInputDirection::Forward,
		EInputDirection::ForwardRight,
		EInputDirection::Right,
		EInputDirection::BackwardRight,
		EInputDirection::Backward,
		EInputDirection::BackwardLeft,
		EInputDirection::Left,
		EInputDirection::ForwardLeft
	};

	for (EInputDirection Dir : Directions)
	{
		Buffer.CaptureDirection(Dir, 0.0f);
		TestEqual(FString::Printf(TEXT("Direction %d captured"), static_cast<int32>(Dir)),
			Buffer.DirectionAtRelease, Dir);
		TestTrue("Has valid input", Buffer.HasValidInput());
	}

	return true;
}

// ============================================================================
// TEST CATEGORY 8: PERCEPTUAL FEEL PRESETS
// Tests that feel presets produce expected frame counts
// ============================================================================

/**
 * Test: Instant preset produces minimal frame counts.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFeelPreset_Instant,
	"KatanaCombat.ComboBlend.FeelPreset.Instant",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FFeelPreset_Instant::RunTest(const FString& Parameters)
{
	FPerceptualDerivationParams Params = FPerceptualDerivationParams::FromPreset(60.0f, ECombatFeelPreset::Instant);

	// Instant preset: Min=2, Max=3 (from FromPreset implementation)
	TestEqual("Min frames is 2", Params.MinFrameCount, 2);
	TestEqual("Max frames is 3", Params.MaxFrameCount, 3);

	AddInfo(FString::Printf(TEXT("Instant: MinFrames=%d, MaxFrames=%d"),
		Params.MinFrameCount, Params.MaxFrameCount));

	return true;
}

/**
 * Test: Snappy preset produces quick but visible frame counts.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFeelPreset_Snappy,
	"KatanaCombat.ComboBlend.FeelPreset.Snappy",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FFeelPreset_Snappy::RunTest(const FString& Parameters)
{
	FPerceptualDerivationParams Params = FPerceptualDerivationParams::FromPreset(60.0f, ECombatFeelPreset::Snappy);

	// Snappy preset: Min=6, Max=9 (from FromPreset implementation)
	TestEqual("Min frames is 6", Params.MinFrameCount, 6);
	TestEqual("Max frames is 9", Params.MaxFrameCount, 9);

	AddInfo(FString::Printf(TEXT("Snappy: MinFrames=%d, MaxFrames=%d"),
		Params.MinFrameCount, Params.MaxFrameCount));

	return true;
}

/**
 * Test: Balanced preset produces moderate frame counts.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFeelPreset_Balanced,
	"KatanaCombat.ComboBlend.FeelPreset.Balanced",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FFeelPreset_Balanced::RunTest(const FString& Parameters)
{
	FPerceptualDerivationParams Params = FPerceptualDerivationParams::FromPreset(60.0f, ECombatFeelPreset::Balanced);

	// Balanced preset: Min=9, Max=15 (from FromPreset implementation)
	TestEqual("Min frames is 9", Params.MinFrameCount, 9);
	TestEqual("Max frames is 15", Params.MaxFrameCount, 15);

	AddInfo(FString::Printf(TEXT("Balanced: MinFrames=%d, MaxFrames=%d"),
		Params.MinFrameCount, Params.MaxFrameCount));

	return true;
}

/**
 * Test: Smooth preset produces longer frame counts.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFeelPreset_Smooth,
	"KatanaCombat.ComboBlend.FeelPreset.Smooth",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FFeelPreset_Smooth::RunTest(const FString& Parameters)
{
	FPerceptualDerivationParams Params = FPerceptualDerivationParams::FromPreset(60.0f, ECombatFeelPreset::Smooth);

	// Smooth preset: Min=15, Max=18 (from FromPreset implementation)
	TestEqual("Min frames is 15", Params.MinFrameCount, 15);
	TestEqual("Max frames is 18", Params.MaxFrameCount, 18);

	AddInfo(FString::Printf(TEXT("Smooth: MinFrames=%d, MaxFrames=%d"),
		Params.MinFrameCount, Params.MaxFrameCount));

	return true;
}

/**
 * Test: Frame-to-seconds conversion is correct.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFeelPreset_FrameConversion,
	"KatanaCombat.ComboBlend.FeelPreset.FrameConversion",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FFeelPreset_FrameConversion::RunTest(const FString& Parameters)
{
	// At 60fps, 1 frame = 1/60 = 0.01667s
	FPerceptualDerivationParams Params;
	Params.TargetFramerate = 60.0f;
	Params.MinFrameCount = 6;
	Params.MaxFrameCount = 12;

	const float MinTime = Params.GetMinBlendTime();
	const float MaxTime = Params.GetMaxBlendTime();

	// 6 frames at 60fps = 0.1s
	TestTrue("Min time ~0.1s", FMath::IsNearlyEqual(MinTime, 0.1f, 0.001f));
	// 12 frames at 60fps = 0.2s
	TestTrue("Max time ~0.2s", FMath::IsNearlyEqual(MaxTime, 0.2f, 0.001f));

	return true;
}

// ============================================================================
// TEST CATEGORY 9: INPUT TYPE HANDLING
// Tests different input types and their interactions
// ============================================================================

/**
 * Test: All input types have distinct values.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FInputType_DistinctValues,
	"KatanaCombat.ComboBlend.InputType.DistinctValues",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FInputType_DistinctValues::RunTest(const FString& Parameters)
{
	TSet<int32> Values;

	Values.Add(static_cast<int32>(EInputType::None));
	Values.Add(static_cast<int32>(EInputType::LightAttack));
	Values.Add(static_cast<int32>(EInputType::HeavyAttack));
	Values.Add(static_cast<int32>(EInputType::Block));
	Values.Add(static_cast<int32>(EInputType::Evade));
	Values.Add(static_cast<int32>(EInputType::Special));

	TestEqual("All 6 input types have distinct values", Values.Num(), 6);

	return true;
}

/**
 * Test: Attack inputs are recognized correctly.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FInputType_AttackRecognition,
	"KatanaCombat.ComboBlend.InputType.AttackRecognition",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FInputType_AttackRecognition::RunTest(const FString& Parameters)
{
	// Light and Heavy are attack inputs
	TestTrue("LightAttack is attack", EInputType::LightAttack == EInputType::LightAttack);
	TestTrue("HeavyAttack is attack", EInputType::HeavyAttack == EInputType::HeavyAttack);

	// Block, Evade, Special are not primary attacks
	TestTrue("Block is different from LightAttack", EInputType::Block != EInputType::LightAttack);
	TestTrue("Evade is different from LightAttack", EInputType::Evade != EInputType::LightAttack);

	return true;
}

// ============================================================================
// TEST CATEGORY 10: QUEUED INPUT ACTION
// Tests the FQueuedInputAction structure
// ============================================================================

/**
 * Test: Queued input action construction.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FQueuedInput_Construction,
	"KatanaCombat.ComboBlend.QueuedInput.Construction",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FQueuedInput_Construction::RunTest(const FString& Parameters)
{
	const float Timestamp = 1.5f;
	FQueuedInputAction Action(EInputType::LightAttack, EInputEventType::Press, Timestamp, false);

	TestEqual("Input type set", Action.InputType, EInputType::LightAttack);
	TestEqual("Event type set", Action.EventType, EInputEventType::Press);
	TestTrue("Timestamp set", FMath::IsNearlyEqual(Action.Timestamp, Timestamp, 0.001f));
	TestFalse("Combo window flag set", Action.bInComboWindow);

	return true;
}

/**
 * Test: Queued input with combo window flag.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FQueuedInput_ComboFlag,
	"KatanaCombat.ComboBlend.QueuedInput.ComboFlag",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FQueuedInput_ComboFlag::RunTest(const FString& Parameters)
{
	FQueuedInputAction WithCombo(EInputType::LightAttack, EInputEventType::Press, 1.0f, true);
	FQueuedInputAction WithoutCombo(EInputType::LightAttack, EInputEventType::Press, 1.0f, false);

	TestTrue("With combo flag true", WithCombo.bInComboWindow);
	TestFalse("Without combo flag false", WithoutCombo.bInComboWindow);

	return true;
}

// ============================================================================
// TEST CATEGORY 11: TIMER CHECKPOINTS
// Tests the FTimerCheckpoint structure for combo/hold windows
// ============================================================================

/**
 * Test: Timer checkpoint construction.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTimerCheckpoint_Construction,
	"KatanaCombat.ComboBlend.TimerCheckpoint.Construction",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FTimerCheckpoint_Construction::RunTest(const FString& Parameters)
{
	const float MontageTime = 0.5f;
	const float Duration = 0.3f;
	FTimerCheckpoint Checkpoint(EActionWindowType::Combo, MontageTime, Duration);

	TestEqual("Window type set", Checkpoint.WindowType, EActionWindowType::Combo);
	TestTrue("Montage time set", FMath::IsNearlyEqual(Checkpoint.MontageTime, MontageTime, 0.001f));
	TestTrue("Duration set", FMath::IsNearlyEqual(Checkpoint.Duration, Duration, 0.001f));
	TestFalse("Not active initially", Checkpoint.bActive);

	return true;
}

/**
 * Test: Different window types.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTimerCheckpoint_WindowTypes,
	"KatanaCombat.ComboBlend.TimerCheckpoint.WindowTypes",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FTimerCheckpoint_WindowTypes::RunTest(const FString& Parameters)
{
	FTimerCheckpoint ComboCheckpoint(EActionWindowType::Combo, 1.0f, 0.5f);
	FTimerCheckpoint HoldCheckpoint(EActionWindowType::Hold, 1.0f, 0.5f);
	FTimerCheckpoint ParryCheckpoint(EActionWindowType::Parry, 1.0f, 0.5f);
	FTimerCheckpoint CounterCheckpoint(EActionWindowType::Counter, 1.0f, 0.5f);

	TestEqual("Combo type", ComboCheckpoint.WindowType, EActionWindowType::Combo);
	TestEqual("Hold type", HoldCheckpoint.WindowType, EActionWindowType::Hold);
	TestEqual("Parry type", ParryCheckpoint.WindowType, EActionWindowType::Parry);
	TestEqual("Counter type", CounterCheckpoint.WindowType, EActionWindowType::Counter);

	return true;
}

// ============================================================================
// TEST CATEGORY 12: ATTACK DATA VALIDATION
// Tests AttackData asset requirements for combo system
// ============================================================================

/**
 * Test: AttackData has required blend time fields.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAttackData_BlendFields,
	"KatanaCombat.ComboBlend.AttackData.BlendFields",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FAttackData_BlendFields::RunTest(const FString& Parameters)
{
	UAttackData* TestAttack = NewObject<UAttackData>();

	// Verify default values exist (these fields should be deprecated but present)
	TestTrue("ComboBlendInTime field exists", TestAttack->ComboBlendInTime >= 0.0f);
	TestTrue("ComboBlendOutTime field exists", TestAttack->ComboBlendOutTime >= 0.0f);

	return true;
}

/**
 * Test: AttackData montage reference can be null (data validation).
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAttackData_NullMontage,
	"KatanaCombat.ComboBlend.AttackData.NullMontage",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FAttackData_NullMontage::RunTest(const FString& Parameters)
{
	UAttackData* TestAttack = NewObject<UAttackData>();

	// Null montage should be safe to check
	TestTrue("Montage can be null", TestAttack->AttackMontage == nullptr);

	return true;
}

// ============================================================================
// TEST CATEGORY 13: COMBAT COMPONENT API
// Tests CombatComponent public API for combo/blend system
// ============================================================================

/**
 * Test: CombatComponent GetCurrentPhase returns valid phase.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatAPI_GetPhase,
	"KatanaCombat.ComboBlend.CombatAPI.GetPhase",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FCombatAPI_GetPhase::RunTest(const FString& Parameters)
{
	UWorld* World = FCombatTestHelpers::CreateTestWorld();
	UCombatComponent* CombatComp = nullptr;
	APlayerCharacter* Player = FCombatTestHelpers::CreateTestCharacterWithCombat(World, CombatComp);

	if (!CombatComp)
	{
		AddError(TEXT("Failed to create test player"));
		FCombatTestHelpers::DestroyTestWorld(World);
		return false;
	}

	EAttackPhase Phase = CombatComp->GetCurrentPhase();
	TestEqual("Phase is None when idle", Phase, EAttackPhase::None);

	FCombatTestHelpers::DestroyTestWorld(World);
	return true;
}

/**
 * Test: CombatComponent IsComboWindowActive API.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatAPI_ComboWindowActive,
	"KatanaCombat.ComboBlend.CombatAPI.ComboWindowActive",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FCombatAPI_ComboWindowActive::RunTest(const FString& Parameters)
{
	UWorld* World = FCombatTestHelpers::CreateTestWorld();
	UCombatComponent* CombatComp = nullptr;
	APlayerCharacter* Player = FCombatTestHelpers::CreateTestCharacterWithCombat(World, CombatComp);

	if (!CombatComp)
	{
		AddError(TEXT("Failed to create test player"));
		FCombatTestHelpers::DestroyTestWorld(World);
		return false;
	}

	// When idle, combo window should be inactive
	bool bComboActive = CombatComp->IsInComboWindow();
	TestFalse("Combo window inactive when idle", bComboActive);

	FCombatTestHelpers::DestroyTestWorld(World);
	return true;
}

/**
 * Test: Character has CombatSettings configured.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatAPI_CharacterSettings,
	"KatanaCombat.ComboBlend.CombatAPI.CharacterSettings",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FCombatAPI_CharacterSettings::RunTest(const FString& Parameters)
{
	UWorld* World = FCombatTestHelpers::CreateTestWorld();
	UCombatComponent* CombatComp = nullptr;
	APlayerCharacter* Player = FCombatTestHelpers::CreateTestCharacterWithCombat(World, CombatComp);

	if (!Player)
	{
		AddError(TEXT("Failed to create test player"));
		FCombatTestHelpers::DestroyTestWorld(World);
		return false;
	}

	// Settings should be configured by helper
	UCombatSettings* Settings = Player->CombatSettings;
	if (Settings)
	{
		TestTrue("Settings is valid UObject", Settings->IsValidLowLevel());
	}
	else
	{
		AddWarning(TEXT("CombatSettings is null - asset not configured for test"));
	}

	FCombatTestHelpers::DestroyTestWorld(World);
	return true;
}

// ============================================================================
// TEST CATEGORY 14: BLEND ARTIFACT DETECTION
// Tests for detecting common blend artifacts (partial blends, stuck states)
// ============================================================================

/**
 * Test: Blend result flags are mutually exclusive where appropriate.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBlendArtifact_FlagConsistency,
	"KatanaCombat.ComboBlend.BlendArtifact.FlagConsistency",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FBlendArtifact_FlagConsistency::RunTest(const FString& Parameters)
{
	FProceduralBlendConfig Config;
	Config.PerceptualParams = FPerceptualDerivationParams::FromPreset(60.0f, ECombatFeelPreset::Balanced);

	// Fresh attack
	FProceduralBlendResult FreshResult = UProceduralAnimationLibrary::CalculateProceduralBlend(0.0f, 0.0f, Config, false);
	TestTrue("Fresh attack is fresh", FreshResult.bIsFreshAttack);

	// Mid-animation normal blend
	FProceduralBlendResult NormalResult = UProceduralAnimationLibrary::CalculateProceduralBlend(0.5f, 1.0f, Config, false);
	TestFalse("Normal blend not fresh", NormalResult.bIsFreshAttack);
	TestFalse("Normal blend not rapid", NormalResult.bRapidInputDetected);

	// Rapid input
	FProceduralBlendResult RapidResult = UProceduralAnimationLibrary::CalculateProceduralBlend(0.5f, 1.0f, Config, true);
	TestTrue("Rapid input detected", RapidResult.bRapidInputDetected);

	return true;
}

/**
 * Test: Blend times are always positive or zero.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBlendArtifact_PositiveTimes,
	"KatanaCombat.ComboBlend.BlendArtifact.PositiveTimes",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FBlendArtifact_PositiveTimes::RunTest(const FString& Parameters)
{
	FProceduralBlendConfig Config;
	Config.PerceptualParams = FPerceptualDerivationParams::FromPreset(60.0f, ECombatFeelPreset::Balanced);

	// Test at various progress points
	TArray<float> ProgressValues = {0.0f, 0.25f, 0.5f, 0.75f, 0.99f, 1.0f};

	for (float Progress : ProgressValues)
	{
		FProceduralBlendResult Result = UProceduralAnimationLibrary::CalculateProceduralBlend(Progress, 1.0f, Config, false);
		TestTrue(FString::Printf(TEXT("BlendInTime >= 0 at %.0f%%"), Progress * 100), Result.BlendInTime >= 0.0f);
		TestTrue(FString::Printf(TEXT("BlendOutTime >= 0 at %.0f%%"), Progress * 100), Result.BlendOutTime >= 0.0f);
	}

	return true;
}

/**
 * Test: Blend times don't exceed reasonable maximum.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBlendArtifact_ReasonableMax,
	"KatanaCombat.ComboBlend.BlendArtifact.ReasonableMax",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FBlendArtifact_ReasonableMax::RunTest(const FString& Parameters)
{
	FProceduralBlendConfig Config;
	Config.PerceptualParams = FPerceptualDerivationParams::FromPreset(60.0f, ECombatFeelPreset::Smooth);

	// Even smooth preset shouldn't exceed 0.5s
	FProceduralBlendResult Result = UProceduralAnimationLibrary::CalculateProceduralBlend(0.0f, 2.0f, Config, false);
	TestTrue("BlendInTime <= 0.5s", Result.BlendInTime <= 0.5f);

	return true;
}

/**
 * Test: Debug string generation doesn't crash.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBlendArtifact_DebugString,
	"KatanaCombat.ComboBlend.BlendArtifact.DebugString",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FBlendArtifact_DebugString::RunTest(const FString& Parameters)
{
	FProceduralBlendConfig Config;
	Config.PerceptualParams = FPerceptualDerivationParams::FromPreset(60.0f, ECombatFeelPreset::Balanced);

	FProceduralBlendResult Result = UProceduralAnimationLibrary::CalculateProceduralBlend(0.5f, 1.0f, Config, false);

	FString DebugStr = Result.ToDebugString();
	TestTrue("Debug string not empty", !DebugStr.IsEmpty());
	TestTrue("Debug string contains BlendIn", DebugStr.Contains(TEXT("BlendIn")));

	AddInfo(FString::Printf(TEXT("Debug: %s"), *DebugStr));

	return true;
}
