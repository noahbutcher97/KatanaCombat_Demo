// Copyright Epic Games, Inc. All Rights Reserved.
#include "Misc/AutomationTest.h"
#include "Analysis/CombatCaptureSession.h"
#include "CombatTestHelpers.h"
#include "Characters/EnemyCharacter.h"
#include "Characters/PlayerCharacter.h"
#include "Core/CombatComponent.h"
#include "Core/WeaponComponent.h"
#include "Data/AttackData.h"
#include "Dom/JsonObject.h"
#include "Engine/World.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonSerializer.h"

namespace
{
TArray<TSharedPtr<FJsonObject>> ReadCaptureMarkers(const FString& Directory, const FString& Label)
{
	TArray<FString> Lines;
	TArray<TSharedPtr<FJsonObject>> Rows;
	FFileHelper::LoadFileToStringArray(Lines, *(Directory / TEXT("markers.jsonl")));
	for (const FString& Line : Lines)
	{
		TSharedPtr<FJsonObject> Row;
		if (FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Line), Row) && Row.IsValid()
			&& Row->GetStringField(TEXT("marker")) == Label)
		{
			Rows.Add(Row);
		}
	}
	return Rows;
}

/** A weapon sweep result against a character, as UWeaponComponent's trace produces it. */
FHitResult CharacterWeaponHit(AActor* Target, const FVector& SourceLocation)
{
	FHitResult Hit;
	Hit.HitObjectHandle = FActorInstanceHandle(Target);
	Hit.TraceStart = SourceLocation;
	Hit.TraceEnd = Target ? Target->GetActorLocation() : SourceLocation + FVector::ForwardVector;
	Hit.ImpactPoint = Hit.TraceEnd;
	Hit.ImpactNormal = FVector::BackwardVector;
	Hit.BoneName = TEXT("spine_03");
	return Hit;
}

FCombatCaptureParticipant CaptureRole(const TCHAR* Role, AActor* Actor)
{
	FCombatCaptureParticipant Participant;
	Participant.Role = Role;
	Participant.Actor = Actor;
	return Participant;
}
} // namespace

/**
 * Character targets take the weapon's rich defense-contact path, which never broadcasts
 * UWeaponComponent::OnWeaponHit. The capture must still mark every landed hit, with its outcome.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatCaptureCommittedContactTest,
	"KatanaCombat.Capture.CommittedContactMarkers", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCombatCaptureCommittedContactTest::RunTest(const FString&)
{
	UWorld* World = FCombatTestHelpers::CreateTestWorld();
	World->WorldType = EWorldType::PIE; // The recorder observes PIE worlds only.
	APlayerCharacter* Player = FCombatTestHelpers::CreateTestPlayerCharacter(World, FVector(100.0f, 0.0f, 0.0f));
	AEnemyCharacter* Enemy = FCombatTestHelpers::CreateTestEnemyCharacter(World);
	UAttackData* Attack = FCombatTestHelpers::CreateTestAttack();
	Attack->BaseDamage = 20.0f;
	Attack->MaxHitCount = 1;

	FCombatCaptureSettings Settings;
	Settings.Scenario = TEXT("CommittedContactMarkers");
	Settings.FrameHz = 0;
	FCombatCaptureSession Capture;
	FString Error;
	if (!TestTrue(TEXT("Capture starts"), Capture.Start(World, Settings, {CaptureRole(TEXT("Attacker"), Player), CaptureRole(TEXT("Victim"), Enemy)}, Error)))
	{
		AddError(Error);
		FCombatTestHelpers::DestroyTestWorld(World);
		return false;
	}
	TestTrue(TEXT("Contact observer binds"), Capture.ObserveContacts(Player, TEXT("Attacker"), Enemy, TEXT("Victim"), Error));

	const float HealthBefore = Enemy->CurrentHealth;
	Player->WeaponComponent->ProcessHitForTesting(CharacterWeaponHit(Enemy, Player->GetActorLocation()), Attack);
	TestTrue(TEXT("The weapon hit landed through the character contact path"), Enemy->CurrentHealth < HealthBefore);
	// Markers are written when marked; no world tick is needed. Ticking this begun-play fixture
	// world would also let TextureShare's world subsystem log an unrelated error at teardown.
	TestTrue(TEXT("Capture exports"), Capture.Stop(TEXT("contacts_observed"), Error));

	const TArray<TSharedPtr<FJsonObject>> Contacts = ReadCaptureMarkers(Capture.GetOutputDirectory(), TEXT("contact"));
	if (TestEqual(TEXT("A landed character hit writes exactly one contact marker"), Contacts.Num(), 1))
	{
		const TSharedPtr<FJsonObject>* Payload = nullptr;
		if (TestTrue(TEXT("Contact marker carries a payload"), Contacts[0]->TryGetObjectField(TEXT("payload"), Payload)))
		{
			const TSharedPtr<FJsonObject>& Fields = *Payload;
			TestEqual(TEXT("Reaction-review stage"), Fields->GetStringField(TEXT("stage")), FString(TEXT("contact")));
			TestEqual(TEXT("Attacker role"), Fields->GetStringField(TEXT("attacker")), FString(TEXT("Attacker")));
			TestEqual(TEXT("Victim role"), Fields->GetStringField(TEXT("victim")), FString(TEXT("Victim")));
			TestEqual(TEXT("Committed outcome is recorded"), Fields->GetStringField(TEXT("outcome")), FString(TEXT("Hit")));
			TestFalse(TEXT("Hit identity is recorded"), Fields->GetStringField(TEXT("hit")).IsEmpty());
			const TArray<TSharedPtr<FJsonValue>>* Direction = nullptr;
			TestTrue(TEXT("Strike direction is a vector"), Fields->TryGetArrayField(TEXT("direction_cm"), Direction) && Direction->Num() == 3);
		}
	}
	int32 WeaponContacts = 0, PairedContacts = 0;
	Capture.GetContactCounts(WeaponContacts, PairedContacts);
	TestEqual(TEXT("A character target never reaches the weapon-trace event"), WeaponContacts, 0);
	TestEqual(TEXT("The committed contact is counted after stop"), Capture.GetCommittedContactCount(), 1);

	// The console recorder observes every recorded pair in either direction; a block keeps its outcome.
	Enemy->SetActorLocationAndRotation(FVector(100.0f, 0.0f, 0.0f), FRotator(0.0f, 180.0f, 0.0f));
	Player->SetActorLocationAndRotation(FVector::ZeroVector, FRotator::ZeroRotator);
	UAttackData* EnemyAttack = FCombatTestHelpers::CreateTestAttack();
	EnemyAttack->BaseDamage = 30.0f;
	if (!TestTrue(TEXT("Participant capture starts"), Capture.Start(World, Settings, {CaptureRole(TEXT("Player1"), Player), CaptureRole(TEXT("Character1"), Enemy)}, Error)))
	{
		AddError(Error);
		FCombatTestHelpers::DestroyTestWorld(World);
		return false;
	}
	TestTrue(TEXT("Participant contact observer binds"), Capture.ObserveParticipantContacts(Error));
	TestTrue(TEXT("Player enters held guard"), Player->CombatComponent->BeginBlock(Enemy));
	const float GuardedHealth = Player->CurrentHealth;
	Enemy->WeaponComponent->ProcessHitForTesting(CharacterWeaponHit(Player, Enemy->GetActorLocation()), EnemyAttack);
	TestEqual(TEXT("The guard absorbed the strike"), Player->CurrentHealth, GuardedHealth);
	TestTrue(TEXT("Participant capture exports"), Capture.Stop(TEXT("block_observed"), Error));
	const TArray<TSharedPtr<FJsonObject>> Blocks = ReadCaptureMarkers(Capture.GetOutputDirectory(), TEXT("contact"));
	if (TestEqual(TEXT("A blocked strike writes exactly one contact marker"), Blocks.Num(), 1))
	{
		const TSharedPtr<FJsonObject>& Fields = Blocks[0]->GetObjectField(TEXT("payload"));
		TestEqual(TEXT("Enemy is the attacker"), Fields->GetStringField(TEXT("attacker")), FString(TEXT("Character1")));
		TestEqual(TEXT("Player is the defender"), Fields->GetStringField(TEXT("victim")), FString(TEXT("Player1")));
		TestEqual(TEXT("Block outcome is recorded"), Fields->GetStringField(TEXT("outcome")), FString(TEXT("NormalBlock")));
		TestEqual(TEXT("Committed contact source"), Fields->GetStringField(TEXT("source")), FString(TEXT("committed_contact")));
	}

	World->DestroyActor(Player);
	World->DestroyActor(Enemy);
	FCombatTestHelpers::DestroyTestWorld(World);
	return true;
}
