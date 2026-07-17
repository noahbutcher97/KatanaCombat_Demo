#include "Misc/AutomationTest.h"

#include "Animation/AnimComposite.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimNotify_ChainStageTransition.h"
#include "Animation/AnimNotifyState_ParryWindow.h"
#include "Animation/AnimNotifyState_PairedAnimationCollision.h"
#include "Animation/AnimNotifyState_PairedAnimationSync.h"
#include "AnimNotifyState_MotionWarping.h"
#include "Data/AttackData.h"
#include "Data/CombatSettings.h"
#include "Data/DefenseConfiguration.h"
#include "Data/PairedAnimationData.h"
#include "DefenseAssetValidationService.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "NiagaraSystem.h"
#include "RootMotionModifier_SkewWarp.h"
#include "Sound/SoundWave.h"
#include "Utilities/CombatGameplayTags.h"

namespace DefenseAssetValidationTests
{
FString ValidManifest()
{
	return TEXT(R"json({
  "schemaVersion": 2,
  "gate": "A",
  "map": "/Game/ProjectFiles/Levels/Lvl_ThirdPerson1.Lvl_ThirdPerson1",
  "defenseConfiguration": "/Game/ProjectFiles/Data/Defense/DA_Defense.DA_Defense",
  "fixture": {
    "playerBlueprint": "/Game/ProjectFiles/Blueprints/BP_Player.BP_Player",
    "playerCombatSettings": "/Game/ProjectFiles/Data/Combat/DA_PlayerCombat.DA_PlayerCombat",
    "enemyBlueprints": ["/Game/ProjectFiles/Blueprints/BP_Enemy.BP_Enemy"],
    "enemyCombatSettings": ["/Game/ProjectFiles/Data/Combat/DA_EnemyCombat.DA_EnemyCombat"],
    "inputAction": "/Game/ProjectFiles/Input/Actions/IA_Block.IA_Block",
    "inputMappingContext": "/Game/ProjectFiles/Input/IMC_Combat.IMC_Combat",
    "blockKey": "ThumbMouseButton",
    "guardAnimBlueprint": "/Game/ProjectFiles/Animation/ABP_Player.ABP_Player",
    "reviewed": true
  },
  "combatSettings": [
    "/Game/ProjectFiles/Data/Combat/DA_PlayerCombat.DA_PlayerCombat",
    "/Game/ProjectFiles/Data/Combat/DA_EnemyCombat.DA_EnemyCombat"
  ],
  "supportingAssets": [
    "/Game/ProjectFiles/Audio/SW_DefenseImpact.SW_DefenseImpact"
  ],
  "proofCases": ["PerfectParryMiddleCenter"],
  "attacks": [{
    "name": "LightAttack_1",
    "attackData": "/Game/ProjectFiles/Data/PDA/Attack/AttackData/Light/New/LightAttack_1.LightAttack_1",
    "montage": "/Game/ProjectFiles/Animation/Montages/Katana/Light/AM_Light_Combo_1.AM_Light_Combo_1",
    "section": "Attack_1",
    "expectedHeight": "Middle",
    "expectedLane": "Center",
    "expectedSwing": "Horizontal",
    "expectedSourceSocket": "weapon_top",
    "expectedTargetBone": "spine_03",
    "requiresBlockedImpactAudio": true,
    "requiresBlockedImpactVFX": true,
    "expectedTags": ["Attack.Defense.Parryable"],
    "parryWindow": {
      "basis": "SectionRelative",
      "startSeconds": 0.20,
      "endSeconds": 0.35,
      "reviewed": true
    }
  }],
  "presentations": [{
    "name": "ParryMiddleCenter",
    "outcome": "PerfectParry",
    "attackerResponse": "ParryStagger",
    "defenderRow": "ParryMiddleCenter",
    "attackerRow": "ParryStaggerMiddleCenter",
    "requiresDefenderMontage": true,
    "requiresAttackerMontage": true,
    "requiresImpactAudio": true,
    "requiresImpactVFX": true,
    "expectedSourceSocket": "weapon_top",
    "expectedTargetBone": "spine_03",
    "reviewed": true
  }],
  "pairedDependencies": [{
    "name": "ParryBridge",
    "role": "Bridge",
    "pairedData": "/Game/ProjectFiles/Data/Paired/DA_ParryBridge.DA_ParryBridge",
    "attackerMontage": "/Game/ProjectFiles/Animation/Paired/AM_ParryBridge_Defender.AM_ParryBridge_Defender",
    "attackerSection": "Bridge",
    "victimMontage": "/Game/ProjectFiles/Animation/Paired/AM_ParryBridge_Attacker.AM_ParryBridge_Attacker",
    "victimSection": "Bridge",
    "driverRole": "Attacker",
    "driverMarker": "CounterReady",
    "attackerWarpTarget": "PairedTarget",
    "victimWarpTarget": "PairedTarget",
    "attackerReadySection": null,
    "victimReadySection": null,
    "attackerTerminalPoseCompatible": true,
    "victimTerminalPoseCompatible": true,
    "reviewed": true
  }],
  "expectedCases": [{
    "name": "PerfectParryMiddleCenter",
    "attack": "LightAttack_1",
    "outcome": "PerfectParry",
    "reason": "None",
    "attackerResponse": "ParryStagger",
    "presentation": "ParryMiddleCenter",
    "pairedDependencies": ["ParryBridge"],
    "reviewed": true
  }]
})json");
}

FString ManifestWithAmbiguousCounterAuthority()
{
	FString Json = ValidManifest();
	Json.ReplaceInline(
		TEXT("\"reviewed\": true\n  }],\n  \"expectedCases\": [{"),
		TEXT(R"json("reviewed": true
  }, {
    "name": "CounterA",
    "role": "Counter",
    "pairedData": "/Game/ProjectFiles/Data/Paired/DA_CounterA.DA_CounterA",
    "attackerMontage": "/Game/ProjectFiles/Animation/Paired/AM_CounterA_Defender.AM_CounterA_Defender",
    "attackerSection": "Counter",
    "victimMontage": "/Game/ProjectFiles/Animation/Paired/AM_CounterA_Attacker.AM_CounterA_Attacker",
    "victimSection": "Counter",
    "driverRole": "Attacker",
    "driverMarker": "CounterComplete",
    "attackerWarpTarget": "PairedTarget",
    "victimWarpTarget": "PairedTarget",
    "attackerReadySection": null,
    "victimReadySection": null,
    "attackerTerminalPoseCompatible": true,
    "victimTerminalPoseCompatible": true,
    "reviewed": true
  }, {
    "name": "CounterB",
    "role": "Counter",
    "pairedData": "/Game/ProjectFiles/Data/Paired/DA_CounterB.DA_CounterB",
    "attackerMontage": "/Game/ProjectFiles/Animation/Paired/AM_CounterB_Defender.AM_CounterB_Defender",
    "attackerSection": "Counter",
    "victimMontage": "/Game/ProjectFiles/Animation/Paired/AM_CounterB_Attacker.AM_CounterB_Attacker",
    "victimSection": "Counter",
    "driverRole": "Attacker",
    "driverMarker": "CounterComplete",
    "attackerWarpTarget": "PairedTarget",
    "victimWarpTarget": "PairedTarget",
    "attackerReadySection": null,
    "victimReadySection": null,
    "attackerTerminalPoseCompatible": true,
    "victimTerminalPoseCompatible": true,
    "reviewed": true
  }],
  "expectedCases": [{)json"));
	Json.ReplaceInline(
		TEXT("\"pairedDependencies\": [\"ParryBridge\"]"),
		TEXT("\"pairedDependencies\": [\"ParryBridge\", \"CounterA\"]"));
	Json.ReplaceInline(
		TEXT("\"expectedCases\": [{"),
		TEXT(R"json("expectedCases": [{
    "name": "PerfectParryAlternateCounter",
    "attack": "LightAttack_1",
    "outcome": "PerfectParry",
    "reason": "None",
    "attackerResponse": "ParryStagger",
    "presentation": "ParryMiddleCenter",
    "pairedDependencies": ["ParryBridge", "CounterB"],
    "reviewed": true
  }, {)json"));
	return Json;
}

bool ErrorsContain(const TArray<FString>& Errors, const FString& Needle)
{
	return Errors.ContainsByPredicate([&Needle](const FString& Error)
	{
		return Error.Contains(Needle, ESearchCase::IgnoreCase);
	});
}

UAnimMontage* CreateSectionMontage(const FName Section = TEXT("Target"))
{
	UAnimMontage* Montage = NewObject<UAnimMontage>(GetTransientPackage());
	Montage->SetCompositeLength(2.0f);
	FCompositeSection Leading;
	Leading.SectionName = TEXT("Leading");
	Leading.SetTime(0.0f);
	Montage->CompositeSections.Add(Leading);
	FCompositeSection Target;
	Target.SectionName = Section;
	Target.SetTime(0.5f);
	Montage->CompositeSections.Add(Target);
	FCompositeSection Trailing;
	Trailing.SectionName = TEXT("Trailing");
	Trailing.SetTime(1.5f);
	Montage->CompositeSections.Add(Trailing);
	return Montage;
}

void AddParryWindow(UAnimMontage* Montage, const float AbsoluteStart, const float Duration)
{
	UAnimNotifyState_ParryWindow* Notify = NewObject<UAnimNotifyState_ParryWindow>(Montage);
	FAnimNotifyEvent Event;
	Event.NotifyStateClass = Notify;
	Event.SetTime(AbsoluteStart);
	Event.SetDuration(Duration);
	Montage->Notifies.Add(Event);
}

FDefenseProofAttackEntry MakeAttackEntry()
{
	FDefenseProofAttackEntry Entry;
	Entry.Name = TEXT("TransientAttack");
	Entry.Section = TEXT("Target");
	Entry.ExpectedHeight = TEXT("Middle");
	Entry.ExpectedLane = TEXT("Center");
	Entry.ExpectedSwing = TEXT("Horizontal");
	Entry.ExpectedSourceSocket = TEXT("weapon_top");
	Entry.ExpectedTargetBone = TEXT("spine_03");
	Entry.bRequiresBlockedImpactAudio = true;
	Entry.bRequiresBlockedImpactVFX = true;
	Entry.ExpectedTags = {TEXT("Attack.Defense.Parryable")};
	Entry.ParryWindow.bPresent = true;
	Entry.ParryWindow.Basis = TEXT("SectionRelative");
	Entry.ParryWindow.StartSeconds = 0.20;
	Entry.ParryWindow.EndSeconds = 0.35;
	Entry.ParryWindow.bReviewed = true;
	return Entry;
}

UAttackData* CreateMatchingAttack(UAnimMontage* Montage)
{
	UAttackData* Attack = NewObject<UAttackData>(GetTransientPackage());
	Attack->AttackMontage = Montage;
	Attack->MontageSection = TEXT("Target");
	Attack->DefenseProfile.Height = EAttackHeight::Middle;
	Attack->DefenseProfile.NominalLane = EIncomingAttackLane::Center;
	Attack->DefenseProfile.SwingShape = ESwingDirection::Horizontal;
	Attack->DefenseProfile.SourceContactSocketOverride = TEXT("weapon_top");
	Attack->DefenseProfile.DefenderTargetBoneFallback = TEXT("spine_03");
	Attack->DefenseProfile.bOverrideBlockedImpactAudio = true;
	Attack->DefenseProfile.BlockedImpactAudio.ImpactSound = NewObject<USoundWave>(Attack);
	Attack->DefenseProfile.bOverrideBlockedImpactVFX = true;
	Attack->DefenseProfile.BlockedImpactVFX.ImpactVFX = NewObject<UNiagaraSystem>(Attack);
	Attack->AttackTags.AddTag(KatanaCombatGameplayTags::AttackDefenseParryable());
	return Attack;
}

FDefenseProofPresentationEntry MakePresentationEntry()
{
	FDefenseProofPresentationEntry Entry;
	Entry.Name = TEXT("NormalBlockMiddleCenter");
	Entry.Outcome = TEXT("NormalBlock");
	Entry.AttackerResponse = TEXT("Recoil");
	Entry.DefenderRow = TEXT("BlockExact");
	Entry.bHasDefenderRow = true;
	Entry.AttackerRow = TEXT("RecoilExact");
	Entry.bHasAttackerRow = true;
	Entry.bRequiresDefenderMontage = true;
	Entry.bRequiresAttackerMontage = true;
	Entry.bRequiresImpactAudio = true;
	Entry.bRequiresImpactVFX = true;
	Entry.ExpectedSourceSocket = TEXT("weapon_top");
	Entry.ExpectedTargetBone = TEXT("spine_03");
	Entry.bReviewed = true;
	return Entry;
}

FDefensePresentationPayload MakeVisiblePayload(UObject* Outer)
{
	FDefensePresentationPayload Payload;
	Payload.Montage = CreateSectionMontage(TEXT("React"));
	Payload.MontageSection = TEXT("React");
	Payload.bOverrideImpactAudio = true;
	Payload.ImpactAudio.ImpactSound = NewObject<USoundWave>(Outer);
	Payload.bOverrideImpactVFX = true;
	Payload.ImpactVFX.ImpactVFX = NewObject<UNiagaraSystem>(Outer);
	Payload.SourceSocketOverride = TEXT("weapon_top");
	Payload.TargetBoneOverride = TEXT("spine_03");
	return Payload;
}

FDefensePresentationRow MakeDefenderRow(
	UObject* Outer,
	const FName Name,
	const bool bGeneric)
{
	FDefensePresentationRow Row;
	Row.RowName = Name;
	Row.Outcome = EDefenseOutcome::NormalBlock;
	Row.bMatchAnyHeight = bGeneric;
	Row.Height = EAttackHeight::Middle;
	Row.bMatchAnyLane = bGeneric;
	Row.Lane = EIncomingAttackLane::Center;
	Row.bMatchAnySwingShape = bGeneric;
	Row.SwingShape = ESwingDirection::Horizontal;
	Row.Payload = MakeVisiblePayload(Outer);
	return Row;
}

FAttackerResponsePresentationRow MakeAttackerRow(
	UObject* Outer,
	const FName Name,
	const bool bGeneric)
{
	FAttackerResponsePresentationRow Row;
	Row.RowName = Name;
	Row.Response = EAttackerResponse::Recoil;
	Row.bMatchAnyHeight = bGeneric;
	Row.Height = EAttackHeight::Middle;
	Row.bMatchAnyLane = bGeneric;
	Row.Lane = EIncomingAttackLane::Center;
	Row.bMatchAnySwingShape = bGeneric;
	Row.SwingShape = ESwingDirection::Horizontal;
	Row.Payload = MakeVisiblePayload(Outer);
	return Row;
}

template <typename NotifyType>
void AddStateNotify(UAnimMontage* Montage, const float Start = 0.60f, const float Duration = 0.50f)
{
	NotifyType* Notify = NewObject<NotifyType>(Montage);
	FAnimNotifyEvent Event;
	Event.NotifyStateClass = Notify;
	Event.SetTime(Start);
	Event.SetDuration(Duration);
	Montage->Notifies.Add(Event);
}

void AddChainMarker(
	UAnimMontage* Montage,
	const FName Marker,
	const EChainStageTransitionType Transition,
	const float Time = 1.0f)
{
	UAnimNotify_ChainStageTransition* Notify = NewObject<UAnimNotify_ChainStageTransition>(Montage);
	Notify->MarkerName = Marker;
	Notify->Transition = Transition;
	FAnimNotifyEvent Event;
	Event.Notify = Notify;
	Event.SetTime(Time);
	Montage->Notifies.Add(Event);
}

void AddRotationWarp(UAnimMontage* Montage, const FName Target)
{
	UAnimNotifyState_MotionWarping* Notify = NewObject<UAnimNotifyState_MotionWarping>(Montage);
	URootMotionModifier_SkewWarp* Modifier = NewObject<URootMotionModifier_SkewWarp>(Notify);
	Modifier->WarpTargetName = Target;
	Modifier->bWarpRotation = true;
	Modifier->bWarpTranslation = true;
	Notify->RootMotionModifier = Modifier;
	FAnimNotifyEvent Event;
	Event.NotifyStateClass = Notify;
	Event.SetTime(0.55f);
	Event.SetDuration(0.90f);
	Montage->Notifies.Add(Event);
}

FDefenseProofPairedDependencyEntry MakeBridgeEntry()
{
	FDefenseProofPairedDependencyEntry Entry;
	Entry.Name = TEXT("Bridge");
	Entry.Role = TEXT("Bridge");
	Entry.AttackerSection = TEXT("Bridge");
	Entry.VictimSection = TEXT("Bridge");
	Entry.DriverRole = TEXT("Attacker");
	Entry.bHasDriverRole = true;
	Entry.DriverMarker = TEXT("CounterReady");
	Entry.bHasDriverMarker = true;
	Entry.AttackerWarpTarget = TEXT("PairedTarget");
	Entry.VictimWarpTarget = TEXT("PairedTarget");
	Entry.bAttackerTerminalPoseCompatible = true;
	Entry.bVictimTerminalPoseCompatible = true;
	Entry.bReviewed = true;
	return Entry;
}

UPairedAnimationData* CreateBridgeData()
{
	UPairedAnimationData* Data = NewObject<UPairedAnimationData>(GetTransientPackage());
	Data->ReactionType = EPairedReactionType::Parry;
	Data->AttackerMontage = CreateSectionMontage(TEXT("Bridge"));
	Data->AttackerMontageSection = TEXT("Bridge");
	Data->VictimMontage = CreateSectionMontage(TEXT("Bridge"));
	Data->VictimMontageSection = TEXT("Bridge");
	Data->AttackerWarpConfig.WarpTargetName = TEXT("PairedTarget");
	Data->AttackerWarpConfig.bWarpRotation = true;
	Data->VictimWarpConfig.WarpTargetName = TEXT("PairedTarget");
	Data->VictimWarpConfig.bWarpRotation = true;
	Data->ChainTransitionPolicy.DriverRole = EPairedAnimationRole::Attacker;
	Data->ChainTransitionPolicy.RequiredMarker = TEXT("CounterReady");
	Data->ChainTransitionPolicy.bAttackerTerminalPoseCompatible = true;
	Data->ChainTransitionPolicy.bVictimTerminalPoseCompatible = true;
	AddChainMarker(Data->AttackerMontage, TEXT("CounterReady"),
		EChainStageTransitionType::OpenCounterWindow);
	AddStateNotify<UAnimNotifyState_PairedAnimationSync>(Data->AttackerMontage);
	AddStateNotify<UAnimNotifyState_PairedAnimationCollision>(Data->AttackerMontage);
	AddStateNotify<UAnimNotifyState_PairedAnimationCollision>(Data->VictimMontage);
	AddRotationWarp(Data->AttackerMontage, TEXT("PairedTarget"));
	AddRotationWarp(Data->VictimMontage, TEXT("PairedTarget"));
	return Data;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDefenseManifestValidSchemaTest,
	"KatanaCombat.Editor.DefenseValidation.Manifest.ValidSchema",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FDefenseManifestValidSchemaTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	FDefenseProofManifest Manifest;
	TArray<FString> Errors;
	TestTrue(TEXT("Reviewed schema-v2 manifest should parse"),
		FDefenseAssetValidationService::ParseManifestJson(
			DefenseAssetValidationTests::ValidManifest(), Manifest, Errors));
	TestEqual(TEXT("Parse should not emit errors"), Errors.Num(), 0);
	TestEqual(TEXT("Gate should parse"), Manifest.Gate, FString(TEXT("A")));
	TestEqual(TEXT("One attack should parse"), Manifest.Attacks.Num(), 1);
	TestEqual(TEXT("One expected case should parse"), Manifest.ExpectedCases.Num(), 1);
	TestEqual(TEXT("One proof case should parse"), Manifest.ProofCases.Num(), 1);
	TestEqual(TEXT("Reviewed timing should parse"), Manifest.Attacks[0].ParryWindow.StartSeconds, 0.20);
	TestTrue(TEXT("Parry timing should be present"), Manifest.Attacks[0].ParryWindow.bPresent);
	TestEqual(TEXT("Swing should parse"), Manifest.Attacks[0].ExpectedSwing, FString(TEXT("Horizontal")));
	TestEqual(TEXT("Fixture block key should parse"), Manifest.Fixture.BlockKey, FString(TEXT("ThumbMouseButton")));
	TestEqual(TEXT("Attacker response should parse"), Manifest.ExpectedCases[0].AttackerResponse,
		FString(TEXT("ParryStagger")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDefenseManifestProofCaseLedgerTest,
	"KatanaCombat.Editor.DefenseValidation.Manifest.ProofCaseLedgerRequired",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FDefenseManifestProofCaseLedgerTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	FDefenseProofManifest Manifest;
	TArray<FString> Errors;
	FString EmptyLedger = DefenseAssetValidationTests::ValidManifest();
	EmptyLedger.ReplaceInline(
		TEXT("\"proofCases\": [\"PerfectParryMiddleCenter\"]"),
		TEXT("\"proofCases\": []"));
	TestFalse(TEXT("A proof manifest must declare at least one runtime proof case"),
		FDefenseAssetValidationService::ParseManifestJson(EmptyLedger, Manifest, Errors));
	TestTrue(TEXT("An empty proof ledger should identify the non-empty contract"),
		DefenseAssetValidationTests::ErrorsContain(Errors, TEXT("non-empty")));

	FString DuplicateLedger = DefenseAssetValidationTests::ValidManifest();
	DuplicateLedger.ReplaceInline(
		TEXT("\"proofCases\": [\"PerfectParryMiddleCenter\"]"),
		TEXT("\"proofCases\": [\"PerfectParryMiddleCenter\", \"PerfectParryMiddleCenter\"]"));
	Errors.Reset();
	TestFalse(TEXT("A proof manifest must reject duplicate runtime proof cases"),
		FDefenseAssetValidationService::ParseManifestJson(DuplicateLedger, Manifest, Errors));
	TestTrue(TEXT("A duplicate proof case should be named"),
		DefenseAssetValidationTests::ErrorsContain(Errors, TEXT("duplicate")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDefenseManifestInputAndTagAuthorityTest,
	"KatanaCombat.Editor.DefenseValidation.Manifest.InputAndTagAuthorityRequired",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FDefenseManifestInputAndTagAuthorityTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	FDefenseProofManifest Manifest;
	TArray<FString> Errors;

	FString InvalidKeyJson = DefenseAssetValidationTests::ValidManifest();
	InvalidKeyJson.ReplaceInline(TEXT("\"blockKey\": \"ThumbMouseButton\""),
		TEXT("\"blockKey\": \"RightMouseButton\""));
	TestFalse(TEXT("Defense proof must not reuse an attack mouse button"),
		FDefenseAssetValidationService::ParseManifestJson(
			InvalidKeyJson, Manifest, Errors));
	TestTrue(TEXT("Rejected block key should identify the thumb-button contract"),
		DefenseAssetValidationTests::ErrorsContain(Errors, TEXT("thumb mouse")));

	FString InvalidTagJson = DefenseAssetValidationTests::ValidManifest();
	InvalidTagJson.ReplaceInline(TEXT("Attack.Defense.Parryable"),
		TEXT("Attack.Defense.UnregisteredProofTag"));
	Errors.Reset();
	TestFalse(TEXT("Defense proof must reject unregistered gameplay tags"),
		FDefenseAssetValidationService::ParseManifestJson(
			InvalidTagJson, Manifest, Errors));
	TestTrue(TEXT("Rejected tag should identify registration"),
		DefenseAssetValidationTests::ErrorsContain(Errors, TEXT("registered")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDefenseManifestMalformedJsonTest,
	"KatanaCombat.Editor.DefenseValidation.Manifest.MalformedJsonRejected",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FDefenseManifestMalformedJsonTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	FDefenseProofManifest Manifest;
	TArray<FString> Errors;
	TestFalse(TEXT("Malformed JSON should fail"),
		FDefenseAssetValidationService::ParseManifestJson(TEXT("{\"schemaVersion\":"), Manifest, Errors));
	TestTrue(TEXT("Failure should explain malformed JSON"),
		DefenseAssetValidationTests::ErrorsContain(Errors, TEXT("JSON")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDefenseManifestUnknownFieldTest,
	"KatanaCombat.Editor.DefenseValidation.Manifest.UnknownFieldsRejected",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FDefenseManifestUnknownFieldTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	FString Json = DefenseAssetValidationTests::ValidManifest();
	Json.ReplaceInline(TEXT("\"schemaVersion\": 2,"), TEXT("\"schemaVersion\": 2, \"unexpected\": true,"));
	Json.ReplaceInline(TEXT("\"name\": \"LightAttack_1\","),
		TEXT("\"name\": \"LightAttack_1\", \"nestedUnexpected\": 3,"));
	FDefenseProofManifest Manifest;
	TArray<FString> Errors;
	TestFalse(TEXT("Unknown top-level and nested fields should fail"),
		FDefenseAssetValidationService::ParseManifestJson(Json, Manifest, Errors));
	TestTrue(TEXT("Top-level field should be named"),
		DefenseAssetValidationTests::ErrorsContain(Errors, TEXT("unexpected")));
	TestTrue(TEXT("Nested field should be named"),
		DefenseAssetValidationTests::ErrorsContain(Errors, TEXT("nestedUnexpected")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDefenseManifestReviewAndTimingTest,
	"KatanaCombat.Editor.DefenseValidation.Manifest.ReviewAndTimingRequired",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FDefenseManifestReviewAndTimingTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	FString Json = DefenseAssetValidationTests::ValidManifest();
	Json.ReplaceInline(
		TEXT("\"startSeconds\": 0.20,\n      \"endSeconds\": 0.35,\n      \"reviewed\": true"),
		TEXT("\"startSeconds\": 0.35,\n      \"endSeconds\": 0.20,\n      \"reviewed\": false"));
	FDefenseProofManifest Manifest;
	TArray<FString> Errors;
	TestFalse(TEXT("Unreviewed or nonpositive timing should fail"),
		FDefenseAssetValidationService::ParseManifestJson(Json, Manifest, Errors));
	TestTrue(TEXT("Review failure should be explicit"),
		DefenseAssetValidationTests::ErrorsContain(Errors, TEXT("reviewed")));
	TestTrue(TEXT("Timing order failure should be explicit"),
		DefenseAssetValidationTests::ErrorsContain(Errors, TEXT("endSeconds")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDefenseManifestReferencesAndDuplicatesTest,
	"KatanaCombat.Editor.DefenseValidation.Manifest.ReferencesAndDuplicatesRejected",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FDefenseManifestReferencesAndDuplicatesTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	FString Json = DefenseAssetValidationTests::ValidManifest();
	Json.ReplaceInline(
		TEXT("/Game/ProjectFiles/Levels/Lvl_ThirdPerson1.Lvl_ThirdPerson1"),
		TEXT("C:/External/Lvl_ThirdPerson1"));
	Json.ReplaceInline(
		TEXT("\"expectedCases\": [{"),
		TEXT("\"expectedCases\": [{\"name\":\"PerfectParryMiddleCenter\",\"attack\":\"LightAttack_1\",\"outcome\":\"PerfectParry\",\"reason\":\"None\",\"attackerResponse\":\"ParryStagger\",\"presentation\":\"ParryMiddleCenter\",\"pairedDependencies\":[\"ParryBridge\"],\"reviewed\":true},{"));
	FDefenseProofManifest Manifest;
	TArray<FString> Errors;
	TestFalse(TEXT("External paths and duplicate case names should fail"),
		FDefenseAssetValidationService::ParseManifestJson(Json, Manifest, Errors));
	TestTrue(TEXT("Path boundary should be explicit"),
		DefenseAssetValidationTests::ErrorsContain(Errors, TEXT("/Game")));
	TestTrue(TEXT("Duplicate case should be explicit"),
		DefenseAssetValidationTests::ErrorsContain(Errors, TEXT("duplicate")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDefenseManifestMutationTargetUniquenessTest,
	"KatanaCombat.Editor.DefenseValidation.Manifest.MutationTargetsMustBeUnique",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FDefenseManifestMutationTargetUniquenessTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	FString Json = DefenseAssetValidationTests::ValidManifest();
	Json.ReplaceInline(
		TEXT("\"attacks\": [{"),
		TEXT(R"json("attacks": [{
    "name": "DuplicateLightAttack_1",
    "attackData": "/Game/ProjectFiles/Data/PDA/Attack/AttackData/Light/New/LightAttack_1.LightAttack_1",
    "montage": "/Game/ProjectFiles/Animation/Montages/Katana/Light/AM_Light_Combo_1.AM_Light_Combo_1",
    "section": "Attack_1",
    "expectedHeight": "Middle",
    "expectedLane": "Center",
    "expectedSwing": "Horizontal",
    "expectedSourceSocket": "weapon_top",
    "expectedTargetBone": "spine_03",
    "requiresBlockedImpactAudio": true,
    "requiresBlockedImpactVFX": true,
    "expectedTags": ["Attack.Defense.Parryable"],
    "parryWindow": {
      "basis": "SectionRelative",
      "startSeconds": 0.20,
      "endSeconds": 0.35,
      "reviewed": true
    }
  }, {)json"));
	FDefenseProofManifest Manifest;
	TArray<FString> Errors;
	TestFalse(TEXT("Mutation targets must not be order-dependent"),
		FDefenseAssetValidationService::ParseManifestJson(Json, Manifest, Errors));
	TestTrue(TEXT("Duplicate AttackData target should be explicit"),
		DefenseAssetValidationTests::ErrorsContain(Errors, TEXT("attackData path")));
	TestTrue(TEXT("Duplicate montage section target should be explicit"),
		DefenseAssetValidationTests::ErrorsContain(Errors, TEXT("montage/section target")));

	FString DuplicatePairedDataJson =
		DefenseAssetValidationTests::ManifestWithAmbiguousCounterAuthority();
	DuplicatePairedDataJson.ReplaceInline(
		TEXT("/Game/ProjectFiles/Data/Paired/DA_CounterB.DA_CounterB"),
		TEXT("/Game/ProjectFiles/Data/Paired/DA_CounterA.DA_CounterA"));
	Errors.Reset();
	TestFalse(TEXT("One paired asset must have one manifest definition"),
		FDefenseAssetValidationService::ParseManifestJson(
			DuplicatePairedDataJson, Manifest, Errors));
	TestTrue(TEXT("Duplicate PairedData target should be explicit"),
		DefenseAssetValidationTests::ErrorsContain(Errors, TEXT("pairedData path")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDefenseManifestNonChainCaseTest,
	"KatanaCombat.Editor.DefenseValidation.Manifest.NonChainCaseAllowsExplicitEmptyDependencies",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FDefenseManifestNonChainCaseTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	FString Json = DefenseAssetValidationTests::ValidManifest();
	Json.ReplaceInline(TEXT("\"Attack.Defense.Parryable\""), TEXT("\"Attack.Defense.BlockInterruptible\""));
	Json.ReplaceInline(
		TEXT("\"parryWindow\": {\n      \"basis\": \"SectionRelative\",\n      \"startSeconds\": 0.20,\n      \"endSeconds\": 0.35,\n      \"reviewed\": true\n    }"),
		TEXT("\"parryWindow\": null"));
	Json.ReplaceInline(TEXT("\"outcome\": \"PerfectParry\""), TEXT("\"outcome\": \"NormalBlock\""));
	Json.ReplaceInline(TEXT("\"attackerResponse\": \"ParryStagger\""), TEXT("\"attackerResponse\": \"Recoil\""));
	Json.ReplaceInline(TEXT("\"reason\": \"None\""), TEXT("\"reason\": \"None\""));
	Json.ReplaceInline(TEXT("\"pairedDependencies\": [\"ParryBridge\"]"), TEXT("\"pairedDependencies\": []"));

	FDefenseProofManifest Manifest;
	TArray<FString> Errors;
	TestTrue(TEXT("A reviewed non-chain case may name no paired dependencies"),
		FDefenseAssetValidationService::ParseManifestJson(Json, Manifest, Errors));
	TestEqual(TEXT("No parser errors expected"), Errors.Num(), 0);
	TestFalse(TEXT("No parry timing should be represented explicitly"),
		Manifest.Attacks[0].ParryWindow.bPresent);
	TestEqual(TEXT("Case should retain an explicit empty dependency set"),
		Manifest.ExpectedCases[0].PairedDependencies.Num(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDefenseManifestPerfectParryDependenciesTest,
	"KatanaCombat.Editor.DefenseValidation.Manifest.PerfectParryRequiresPairedDependencies",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FDefenseManifestPerfectParryDependenciesTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	FString Json = DefenseAssetValidationTests::ValidManifest();
	Json.ReplaceInline(TEXT("\"pairedDependencies\": [\"ParryBridge\"]"), TEXT("\"pairedDependencies\": []"));

	FDefenseProofManifest Manifest;
	TArray<FString> Errors;
	TestFalse(TEXT("Perfect parry must bind its paired proof dependencies"),
		FDefenseAssetValidationService::ParseManifestJson(Json, Manifest, Errors));
	TestTrue(TEXT("Dependency requirement should be explicit"),
		DefenseAssetValidationTests::ErrorsContain(Errors, TEXT("PerfectParry")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDefenseManifestPairedDependencyAuthorityTest,
	"KatanaCombat.Editor.DefenseValidation.Manifest.PairedDependencyAuthority",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FDefenseManifestPairedDependencyAuthorityTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	FDefenseProofManifest Manifest;
	TArray<FString> Errors;

	FString DuplicateJson = DefenseAssetValidationTests::ValidManifest();
	DuplicateJson.ReplaceInline(
		TEXT("\"pairedDependencies\": [\"ParryBridge\"]"),
		TEXT("\"pairedDependencies\": [\"ParryBridge\", \"ParryBridge\"]"));
	TestFalse(TEXT("One expected case must not repeat a paired dependency"),
		FDefenseAssetValidationService::ParseManifestJson(DuplicateJson, Manifest, Errors));
	TestTrue(TEXT("Duplicate dependency failure should be explicit"),
		DefenseAssetValidationTests::ErrorsContain(Errors, TEXT("duplicate")));

	Errors.Reset();
	TestFalse(TEXT("One attack must not resolve to competing Counter dependencies"),
		FDefenseAssetValidationService::ParseManifestJson(
			DefenseAssetValidationTests::ManifestWithAmbiguousCounterAuthority(),
			Manifest, Errors));
	TestTrue(TEXT("Competing role ownership should be explicit"),
		DefenseAssetValidationTests::ErrorsContain(Errors, TEXT("ambiguous Counter")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDefenseManifestNoPresentationCaseTest,
	"KatanaCombat.Editor.DefenseValidation.Manifest.NonDefenseOutcomeAllowsExplicitNoPresentation",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FDefenseManifestNoPresentationCaseTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	FString Json = DefenseAssetValidationTests::ValidManifest();
	Json.ReplaceInline(TEXT("\"outcome\": \"PerfectParry\""), TEXT("\"outcome\": \"Hit\""));
	Json.ReplaceInline(TEXT("\"attackerResponse\": \"ParryStagger\""), TEXT("\"attackerResponse\": \"None\""));
	Json.ReplaceInline(TEXT("\"presentation\": \"ParryMiddleCenter\""), TEXT("\"presentation\": null"));
	Json.ReplaceInline(TEXT("\"pairedDependencies\": [\"ParryBridge\"]"), TEXT("\"pairedDependencies\": []"));

	FDefenseProofManifest Manifest;
	TArray<FString> Errors;
	TestTrue(TEXT("A non-defense result may explicitly expect no presentation"),
		FDefenseAssetValidationService::ParseManifestJson(Json, Manifest, Errors));
	TestEqual(TEXT("No parser errors expected"), Errors.Num(), 0);
	TestFalse(TEXT("No-presentation intent should be preserved"),
		Manifest.ExpectedCases[0].bHasPresentation);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDefenseManifestCasePresentationAgreementTest,
	"KatanaCombat.Editor.DefenseValidation.Manifest.CasePresentationAgreementRequired",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FDefenseManifestCasePresentationAgreementTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	FString Json = DefenseAssetValidationTests::ValidManifest();
	const int32 CaseOffset = Json.Find(TEXT("\"expectedCases\""));
	TestTrue(TEXT("Fixture should contain expectedCases"), CaseOffset != INDEX_NONE);
	if (CaseOffset != INDEX_NONE)
	{
		const int32 OutcomeOffset = Json.Find(TEXT("\"outcome\": \"PerfectParry\""),
			ESearchCase::CaseSensitive, ESearchDir::FromStart, CaseOffset);
		TestTrue(TEXT("Fixture should contain a case outcome"), OutcomeOffset != INDEX_NONE);
		if (OutcomeOffset != INDEX_NONE)
		{
			Json.RemoveAt(OutcomeOffset, FCString::Strlen(TEXT("\"outcome\": \"PerfectParry\"")));
			Json.InsertAt(OutcomeOffset, TEXT("\"outcome\": \"NormalBlock\""));
		}
	}

	FDefenseProofManifest Manifest;
	TArray<FString> Errors;
	TestFalse(TEXT("A case cannot reference a presentation for another outcome"),
		FDefenseAssetValidationService::ParseManifestJson(Json, Manifest, Errors));
	TestTrue(TEXT("Agreement failure should be explicit"),
		DefenseAssetValidationTests::ErrorsContain(Errors, TEXT("does not match")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDefenseAssetAttackAgreementTest,
	"KatanaCombat.Editor.DefenseValidation.Assets.AttackProfileTagAndTimingAgreement",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FDefenseAssetAttackAgreementTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	UAnimMontage* Montage = DefenseAssetValidationTests::CreateSectionMontage();
	DefenseAssetValidationTests::AddParryWindow(Montage, 0.70f, 0.15f);
	UAttackData* Attack = DefenseAssetValidationTests::CreateMatchingAttack(Montage);
	const FDefenseProofAttackEntry Entry = DefenseAssetValidationTests::MakeAttackEntry();
	FDefenseAssetValidationResult Result;

	FDefenseAssetValidationService::ValidateAttackEntry(Entry, Attack, Montage, Result);
	TestFalse(TEXT("A matching reviewed attack should have no errors"), Result.HasErrors());
	TestEqual(TEXT("Exactly one parry window should be inventoried"),
		Result.FindRows(TEXT("Attack")).Num(), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDefenseAssetAttackProfileMismatchTest,
	"KatanaCombat.Editor.DefenseValidation.Assets.AttackProfileMismatchRejected",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FDefenseAssetAttackProfileMismatchTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	UAnimMontage* Montage = DefenseAssetValidationTests::CreateSectionMontage();
	DefenseAssetValidationTests::AddParryWindow(Montage, 0.70f, 0.15f);
	UAttackData* Attack = DefenseAssetValidationTests::CreateMatchingAttack(Montage);
	Attack->DefenseProfile.Height = EAttackHeight::High;
	FDefenseAssetValidationResult Result;
	FDefenseAssetValidationService::ValidateAttackEntry(
		DefenseAssetValidationTests::MakeAttackEntry(), Attack, Montage, Result);
	TestTrue(TEXT("Authored profile drift should be rejected"),
		Result.HasFinding(TEXT("AttackProfileMismatch")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDefenseAssetParryWindowCardinalityTest,
	"KatanaCombat.Editor.DefenseValidation.Assets.ParryWindowCardinalityAndSectionTiming",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FDefenseAssetParryWindowCardinalityTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	UAnimMontage* Montage = DefenseAssetValidationTests::CreateSectionMontage();
	DefenseAssetValidationTests::AddParryWindow(Montage, 0.70f, 0.15f);
	DefenseAssetValidationTests::AddParryWindow(Montage, 0.80f, 0.10f);
	UAttackData* Attack = DefenseAssetValidationTests::CreateMatchingAttack(Montage);
	FDefenseAssetValidationResult DuplicateResult;
	FDefenseAssetValidationService::ValidateAttackEntry(
		DefenseAssetValidationTests::MakeAttackEntry(), Attack, Montage, DuplicateResult);
	TestTrue(TEXT("Duplicate section windows should fail closed"),
		DuplicateResult.HasFinding(TEXT("ParryWindowCardinality")));

	Montage->Notifies.Reset();
	DefenseAssetValidationTests::AddParryWindow(Montage, 1.45f, 0.15f);
	FDefenseAssetValidationResult OutsideResult;
	FDefenseAssetValidationService::ValidateAttackEntry(
		DefenseAssetValidationTests::MakeAttackEntry(), Attack, Montage, OutsideResult);
	TestTrue(TEXT("A window crossing the named section boundary should be rejected"),
		OutsideResult.HasFinding(TEXT("ParryWindowOutsideSection")));

	Montage->Notifies.Reset();
	DefenseAssetValidationTests::AddParryWindow(Montage, 0.70f, 0.15f);
	DefenseAssetValidationTests::AddParryWindow(Montage, 1.60f, 0.15f);
	FDefenseAssetValidationResult OtherSectionResult;
	FDefenseAssetValidationService::ValidateAttackEntry(
		DefenseAssetValidationTests::MakeAttackEntry(), Attack, Montage, OtherSectionResult);
	TestFalse(TEXT("A parry window wholly owned by another combo section is unrelated"),
		OtherSectionResult.HasFinding(TEXT("ParryWindowOutsideSection")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDefenseAssetTagWindowMismatchTest,
	"KatanaCombat.Editor.DefenseValidation.Assets.TagWindowMismatchRejected",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FDefenseAssetTagWindowMismatchTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	UAnimMontage* Montage = DefenseAssetValidationTests::CreateSectionMontage();
	UAttackData* Attack = DefenseAssetValidationTests::CreateMatchingAttack(Montage);
	FDefenseAssetValidationResult Result;
	FDefenseAssetValidationService::ValidateAttackEntry(
		DefenseAssetValidationTests::MakeAttackEntry(), Attack, Montage, Result);
	TestTrue(TEXT("Parryable without the reviewed notify should fail"),
		Result.HasFinding(TEXT("TagWindowMismatch")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDefenseAssetPresentationSelectionTest,
	"KatanaCombat.Editor.DefenseValidation.Assets.PresentationSelectionAndFallbackCoverage",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FDefenseAssetPresentationSelectionTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	UDefenseConfiguration* Defender = NewObject<UDefenseConfiguration>(GetTransientPackage());
	UDefenseConfiguration* Attacker = NewObject<UDefenseConfiguration>(GetTransientPackage());
	Defender->DefenderPresentationRows = {
		DefenseAssetValidationTests::MakeDefenderRow(Defender, TEXT("BlockExact"), false),
		DefenseAssetValidationTests::MakeDefenderRow(Defender, TEXT("BlockGeneric"), true)};
	Attacker->AttackerResponseRows = {
		DefenseAssetValidationTests::MakeAttackerRow(Attacker, TEXT("RecoilExact"), false),
		DefenseAssetValidationTests::MakeAttackerRow(Attacker, TEXT("RecoilGeneric"), true)};
	UAnimMontage* AttackMontage = DefenseAssetValidationTests::CreateSectionMontage();
	UAttackData* Attack = DefenseAssetValidationTests::CreateMatchingAttack(AttackMontage);
	FDefenseAssetValidationResult Result;
	FDefenseAssetValidationService::ValidatePresentationEntry(
		DefenseAssetValidationTests::MakePresentationEntry(),
		DefenseAssetValidationTests::MakeAttackEntry(), Attack, Defender, Attacker, Result);
	TestFalse(TEXT("Exact rows with generic fallbacks and visible payloads should validate"),
		Result.HasErrors());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDefenseAssetPresentationAmbiguityTest,
	"KatanaCombat.Editor.DefenseValidation.Assets.PresentationAmbiguityRejected",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FDefenseAssetPresentationAmbiguityTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	UDefenseConfiguration* Defender = NewObject<UDefenseConfiguration>(GetTransientPackage());
	UDefenseConfiguration* Attacker = NewObject<UDefenseConfiguration>(GetTransientPackage());
	Defender->DefenderPresentationRows = {
		DefenseAssetValidationTests::MakeDefenderRow(Defender, TEXT("BlockExact"), false),
		DefenseAssetValidationTests::MakeDefenderRow(Defender, TEXT("BlockExactTie"), false),
		DefenseAssetValidationTests::MakeDefenderRow(Defender, TEXT("BlockGeneric"), true)};
	Attacker->AttackerResponseRows = {
		DefenseAssetValidationTests::MakeAttackerRow(Attacker, TEXT("RecoilExact"), false),
		DefenseAssetValidationTests::MakeAttackerRow(Attacker, TEXT("RecoilGeneric"), true)};
	UAnimMontage* AttackMontage = DefenseAssetValidationTests::CreateSectionMontage();
	UAttackData* Attack = DefenseAssetValidationTests::CreateMatchingAttack(AttackMontage);
	FDefenseAssetValidationResult Result;
	FDefenseAssetValidationService::ValidatePresentationEntry(
		DefenseAssetValidationTests::MakePresentationEntry(),
		DefenseAssetValidationTests::MakeAttackEntry(), Attack, Defender, Attacker, Result);
	TestTrue(TEXT("Equal-rank rows should block proof"),
		Result.HasFinding(TEXT("PresentationAmbiguous")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDefenseAssetPresentationFallbackTest,
	"KatanaCombat.Editor.DefenseValidation.Assets.MissingGenericFallbackRejected",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FDefenseAssetPresentationFallbackTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	UDefenseConfiguration* Defender = NewObject<UDefenseConfiguration>(GetTransientPackage());
	UDefenseConfiguration* Attacker = NewObject<UDefenseConfiguration>(GetTransientPackage());
	Defender->DefenderPresentationRows = {
		DefenseAssetValidationTests::MakeDefenderRow(Defender, TEXT("BlockExact"), false)};
	Attacker->AttackerResponseRows = {
		DefenseAssetValidationTests::MakeAttackerRow(Attacker, TEXT("RecoilExact"), false)};
	UAnimMontage* AttackMontage = DefenseAssetValidationTests::CreateSectionMontage();
	UAttackData* Attack = DefenseAssetValidationTests::CreateMatchingAttack(AttackMontage);
	FDefenseAssetValidationResult Result;
	FDefenseAssetValidationService::ValidatePresentationEntry(
		DefenseAssetValidationTests::MakePresentationEntry(),
		DefenseAssetValidationTests::MakeAttackEntry(), Attack, Defender, Attacker, Result);
	TestTrue(TEXT("A proof row without deterministic generic coverage should fail"),
		Result.HasFinding(TEXT("MissingGenericFallback")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDefenseAssetRootMotionBudgetTest,
	"KatanaCombat.Editor.DefenseValidation.Assets.RootMotionBudgets",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FDefenseAssetRootMotionBudgetTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	FDefenseRootMotionMeasurement BlockMeasurement;
	BlockMeasurement.HorizontalTranslation = 1.01;
	BlockMeasurement.MaximumYawRate = 180.01;
	FDefenseAssetValidationResult BlockResult;
	FDefenseAssetValidationService::ValidateRootMotionBudget(
		TEXT("NormalBlock"), BlockMeasurement, 1.0, 180.0, BlockResult);
	TestTrue(TEXT("Normal-block translation above one centimeter should fail"),
		BlockResult.HasFinding(TEXT("RootTranslationBudget")));
	TestTrue(TEXT("Authored yaw above the capability should fail"),
		BlockResult.HasFinding(TEXT("RootYawRateBudget")));

	FDefenseRootMotionMeasurement ParryMeasurement;
	ParryMeasurement.HorizontalTranslation = 75.01;
	FDefenseAssetValidationResult ParryResult;
	FDefenseAssetValidationService::ValidateRootMotionBudget(
		TEXT("PerfectParryRole"), ParryMeasurement, 75.0, 180.0, ParryResult);
	TestTrue(TEXT("Perfect-parry role translation above 75 centimeters should fail"),
		ParryResult.HasFinding(TEXT("RootTranslationBudget")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDefenseAssetPairedDependencyTest,
	"KatanaCombat.Editor.DefenseValidation.Assets.PairedDependencyReadiness",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FDefenseAssetPairedDependencyTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	UPairedAnimationData* Data = DefenseAssetValidationTests::CreateBridgeData();
	FDefenseProofPairedDependencyEntry Entry = DefenseAssetValidationTests::MakeBridgeEntry();
	Entry.PairedData = Data->GetPathName();
	Entry.AttackerMontage = Data->AttackerMontage->GetPathName();
	Entry.VictimMontage = Data->VictimMontage->GetPathName();
	UDefenseConfiguration* Configuration = NewObject<UDefenseConfiguration>(GetTransientPackage());
	FDefenseAssetValidationResult Result;
	FDefenseAssetValidationService::ValidatePairedDependency(Entry, Data, Configuration, Result);
	TestFalse(TEXT("A reviewed bridge with exact role ownership should validate"), Result.HasErrors());
	TestEqual(TEXT("One paired row should be inventoried"), Result.FindRows(TEXT("Paired")).Num(), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDefenseAssetBridgeBudgetScopeTest,
	"KatanaCombat.Editor.DefenseValidation.Assets.BridgeBudgetDoesNotConstrainLaterStages",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FDefenseAssetBridgeBudgetScopeTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	UDefenseConfiguration* Configuration = NewObject<UDefenseConfiguration>(GetTransientPackage());
	Configuration->PerfectParryTranslationAllowancePerRole = -1.0f;
	Configuration->DefenseTurnRate = -1.0f;

	UPairedAnimationData* CounterData = DefenseAssetValidationTests::CreateBridgeData();
	CounterData->ReactionType = EPairedReactionType::Counter;
	FDefenseProofPairedDependencyEntry CounterEntry = DefenseAssetValidationTests::MakeBridgeEntry();
	CounterEntry.Name = TEXT("Counter");
	CounterEntry.Role = TEXT("Counter");
	CounterEntry.PairedData = CounterData->GetPathName();
	CounterEntry.AttackerMontage = CounterData->AttackerMontage->GetPathName();
	CounterEntry.VictimMontage = CounterData->VictimMontage->GetPathName();
	FDefenseAssetValidationResult CounterResult;
	FDefenseAssetValidationService::ValidatePairedDependency(
		CounterEntry, CounterData, Configuration, CounterResult);
	TestFalse(TEXT("Perfect-parry bridge budgets must not reject the counter stage"),
		CounterResult.HasFinding(TEXT("RootTranslationBudget"))
			|| CounterResult.HasFinding(TEXT("RootYawRateBudget")));

	UPairedAnimationData* FinisherData = DefenseAssetValidationTests::CreateBridgeData();
	FinisherData->ReactionType = EPairedReactionType::Finisher;
	FinisherData->ChainTransitionPolicy.RequiredMarker = NAME_None;
	FDefenseProofPairedDependencyEntry FinisherEntry = DefenseAssetValidationTests::MakeBridgeEntry();
	FinisherEntry.Name = TEXT("Finisher");
	FinisherEntry.Role = TEXT("Finisher");
	FinisherEntry.PairedData = FinisherData->GetPathName();
	FinisherEntry.AttackerMontage = FinisherData->AttackerMontage->GetPathName();
	FinisherEntry.VictimMontage = FinisherData->VictimMontage->GetPathName();
	FinisherEntry.bHasDriverRole = false;
	FinisherEntry.DriverRole.Reset();
	FinisherEntry.bHasDriverMarker = false;
	FinisherEntry.DriverMarker.Reset();
	FDefenseAssetValidationResult FinisherResult;
	FDefenseAssetValidationService::ValidatePairedDependency(
		FinisherEntry, FinisherData, Configuration, FinisherResult);
	TestFalse(TEXT("Perfect-parry bridge budgets must not reject the finisher stage"),
		FinisherResult.HasFinding(TEXT("RootTranslationBudget"))
			|| FinisherResult.HasFinding(TEXT("RootYawRateBudget")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDefenseAssetPairedMarkerAmbiguityTest,
	"KatanaCombat.Editor.DefenseValidation.Assets.PairedDriverMarkerAmbiguityRejected",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FDefenseAssetPairedMarkerAmbiguityTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	UPairedAnimationData* Data = DefenseAssetValidationTests::CreateBridgeData();
	DefenseAssetValidationTests::AddChainMarker(Data->VictimMontage, TEXT("CounterReady"),
		EChainStageTransitionType::OpenCounterWindow);
	FDefenseProofPairedDependencyEntry Entry = DefenseAssetValidationTests::MakeBridgeEntry();
	Entry.PairedData = Data->GetPathName();
	Entry.AttackerMontage = Data->AttackerMontage->GetPathName();
	Entry.VictimMontage = Data->VictimMontage->GetPathName();
	FDefenseAssetValidationResult Result;
	FDefenseAssetValidationService::ValidatePairedDependency(
		Entry, Data, NewObject<UDefenseConfiguration>(GetTransientPackage()), Result);
	TestTrue(TEXT("A marker on the non-driver role should fail"),
		Result.HasFinding(TEXT("DriverMarkerAmbiguous")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDefenseAssetPairedMarkerSectionBoundaryTest,
	"KatanaCombat.Editor.DefenseValidation.Assets.PairedDriverMarkerSectionBoundaryRejected",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FDefenseAssetPairedMarkerSectionBoundaryTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	UPairedAnimationData* Data = DefenseAssetValidationTests::CreateBridgeData();
	FAnimNotifyEvent& DriverMarker = Data->AttackerMontage->Notifies[0];
	DriverMarker.SetTime(1.5f);
	FDefenseProofPairedDependencyEntry Entry = DefenseAssetValidationTests::MakeBridgeEntry();
	Entry.PairedData = Data->GetPathName();
	Entry.AttackerMontage = Data->AttackerMontage->GetPathName();
	Entry.VictimMontage = Data->VictimMontage->GetPathName();
	FDefenseAssetValidationResult Result;
	FDefenseAssetValidationService::ValidatePairedDependency(
		Entry, Data, NewObject<UDefenseConfiguration>(GetTransientPackage()), Result);
	TestTrue(TEXT("A marker at the exclusive end of the played section should fail"),
		Result.HasFinding(TEXT("DriverMarkerOutsideActiveSection")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDefenseAssetManifestMontageAuthorityTest,
	"KatanaCombat.Editor.DefenseValidation.Assets.ManifestMontageAuthority",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FDefenseAssetManifestMontageAuthorityTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	UPairedAnimationData* Data = DefenseAssetValidationTests::CreateBridgeData();
	UAnimMontage* ExpectedAttackerMontage = Data->AttackerMontage;
	UAnimMontage* ExpectedVictimMontage = Data->VictimMontage;
	FDefenseProofPairedDependencyEntry Entry = DefenseAssetValidationTests::MakeBridgeEntry();
	Entry.PairedData = Data->GetPathName();
	Entry.AttackerMontage = ExpectedAttackerMontage->GetPathName();
	Entry.VictimMontage = ExpectedVictimMontage->GetPathName();
	Data->AttackerMontage = DefenseAssetValidationTests::CreateSectionMontage(TEXT("Bridge"));
	Data->VictimMontage = DefenseAssetValidationTests::CreateSectionMontage(TEXT("Bridge"));

	FDefenseAssetValidationResult Result;
	FDefenseAssetValidationService::ValidatePairedDependency(
		Entry, Data, NewObject<UDefenseConfiguration>(GetTransientPackage()), Result,
		ExpectedAttackerMontage, ExpectedVictimMontage);
	TestTrue(TEXT("A stale PairedData montage reference should be correctable"),
		Result.HasFinding(TEXT("PairedMontageSectionMismatch")));
	TestFalse(TEXT("Marker proof must inspect the manifest-target montage"),
		Result.HasFinding(TEXT("DriverMarkerAmbiguous")));
	TestFalse(TEXT("Warp proof must inspect the manifest-target montage"),
		Result.HasFinding(TEXT("PairedRotationWarpMissing")));
	TestFalse(TEXT("Sync proof must inspect the manifest-target montage"),
		Result.HasFinding(TEXT("PairedSyncNotifyMissing")));
	TestFalse(TEXT("Collision proof must inspect the manifest-target montages"),
		Result.HasFinding(TEXT("PairedCollisionNotifyMissing")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDefenseAssetPairedWarpTest,
	"KatanaCombat.Editor.DefenseValidation.Assets.PairedRotationWarpRequired",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FDefenseAssetPairedWarpTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	UPairedAnimationData* Data = DefenseAssetValidationTests::CreateBridgeData();
	Data->VictimWarpConfig.bWarpRotation = false;
	FDefenseProofPairedDependencyEntry Entry = DefenseAssetValidationTests::MakeBridgeEntry();
	Entry.PairedData = Data->GetPathName();
	Entry.AttackerMontage = Data->AttackerMontage->GetPathName();
	Entry.VictimMontage = Data->VictimMontage->GetPathName();
	FDefenseAssetValidationResult Result;
	FDefenseAssetValidationService::ValidatePairedDependency(
		Entry, Data, NewObject<UDefenseConfiguration>(GetTransientPackage()), Result);
	TestTrue(TEXT("Both roles must opt into rotation warp"),
		Result.HasFinding(TEXT("PairedRotationWarpConfigMismatch")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDefenseAssetPairedReadyPoseTest,
	"KatanaCombat.Editor.DefenseValidation.Assets.PairedReadyPoseOwnershipRequired",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FDefenseAssetPairedReadyPoseTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	UPairedAnimationData* Data = DefenseAssetValidationTests::CreateBridgeData();
	Data->ChainTransitionPolicy.bVictimTerminalPoseCompatible = false;
	FDefenseProofPairedDependencyEntry Entry = DefenseAssetValidationTests::MakeBridgeEntry();
	Entry.PairedData = Data->GetPathName();
	Entry.AttackerMontage = Data->AttackerMontage->GetPathName();
	Entry.VictimMontage = Data->VictimMontage->GetPathName();
	FDefenseAssetValidationResult Result;
	FDefenseAssetValidationService::ValidatePairedDependency(
		Entry, Data, NewObject<UDefenseConfiguration>(GetTransientPackage()), Result);
	TestTrue(TEXT("Both roles need a reviewed retained pose"),
		Result.HasFinding(TEXT("PairedReadyPoseMissing")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDefenseAssetAdjacentMontageReuseTest,
	"KatanaCombat.Editor.DefenseValidation.Assets.AdjacentSameRoleMontageReuseRejected",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FDefenseAssetAdjacentMontageReuseTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	UPairedAnimationData* Bridge = DefenseAssetValidationTests::CreateBridgeData();
	UPairedAnimationData* Counter = DefenseAssetValidationTests::CreateBridgeData();
	Counter->ReactionType = EPairedReactionType::Counter;
	Counter->AttackerMontage = Bridge->AttackerMontage;
	FDefenseProofPairedDependencyEntry BridgeEntry = DefenseAssetValidationTests::MakeBridgeEntry();
	FDefenseProofPairedDependencyEntry CounterEntry = DefenseAssetValidationTests::MakeBridgeEntry();
	BridgeEntry.Name = TEXT("Bridge");
	BridgeEntry.Role = TEXT("Bridge");
	BridgeEntry.AttackerMontage = Bridge->AttackerMontage->GetPathName();
	BridgeEntry.VictimMontage = Bridge->VictimMontage->GetPathName();
	CounterEntry.Name = TEXT("Counter");
	CounterEntry.Role = TEXT("Counter");
	CounterEntry.AttackerMontage = Counter->AttackerMontage->GetPathName();
	CounterEntry.VictimMontage = Counter->VictimMontage->GetPathName();
	const TArray<FDefenseProofPairedDependencyEntry> Entries = {BridgeEntry, CounterEntry};
	const TArray<const UPairedAnimationData*> Assets = {Bridge, Counter};
	FDefenseAssetValidationResult Result;
	FDefenseAssetValidationService::ValidatePairedSequence(Entries, Assets, Result);
	TestTrue(TEXT("Adjacent stages cannot reuse the same-role montage"),
		Result.HasFinding(TEXT("AdjacentMontageReuse")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDefenseManifestExplicitPathClosureTest,
	"KatanaCombat.Editor.DefenseValidation.Manifest.ExplicitPathClosure",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FDefenseManifestExplicitPathClosureTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	FDefenseProofManifest Manifest;
	TArray<FString> Errors;
	TestTrue(TEXT("Fixture manifest should parse"),
		FDefenseAssetValidationService::ParseManifestJson(
			DefenseAssetValidationTests::ValidManifest(), Manifest, Errors));

	const TArray<FString> Paths =
		FDefenseAssetValidationService::CollectExplicitObjectPaths(Manifest);
	TestEqual(TEXT("Every declared object path should appear exactly once"), Paths.Num(), 15);
	TestTrue(TEXT("Map should be explicit"), Paths.Contains(Manifest.Map));
	TestTrue(TEXT("Block input action should be explicit"), Paths.Contains(Manifest.Fixture.InputAction));
	TestTrue(TEXT("Supporting assets should be explicit"),
		Paths.Contains(Manifest.SupportingAssets[0]));
	TestTrue(TEXT("Paired victim montage should be explicit"),
		Paths.Contains(Manifest.PairedDependencies[0].VictimMontage));
	for (int32 Index = 1; Index < Paths.Num(); ++Index)
	{
		TestTrue(TEXT("Explicit paths should be deterministically sorted"), Paths[Index - 1] < Paths[Index]);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDefenseManifestUndeclaredDependencyTest,
	"KatanaCombat.Editor.DefenseValidation.Assets.UndeclaredDependencyRejected",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FDefenseManifestUndeclaredDependencyTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	FDefenseProofManifest Manifest;
	TArray<FString> Errors;
	TestTrue(TEXT("Fixture manifest should parse"),
		FDefenseAssetValidationService::ParseManifestJson(
			DefenseAssetValidationTests::ValidManifest(), Manifest, Errors));

	UDefenseConfiguration* Configuration = NewObject<UDefenseConfiguration>(GetTransientPackage());
	UPackage* AudioPackage = CreatePackage(TEXT("/Game/__Automation__/SW_UndeclaredDefenseImpact"));
	USoundWave* UndeclaredAudio = NewObject<USoundWave>(
		AudioPackage, TEXT("SW_UndeclaredDefenseImpact"));
	Configuration->DefaultBlockImpactAudio.ImpactSound = UndeclaredAudio;

	FDefenseProofAssetSet Assets;
	Assets.Add(Manifest.DefenseConfiguration, Configuration);
	FDefenseAssetValidationResult Result;
	FDefenseAssetValidationService::ValidateManifestObjects(Manifest, Assets, Result);
	TestTrue(TEXT("Discovered /Game dependencies must be declared explicitly"),
		Result.HasFinding(TEXT("UndeclaredManifestDependency")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDefenseManifestUndeclaredMontageSourceTest,
	"KatanaCombat.Editor.DefenseValidation.Assets.UndeclaredMontageSourceRejected",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FDefenseManifestUndeclaredMontageSourceTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	FDefenseProofManifest Manifest;
	TArray<FString> Errors;
	TestTrue(TEXT("Fixture manifest should parse"),
		FDefenseAssetValidationService::ParseManifestJson(
			DefenseAssetValidationTests::ValidManifest(), Manifest, Errors));

	UAnimMontage* Montage = NewObject<UAnimMontage>(GetTransientPackage());
	UPackage* SequencePackage = CreatePackage(TEXT("/Game/__Automation__/AC_UndeclaredDefenseSource"));
	UAnimComposite* SourceSequence = NewObject<UAnimComposite>(
		SequencePackage, TEXT("AC_UndeclaredDefenseSource"));
	FSlotAnimationTrack& Slot = Montage->AddSlot(TEXT("DefaultSlot"));
	FAnimSegment Segment;
	Segment.SetAnimReference(SourceSequence);
	Segment.AnimStartTime = 0.0f;
	Segment.AnimEndTime = 1.0f;
	Slot.AnimTrack.AnimSegments.Add(Segment);
	UAttackData* Attack = DefenseAssetValidationTests::CreateMatchingAttack(Montage);

	FDefenseProofAssetSet Assets;
	Assets.Add(Manifest.Attacks[0].AttackData, Attack);
	Assets.Add(Manifest.Attacks[0].Montage, Montage);
	FDefenseAssetValidationResult Result;
	FDefenseAssetValidationService::ValidateManifestObjects(Manifest, Assets, Result);
	TestTrue(TEXT("Montage source animations must be part of explicit dependency closure"),
		Result.Findings.ContainsByPredicate([](const FDefenseAssetValidationFinding& Finding)
		{
			return Finding.Code == TEXT("UndeclaredManifestDependency")
				&& Finding.Context.Contains(TEXT("AC_UndeclaredDefenseSource"));
		}));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDefenseManifestMissingObjectClosureTest,
	"KatanaCombat.Editor.DefenseValidation.Assets.MissingObjectClosureRejected",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FDefenseManifestMissingObjectClosureTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	FDefenseProofManifest Manifest;
	TArray<FString> Errors;
	TestTrue(TEXT("Fixture manifest should parse"),
		FDefenseAssetValidationService::ParseManifestJson(
			DefenseAssetValidationTests::ValidManifest(), Manifest, Errors));

	FDefenseProofAssetSet EmptyAssets;
	FDefenseAssetValidationResult Result;
	FDefenseAssetValidationService::ValidateManifestObjects(Manifest, EmptyAssets, Result);
	TestTrue(TEXT("An incomplete explicit object graph must fail closed"),
		Result.HasFinding(TEXT("MissingManifestAsset")));
	TestEqual(TEXT("Every attack still receives a report row"),
		Result.FindRows(TEXT("Attack")).Num(), Manifest.Attacks.Num());
	TestEqual(TEXT("Every presentation still receives a report row"),
		Result.FindRows(TEXT("Presentation")).Num(), Manifest.Presentations.Num());
	TestEqual(TEXT("Every paired dependency still receives a report row"),
		Result.FindRows(TEXT("Paired")).Num(), Manifest.PairedDependencies.Num());
	TestEqual(TEXT("Every expected case still receives a report row"),
		Result.FindRows(TEXT("ExpectedCase")).Num(), Manifest.ExpectedCases.Num());
	TestEqual(TEXT("Every runtime proof case still receives a report row"),
		Result.FindRows(TEXT("ProofCase")).Num(), Manifest.ProofCases.Num());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDefenseManifestSettingsAssignmentTest,
	"KatanaCombat.Editor.DefenseValidation.Assets.SettingsAssignmentRequired",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FDefenseManifestSettingsAssignmentTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	FDefenseProofManifest Manifest;
	TArray<FString> Errors;
	TestTrue(TEXT("Fixture manifest should parse"),
		FDefenseAssetValidationService::ParseManifestJson(
			DefenseAssetValidationTests::ValidManifest(), Manifest, Errors));

	UDefenseConfiguration* Configuration = NewObject<UDefenseConfiguration>(GetTransientPackage());
	UCombatSettings* PlayerSettings = NewObject<UCombatSettings>(GetTransientPackage());
	UCombatSettings* EnemySettings = NewObject<UCombatSettings>(GetTransientPackage());
	FDefenseProofAssetSet Assets;
	Assets.Add(Manifest.DefenseConfiguration, Configuration);
	Assets.Add(Manifest.CombatSettings[0], PlayerSettings);
	Assets.Add(Manifest.CombatSettings[1], EnemySettings);

	FDefenseAssetValidationResult Result;
	FDefenseAssetValidationService::ValidateManifestObjects(Manifest, Assets, Result);
	TestTrue(TEXT("Every named combat settings asset must reference the manifest defense configuration"),
		Result.HasFinding(TEXT("DefenseConfigurationAssignmentMismatch")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDefenseManifestThumbMouseMappingTest,
	"KatanaCombat.Editor.DefenseValidation.Assets.ThumbMouseMappingRequired",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FDefenseManifestThumbMouseMappingTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	FDefenseProofManifest Manifest;
	TArray<FString> Errors;
	TestTrue(TEXT("Fixture manifest should parse"),
		FDefenseAssetValidationService::ParseManifestJson(
			DefenseAssetValidationTests::ValidManifest(), Manifest, Errors));

	UInputAction* BlockAction = NewObject<UInputAction>(GetTransientPackage());
	UInputMappingContext* MappingContext = NewObject<UInputMappingContext>(GetTransientPackage());
	MappingContext->MapKey(BlockAction, EKeys::RightMouseButton);
	FDefenseProofAssetSet Assets;
	Assets.Add(Manifest.Fixture.InputAction, BlockAction);
	Assets.Add(Manifest.Fixture.InputMappingContext, MappingContext);

	FDefenseAssetValidationResult Result;
	FDefenseAssetValidationService::ValidateManifestObjects(Manifest, Assets, Result);
	TestTrue(TEXT("A heavy-attack mouse binding cannot stand in for the reviewed block key"),
		Result.HasFinding(TEXT("BlockInputMappingMissing")));
	return true;
}
