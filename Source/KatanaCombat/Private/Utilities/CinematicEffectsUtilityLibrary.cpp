// Copyright Epic Games, Inc. All Rights Reserved.

#include "Utilities/CinematicEffectsUtilityLibrary.h"
#include "GameFramework/WorldSettings.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "Camera/PlayerCameraManager.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Containers/Ticker.h"
#include "HAL/PlatformTime.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraComponent.h"
#include "KatanaCombat.h"
#include "Data/CombatFXData.h"
#include "Subsystems/CombatEffectsWorldSubsystem.h"

#if WITH_AUTOMATION_TESTS
UCinematicEffectsUtilityLibrary::FOnImpactSoundPlaybackInvokedForTesting
    UCinematicEffectsUtilityLibrary::OnImpactSoundPlaybackInvokedForTesting;
UCinematicEffectsUtilityLibrary::FOnImpactVFXSpawnInvokedForTesting
    UCinematicEffectsUtilityLibrary::OnImpactVFXSpawnInvokedForTesting;
#endif

namespace
{
	constexpr float HitstopFreezeDilation = 0.0001f;
	constexpr double CompatibilityLeaseWatchdogSeconds = 10.0;

	TMap<TWeakObjectPtr<UWorld>, FTimeDilationLeaseHandle> GCompatibilityWorldLeases;
	TMap<TWeakObjectPtr<AActor>, FTimeDilationLeaseHandle> GCompatibilityActorLeases;
	TMap<TWeakObjectPtr<AActor>, TArray<FTimeDilationLeaseHandle>> GCompatibilityFreezeLeases;
	TMap<TWeakObjectPtr<AActor>, TArray<FTimeDilationLeaseHandle>> GCompatibilitySavedFreezeLeases;

	template <typename TObjectType, typename TValueType>
	void PruneInvalidCompatibilityKeys(TMap<TWeakObjectPtr<TObjectType>, TValueType>& Entries)
	{
		for (auto It = Entries.CreateIterator(); It; ++It)
		{
			if (!It.Key().IsValid())
			{
				It.RemoveCurrent();
			}
		}
	}

	void PruneInvalidCompatibilityEntries()
	{
		PruneInvalidCompatibilityKeys(GCompatibilityWorldLeases);
		PruneInvalidCompatibilityKeys(GCompatibilityActorLeases);
		PruneInvalidCompatibilityKeys(GCompatibilityFreezeLeases);
		PruneInvalidCompatibilityKeys(GCompatibilitySavedFreezeLeases);
	}

	void PruneInactiveActorCompatibilityHandles(
		AActor* Actor,
		const UCombatEffectsWorldSubsystem* Effects,
		TMap<TWeakObjectPtr<AActor>, TArray<FTimeDilationLeaseHandle>>& Entries)
	{
		if (!Actor || !Effects)
		{
			return;
		}
		const TWeakObjectPtr<AActor> ActorKey(Actor);
		if (TArray<FTimeDilationLeaseHandle>* Handles = Entries.Find(ActorKey))
		{
			Handles->RemoveAll([Effects](const FTimeDilationLeaseHandle Handle)
			{
				return !Effects->IsLeaseActive(Handle);
			});
			if (Handles->IsEmpty())
			{
				Entries.Remove(ActorKey);
			}
		}
	}

	UCombatEffectsWorldSubsystem* GetEffectsSubsystem(UWorld* World)
	{
		PruneInvalidCompatibilityEntries();
		return World ? World->GetSubsystem<UCombatEffectsWorldSubsystem>() : nullptr;
	}

	UCombatEffectsWorldSubsystem* GetEffectsSubsystem(AActor* Actor)
	{
		return Actor ? GetEffectsSubsystem(Actor->GetWorld()) : nullptr;
	}
}

// ============================================================================
// TIME DILATION
// ============================================================================

bool UCinematicEffectsUtilityLibrary::ApplySlowMotion(UWorld* World, float Scale)
{
    if (!World || !FMath::IsFinite(Scale))
    {
        return false;
    }

	AWorldSettings* WorldSettings = World->GetWorldSettings();
	UCombatEffectsWorldSubsystem* Effects = GetEffectsSubsystem(World);
	if (!WorldSettings || !Effects)
    {
        return false;
    }

    // Clamp scale to valid range (never allow zero - use freeze for that)
    const float ClampedScale = FMath::Clamp(Scale, 0.01f, 1.0f);

    // ========================================================================
    // STACKING PREVENTION (Gap 17.5)
    // ========================================================================
    // Prevent time dilation stacking from multiple sources.
    // If slow-mo is already active at an equal or slower scale, don't override.
    // This prevents parry slow-mo (0.5) from overriding finisher slow-mo (0.3).
    const float CurrentDilation = WorldSettings->TimeDilation;
    if (CurrentDilation < 1.0f && CurrentDilation <= ClampedScale)
    {
        // Already in slower or equal slow-mo - don't stack
        UE_LOG(LogKatanaCombat, Log, TEXT("[CINEMATIC] Slow motion stacking prevented: Current=%.2f, Requested=%.2f"),
            CurrentDilation, ClampedScale);
        return false;
    }

	const TWeakObjectPtr<UWorld> WorldKey(World);
	const FTimeDilationLeaseHandle Existing = GCompatibilityWorldLeases.FindRef(WorldKey);
	const FTimeDilationLeaseHandle Successor = Effects->AcquireWorldLease(
		TEXT("CinematicEffects.LegacySlowMotion"),
		ClampedScale,
		CompatibilityLeaseWatchdogSeconds);
	if (!Successor.IsValid())
	{
		return false;
	}
	GCompatibilityWorldLeases.Add(WorldKey, Successor);
	Effects->ReleaseLease(Existing);

    UE_LOG(LogKatanaCombat, Verbose, TEXT("[CINEMATIC] Slow motion applied: Scale=%.2f (was %.2f)"),
        ClampedScale, CurrentDilation);
    return true;
}

void UCinematicEffectsUtilityLibrary::RestoreTimeDilation(UWorld* World)
{
    if (!World)
    {
        return;
    }

	UCombatEffectsWorldSubsystem* Effects = GetEffectsSubsystem(World);
	if (!Effects)
    {
        return;
    }

	const TWeakObjectPtr<UWorld> WorldKey(World);
	FTimeDilationLeaseHandle Handle;
	if (GCompatibilityWorldLeases.RemoveAndCopyValue(WorldKey, Handle))
	{
		Effects->ReleaseLease(Handle);
		UE_LOG(LogKatanaCombat, Verbose, TEXT("[CINEMATIC] Released legacy world time-dilation lease"));
	}
}

float UCinematicEffectsUtilityLibrary::GetTimeDilation(UWorld* World)
{
    if (!World)
    {
        return 1.0f;
    }

    AWorldSettings* WorldSettings = World->GetWorldSettings();
    if (!WorldSettings)
    {
        return 1.0f;
    }

    return WorldSettings->TimeDilation;
}

bool UCinematicEffectsUtilityLibrary::IsSlowMotionActive(UWorld* World)
{
    return GetTimeDilation(World) < 1.0f;
}

// ============================================================================
// CAMERA SHAKE
// ============================================================================

bool UCinematicEffectsUtilityLibrary::PlayCameraShakeOnActor(
    AActor* Actor,
    TSubclassOf<UCameraShakeBase> CameraShakeClass,
    float Scale)
{
    if (!Actor || !CameraShakeClass)
    {
        return false;
    }

    // Get the pawn (actor might be a pawn or owned by one)
    APawn* Pawn = Cast<APawn>(Actor);
    if (!Pawn)
    {
        // Try to get pawn owner
        Pawn = Actor->GetInstigator<APawn>();
    }

    if (!Pawn)
    {
        return false;
    }

    // Get player controller
    APlayerController* PC = Cast<APlayerController>(Pawn->GetController());
    if (!PC || !PC->IsLocalController())
    {
        return false;
    }

    // Play camera shake via PlayerCameraManager
    if (APlayerCameraManager* CameraManager = PC->PlayerCameraManager)
    {
        CameraManager->StartCameraShake(CameraShakeClass, Scale);

        UE_LOG(LogKatanaCombat, Verbose, TEXT("[CINEMATIC] Camera shake played: %s (Scale: %.2f)"),
            *CameraShakeClass->GetName(), Scale);
        return true;
    }

    return false;
}

int32 UCinematicEffectsUtilityLibrary::PlayCameraShakeAtLocation(
    UWorld* World,
    const FVector& Location,
    TSubclassOf<UCameraShakeBase> CameraShakeClass,
    float InnerRadius,
    float OuterRadius,
    float Falloff)
{
    if (!World || !CameraShakeClass)
    {
        return 0;
    }

    // Use UGameplayStatics to play camera shake at location
    UGameplayStatics::PlayWorldCameraShake(
        World,
        CameraShakeClass,
        Location,
        InnerRadius,
        OuterRadius,
        Falloff);

    // Return 1 since we don't have easy access to player count affected
    // This could be enhanced to iterate players and count affected
    return 1;
}

// ============================================================================
// HITSTOP (Per-Hit Impact Freeze)
// ============================================================================

bool UCinematicEffectsUtilityLibrary::ApplyHitstop(
    AActor* Attacker,
    AActor* Victim,
    const FHitstopConfig& Config,
    bool bWasBlocked)
{
    // Validate config
    if (!Config.IsActive())
    {
        return false;
    }

    // Must have at least one valid actor
    if (!Attacker && !Victim)
    {
        return false;
    }

    // Calculate effective duration
    float EffectiveDuration = Config.Duration;
    if (bWasBlocked)
    {
        if (!Config.bApplyOnBlock)
        {
            return false;
        }
        EffectiveDuration *= Config.BlockedDurationMultiplier;
    }

    // Validate duration after multiplier
    if (EffectiveDuration <= 0.0f)
    {
        return false;
    }

    // ====================================================================
    // CAMERA SHAKE (fires immediately -- camera continues during hitstop)
    // ====================================================================
    if (Config.CameraShake)
    {
        // Try attacker first (player is usually the attacker in single-player)
        if (Attacker)
        {
            PlayCameraShakeOnActor(Attacker, Config.CameraShake, Config.CameraShakeScale);
        }
        // If attacker is not a player, try victim (for when player is hit)
        if (Victim && Victim != Attacker)
        {
            PlayCameraShakeOnActor(Victim, Config.CameraShake, Config.CameraShakeScale);
        }
    }

    // ====================================================================
    // FREEZE PARTICIPANTS (Symmetric hitstop)
    // ====================================================================
    TArray<AActor*> ActorsToFreeze;
    if (Attacker)
    {
        ActorsToFreeze.Add(Attacker);
    }
    if (Victim && Victim != Attacker)
    {
        ActorsToFreeze.Add(Victim);
    }

    if (!ApplyHitstopToActors(ActorsToFreeze, EffectiveDuration))
    {
        return false;
    }

    UE_LOG(LogKatanaCombat, Log, TEXT("[HITSTOP] Applied: %.3fs to %d actors (blocked: %s)"),
        EffectiveDuration, ActorsToFreeze.Num(),
        bWasBlocked ? TEXT("YES") : TEXT("NO"));

    return true;
}

bool UCinematicEffectsUtilityLibrary::ApplyHitstopToActors(const TArray<AActor*>& Actors, float Duration)
{
	if (!FMath::IsFinite(Duration) || Duration <= 0.0f)
    {
        return false;
    }

    TArray<AActor*> UniqueActors;
    UniqueActors.Reserve(Actors.Num());
    for (AActor* Actor : Actors)
    {
        if (Actor)
        {
            UniqueActors.AddUnique(Actor);
        }
    }

    if (UniqueActors.IsEmpty())
    {
        return false;
    }

	bool bAcquiredAny = false;
	for (AActor* Actor : UniqueActors)
	{
		if (UCombatEffectsWorldSubsystem* Effects = GetEffectsSubsystem(Actor))
		{
			bAcquiredAny |= Effects->AcquireActorLease(
				Actor,
				TEXT("CinematicEffects.Hitstop"),
				HitstopFreezeDilation,
				static_cast<double>(Duration)).IsValid();
		}
	}
	return bAcquiredAny;
}

// ============================================================================
// IMPACT AUDIO
// ============================================================================

bool UCinematicEffectsUtilityLibrary::PlayImpactSound(
    UWorld* World,
    const FImpactAudioConfig& Config,
    USoundBase* WeaponFallbackSound,
    const FVector& ImpactLocation,
    AActor* Attacker)
{
    if (!World)
    {
        return false;
    }

    // Resolve sound: config sound takes priority over weapon fallback
    USoundBase* SoundToPlay = Config.ImpactSound;
    if (!SoundToPlay && Config.bUseWeaponFallback)
    {
        SoundToPlay = WeaponFallbackSound;
    }

    if (!SoundToPlay)
    {
        return false;
    }

    // Calculate randomized pitch for variation
    const float FinalPitch = Config.GetRandomizedPitch();
    const float FinalVolume = Config.VolumeMultiplier;

    // Play sound at impact location (spatial 3D audio)
    // UE 5.6 overload: Location, Rotation, Volume, Pitch, StartTime, Attenuation, Concurrency, Owner
    UGameplayStatics::PlaySoundAtLocation(
        World,
        SoundToPlay,
        ImpactLocation,
        FRotator::ZeroRotator,
        FinalVolume,
        FinalPitch,
        0.0f,       // Start time
        nullptr,    // Sound attenuation (use sound's default)
        nullptr,    // Concurrency settings
        Attacker);  // Owner for attenuation override

#if WITH_AUTOMATION_TESTS
    OnImpactSoundPlaybackInvokedForTesting.Broadcast(
        World,
        SoundToPlay,
        ImpactLocation,
        Attacker);
#endif

    UE_LOG(LogKatanaCombat, Verbose, TEXT("[AUDIO] Impact sound played: %s (Vol: %.2f, Pitch: %.2f) at %s"),
        *SoundToPlay->GetName(), FinalVolume, FinalPitch, *ImpactLocation.ToString());

    return true;
}

bool UCinematicEffectsUtilityLibrary::ResolveAndPlayImpactSound(
    UWorld* World,
    const FImpactAudioConfig& AudioConfig,
    const UCombatFXData* CombatFXData,
    EAttackType AttackType,
    USoundBase* WeaponFallbackSound,
    const FVector& ImpactLocation,
    bool bWasBlocked,
    AActor* Attacker)
{
    if (!World)
    {
        return false;
    }

    // ================================================================
    // TIER 1: Per-attack override (AttackData.ImpactAudioConfig.ImpactSound)
    // ================================================================
    if (AudioConfig.ImpactSound)
    {
        UE_LOG(LogCombatFX, Log, TEXT("[POOL-AUDIO] Tier 1: Per-attack override (sound: %s)"),
            *AudioConfig.ImpactSound->GetName());
        // Use per-attack config directly (existing path)
        return PlayImpactSound(World, AudioConfig, nullptr, ImpactLocation, Attacker);
    }

    // ================================================================
    // TIER 2: Pooled FX (CombatFXData pool for this attack type)
    // ================================================================
    if (CombatFXData)
    {
        UE_LOG(LogCombatFX, Log, TEXT("[POOL-AUDIO] Tier 2 check: CombatFXData=%s, AttackType=%d"),
            *CombatFXData->GetName(), static_cast<uint8>(AttackType));

        if (const FImpactFXPool* Pool = CombatFXData->ResolvePool(AttackType, bWasBlocked))
        {
            UE_LOG(LogCombatFX, Log, TEXT("[POOL-AUDIO] Pool found. HasSounds=%s (count=%d)"),
                Pool->HasSounds() ? TEXT("YES") : TEXT("NO"), Pool->ImpactSounds.Num());

            if (const FImpactSoundEntry* Entry = Pool->GetRandomSound())
            {
                // Build a temporary FImpactAudioConfig from pool entry
                FImpactAudioConfig PoolConfig;
                PoolConfig.ImpactSound = Entry->Sound;
                PoolConfig.VolumeMultiplier = Entry->VolumeMultiplier;
                PoolConfig.PitchMultiplier = Entry->PitchMultiplier;
                PoolConfig.PitchVariation = Pool->PitchVariation;
                PoolConfig.bUseWeaponFallback = false; // Pool resolved; don't chain

                UE_LOG(LogCombatFX, Log, TEXT("[POOL-AUDIO] Tier 2: Pool selection (type: %d, selected: %s)"),
                    static_cast<uint8>(AttackType), *Entry->Sound->GetName());

                return PlayImpactSound(World, PoolConfig, nullptr, ImpactLocation, Attacker);
            }
            else
            {
                UE_LOG(LogCombatFX, Warning, TEXT("[POOL-AUDIO] Pool found but GetRandomSound() returned nullptr"));
            }
        }
        else
        {
            UE_LOG(LogCombatFX, Log, TEXT("[POOL-AUDIO] No pool found for AttackType=%d"),
                static_cast<uint8>(AttackType));
        }
    }
    else
    {
        UE_LOG(LogCombatFX, Log, TEXT("[POOL-AUDIO] Tier 2 skipped: CombatFXData is nullptr"));
    }

    // ================================================================
    // TIER 3: Weapon fallback (existing PlayImpactSound path)
    // ================================================================
    if (AudioConfig.bUseWeaponFallback && WeaponFallbackSound)
    {
        FImpactAudioConfig FallbackConfig;
        FallbackConfig.ImpactSound = WeaponFallbackSound;
        FallbackConfig.VolumeMultiplier = AudioConfig.VolumeMultiplier;
        FallbackConfig.PitchMultiplier = AudioConfig.PitchMultiplier;
        FallbackConfig.PitchVariation = AudioConfig.PitchVariation;
        FallbackConfig.bUseWeaponFallback = false;

        UE_LOG(LogCombatFX, Log, TEXT("[POOL-AUDIO] Tier 3: Weapon fallback (sound: %s)"),
            *WeaponFallbackSound->GetName());

        return PlayImpactSound(World, FallbackConfig, nullptr, ImpactLocation, Attacker);
    }

    // ================================================================
    // TIER 4: Silent
    // ================================================================
    UE_LOG(LogCombatFX, Log, TEXT("[POOL-AUDIO] Tier 4: Silent (no sound available for attack type %d)"),
        static_cast<uint8>(AttackType));
    return false;
}

// ============================================================================
// IMPACT VFX (U-16)
// ============================================================================

bool UCinematicEffectsUtilityLibrary::SpawnImpactVFX(
    UWorld* World,
    const FImpactVFXConfig& Config,
    UNiagaraSystem* WeaponFallbackVFX,
    const FVector& ImpactLocation,
    const FVector& ImpactNormal,
    FName BoneName)
{
    if (!World)
    {
        UE_LOG(LogCombatFX, Warning, TEXT("[VFX] SpawnImpactVFX failed: null World"));
        return false;
    }

    // Resolve which VFX to spawn
    UNiagaraSystem* VFXToSpawn = Config.ImpactVFX;
    if (!VFXToSpawn && Config.bUseWeaponFallback)
    {
        VFXToSpawn = WeaponFallbackVFX;
    }

    if (!VFXToSpawn)
    {
        // No VFX configured - this is valid, just means no VFX for this attack
        return false;
    }

    // Calculate rotation: align to surface normal or use world up
    FRotator VFXRotation;
    if (Config.bAlignToSurface && !ImpactNormal.IsNearlyZero())
    {
        // Create rotation from surface normal (VFX "up" aligns with normal)
        VFXRotation = ImpactNormal.Rotation();
    }
    else
    {
        VFXRotation = FRotator::ZeroRotator;
    }

    // Spawn the Niagara system
    UNiagaraComponent* SpawnedVFX = UNiagaraFunctionLibrary::SpawnSystemAtLocation(
        World,
        VFXToSpawn,
        ImpactLocation,
        VFXRotation,
        FVector(Config.ScaleMultiplier),  // Scale
        true,   // bAutoDestroy
        true,   // bAutoActivate
        ENCPoolMethod::AutoRelease,
        true    // bPreCullCheck
    );

    if (SpawnedVFX)
    {
#if WITH_AUTOMATION_TESTS
        OnImpactVFXSpawnInvokedForTesting.Broadcast(
            World, VFXToSpawn, ImpactLocation, BoneName);
#endif
        UE_LOG(LogCombatFX, Verbose, TEXT("[VFX] Impact VFX spawned: %s at %s (scale: %.2f, aligned: %s)"),
            *VFXToSpawn->GetName(),
            *ImpactLocation.ToString(),
            Config.ScaleMultiplier,
            Config.bAlignToSurface ? TEXT("YES") : TEXT("NO"));
        return true;
    }

    UE_LOG(LogCombatFX, Warning, TEXT("[VFX] SpawnImpactVFX failed to spawn Niagara system: %s"),
        *VFXToSpawn->GetName());
    return false;
}

bool UCinematicEffectsUtilityLibrary::ResolveAndSpawnImpactVFX(
    UWorld* World,
    const FImpactVFXConfig& VFXConfig,
    const UCombatFXData* CombatFXData,
    EAttackType AttackType,
    UNiagaraSystem* WeaponFallbackVFX,
    const FVector& ImpactLocation,
    const FVector& ImpactNormal,
    bool bWasBlocked,
    FName BoneName)
{
    if (!World)
    {
        UE_LOG(LogCombatFX, Warning, TEXT("[POOL-VFX] ResolveAndSpawnImpactVFX failed: null World"));
        return false;
    }

    // ================================================================
    // TIER 1: Per-attack override (AttackData.ImpactVFXConfig.ImpactVFX)
    // ================================================================
    if (VFXConfig.ImpactVFX)
    {
        UE_LOG(LogCombatFX, Log, TEXT("[POOL-VFX] Tier 1: Per-attack override (VFX: %s)"),
            *VFXConfig.ImpactVFX->GetName());

        return SpawnImpactVFX(World, VFXConfig, nullptr, ImpactLocation, ImpactNormal, BoneName);
    }

    // ================================================================
    // TIER 2: CombatFXData pool (random from pool)
    // ================================================================
    if (CombatFXData)
    {
        UE_LOG(LogCombatFX, Log, TEXT("[POOL-VFX] Tier 2 check: CombatFXData=%s, AttackType=%d"),
            *CombatFXData->GetName(), static_cast<uint8>(AttackType));

        const FImpactFXPool* Pool = CombatFXData->ResolvePool(AttackType, bWasBlocked);
        if (Pool)
        {
            UE_LOG(LogCombatFX, Log, TEXT("[POOL-VFX] Pool found. HasVFX=%s (count=%d)"),
                Pool->HasVFX() ? TEXT("YES") : TEXT("NO"), Pool->ImpactVFX.Num());

            if (Pool->HasVFX())
            {
                const FImpactVFXEntry* Entry = Pool->GetRandomVFX();
                if (Entry && Entry->IsValid())
                {
                    // Build config from pool entry
                    FImpactVFXConfig PoolConfig;
                    PoolConfig.ImpactVFX = Entry->VFX;
                    PoolConfig.ScaleMultiplier = Entry->ScaleMultiplier;
                    PoolConfig.bAlignToSurface = Pool->bAlignVFXToSurface;
                    PoolConfig.bUseWeaponFallback = false; // Pool resolved; don't chain

                    UE_LOG(LogCombatFX, Log, TEXT("[POOL-VFX] Tier 2: Pool selection (type: %d, selected: %s)"),
                        static_cast<uint8>(AttackType), *Entry->VFX->GetName());

                    return SpawnImpactVFX(World, PoolConfig, nullptr, ImpactLocation, ImpactNormal, BoneName);
                }
                else
                {
                    UE_LOG(LogCombatFX, Warning, TEXT("[POOL-VFX] Pool has VFX but GetRandomVFX() returned invalid"));
                }
            }
        }
        else
        {
            UE_LOG(LogCombatFX, Log, TEXT("[POOL-VFX] No pool found for AttackType=%d"),
                static_cast<uint8>(AttackType));
        }
    }
    else
    {
        UE_LOG(LogCombatFX, Log, TEXT("[POOL-VFX] Tier 2 skipped: CombatFXData is nullptr"));
    }

    // ================================================================
    // TIER 3: Weapon fallback
    // ================================================================
    if (VFXConfig.bUseWeaponFallback && WeaponFallbackVFX)
    {
        FImpactVFXConfig FallbackConfig;
        FallbackConfig.ImpactVFX = WeaponFallbackVFX;
        FallbackConfig.ScaleMultiplier = VFXConfig.ScaleMultiplier;
        FallbackConfig.bAlignToSurface = VFXConfig.bAlignToSurface;
        FallbackConfig.bUseWeaponFallback = false;

        UE_LOG(LogCombatFX, Log, TEXT("[POOL-VFX] Tier 3: Weapon fallback (VFX: %s)"),
            *WeaponFallbackVFX->GetName());

        return SpawnImpactVFX(World, FallbackConfig, nullptr, ImpactLocation, ImpactNormal, BoneName);
    }

    // ================================================================
    // TIER 4: Nothing
    // ================================================================
    UE_LOG(LogCombatFX, Log, TEXT("[POOL-VFX] Tier 4: No VFX available for attack type %d"),
        static_cast<uint8>(AttackType));
    return false;
}

// ============================================================================
// ACTOR TIME DILATION (Per-Actor Effects)
// ============================================================================

void UCinematicEffectsUtilityLibrary::SetActorTimeDilation(AActor* Actor, float TimeDilation)
{
    if (!Actor)
    {
        return;
    }

	UCombatEffectsWorldSubsystem* Effects = GetEffectsSubsystem(Actor);
	if (!Effects || !FMath::IsFinite(TimeDilation))
	{
		return;
	}
	const TWeakObjectPtr<AActor> ActorKey(Actor);
	const FTimeDilationLeaseHandle Existing = GCompatibilityActorLeases.FindRef(ActorKey);
	const FTimeDilationLeaseHandle Successor = Effects->AcquireActorLease(
		Actor,
		TEXT("CinematicEffects.LegacyActorDilation"),
		FMath::Max(HitstopFreezeDilation, TimeDilation),
		CompatibilityLeaseWatchdogSeconds);
	if (!Successor.IsValid())
	{
		return;
	}
	GCompatibilityActorLeases.Add(ActorKey, Successor);
	Effects->ReleaseLease(Existing);

    UE_LOG(LogKatanaCombat, Verbose, TEXT("[CINEMATIC] Actor %s time dilation set to: %.2f"),
        *Actor->GetName(), Actor->CustomTimeDilation);
}

void UCinematicEffectsUtilityLibrary::RestoreActorTimeDilation(AActor* Actor)
{
    if (!Actor)
    {
        return;
    }

	if (UCombatEffectsWorldSubsystem* Effects = GetEffectsSubsystem(Actor))
	{
		FTimeDilationLeaseHandle Handle;
		if (GCompatibilityActorLeases.RemoveAndCopyValue(Actor, Handle))
		{
			Effects->ReleaseLease(Handle);
		}
	}
}

void UCinematicEffectsUtilityLibrary::FreezeActors(const TArray<AActor*>& Actors)
{
	for (AActor* Actor : Actors)
	{
		if (UCombatEffectsWorldSubsystem* Effects = GetEffectsSubsystem(Actor))
		{
			PruneInactiveActorCompatibilityHandles(
				Actor, Effects, GCompatibilityFreezeLeases);
			const FTimeDilationLeaseHandle Handle = Effects->AcquireActorLease(
				Actor,
				TEXT("CinematicEffects.LegacyFreeze"),
				HitstopFreezeDilation,
				CompatibilityLeaseWatchdogSeconds);
			if (Handle.IsValid())
			{
				GCompatibilityFreezeLeases.FindOrAdd(Actor).Add(Handle);
			}
		}
    }

    UE_LOG(LogKatanaCombat, Verbose, TEXT("[CINEMATIC] Froze %d actors"), Actors.Num());
}

void UCinematicEffectsUtilityLibrary::RestoreActors(const TArray<AActor*>& Actors)
{
	for (AActor* Actor : Actors)
	{
		if (UCombatEffectsWorldSubsystem* Effects = GetEffectsSubsystem(Actor))
		{
			PruneInactiveActorCompatibilityHandles(
				Actor, Effects, GCompatibilityFreezeLeases);
			if (TArray<FTimeDilationLeaseHandle>* Handles = GCompatibilityFreezeLeases.Find(Actor))
			{
				if (!Handles->IsEmpty())
				{
					Effects->ReleaseLease(Handles->Pop(EAllowShrinking::No));
				}
				if (Handles->IsEmpty())
				{
					GCompatibilityFreezeLeases.Remove(Actor);
				}
			}
		}
    }

    UE_LOG(LogKatanaCombat, Verbose, TEXT("[CINEMATIC] Restored %d actors"), Actors.Num());
}

TMap<TWeakObjectPtr<AActor>, float> UCinematicEffectsUtilityLibrary::FreezeActorsWithSave(const TArray<AActor*>& Actors)
{
    TMap<TWeakObjectPtr<AActor>, float> SavedDilations;
    SavedDilations.Reserve(Actors.Num());
	TSet<TWeakObjectPtr<AActor>> SeenActors;
	SeenActors.Reserve(Actors.Num());

    for (AActor* Actor : Actors)
    {
		if (!Actor || SeenActors.Contains(Actor))
		{
			continue;
		}
		SeenActors.Add(Actor);
		if (UCombatEffectsWorldSubsystem* Effects = GetEffectsSubsystem(Actor))
		{
			PruneInactiveActorCompatibilityHandles(
				Actor, Effects, GCompatibilitySavedFreezeLeases);
			SavedDilations.Add(Actor, Actor->CustomTimeDilation);
			const FTimeDilationLeaseHandle Handle = Effects->AcquireActorLease(
				Actor,
				TEXT("CinematicEffects.LegacySavedFreeze"),
				HitstopFreezeDilation,
				CompatibilityLeaseWatchdogSeconds);
			if (Handle.IsValid())
			{
				GCompatibilitySavedFreezeLeases.FindOrAdd(Actor).Add(Handle);
			}
        }
    }

    UE_LOG(LogKatanaCombat, Verbose, TEXT("[CINEMATIC] Froze %d actors (with save)"), Actors.Num());
    return SavedDilations;
}

void UCinematicEffectsUtilityLibrary::RestoreActorsFromSaved(const TMap<TWeakObjectPtr<AActor>, float>& SavedDilations)
{
    int32 RestoredCount = 0;
	for (const auto& Pair : SavedDilations)
	{
		if (AActor* Actor = Pair.Key.Get())
		{
			if (UCombatEffectsWorldSubsystem* Effects = GetEffectsSubsystem(Actor))
			{
				PruneInactiveActorCompatibilityHandles(
					Actor, Effects, GCompatibilitySavedFreezeLeases);
				if (TArray<FTimeDilationLeaseHandle>* Handles = GCompatibilitySavedFreezeLeases.Find(Actor))
				{
					if (!Handles->IsEmpty() && Effects->ReleaseLease(Handles->Pop(EAllowShrinking::No)))
					{
						++RestoredCount;
					}
					if (Handles->IsEmpty())
					{
						GCompatibilitySavedFreezeLeases.Remove(Actor);
					}
				}
			}
		}
    }

    UE_LOG(LogKatanaCombat, Verbose, TEXT("[CINEMATIC] Restored %d actors from saved dilations"), RestoredCount);
}
