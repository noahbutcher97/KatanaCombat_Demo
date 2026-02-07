// Copyright Epic Games, Inc. All Rights Reserved.

#include "AI/CombatTokenSubsystem.h"
#include "Engine/World.h"

void UCombatTokenSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	// Clean state on initialization
	ActiveAttackers.Empty();
	TokenQueue.Empty();
	LastTokenTime.Empty();
}

void UCombatTokenSubsystem::Deinitialize()
{
	ResetAllTokens();
	Super::Deinitialize();
}

bool UCombatTokenSubsystem::RequestAttackToken(AActor* Requester)
{
	if (!Requester)
	{
		return false;
	}

	// Already has a token
	if (HasAttackToken(Requester))
	{
		return true;
	}

	// Already in queue
	if (IsInTokenQueue(Requester))
	{
		return false;
	}

	// Check cooldown
	if (IsOnCooldown(Requester))
	{
		return false;
	}

	// Cleanup any destroyed actors first
	CleanupInvalidActors();

	// Can we grant immediately?
	if (ActiveAttackers.Num() < MaxConcurrentAttackers)
	{
		// Grant token
		ActiveAttackers.Add(Requester);
		LastTokenTime.Add(Requester, GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f);

		// Broadcast
		OnTokenGranted.Broadcast(Requester);

		UE_LOG(LogTemp, Log, TEXT("[TOKEN] Granted to %s (Active: %d/%d)"),
			*Requester->GetName(), ActiveAttackers.Num(), MaxConcurrentAttackers);

		return true;
	}

	// Add to queue
	TokenQueue.Add(Requester);

	UE_LOG(LogTemp, Log, TEXT("[TOKEN] %s added to queue (Position: %d)"),
		*Requester->GetName(), TokenQueue.Num());

	return false;
}

void UCombatTokenSubsystem::ReleaseAttackToken(AActor* Holder)
{
	if (!Holder)
	{
		return;
	}

	// Find and remove from active attackers
	int32 RemovedCount = ActiveAttackers.RemoveAll([Holder](const TWeakObjectPtr<AActor>& Actor)
	{
		return Actor.Get() == Holder;
	});

	if (RemovedCount > 0)
	{
		UE_LOG(LogTemp, Log, TEXT("[TOKEN] Released by %s (Active: %d/%d)"),
			*Holder->GetName(), ActiveAttackers.Num(), MaxConcurrentAttackers);

		// Broadcast release
		OnTokenReleased.Broadcast(Holder);

		// Try to grant to next in queue
		TryGrantQueuedToken();
	}
}

bool UCombatTokenSubsystem::HasAttackToken(AActor* Actor) const
{
	if (!Actor)
	{
		return false;
	}

	return ActiveAttackers.ContainsByPredicate([Actor](const TWeakObjectPtr<AActor>& Attacker)
	{
		return Attacker.Get() == Actor;
	});
}

bool UCombatTokenSubsystem::IsInTokenQueue(AActor* Actor) const
{
	if (!Actor)
	{
		return false;
	}

	return TokenQueue.ContainsByPredicate([Actor](const TWeakObjectPtr<AActor>& Queued)
	{
		return Queued.Get() == Actor;
	});
}

void UCombatTokenSubsystem::RemoveFromQueue(AActor* Actor)
{
	if (!Actor)
	{
		return;
	}

	int32 RemovedCount = TokenQueue.RemoveAll([Actor](const TWeakObjectPtr<AActor>& Queued)
	{
		return Queued.Get() == Actor;
	});

	if (RemovedCount > 0)
	{
		UE_LOG(LogTemp, Log, TEXT("[TOKEN] %s removed from queue"), *Actor->GetName());
	}
}

void UCombatTokenSubsystem::ResetAllTokens()
{
	// Clear active attackers
	for (const TWeakObjectPtr<AActor>& Attacker : ActiveAttackers)
	{
		if (AActor* Actor = Attacker.Get())
		{
			OnTokenReleased.Broadcast(Actor);
		}
	}

	ActiveAttackers.Empty();
	TokenQueue.Empty();
	LastTokenTime.Empty();

	UE_LOG(LogTemp, Log, TEXT("[TOKEN] All tokens reset"));
}

TArray<AActor*> UCombatTokenSubsystem::GetActiveAttackers() const
{
	TArray<AActor*> Result;
	Result.Reserve(ActiveAttackers.Num());

	for (const TWeakObjectPtr<AActor>& Attacker : ActiveAttackers)
	{
		if (AActor* Actor = Attacker.Get())
		{
			Result.Add(Actor);
		}
	}

	return Result;
}

void UCombatTokenSubsystem::TryGrantQueuedToken()
{
	// Cleanup invalid actors first
	CleanupInvalidActors();

	// Check if we have capacity and queue isn't empty
	while (ActiveAttackers.Num() < MaxConcurrentAttackers && TokenQueue.Num() > 0)
	{
		// Get next in queue (FIFO)
		TWeakObjectPtr<AActor> NextInQueue = TokenQueue[0];
		TokenQueue.RemoveAt(0);

		AActor* NextActor = NextInQueue.Get();
		if (!NextActor)
		{
			// Actor was destroyed while in queue, try next
			continue;
		}

		// Check cooldown
		if (IsOnCooldown(NextActor))
		{
			// Put back at end of queue
			TokenQueue.Add(NextInQueue);
			continue;
		}

		// Grant token
		ActiveAttackers.Add(NextInQueue);
		LastTokenTime.Add(NextInQueue, GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f);

		UE_LOG(LogTemp, Log, TEXT("[TOKEN] Granted to %s from queue (Active: %d/%d)"),
			*NextActor->GetName(), ActiveAttackers.Num(), MaxConcurrentAttackers);

		// Broadcast
		OnTokenGranted.Broadcast(NextActor);
	}
}

bool UCombatTokenSubsystem::IsOnCooldown(AActor* Actor) const
{
	if (!Actor || TokenCooldownPerEnemy <= 0.0f)
	{
		return false;
	}

	const float* LastTime = LastTokenTime.Find(Actor);
	if (!LastTime)
	{
		return false;
	}

	const float CurrentTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
	return (CurrentTime - *LastTime) < TokenCooldownPerEnemy;
}

void UCombatTokenSubsystem::CleanupInvalidActors()
{
	// Remove destroyed actors from active list
	ActiveAttackers.RemoveAll([](const TWeakObjectPtr<AActor>& Actor)
	{
		return !Actor.IsValid();
	});

	// Remove destroyed actors from queue
	TokenQueue.RemoveAll([](const TWeakObjectPtr<AActor>& Actor)
	{
		return !Actor.IsValid();
	});

	// Clean up cooldown map (remove entries for destroyed actors)
	TArray<TWeakObjectPtr<AActor>> KeysToRemove;
	for (const auto& Pair : LastTokenTime)
	{
		if (!Pair.Key.IsValid())
		{
			KeysToRemove.Add(Pair.Key);
		}
	}
	for (const auto& Key : KeysToRemove)
	{
		LastTokenTime.Remove(Key);
	}
}
