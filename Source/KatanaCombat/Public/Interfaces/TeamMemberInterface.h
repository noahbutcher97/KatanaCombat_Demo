// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "TeamMemberInterface.generated.h"

/**
 * Team identifiers for combat targeting
 * Used by soft aim assist to filter valid targets
 */
UENUM(BlueprintType)
enum class ETeamId : uint8
{
    Neutral = 0     UMETA(DisplayName = "Neutral"),
    Player = 1      UMETA(DisplayName = "Player"),
    Enemy = 2       UMETA(DisplayName = "Enemy"),
    Ally = 3        UMETA(DisplayName = "Ally")
};

UINTERFACE(MinimalAPI, Blueprintable)
class UTeamMemberInterface : public UInterface
{
    GENERATED_BODY()
};

/**
 * Interface for actors that belong to a team
 * Used by targeting system to determine friend/foe relationships
 */
class KATANACOMBAT_API ITeamMemberInterface
{
    GENERATED_BODY()

public:
    /**
     * Get this actor's team ID
     * @return Team identifier
     */
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Team")
    ETeamId GetTeamId() const;

    /**
     * Check if this actor is hostile to another actor
     * @param Other - Actor to check against
     * @return True if hostile (valid attack target)
     */
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Team")
    bool IsHostileTo(AActor* Other) const;

    /**
     * Check if this actor is friendly to another actor
     * @param Other - Actor to check against
     * @return True if friendly (should not target)
     */
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Team")
    bool IsFriendlyTo(AActor* Other) const;
};
