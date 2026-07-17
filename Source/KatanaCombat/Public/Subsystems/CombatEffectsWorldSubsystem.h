// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "Subsystems/WorldSubsystem.h"
#include "CombatTypes.h"
#include "CombatEffectsWorldSubsystem.generated.h"

/**
 * Owns overlap-safe world and actor time-dilation requests.
 * It has no combat outcome authority and only ticks through bounded watchdog callbacks.
 */
UCLASS()
class KATANACOMBAT_API UCombatEffectsWorldSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Deinitialize() override;

	FTimeDilationLeaseHandle AcquireWorldLease(
		FName Owner,
		float AbsoluteScale,
		double WatchdogSeconds);

	FTimeDilationLeaseHandle AcquireActorLease(
		AActor* Actor,
		FName Owner,
		float AbsoluteScale,
		double WatchdogSeconds);

	/** Returns true only when this call released a live lease. */
	bool ReleaseLease(FTimeDilationLeaseHandle Handle);

	bool IsLeaseActive(FTimeDilationLeaseHandle Handle) const
	{
		return Handle.IsValid() && ActiveLeases.Contains(Handle);
	}

	int32 GetActiveLeaseCount() const { return ActiveLeases.Num(); }

private:
	enum class ELeaseTarget : uint8
	{
		World,
		Actor
	};

	struct FLeaseRecord
	{
		ELeaseTarget Target = ELeaseTarget::World;
		TWeakObjectPtr<AActor> Actor;
		FName Owner = NAME_None;
		float AbsoluteScale = 1.0f;
		FTSTicker::FDelegateHandle WatchdogHandle;
	};

	struct FActorLeaseState
	{
		float Baseline = 1.0f;
		float ExpectedApplied = 1.0f;
		TSet<FTimeDilationLeaseHandle> Handles;
	};

	FTimeDilationLeaseHandle AllocateHandle();
	void ArmWatchdog(FTimeDilationLeaseHandle Handle, double WatchdogSeconds);
	bool HandleWatchdog(FTimeDilationLeaseHandle Handle, float DeltaTime);
	bool ReleaseLeaseInternal(FTimeDilationLeaseHandle Handle, bool bFromWatchdog);
	void RecomputeWorldDilation();
	void RecomputeActorDilation(const TWeakObjectPtr<AActor>& ActorKey);

	TMap<FTimeDilationLeaseHandle, FLeaseRecord> ActiveLeases;
	TSet<FTimeDilationLeaseHandle> WorldLeaseHandles;
	TMap<TWeakObjectPtr<AActor>, FActorLeaseState> ActorLeaseStates;
	uint64 NextLeaseId = 0;
	bool bWorldBaselineCaptured = false;
	float WorldBaseline = 1.0f;
	float ExpectedWorldDilation = 1.0f;
};
