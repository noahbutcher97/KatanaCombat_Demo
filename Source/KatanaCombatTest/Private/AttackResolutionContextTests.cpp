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

	AddExpectedErrorPlain(TEXT("Default Light attack is nullptr"), EAutomationExpectedErrorFlags::Contains, 1);
	AddExpectedErrorPlain(TEXT("Emergency fallback attack"), EAutomationExpectedErrorFlags::Contains, 1);

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
