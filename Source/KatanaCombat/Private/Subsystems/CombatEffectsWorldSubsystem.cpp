// Copyright Epic Games, Inc. All Rights Reserved.

#include "Subsystems/CombatEffectsWorldSubsystem.h"

#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameFramework/WorldSettings.h"
#include "KatanaCombat.h"

namespace
{
constexpr float MinimumTimeScale = 0.0001f;

bool IsValidLeaseRequest(const FName Owner, const float Scale, const double WatchdogSeconds)
{
	return !Owner.IsNone()
		&& FMath::IsFinite(Scale)
		&& Scale >= MinimumTimeScale
		&& FMath::IsFinite(WatchdogSeconds)
		&& WatchdogSeconds > 0.0
		&& WatchdogSeconds <= static_cast<double>(TNumericLimits<float>::Max());
}
}

void UCombatEffectsWorldSubsystem::Deinitialize()
{
	for (TPair<FTimeDilationLeaseHandle, FLeaseRecord>& Pair : ActiveLeases)
	{
		if (Pair.Value.WatchdogHandle.IsValid())
		{
			FTSTicker::RemoveTicker(Pair.Value.WatchdogHandle);
			Pair.Value.WatchdogHandle.Reset();
		}
	}

	if (bWorldBaselineCaptured)
	{
		if (UWorld* World = GetWorld())
		{
			if (AWorldSettings* Settings = World->GetWorldSettings())
			{
				Settings->SetTimeDilation(WorldBaseline);
			}
		}
	}

	for (TPair<TWeakObjectPtr<AActor>, FActorLeaseState>& Pair : ActorLeaseStates)
	{
		if (AActor* Actor = Pair.Key.Get())
		{
			Actor->CustomTimeDilation = Pair.Value.Baseline;
		}
	}

	ActiveLeases.Reset();
	WorldLeaseHandles.Reset();
	ActorLeaseStates.Reset();
	bWorldBaselineCaptured = false;
	WorldBaseline = 1.0f;
	ExpectedWorldDilation = 1.0f;

	Super::Deinitialize();
}

FTimeDilationLeaseHandle UCombatEffectsWorldSubsystem::AllocateHandle()
{
	do
	{
		++NextLeaseId;
	}
	while (NextLeaseId == 0
		|| ActiveLeases.Contains(FTimeDilationLeaseHandle(NextLeaseId)));
	return FTimeDilationLeaseHandle(NextLeaseId);
}

FTimeDilationLeaseHandle UCombatEffectsWorldSubsystem::AcquireWorldLease(
	const FName Owner,
	const float AbsoluteScale,
	const double WatchdogSeconds)
{
	UWorld* World = GetWorld();
	AWorldSettings* Settings = World ? World->GetWorldSettings() : nullptr;
	if (!Settings || !IsValidLeaseRequest(Owner, AbsoluteScale, WatchdogSeconds))
	{
		return {};
	}

	if (!bWorldBaselineCaptured)
	{
		bWorldBaselineCaptured = true;
		WorldBaseline = Settings->TimeDilation;
		ExpectedWorldDilation = WorldBaseline;
	}

	const FTimeDilationLeaseHandle Handle = AllocateHandle();
	FLeaseRecord& Record = ActiveLeases.Add(Handle);
	Record.Target = ELeaseTarget::World;
	Record.Owner = Owner;
	Record.AbsoluteScale = AbsoluteScale;
	WorldLeaseHandles.Add(Handle);
	ArmWatchdog(Handle, WatchdogSeconds);
	RecomputeWorldDilation();
	return Handle;
}

FTimeDilationLeaseHandle UCombatEffectsWorldSubsystem::AcquireActorLease(
	AActor* Actor,
	const FName Owner,
	const float AbsoluteScale,
	const double WatchdogSeconds)
{
	if (!Actor
		|| Actor->GetWorld() != GetWorld()
		|| !IsValidLeaseRequest(Owner, AbsoluteScale, WatchdogSeconds))
	{
		return {};
	}

	const TWeakObjectPtr<AActor> ActorKey(Actor);
	FActorLeaseState& State = ActorLeaseStates.FindOrAdd(ActorKey);
	if (State.Handles.IsEmpty())
	{
		State.Baseline = Actor->CustomTimeDilation;
		State.ExpectedApplied = State.Baseline;
	}

	const FTimeDilationLeaseHandle Handle = AllocateHandle();
	FLeaseRecord& Record = ActiveLeases.Add(Handle);
	Record.Target = ELeaseTarget::Actor;
	Record.Actor = Actor;
	Record.Owner = Owner;
	Record.AbsoluteScale = AbsoluteScale;
	State.Handles.Add(Handle);
	ArmWatchdog(Handle, WatchdogSeconds);
	RecomputeActorDilation(ActorKey);
	return Handle;
}

void UCombatEffectsWorldSubsystem::ArmWatchdog(
	const FTimeDilationLeaseHandle Handle,
	const double WatchdogSeconds)
{
	FLeaseRecord* Record = ActiveLeases.Find(Handle);
	if (!Record)
	{
		return;
	}

	const TWeakObjectPtr<UCombatEffectsWorldSubsystem> WeakThis(this);
	Record->WatchdogHandle = FTSTicker::GetCoreTicker().AddTicker(
		FTickerDelegate::CreateLambda(
			[WeakThis, Handle](const float DeltaTime)
			{
				UCombatEffectsWorldSubsystem* Subsystem = WeakThis.Get();
				return Subsystem ? Subsystem->HandleWatchdog(Handle, DeltaTime) : false;
			}),
		static_cast<float>(WatchdogSeconds));
}

bool UCombatEffectsWorldSubsystem::HandleWatchdog(
	const FTimeDilationLeaseHandle Handle,
	const float DeltaTime)
{
	ReleaseLeaseInternal(Handle, true);
	return false;
}

bool UCombatEffectsWorldSubsystem::ReleaseLease(const FTimeDilationLeaseHandle Handle)
{
	return ReleaseLeaseInternal(Handle, false);
}

bool UCombatEffectsWorldSubsystem::ReleaseLeaseInternal(
	const FTimeDilationLeaseHandle Handle,
	const bool bFromWatchdog)
{
	FLeaseRecord Record;
	if (!Handle.IsValid() || !ActiveLeases.RemoveAndCopyValue(Handle, Record))
	{
		return false;
	}

	if (!bFromWatchdog && Record.WatchdogHandle.IsValid())
	{
		FTSTicker::RemoveTicker(Record.WatchdogHandle);
	}

	if (Record.Target == ELeaseTarget::World)
	{
		WorldLeaseHandles.Remove(Handle);
		RecomputeWorldDilation();
	}
	else
	{
		const TWeakObjectPtr<AActor> ActorKey = Record.Actor;
		if (FActorLeaseState* State = ActorLeaseStates.Find(ActorKey))
		{
			State->Handles.Remove(Handle);
		}
		RecomputeActorDilation(ActorKey);
	}
	return true;
}

void UCombatEffectsWorldSubsystem::RecomputeWorldDilation()
{
	UWorld* World = GetWorld();
	AWorldSettings* Settings = World ? World->GetWorldSettings() : nullptr;
	if (!Settings || !bWorldBaselineCaptured)
	{
		return;
	}

	if (!FMath::IsNearlyEqual(Settings->TimeDilation, ExpectedWorldDilation))
	{
		UE_LOG(LogKatanaCombat, Warning,
			TEXT("[TIME LEASE] External world dilation mutation detected: expected %.4f, observed %.4f"),
			ExpectedWorldDilation,
			Settings->TimeDilation);
	}

	if (WorldLeaseHandles.IsEmpty())
	{
		Settings->SetTimeDilation(WorldBaseline);
		ExpectedWorldDilation = WorldBaseline;
		bWorldBaselineCaptured = false;
		WorldBaseline = 1.0f;
		return;
	}

	float Effective = WorldBaseline;
	for (const FTimeDilationLeaseHandle Handle : WorldLeaseHandles)
	{
		if (const FLeaseRecord* Record = ActiveLeases.Find(Handle))
		{
			Effective = FMath::Min(Effective, Record->AbsoluteScale);
		}
	}
	Settings->SetTimeDilation(Effective);
	ExpectedWorldDilation = Effective;
}

void UCombatEffectsWorldSubsystem::RecomputeActorDilation(
	const TWeakObjectPtr<AActor>& ActorKey)
{
	FActorLeaseState* State = ActorLeaseStates.Find(ActorKey);
	if (!State)
	{
		return;
	}

	AActor* Actor = ActorKey.Get();
	if (!Actor)
	{
		ActorLeaseStates.Remove(ActorKey);
		return;
	}

	if (!FMath::IsNearlyEqual(Actor->CustomTimeDilation, State->ExpectedApplied))
	{
		UE_LOG(LogKatanaCombat, Warning,
			TEXT("[TIME LEASE] External actor dilation mutation detected on %s: expected %.4f, observed %.4f"),
			*Actor->GetName(),
			State->ExpectedApplied,
			Actor->CustomTimeDilation);
	}

	if (State->Handles.IsEmpty())
	{
		Actor->CustomTimeDilation = State->Baseline;
		ActorLeaseStates.Remove(ActorKey);
		return;
	}

	float Effective = State->Baseline;
	for (const FTimeDilationLeaseHandle Handle : State->Handles)
	{
		if (const FLeaseRecord* Record = ActiveLeases.Find(Handle))
		{
			Effective = FMath::Min(Effective, Record->AbsoluteScale);
		}
	}
	Actor->CustomTimeDilation = Effective;
	State->ExpectedApplied = Effective;
}
