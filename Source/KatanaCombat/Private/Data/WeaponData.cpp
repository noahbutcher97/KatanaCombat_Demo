// Copyright Epic Games, Inc. All Rights Reserved.

#include "Data/WeaponData.h"

UWeaponData::UWeaponData()
{
    // Default identity
    DisplayName = FText::FromString(TEXT("Unnamed Weapon"));

    // Default visual
    MeshScale = FVector(1.0f, 1.0f, 1.0f);
    MeshAttachOffset = FTransform::Identity;

    // Default sockets
    EquippedSocket = "weapon_r";
    HolsteredSocket = "weapon_back";
    TraceStartSocket = "weapon_start";
    TraceEndSocket = "weapon_end";
    bUseCharacterSocketsForTrace = true;

    // Default animations
    EquipPlayRate = 1.0f;
    HolsterPlayRate = 1.0f;

    // Default combat
    TraceRadius = 5.0f;
    DamageMultiplier = 1.0f;
    WeaponReach = 150.0f;
}

#if WITH_EDITOR
void UWeaponData::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
    Super::PostEditChangeProperty(PropertyChangedEvent);

    // Validate damage multiplier
    DamageMultiplier = FMath::Clamp(DamageMultiplier, 0.1f, 5.0f);

    // Validate trace radius
    TraceRadius = FMath::Clamp(TraceRadius, 1.0f, 50.0f);

    // Validate weapon reach
    WeaponReach = FMath::Clamp(WeaponReach, 50.0f, 500.0f);
}
#endif
