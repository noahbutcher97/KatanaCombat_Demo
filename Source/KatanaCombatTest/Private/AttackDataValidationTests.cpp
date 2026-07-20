// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_EDITOR

#include "Misc/AutomationTest.h"
#include "Data/AttackData.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAttackDataCycleValidationDeduplicatesBranchesTest,
	"KatanaCombat.Data.AttackData.Validation.CycleDeduplicatesBranches",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FAttackDataCycleValidationDeduplicatesBranchesTest::RunTest(const FString& Parameters)
{
	UAttackData* Root = NewObject<UAttackData>(GetTransientPackage(), TEXT("CycleRoot"));
	UAttackData* Child = NewObject<UAttackData>(GetTransientPackage(), TEXT("CycleChild"));

	Root->NextComboAttack = Child;
	Root->HeavyComboAttack = Child;
	Child->NextComboAttack = Root;

	TSet<const UAttackData*> Visited;
	TArray<FText> Errors;
	const bool bFoundCycle = Root->DetectCycles(Visited, Errors);

	TestTrue(TEXT("Cycle should be detected"), bFoundCycle);
	TestEqual(TEXT("One root validation should report the cycle once"), Errors.Num(), 1);
	TestTrue(TEXT("Cycle error should identify the validated root"),
		Errors.Num() == 1 && Errors[0].ToString().StartsWith(TEXT("CycleRoot:")));
	TestEqual(TEXT("Traversal path should be empty after validation"), Visited.Num(), 0);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAttackDataCycleValidationAllowsAcyclicFanInTest,
	"KatanaCombat.Data.AttackData.Validation.AllowsAcyclicFanIn",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FAttackDataCycleValidationAllowsAcyclicFanInTest::RunTest(const FString& Parameters)
{
	UAttackData* Root = NewObject<UAttackData>(GetTransientPackage(), TEXT("FanInRoot"));
	UAttackData* SharedChild = NewObject<UAttackData>(GetTransientPackage(), TEXT("SharedChild"));

	Root->NextComboAttack = SharedChild;
	Root->HeavyComboAttack = SharedChild;

	TSet<const UAttackData*> Visited;
	TArray<FText> Errors;
	const bool bFoundCycle = Root->DetectCycles(Visited, Errors);

	TestFalse(TEXT("Shared acyclic descendants should not be treated as a cycle"), bFoundCycle);
	TestEqual(TEXT("Acyclic traversal should not report errors"), Errors.Num(), 0);
	TestEqual(TEXT("Traversal path should be empty after validation"), Visited.Num(), 0);

	return true;
}

#endif
