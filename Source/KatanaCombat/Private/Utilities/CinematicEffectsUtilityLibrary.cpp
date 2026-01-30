// Copyright Epic Games, Inc. All Rights Reserved.

#include "Utilities/CinematicEffectsUtilityLibrary.h"
#include "GameFramework/WorldSettings.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "Camera/PlayerCameraManager.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"

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
    WorldSettings->SetTimeDilation(ClampedScale);

    UE_LOG(LogTemp, Verbose, TEXT("[CINEMATIC] Slow motion applied: Scale=%.2f"), ClampedScale);
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
        UE_LOG(LogTemp, Verbose, TEXT("[CINEMATIC] Time dilation restored to 1.0"));
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

        UE_LOG(LogTemp, Verbose, TEXT("[CINEMATIC] Camera shake played: %s (Scale: %.2f)"),
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

    Actor->CustomTimeDilation = FMath::Max(0.0f, TimeDilation);

    UE_LOG(LogTemp, Verbose, TEXT("[CINEMATIC] Actor %s time dilation set to: %.2f"),
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
        UE_LOG(LogTemp, Verbose, TEXT("[CINEMATIC] Actor %s time dilation restored to 1.0"),
            *Actor->GetName());
    }
}

void UCinematicEffectsUtilityLibrary::FreezeActors(const TArray<AActor*>& Actors)
{
    for (AActor* Actor : Actors)
    {
        if (Actor)
        {
            Actor->CustomTimeDilation = 0.0f;
        }
    }

    UE_LOG(LogTemp, Verbose, TEXT("[CINEMATIC] Froze %d actors"), Actors.Num());
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

    UE_LOG(LogTemp, Verbose, TEXT("[CINEMATIC] Restored %d actors"), Actors.Num());
}
