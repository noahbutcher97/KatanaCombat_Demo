// Copyright Epic Games, Inc. All Rights Reserved.
#include "Analysis/CombatCaptureContactObserver.h"
#include "Analysis/CombatCaptureSession.h"
#include "Characters/BaseCombatCharacter.h"
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
}

void UCombatCaptureContactObserver::Unbind()
{
	if (Weapon.IsValid()) { Weapon->OnWeaponHit.RemoveDynamic(this, &UCombatCaptureContactObserver::HandleWeaponHit); }
	if (Paired.IsValid()) { Paired->OnPairedAnimationSyncPoint.RemoveDynamic(this, &UCombatCaptureContactObserver::HandlePairedSyncPoint); }
	Weapon.Reset();
	Paired.Reset();
	Session = nullptr;
}

TSharedPtr<FJsonObject> UCombatCaptureContactObserver::BasePayload(const FString& Hit, const FString& Source) const
{
	auto Payload = MakeShared<FJsonObject>();
	Payload->SetStringField(TEXT("stage"), TEXT("contact"));
	Payload->SetStringField(TEXT("hit"), Hit);
	Payload->SetStringField(TEXT("attacker"), AttackerId);
	Payload->SetStringField(TEXT("victim"), VictimId);
	Payload->SetStringField(TEXT("source"), Source);
	return Payload;
}

void UCombatCaptureContactObserver::HandleWeaponHit(AActor* HitActor, const FHitResult& HitResult, UAttackData*)
{
	if (!Session || !Session->IsRecording() || HitActor != VictimActor.Get()) { return; }
	++WeaponContacts;
	auto Payload = BasePayload(FString::Printf(TEXT("weapon-%d"), WeaponContacts), TEXT("weapon_trace"));
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
	auto Payload = BasePayload(FString::Printf(TEXT("paired-%d"), PairedContacts), TEXT("paired_sync"));
	Payload->SetStringField(TEXT("region"), TEXT(""));
	Payload->SetStringField(TEXT("sync_point"), SyncPointName.ToString());
	const FVector Direction = (VictimActor->GetActorLocation() - AttackerActor->GetActorLocation()).GetSafeNormal();
	if (!Direction.IsNearlyZero()) { Payload->SetArrayField(TEXT("direction_cm"), VectorField(Direction)); }
	Session->Mark(TEXT("contact"), Payload);
}
