// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "AnimNotify_WeaponEquip.generated.h"

/**
 * Animation notify to equip weapon (move from holster to hand)
 * Place at the frame in draw/unsheathe animations where the weapon
 * should visually move to the character's hand
 */
UCLASS(DisplayName = "Weapon Equip")
class KATANACOMBAT_API UAnimNotify_WeaponEquip : public UAnimNotify
{
    GENERATED_BODY()

public:
    virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
        const FAnimNotifyEventReference& EventReference) override;

    virtual FString GetNotifyName_Implementation() const override
    {
        return TEXT("Weapon Equip");
    }
};
