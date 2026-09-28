// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Data/PairedAnimationTypes.h"
#include "Engine/HitResult.h"
#include "UObject/Object.h"
#include "CombatCaptureContactObserver.generated.h"

class ABaseCombatCharacter;
class FCombatCaptureSession;
class UAttackData;
class UPairedAnimationComponent;
class UWeaponComponent;

/**
 * Editor-only listener that turns two gameplay contact observations into `contact` markers
 * on a capture session: the attacker's weapon trace hitting the victim, and the attacker's
 * paired-animation sync points. Each marker payload states which source observed it; the
 * recorder stores the payload verbatim and the reaction review reads it as marker evidence.
 * Owned by the session; it never keeps the session alive and unbinds when told to.
 */
UCLASS()
class KATANACOMBATEDITOR_API UCombatCaptureContactObserver : public UObject
{
	GENERATED_BODY()

public:
	void Bind(FCombatCaptureSession* InSession, ABaseCombatCharacter* Attacker, const FString& AttackerRole,
		ABaseCombatCharacter* Victim, const FString& VictimRole);
	void Unbind();
	int32 GetWeaponContactCount() const { return WeaponContacts; }
	int32 GetPairedContactCount() const { return PairedContacts; }

private:
	UFUNCTION()
	void HandleWeaponHit(AActor* HitActor, const FHitResult& HitResult, UAttackData* AttackData);
	UFUNCTION()
	void HandlePairedSyncPoint(EPairedReactionType Type, FName SyncPointName);

	TSharedPtr<class FJsonObject> BasePayload(const FString& Hit, const FString& Source) const;

	FCombatCaptureSession* Session = nullptr;
	TWeakObjectPtr<ABaseCombatCharacter> AttackerActor, VictimActor;
	TWeakObjectPtr<UWeaponComponent> Weapon;
	TWeakObjectPtr<UPairedAnimationComponent> Paired;
	FString AttackerId, VictimId;
	int32 WeaponContacts = 0, PairedContacts = 0;
};
