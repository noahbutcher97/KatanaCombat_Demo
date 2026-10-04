// Copyright Epic Games, Inc. All Rights Reserved.
#include "Analysis/CombatCaptureContactObserver.h"
#include "Analysis/CombatCaptureSession.h"
#include "Characters/BaseCombatCharacter.h"
#include "CombatTypes.h"
#include "Core/CombatComponent.h"
#include "Core/PairedAnimationComponent.h"
#include "Core/WeaponComponent.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"

namespace
{
TArray<TSharedPtr<FJsonValue>> VectorField(const FVector& Value)
{
	return {MakeShared<FJsonValueNumber>(Value.X), MakeShared<FJsonValueNumber>(Value.Y), MakeShared<FJsonValueNumber>(Value.Z)};
}

template <typename EnumType>
FString EnumName(EnumType Value)
{
	return StaticEnum<EnumType>()->GetNameStringByValue(static_cast<int64>(Value));
}
} // namespace

void UCombatCaptureContactObserver::Bind(FCombatCaptureSession* InSession, ABaseCombatCharacter* Attacker,
	const FString& AttackerRole, ABaseCombatCharacter* Victim, const FString& VictimRole)
{
	Unbind();
	Session = InSession;
	AttackerActor = Attacker;
	VictimActor = Victim;
	AttackerId = AttackerRole;
	VictimId = VictimRole;
	Weapon = Attacker ? Attacker->GetWeaponComponent() : nullptr;
	Paired = Attacker ? Attacker->PairedAnimationComponent : nullptr;
	if (Weapon.IsValid()) { Weapon->OnWeaponHit.AddDynamic(this, &UCombatCaptureContactObserver::HandleWeaponHit); }
	if (Paired.IsValid()) { Paired->OnPairedAnimationSyncPoint.AddDynamic(this, &UCombatCaptureContactObserver::HandlePairedSyncPoint); }
	if (Attacker) { AttackerRoles.Add(Attacker, AttackerRole); }
	ObserveDefender(Victim, VictimRole);
}

void UCombatCaptureContactObserver::BindParticipants(FCombatCaptureSession* InSession,
	TConstArrayView<TPair<ABaseCombatCharacter*, FString>> Participants)
{
	Unbind();
	Session = InSession;
	for (const auto& Participant : Participants)
	{
		if (Participant.Key) { AttackerRoles.Add(Participant.Key, Participant.Value); }
	}
	for (const auto& Participant : Participants) { ObserveDefender(Participant.Key, Participant.Value); }
}

void UCombatCaptureContactObserver::ObserveDefender(ABaseCombatCharacter* Defender, const FString& Role)
{
	UCombatComponent* Combat = Defender ? Defender->CombatComponent.Get() : nullptr;
	if (!Combat) { return; }
	const int32 Index = Defenders.Num();
	FDefender& Entry = Defenders.AddDefaulted_GetRef();
	Entry.Combat = Combat;
	Entry.Role = Role;
	Entry.Handle = Combat->OnDefenseResolvedNative.AddUObject(this, &UCombatCaptureContactObserver::HandleDefenseResolved, Index);
}

void UCombatCaptureContactObserver::Unbind()
{
	if (Weapon.IsValid()) { Weapon->OnWeaponHit.RemoveDynamic(this, &UCombatCaptureContactObserver::HandleWeaponHit); }
	if (Paired.IsValid()) { Paired->OnPairedAnimationSyncPoint.RemoveDynamic(this, &UCombatCaptureContactObserver::HandlePairedSyncPoint); }
	for (const FDefender& Defender : Defenders)
	{
		if (UCombatComponent* Combat = Defender.Combat.Get()) { Combat->OnDefenseResolvedNative.Remove(Defender.Handle); }
	}
	Defenders.Reset();
	AttackerRoles.Reset();
	Weapon.Reset();
	Paired.Reset();
	Session = nullptr;
}

TSharedPtr<FJsonObject> UCombatCaptureContactObserver::BasePayload(const FString& Hit, const FString& Source,
	const FString& Attacker, const FString& Victim) const
{
	auto Payload = MakeShared<FJsonObject>();
	Payload->SetStringField(TEXT("stage"), TEXT("contact"));
	Payload->SetStringField(TEXT("hit"), Hit);
	Payload->SetStringField(TEXT("attacker"), Attacker);
	Payload->SetStringField(TEXT("victim"), Victim);
	Payload->SetStringField(TEXT("source"), Source);
	return Payload;
}

void UCombatCaptureContactObserver::HandleWeaponHit(AActor* HitActor, const FHitResult& HitResult, UAttackData*)
{
	if (!Session || !Session->IsRecording() || HitActor != VictimActor.Get()) { return; }
	++WeaponContacts;
	auto Payload = BasePayload(FString::Printf(TEXT("weapon-%d"), WeaponContacts), TEXT("weapon_trace"), AttackerId, VictimId);
	Payload->SetStringField(TEXT("region"), HitResult.BoneName.ToString());
	Payload->SetArrayField(TEXT("impact_cm"), VectorField(HitResult.ImpactPoint));
	// The impact normal faces the weapon; the strike travels the other way.
	const FVector Direction = -HitResult.ImpactNormal.GetSafeNormal();
	if (!Direction.IsNearlyZero()) { Payload->SetArrayField(TEXT("direction_cm"), VectorField(Direction)); }
	Session->Mark(TEXT("contact"), Payload);
}

void UCombatCaptureContactObserver::HandlePairedSyncPoint(EPairedReactionType, FName SyncPointName)
{
	if (!Session || !Session->IsRecording() || !AttackerActor.IsValid() || !VictimActor.IsValid()) { return; }
	++PairedContacts;
	auto Payload = BasePayload(FString::Printf(TEXT("paired-%d"), PairedContacts), TEXT("paired_sync"), AttackerId, VictimId);
	Payload->SetStringField(TEXT("region"), TEXT(""));
	Payload->SetStringField(TEXT("sync_point"), SyncPointName.ToString());
	const FVector Direction = (VictimActor->GetActorLocation() - AttackerActor->GetActorLocation()).GetSafeNormal();
	if (!Direction.IsNearlyZero()) { Payload->SetArrayField(TEXT("direction_cm"), VectorField(Direction)); }
	Session->Mark(TEXT("contact"), Payload);
}

void UCombatCaptureContactObserver::HandleDefenseResolved(const FDefenseResolution& Resolution, int32 DefenderIndex)
{
	if (!Session || !Session->IsRecording() || !Defenders.IsValidIndex(DefenderIndex)) { return; }
	AActor* Source = Resolution.Decision.AttackInstance.Attacker.Get();
	if (!Source && Resolution.bHasActualContact) { Source = Resolution.ActualContact.HitInfo.Attacker; }
	const FString* Attacker = Source ? AttackerRoles.Find(Source) : nullptr;
	const FString Victim = Defenders[DefenderIndex].Role;
	if (!Attacker || *Attacker == Victim) { return; }
	const EDefenseOutcome Outcome = Resolution.Decision.Outcome;
	// Ignored resolutions (friendly, invulnerable, consumed, invalid) had no gameplay effect, and an
	// input-stage resolution, such as a perfect parry on the guard press, has no physical contact
	// yet. Both keep their own labels so contact consumers never read them as strikes.
	const bool bIgnored = Outcome == EDefenseOutcome::Rejected || Outcome == EDefenseOutcome::IgnoredFriendly
		|| Outcome == EDefenseOutcome::IgnoredInvulnerable || Outcome == EDefenseOutcome::IgnoredConsumed
		|| Outcome == EDefenseOutcome::IgnoredInvalid;
	const bool bContact = Resolution.bHasActualContact && Resolution.ActualContact.bIsValid;
	const FString HitLabel = bIgnored ? FString::Printf(TEXT("ignored-%d"), ++IgnoredContacts)
		: FString::Printf(TEXT("committed-%d"), ++CommittedContacts);
	auto Payload = BasePayload(HitLabel, TEXT("committed_contact"), *Attacker, Victim);
	Payload->SetStringField(TEXT("stage"), bIgnored ? TEXT("ignored") : bContact ? TEXT("contact") : TEXT("defense"));
	Payload->SetStringField(TEXT("outcome"), EnumName(Outcome));
	Payload->SetStringField(TEXT("query_stage"), EnumName(Resolution.Stage));
	Payload->SetStringField(TEXT("attacker_response"), EnumName(Resolution.Decision.AttackerResponse));
	Payload->SetStringField(TEXT("damage_disposition"), EnumName(Resolution.Decision.DamageDisposition));
	Payload->SetStringField(TEXT("region"), TEXT(""));
	if (bContact)
	{
		const FActualDefenseContact& Contact = Resolution.ActualContact;
		const FHitReactionInfo& Hit = Contact.HitInfo;
		Payload->SetStringField(TEXT("region"), (Contact.ResolvedTargetBone.IsNone() ? Hit.BoneName : Contact.ResolvedTargetBone).ToString());
		Payload->SetArrayField(TEXT("impact_cm"), VectorField(Hit.ImpactPoint));
		// Strike travel: the classified contact trajectory, else the weapon velocity, else away from the attacker.
		FVector Direction = Contact.IncomingTrajectory.GetSafeNormal();
		const TCHAR* DirectionSource = TEXT("incoming_trajectory");
		if (Direction.IsNearlyZero()) { Direction = Hit.WeaponVelocity.GetSafeNormal(); DirectionSource = TEXT("weapon_velocity"); }
		if (Direction.IsNearlyZero()) { Direction = -Hit.DirectionToAttacker.GetSafeNormal(); DirectionSource = TEXT("away_from_attacker"); }
		if (!Direction.IsNearlyZero())
		{
			Payload->SetArrayField(TEXT("direction_cm"), VectorField(Direction));
			Payload->SetStringField(TEXT("direction_source"), DirectionSource);
		}
	}
	Session->Mark(bIgnored ? TEXT("contact_ignored") : bContact ? TEXT("contact") : TEXT("defense"), Payload);
}
