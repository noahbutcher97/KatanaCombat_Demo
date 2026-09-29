// Copyright Epic Games, Inc. All Rights Reserved.

#include "CombatTestHelpers.h"
#include "Utilities/CombatTargetQuery.h"
#include "Characters/PlayerCharacter.h"
#include "Characters/EnemyCharacter.h"
#include "Components/CapsuleComponent.h"
#include "Interfaces/DamageableInterface.h"

namespace CombatTargetQueryTestUtils
{
	/** Kill through the public damage path so IsAlive reflects a real death. */
	void Kill(APlayerCharacter* Killer, AEnemyCharacter* Victim)
	{
		const float Lethal = IDamageableInterface::Execute_GetCurrentHealth(Victim) + 1.0f;
		IDamageableInterface::Execute_ApplyDamage(Victim, FCombatTestHelpers::CreateTestHitInfo(Killer, Lethal));
		FCombatTestHelpers::FinalizeDeathIfDying(Victim);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatTargetQueryFiltersTest, "KatanaCombat.CombatTargetQuery.Filters", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FCombatTargetQueryFiltersTest::RunTest(const FString& Parameters)
{
	UWorld* World = FCombatTestHelpers::CreateTestWorld();
	APlayerCharacter* Player = FCombatTestHelpers::CreateTestPlayerCharacter(World, FVector::ZeroVector);
	AEnemyCharacter* Far = FCombatTestHelpers::CreateTestEnemyCharacter(World, FVector(400, 0, 0));
	AEnemyCharacter* Near = FCombatTestHelpers::CreateTestEnemyCharacter(World, FVector(0, 200, 0));
	AEnemyCharacter* Dead = FCombatTestHelpers::CreateTestEnemyCharacter(World, FVector(0, -250, 0));
	AEnemyCharacter* Ally = FCombatTestHelpers::CreateTestEnemyCharacter(World, FVector(-250, 0, 0));
	if (!TestNotNull(TEXT("Player"), Player) || !Far || !Near || !Dead || !Ally)
	{
		FCombatTestHelpers::DestroyTestWorld(World);
		return false;
	}
	Ally->TeamId = ETeamId::Ally;
	CombatTargetQueryTestUtils::Kill(Player, Dead);
	TestFalse(TEXT("Fixture: dead enemy reports not alive"), IDamageableInterface::Execute_IsAlive(Dead));

	// A second pawn-channel collider makes the overlap return the actor twice.
	UCapsuleComponent* ExtraCollider = NewObject<UCapsuleComponent>(Near);
	ExtraCollider->SetupAttachment(Near->GetRootComponent());
	ExtraCollider->SetCollisionProfileName(UCollisionProfile::Pawn_ProfileName);
	ExtraCollider->InitCapsuleSize(30.0f, 60.0f);
	ExtraCollider->RegisterComponent();

	FCombatTargetQuery Query;
	Query.Radius = 1000.0f;
	TArray<AActor*> Targets;
	CombatTargetQuery::GatherTargets(Player, Query, Targets);

	TestEqual(TEXT("Only the two living hostile enemies"), Targets.Num(), 2);
	TestTrue(TEXT("Nearest first"), Targets.Num() == 2 && Targets[0] == Near && Targets[1] == Far);
	TestFalse(TEXT("Dead enemy excluded"), Targets.Contains(Dead));
	TestFalse(TEXT("Ally excluded"), Targets.Contains(Ally));
	TestFalse(TEXT("Querier excluded"), Targets.Contains(Player));

	// Death also disables the capsule, so the overlap alone would skip the dead enemy.
	// Check the eligibility rules directly so the alive and hostility filters are proven.
	FCombatTargetQuery Inclusive = Query;
	Inclusive.bRequireAlive = false;
	Inclusive.bRequireHostile = false;
	TestFalse(TEXT("Alive rule rejects the dead enemy"), CombatTargetQuery::IsEligible(Player, Dead, Query));
	TestTrue(TEXT("Without the alive rule the dead enemy is eligible"), CombatTargetQuery::IsEligible(Player, Dead, Inclusive));
	TestFalse(TEXT("Hostility rule rejects the ally"), CombatTargetQuery::IsEligible(Player, Ally, Query));

	TArray<AActor*> Everyone;
	CombatTargetQuery::GatherTargets(Player, Inclusive, Everyone);
	TestTrue(TEXT("Relaxed query includes the ally"), Everyone.Contains(Ally));
	TestEqual(TEXT("Relaxed query is still deduplicated"), Everyone.Num(), TSet<AActor*>(Everyone).Num());

	FCombatTargetQuery Invalid = Query;
	Invalid.Radius = -1.0f;
	TArray<AActor*> None;
	CombatTargetQuery::GatherTargets(Player, Invalid, None);
	TestEqual(TEXT("Negative radius finds nothing"), None.Num(), 0);

	FCombatTestHelpers::DestroyTestWorld(World);
	return true;
}
