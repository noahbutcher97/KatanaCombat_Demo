// Copyright Epic Games, Inc. All Rights Reserved.

#include "Animation/AnimNotify_WeaponEquip.h"
#include "Core/WeaponComponent.h"
#include "Components/SkeletalMeshComponent.h"

void UAnimNotify_WeaponEquip::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
    const FAnimNotifyEventReference& EventReference)
{
    Super::Notify(MeshComp, Animation, EventReference);

    if (!MeshComp)
    {
        return;
    }

    AActor* Owner = MeshComp->GetOwner();
    if (!Owner)
    {
        return;
    }

    if (UWeaponComponent* WeaponComp = Owner->FindComponentByClass<UWeaponComponent>())
    {
        WeaponComp->Equip();
    }
}
