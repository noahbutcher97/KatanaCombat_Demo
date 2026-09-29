#include "Utilities/CombatTargetQuery.h"

#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Interfaces/DamageableInterface.h"
#include "Interfaces/TeamMemberInterface.h"

namespace CombatTargetQuery
{
bool IsEligible(const AActor* Querier, AActor* Candidate, const FCombatTargetQuery& Query)
{
	if (!Querier || !Candidate || Candidate == Querier)
	{
		return false;
	}

	const bool bDamageable = Candidate->Implements<UDamageableInterface>();
	if ((Query.bRequireDamageable || Query.bRequireAlive) && !bDamageable)
	{
		return false;
	}

	if (Query.bRequireAlive && !IDamageableInterface::Execute_IsAlive(Candidate))
	{
		return false;
	}

	if (Query.bRequireHostile && Querier->Implements<UTeamMemberInterface>()
		&& !ITeamMemberInterface::Execute_IsHostileTo(Querier, Candidate))
	{
		return false;
	}

	return true;
}

void GatherTargets(const AActor* Querier, const FCombatTargetQuery& Query, TArray<AActor*>& OutTargets)
{
	UWorld* World = Querier ? Querier->GetWorld() : nullptr;
	if (!World || !FMath::IsFinite(Query.Radius) || Query.Radius < 0.0f)
	{
		return;
	}

	const FVector Origin = Querier->GetActorLocation();
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(CombatTargetQuery), false, Querier);

	TArray<FOverlapResult> Overlaps;
	World->OverlapMultiByChannel(
		Overlaps, Origin, FQuat::Identity, Query.Channel, FCollisionShape::MakeSphere(Query.Radius), QueryParams);

	// One overlap result is returned per colliding component; collapse to actors.
	TArray<AActor*> Found;
	for (const FOverlapResult& Overlap : Overlaps)
	{
		AActor* Actor = Overlap.GetActor();
		if (Actor && !Found.Contains(Actor) && IsEligible(Querier, Actor, Query))
		{
			Found.Add(Actor);
		}
	}

	Found.Sort([&Origin](const AActor& A, const AActor& B)
	{
		return FVector::DistSquared(Origin, A.GetActorLocation()) < FVector::DistSquared(Origin, B.GetActorLocation());
	});
	OutTargets.Append(Found);
}
}
