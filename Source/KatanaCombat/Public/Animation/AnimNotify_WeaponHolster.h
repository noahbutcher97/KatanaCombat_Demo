// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "AnimNotify_WeaponHolster.generated.h"

/**
 * Animation notify to holster weapon (move from hand to holster position)
 * Place at the frame in sheathe animations where the weapon
 * should visually move to the holster socket
 */
UCLASS(DisplayName = "Weapon Holster")
class KATANACOMBAT_API UAnimNotify_WeaponHolster : public UAnimNotify
{
    GENERATED_BODY()

public:
    virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
        const FAnimNotifyEventReference& EventReference) override;

    virtual FString GetNotifyName_Implementation() const override
    {
        return TEXT("Weapon Holster");
    }
};
