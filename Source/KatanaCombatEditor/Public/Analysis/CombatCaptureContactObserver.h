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
class UCombatComponent;
class UPairedAnimationComponent;
class UWeaponComponent;
struct FDefenseResolution;

/**
 * Editor-only listener that turns gameplay contact observations into `contact` markers on a
 * capture session: the attacker's weapon trace hitting a non-character victim, committed
 * defense contacts on the victim (character hits, blocks and parries, with their outcome),
 * and the attacker's paired-animation sync points. Each marker payload states which source
 * observed it; the recorder stores the payload verbatim and the reaction review reads it as
 * marker evidence. Owned by the session; it never keeps the session alive and unbinds when told to.
 */
UCLASS()
class KATANACOMBATEDITOR_API UCombatCaptureContactObserver : public UObject
{
	GENERATED_BODY()

public:
	/** One attacker and one victim: weapon trace and paired sync on the attacker, plus committed
	 * defense contacts from the attacker on the victim. */
	void Bind(FCombatCaptureSession* InSession, ABaseCombatCharacter* Attacker, const FString& AttackerRole,
		ABaseCombatCharacter* Victim, const FString& VictimRole);
	/** Committed defense contacts between any two of these participants, in either direction. */
	void BindParticipants(FCombatCaptureSession* InSession, TConstArrayView<TPair<ABaseCombatCharacter*, FString>> Participants);
	void Unbind();
	int32 GetWeaponContactCount() const { return WeaponContacts; }
	int32 GetPairedContactCount() const { return PairedContacts; }
	int32 GetCommittedContactCount() const { return CommittedContacts; }

private:
	UFUNCTION()
	void HandleWeaponHit(AActor* HitActor, const FHitResult& HitResult, UAttackData* AttackData);
	UFUNCTION()
	void HandlePairedSyncPoint(EPairedReactionType Type, FName SyncPointName);
	/** Character targets take the weapon's rich contact path, which never reaches OnWeaponHit;
	 * the defender's committed resolution is the observation for hits, blocks and parries. */
	void HandleDefenseResolved(const FDefenseResolution& Resolution, int32 DefenderIndex);
	void ObserveDefender(ABaseCombatCharacter* Defender, const FString& Role);

	TSharedPtr<class FJsonObject> BasePayload(const FString& Hit, const FString& Source,
		const FString& Attacker, const FString& Victim) const;

	struct FDefender
	{
		TWeakObjectPtr<UCombatComponent> Combat;
		FString Role;
		FDelegateHandle Handle;
	};

	FCombatCaptureSession* Session = nullptr;
	TWeakObjectPtr<ABaseCombatCharacter> AttackerActor, VictimActor;
	TWeakObjectPtr<UWeaponComponent> Weapon;
	TWeakObjectPtr<UPairedAnimationComponent> Paired;
	FString AttackerId, VictimId;
	TArray<FDefender> Defenders;
	TMap<TWeakObjectPtr<AActor>, FString> AttackerRoles;
	int32 WeaponContacts = 0, PairedContacts = 0, CommittedContacts = 0, IgnoredContacts = 0;
};
