// Copyright Epic Games, Inc. All Rights Reserved.

#include "Utilities/CinematicEffectsUtilityLibrary.h"
#include "GameFramework/WorldSettings.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "Camera/PlayerCameraManager.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "KatanaCombat.h"

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
