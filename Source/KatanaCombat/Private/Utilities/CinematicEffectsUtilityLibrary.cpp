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
#include "KatanaCombat.h"
#include "Data/CombatFXData.h"

// ============================================================================
// TIME DILATION
// ============================================================================

bool UCinematicEffectsUtilityLibrary::ApplySlowMotion(UWorld* World, float Scale)
{
    if (!World)
    {
        return false;
    }

    AWorldSettings* WorldSettings = World->GetWorldSettings();
    if (!WorldSettings)
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

    WorldSettings->SetTimeDilation(ClampedScale);

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

    AWorldSettings* WorldSettings = World->GetWorldSettings();
    if (!WorldSettings)
    {
        return;
    }

    // Only log if actually changing
    if (WorldSettings->TimeDilation != 1.0f)
    {
        WorldSettings->SetTimeDilation(1.0f);
        UE_LOG(LogKatanaCombat, Verbose, TEXT("[CINEMATIC] Time dilation restored to 1.0"));
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

    // Save pre-freeze time dilations (supports overlapping slow-mo)
    TMap<TWeakObjectPtr<AActor>, float> SavedTimeDilations;
    SavedTimeDilations.Reserve(ActorsToFreeze.Num());

    for (AActor* Actor : ActorsToFreeze)
    {
        SavedTimeDilations.Add(Actor, Actor->CustomTimeDilation);
        Actor->CustomTimeDilation = 0.0001f;
    }

    UE_LOG(LogKatanaCombat, Log, TEXT("[HITSTOP] Applied: %.3fs to %d actors (blocked: %s)"),
        EffectiveDuration, ActorsToFreeze.Num(),
        bWasBlocked ? TEXT("YES") : TEXT("NO"));

    // ====================================================================
    // PLATFORM TIME-BASED RESTORATION
    // ====================================================================
    // Identical pattern to AnimNotifyState_PairedAnimationSync (lines 188-219).
    // FPlatformTime::Seconds() ensures wall-clock accuracy regardless of any
    // world or actor time dilation currently in effect.

    const double HitstopEndTime = FPlatformTime::Seconds()
        + static_cast<double>(EffectiveDuration);

    FTSTicker::GetCoreTicker().AddTicker(
        FTickerDelegate::CreateLambda(
            [SavedTimeDilations, HitstopEndTime](float DeltaTime) -> bool
            {
                if (FPlatformTime::Seconds() >= HitstopEndTime)
                {
                    for (const auto& Pair : SavedTimeDilations)
                    {
                        if (AActor* Actor = Pair.Key.Get())
                        {
                            Actor->CustomTimeDilation = Pair.Value;
                        }
                    }

                    UE_LOG(LogKatanaCombat, Verbose, TEXT("[HITSTOP] Restored %d actors"),
                        SavedTimeDilations.Num());
                    return false; // Remove ticker
                }
                return true; // Continue ticking
            })
    );

    return true;
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
        UE_LOG(LogCombatFX, Verbose, TEXT("[POOL] Tier 1: Per-attack override used (sound: %s)"),
            *AudioConfig.ImpactSound->GetName());
        // Use per-attack config directly (existing path)
        return PlayImpactSound(World, AudioConfig, nullptr, ImpactLocation, Attacker);
    }

    // ================================================================
    // TIER 2: Pooled FX (CombatFXData pool for this attack type)
    // ================================================================
    if (CombatFXData)
    {
        if (const FImpactFXPool* Pool = CombatFXData->ResolvePool(AttackType, bWasBlocked))
        {
            if (const FImpactSoundEntry* Entry = Pool->GetRandomSound())
            {
                // Build a temporary FImpactAudioConfig from pool entry
                FImpactAudioConfig PoolConfig;
                PoolConfig.ImpactSound = Entry->Sound;
                PoolConfig.VolumeMultiplier = Entry->VolumeMultiplier;
                PoolConfig.PitchMultiplier = Entry->PitchMultiplier;
                PoolConfig.PitchVariation = Pool->PitchVariation;
                PoolConfig.bUseWeaponFallback = false; // Pool resolved; don't chain

                UE_LOG(LogCombatFX, Verbose, TEXT("[POOL] Tier 2: Pool selection (type: %d, selected: %s)"),
                    static_cast<uint8>(AttackType), *Entry->Sound->GetName());

                return PlayImpactSound(World, PoolConfig, nullptr, ImpactLocation, Attacker);
            }
        }
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

        UE_LOG(LogCombatFX, Verbose, TEXT("[POOL] Tier 3: Weapon fallback used (sound: %s)"),
            *WeaponFallbackSound->GetName());

        return PlayImpactSound(World, FallbackConfig, nullptr, ImpactLocation, Attacker);
    }

    // ================================================================
    // TIER 4: Silent
    // ================================================================
    UE_LOG(LogCombatFX, Log, TEXT("[POOL] Tier 4: Silent (no sound available for attack type %d)"),
        static_cast<uint8>(AttackType));
    return false;
}

// ============================================================================
// IMPACT VFX (Scaffold for U-16)
// ============================================================================

bool UCinematicEffectsUtilityLibrary::SpawnImpactVFX(
    UWorld* World,
    const FImpactVFXConfig& Config,
    UNiagaraSystem* WeaponFallbackVFX,
    const FVector& ImpactLocation,
    const FVector& ImpactNormal,
    FName BoneName)
{
    // SCAFFOLD FOR U-16
    // TODO: Implement Niagara system spawning with surface alignment
    // UNiagaraSystem* VFXToSpawn = Config.ImpactVFX ? Config.ImpactVFX : (Config.bUseWeaponFallback ? WeaponFallbackVFX : nullptr);
    // if (VFXToSpawn) { UNiagaraFunctionLibrary::SpawnSystemAtLocation(...) }
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
    // SCAFFOLD FOR U-16
    // Same 4-tier resolution as audio, but for VFX.
    // TODO: Implement when U-16 ships
    UE_LOG(LogCombatFX, Verbose, TEXT("[POOL] ResolveAndSpawnImpactVFX not yet implemented (U-16)"));
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

    Actor->CustomTimeDilation = FMath::Max(0.0001f, TimeDilation);

    UE_LOG(LogKatanaCombat, Verbose, TEXT("[CINEMATIC] Actor %s time dilation set to: %.2f"),
        *Actor->GetName(), Actor->CustomTimeDilation);
}

void UCinematicEffectsUtilityLibrary::RestoreActorTimeDilation(AActor* Actor)
{
    if (!Actor)
    {
        return;
    }

    if (Actor->CustomTimeDilation != 1.0f)
    {
        Actor->CustomTimeDilation = 1.0f;
        UE_LOG(LogKatanaCombat, Verbose, TEXT("[CINEMATIC] Actor %s time dilation restored to 1.0"),
            *Actor->GetName());
    }
}

void UCinematicEffectsUtilityLibrary::FreezeActors(const TArray<AActor*>& Actors)
{
    for (AActor* Actor : Actors)
    {
        if (Actor)
        {
            Actor->CustomTimeDilation = 0.0001f;
        }
    }

    UE_LOG(LogKatanaCombat, Verbose, TEXT("[CINEMATIC] Froze %d actors"), Actors.Num());
}

void UCinematicEffectsUtilityLibrary::RestoreActors(const TArray<AActor*>& Actors)
{
    for (AActor* Actor : Actors)
    {
        if (Actor)
        {
            Actor->CustomTimeDilation = 1.0f;
        }
    }

    UE_LOG(LogKatanaCombat, Verbose, TEXT("[CINEMATIC] Restored %d actors"), Actors.Num());
}

TMap<TWeakObjectPtr<AActor>, float> UCinematicEffectsUtilityLibrary::FreezeActorsWithSave(const TArray<AActor*>& Actors)
{
    TMap<TWeakObjectPtr<AActor>, float> SavedDilations;
    SavedDilations.Reserve(Actors.Num());

    for (AActor* Actor : Actors)
    {
        if (Actor)
        {
            SavedDilations.Add(Actor, Actor->CustomTimeDilation);
            Actor->CustomTimeDilation = 0.0001f;
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
            Actor->CustomTimeDilation = Pair.Value;
            RestoredCount++;
        }
    }

    UE_LOG(LogKatanaCombat, Verbose, TEXT("[CINEMATIC] Restored %d actors from saved dilations"), RestoredCount);
}
