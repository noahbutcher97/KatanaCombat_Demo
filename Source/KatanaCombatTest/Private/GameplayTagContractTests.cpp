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
