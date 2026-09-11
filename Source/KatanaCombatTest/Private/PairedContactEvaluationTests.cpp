// Copyright Epic Games, Inc. All Rights Reserved.
#include "Misc/AutomationTest.h"
#include "PairedAnimationAnalysisLibrary.h"
#include "Subsystems/PairedAnimationAnalysisSubsystem.h"
#include "Animation/DebugSkelMeshComponent.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimNotifyState_PairedAnimationSync.h"
#include "Animation/AnimNotifyQueue.h"
#include "Editor.h"
#include "HAL/FileManager.h"
#include "PreviewScene.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonSerializer.h"

namespace PairedContactTests
{
FPairedContactRule Strike()
{
	FPairedContactRule Rule;
	Rule.Name = TEXT("BladeToTorso"); Rule.SourcePoint = TEXT("Attacker:weapon_start");
	Rule.SourceEndPoint = TEXT("Attacker:weapon_end"); Rule.TargetPoint = TEXT("Victim:spine_03");
	Rule.StartSeconds = 0.1; Rule.EndSeconds = 0.2; Rule.MaximumSampleGapSeconds = 0.051;
	Rule.TargetRadiusCm = 10; Rule.MinimumGapCm = -10; Rule.MaximumGapCm = 2;
	return Rule;
}
TArray<FPairedContactPose> Poses()
{
	TArray<FPairedContactPose> Samples;
	for (int32 I = 0; I <= 6; ++I)
	{
		FPairedContactPose& Sample = Samples.AddDefaulted_GetRef(); Sample.TimeSeconds = I * 0.05; Sample.OriginalTimeSeconds = Sample.TimeSeconds + 10;
		Sample.Points.Add(TEXT("Attacker:weapon_start"), FTransform(FVector(0, 0, 0)));
		Sample.Points.Add(TEXT("Attacker:weapon_end"), FTransform(FVector(100, 0, 0)));
		Sample.Points.Add(TEXT("Victim:spine_03"), FTransform(FVector(50, 10, 0)));
		Sample.Points.Add(TEXT("Victim:foot_l"), FTransform(FVector(0, 0, 0)));
	}
	return Samples;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPairedSyncBoundaryTest, "KatanaCombat.Editor.PairedEvaluation.SyncStateSectionBoundaries", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPairedSyncBoundaryTest::RunTest(const FString&)
{
	auto* Montage = NewObject<UAnimMontage>(); Montage->RateScale = 2;
	auto Add = [&](FName Name, float Time, float Duration, float Offset, bool bPrimary)
	{
		auto* Sync = NewObject<UAnimNotifyState_PairedAnimationSync>(Montage);
		Sync->SyncPointName = Name; Sync->bIsPrimarySyncPoint = bPrimary; Sync->bApplyDamage = true;
		auto& Event = Montage->Notifies.AddDefaulted_GetRef(); Event.NotifyStateClass = Sync;
		Event.SetTime(Time); Event.SetDuration(Duration); Event.TriggerTimeOffset = Offset;
	};
	Add(TEXT("OpeningImpact"), 1, .4f, -.0001f, true);
	Add(TEXT("InteriorImpact"), 1.5f, .1f, .0001f, false);
	Add(TEXT("AlreadyActive"), .8f, .3f, 0, true);
	Add(TEXT("EarlierFinished"), .2f, .1f, 0, true);
	Add(TEXT("EndsAtEntry"), .5f, .5f, 0, true);
	Add(TEXT("LaterSection"), 2.1f, .2f, 0, true);
	FAnimNotifyContext Native;
	Montage->GetAnimNotifiesFromDeltaPositions(1.0f, 2.0f, Native);
	const auto Events = UPairedAnimationAnalysisSubsystem::CollectMontageSyncEvents(Montage, 1, 2);
	TestEqual(TEXT("Exporter matches Unreal forward notify-state extraction"), Events.Num(), Native.ActiveNotifies.Num());
	if (!TestEqual(TEXT("Only the three overlapping states are included"), Events.Num(), 3)) { return false; }
	TestEqual(TEXT("Opening authored time is retained"), Events[0].NominalMontageTime, 1.0);
	TestTrue(TEXT("Negative trigger offset is retained"), Events[0].TriggerMontageTime < 1);
	TestEqual(TEXT("Opening state anchors to entry instead of negative paired time"), Events[0].PairTime, 0.0);
	TestTrue(TEXT("Opening state requests damage"), Events[0].bPrimary && Events[0].bDamageConfigured);
	TestTrue(TEXT("Interior clock includes trigger offset and montage rate"), FMath::IsNearlyEqual(Events[1].PairTime, .25005, .000001));
	TestFalse(TEXT("Configured secondary damage flag is not a primary request"), Events[1].bPrimary);
	TestTrue(TEXT("Secondary configured flag remains inspectable"), Events[1].bDamageConfigured);
	TestTrue(TEXT("State started before section is explicitly active at entry"), Events[2].bActiveAtEntry);
	TestEqual(TEXT("Overlapping state begins at fresh playback entry"), Events[2].PairTime, 0.0);
	TestTrue(TEXT("Zero-length playback interval has no inferred events"), UPairedAnimationAnalysisSubsystem::CollectMontageSyncEvents(Montage, 1, 1).IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPairedContactIntentTest, "KatanaCombat.Editor.PairedEvaluation.IntendedGeometry", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPairedContactIntentTest::RunTest(const FString&)
{
	auto Rule = PairedContactTests::Strike(); auto Poses = PairedContactTests::Poses();
	auto Evaluate = [&]() { return UPairedAnimationAnalysisLibrary::EvaluateIntendedContact(Rule, Poses); };
	TestEqual(TEXT("Segment intersects the declared torso-region boundary"), Evaluate().Status, EPairedContactResult::Pass);
	TestEqual(TEXT("Boundary gap is zero centimetres"), Evaluate().Observations[0].SignedGapCm, 0.0);
	for (auto& Pose : Poses) { Pose.Points[Rule.TargetPoint].AddToTranslation(FVector(0, 50, 0)); }
	TestEqual(TEXT("Nearby unrelated foot cannot satisfy torso contact"), Evaluate().Status, EPairedContactResult::Fail);
	for (auto& Pose : Poses) { Pose.Points[Rule.TargetPoint].AddToTranslation(FVector(0, -50, 0)); }
	TestEqual(TEXT("Removing alignment defect restores the same criteria"), Evaluate().Status, EPairedContactResult::Pass);
	Rule.MinimumGapCm = 1;
	TestEqual(TEXT("Forbidden overlap/separation is respected"), Evaluate().Status, EPairedContactResult::Fail);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPairedContactTimingTest, "KatanaCombat.Editor.PairedEvaluation.TimingAndSustainedContact", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPairedContactTimingTest::RunTest(const FString&)
{
	auto Rule = PairedContactTests::Strike(); auto Poses = PairedContactTests::Poses();
	for (auto& Pose : Poses) { if (Pose.TimeSeconds > 0.11) { Pose.Points[Rule.TargetPoint].AddToTranslation(FVector(0, 50, 0)); } }
	TestEqual(TEXT("Strike needs one observed contact inside its window"), UPairedAnimationAnalysisLibrary::EvaluateIntendedContact(Rule, Poses).Status, EPairedContactResult::Pass);
	Rule.bSustained = true;
	TestEqual(TEXT("A sustained grab cannot pass on one moment of proximity"), UPairedAnimationAnalysisLibrary::EvaluateIntendedContact(Rule, Poses).Status, EPairedContactResult::Fail);
	Rule.bSustained = false; Rule.StartSeconds = .15;
	TestEqual(TEXT("Earlier contact cannot satisfy a later strike window"), UPairedAnimationAnalysisLibrary::EvaluateIntendedContact(Rule, Poses).Status, EPairedContactResult::Fail);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPairedContactEvidenceTest, "KatanaCombat.Editor.PairedEvaluation.MissingAndSparseEvidence", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPairedContactEvidenceTest::RunTest(const FString&)
{
	auto Rule = PairedContactTests::Strike(); auto Poses = PairedContactTests::Poses();
	Poses[3].Points.Remove(Rule.SourceEndPoint);
	TestEqual(TEXT("Missing weapon endpoint is inconclusive"), UPairedAnimationAnalysisLibrary::EvaluateIntendedContact(Rule, Poses).Status, EPairedContactResult::Inconclusive);
	Poses = PairedContactTests::Poses(); Poses[3].Points[Rule.SourceEndPoint] = Poses[3].Points[Rule.SourcePoint];
	TestEqual(TEXT("Collapsed blade cannot become a successful point-contact fallback"), UPairedAnimationAnalysisLibrary::EvaluateIntendedContact(Rule, Poses).Status, EPairedContactResult::Inconclusive);
	Poses = PairedContactTests::Poses(); Poses[3].bEligible = false; Poses[3].IneligibilityReason = TEXT("Stale pose");
	TestEqual(TEXT("Stale participant cannot pass"), UPairedAnimationAnalysisLibrary::EvaluateIntendedContact(Rule, Poses).Status, EPairedContactResult::Inconclusive);
	Poses = PairedContactTests::Poses(); Poses.RemoveAt(3);
	TestEqual(TEXT("Gap across intended contact is inconclusive"), UPairedAnimationAnalysisLibrary::EvaluateIntendedContact(Rule, Poses).Status, EPairedContactResult::Inconclusive);
	Poses = PairedContactTests::Poses(); Rule.EndSeconds = 1;
	TestEqual(TEXT("Unobserved interval tail cannot pass"), UPairedAnimationAnalysisLibrary::EvaluateIntendedContact(Rule, Poses).Status, EPairedContactResult::Inconclusive);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPairedContactSamplingPhaseTest, "KatanaCombat.Editor.PairedEvaluation.BriefContactSamplingPhases", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPairedContactSamplingPhaseTest::RunTest(const FString&)
{
	auto Rule = PairedContactTests::Strike();
	Rule.StartSeconds = .1; Rule.EndSeconds = .3; Rule.MaximumSampleGapSeconds = .025;
	// A point moving at 400 cm/s through a sphere of radius 10 cm has a known
	// 50 ms contact duration. This is a measurement control, not an asset tolerance.
	Rule.MinimumGapCm = -10; Rule.MaximumGapCm = 0;
	auto Sample = [&](double Step, double Phase, double ContactCenter)
	{
		TArray<FPairedContactPose> Poses;
		for (double Time = Phase; Time <= .4; Time += Step)
		{
			auto& Pose = Poses.AddDefaulted_GetRef(); Pose.TimeSeconds = Time; Pose.OriginalTimeSeconds = Time + 10;
			Pose.Points.Add(Rule.SourcePoint, FTransform(FVector::ZeroVector));
			Pose.Points.Add(Rule.SourceEndPoint, FTransform(FVector(100, 0, 0)));
			Pose.Points.Add(Rule.TargetPoint, FTransform(FVector(50, (Time - ContactCenter) * 400, 0)));
		}
		return UPairedAnimationAnalysisLibrary::EvaluateIntendedContact(Rule, Poses);
	};
	for (int32 Phase = 0; Phase < 60; ++Phase)
	{
		const auto Dense = Sample(1.0 / 60, Phase / 3600.0, .2);
		TestEqual(TEXT("Known 50 ms contact is detected at every tested 60 Hz phase"), Dense.Status, EPairedContactResult::Pass);
		for (const auto& Observation : Dense.Observations)
		{
			TestEqual(TEXT("Original observation clock is preserved"), Observation.OriginalTimeSeconds, Observation.TimeSeconds + 10);
		}
		TestEqual(TEXT("A 70 ms gap is inconclusive under the 25 ms brief-contact requirement"), Sample(.07, Phase * .07 / 60, .2).Status, EPairedContactResult::Inconclusive);
		TestEqual(TEXT("Dense samples reject contact outside the declared interval"), Sample(1.0 / 60, Phase / 3600.0, .35).Status, EPairedContactResult::Fail);
	}
	Rule.MaximumSampleGapSeconds = .075;
	// These 70 ms samples straddle a 50 ms contact at .15..20 seconds. An eligible
	// sampled miss is possible: the broad 75 ms diagnostic bound cannot prove absence.
	TestEqual(TEXT("Coarse eligibility can miss a real brief contact between samples"), Sample(.07, 0, .175).Status, EPairedContactResult::Fail);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPairedContactOrientationTest, "KatanaCombat.Editor.PairedEvaluation.OrientationAndCoordinateInvariance", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPairedContactOrientationTest::RunTest(const FString&)
{
	auto Rule = PairedContactTests::Strike(); auto Poses = PairedContactTests::Poses(); Rule.bMeasureOrientation = true;
	TestEqual(TEXT("Configured weapon axis agrees with victim region axis"), UPairedAnimationAnalysisLibrary::EvaluateIntendedContact(Rule, Poses).Status, EPairedContactResult::Pass);
	const FTransform Relocate(FRotator(0, 73, 0), FVector(800, -150, 300));
	for (auto& Pose : Poses) { for (auto& Point : Pose.Points) { Point.Value = Point.Value * Relocate; } }
	TestEqual(TEXT("Common world rotation/translation preserves the result"), UPairedAnimationAnalysisLibrary::EvaluateIntendedContact(Rule, Poses).Status, EPairedContactResult::Pass);
	Rule.ExpectedAngleDegrees = 90;
	TestEqual(TEXT("Position agreement does not conceal wrong orientation"), UPairedAnimationAnalysisLibrary::EvaluateIntendedContact(Rule, Poses).Status, EPairedContactResult::Fail);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPairedAlignmentBudgetTest, "KatanaCombat.Editor.PairedEvaluation.RelativeAlignmentBudgets", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPairedAlignmentBudgetTest::RunTest(const FString&)
{
	TArray<FPairedContactPose> Authored;
	for (int32 I = 0; I <= 6; ++I)
	{
		auto& Pose = Authored.AddDefaulted_GetRef(); Pose.TimeSeconds = I * .05; Pose.OriginalTimeSeconds = Pose.TimeSeconds; Pose.VictimTimeSeconds = Pose.TimeSeconds;
		Pose.Points.Add(TEXT("Attacker:root"), FTransform(FVector(I * 2, 0, 0)));
		Pose.Points.Add(TEXT("Victim:root"), FTransform(FVector(I * 2 + 100, 0, 0)));
	}
	FPairedAlignmentRule Rule; Rule.Name = TEXT("ContactAlignment"); Rule.StartSeconds = .1; Rule.EndSeconds = .2;
	Rule.MaximumTranslationCm = 5; Rule.MaximumRotationDegrees = 5; Rule.MaximumVictimTimingErrorSeconds = .02;
	auto Runtime = Authored;
	auto Evaluate = [&]() { return UPairedAnimationAnalysisLibrary::EvaluateRelativeAlignment(Rule, Authored, Runtime); };
	TestEqual(TEXT("Identical sampled motion passes"), Evaluate().Status, EPairedContactResult::Pass);
	const FTransform WorldChange(FRotator(0, 83, 0), FVector(150, 700, 10));
	for (auto& Pose : Runtime) { for (auto& Point : Pose.Points) { Point.Value = Point.Value * WorldChange; } }
	TestEqual(TEXT("World location and heading do not consume relative budget"), Evaluate().Status, EPairedContactResult::Pass);
	for (auto& Pose : Runtime) { for (auto& Point : Pose.Points) { Point.Value.SetScale3D(FVector(2)); } }
	TestEqual(TEXT("Root scale metadata does not change world-centimetre placement"), Evaluate().Status, EPairedContactResult::Pass);
	Runtime = Authored; Runtime[3].Points[TEXT("Victim:root")].AddToTranslation(FVector(10, 0, 0));
	TestEqual(TEXT("Ten centimetre displacement exceeds five centimetre budget"), Evaluate().Status, EPairedContactResult::Fail);
	TestEqual(TEXT("Known translation reported in centimetres"), Evaluate().Observations[1].TranslationCm, 10.0);
	Runtime = Authored; Runtime[3].Points[TEXT("Victim:root")].SetRotation(FQuat(FRotator(0, 15, 0)));
	TestEqual(TEXT("Orientation error is independent of translation"), Evaluate().Status, EPairedContactResult::Fail);
	Runtime = Authored; Runtime[3].VictimTimeSeconds += .1;
	TestEqual(TEXT("Timing drift is not fitted away"), Evaluate().Status, EPairedContactResult::Fail);
	Runtime = Authored; Runtime[3].VictimTimeSeconds = std::numeric_limits<double>::quiet_NaN();
	TestEqual(TEXT("Missing victim clock is inconclusive"), Evaluate().Status, EPairedContactResult::Inconclusive);
	Runtime = Authored; Runtime.RemoveAt(3);
	TestEqual(TEXT("Sparse runtime bracket cannot pass alignment"), Evaluate().Status, EPairedContactResult::Inconclusive);
	Runtime = Authored;
	TestEqual(TEXT("Defect removal restores the original criteria"), Evaluate().Status, EPairedContactResult::Pass);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPairedPreviewSamplingTest, "KatanaCombat.Editor.PairedEvaluation.AbsolutePreviewSampling", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPairedPreviewSamplingTest::RunTest(const FString&)
{
	auto* Subsystem = GEditor->GetEditorSubsystem<UPairedAnimationAnalysisSubsystem>();
	FPairedAnimationPreviewModel Model; FString Error;
	if (!TestTrue(TEXT("Existing finisher profile loads"), Subsystem->LoadContactProfileIntoPreview(FPaths::ProjectDir() / TEXT("Tools/CombatCapture/pairs/finisher-contact.json"), Model, Error))) { AddError(Error); return false; }
	FPreviewScene Scene{FPreviewScene::ConstructionValues()};
	auto* Attacker = NewObject<UDebugSkelMeshComponent>(); auto* Victim = NewObject<UDebugSkelMeshComponent>();
	Scene.AddComponent(Attacker, FTransform::Identity); Scene.AddComponent(Victim, FTransform::Identity);
	auto Sample = [&](double Time) { return Subsystem->SampleContactPreviewPose(Model, Time, Attacker, Victim, Error); };
	TestTrue(TEXT("First absolute time samples"), Sample(.3));
	const FTransform First = Attacker->GetSocketTransform(TEXT("hand_l")); const FTransform VictimFirst = Victim->GetSocketTransform(TEXT("spine_03"));
	Scene.GetWorld()->Tick(LEVELTICK_ViewportsOnly, .1f);
	TestTrue(TEXT("Preview world tick cannot advance beyond the selected attacker time"), First.Equals(Attacker->GetSocketTransform(TEXT("hand_l")), .001));
	TestTrue(TEXT("Preview world tick cannot advance beyond the selected victim time"), VictimFirst.Equals(Victim->GetSocketTransform(TEXT("spine_03")), .001));
	TestTrue(TEXT("Later absolute time samples"), Sample(1.2));
	TestFalse(TEXT("Montage evaluation changes the hand pose"), First.Equals(Attacker->GetSocketTransform(TEXT("hand_l")), .1));
	TestTrue(TEXT("Backward scrub samples"), Sample(.3));
	TestTrue(TEXT("Attacker scrub does not accumulate root motion"), First.Equals(Attacker->GetSocketTransform(TEXT("hand_l")), .001));
	TestTrue(TEXT("Victim scrub does not accumulate root motion"), VictimFirst.Equals(Victim->GetSocketTransform(TEXT("spine_03")), .001));
	TestFalse(TEXT("Outside paired duration is rejected"), Sample(Model.MaxDuration + .1));
	TestTrue(TEXT("Held-out counter loads through the same service"), Subsystem->LoadContactProfileIntoPreview(FPaths::ProjectDir() / TEXT("Tools/CombatCapture/pairs/counter-contact.json"), Model, Error));
	TestTrue(TEXT("Counter samples through the same path"), Sample(.5));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPairedPreviewContactWorkflowTest, "KatanaCombat.Editor.PairedEvaluation.PreviewReportParityAndDefectRemoval", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPairedPreviewContactWorkflowTest::RunTest(const FString&)
{
	auto Read = [](const FString& Path) { FString Text; TSharedPtr<FJsonObject> Json; FFileHelper::LoadFileToString(Text, *Path); FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Json); return Json; };
	const FString Source = FPaths::ProjectDir() / TEXT("Tools/CombatCapture/pairs/finisher-contact.json");
	auto Definition = Read(Source);
	if (!TestTrue(TEXT("Tracked profile parses"), Definition.IsValid())) { return false; }
	// A deliberately constructed hand/torso contact on the real pair is an instrument
	// control. It is not a change to the asset or a promoted quality reference.
	auto Contact = MakeShared<FJsonObject>();
	Contact->SetStringField(TEXT("name"), TEXT("KnownHandContact")); Contact->SetStringField(TEXT("source"), TEXT("Attacker:hand_l")); Contact->SetStringField(TEXT("target"), TEXT("Victim:spine_03"));
	Contact->SetNumberField(TEXT("start_s"), .49); Contact->SetNumberField(TEXT("end_s"), .51); Contact->SetNumberField(TEXT("target_radius_cm"), 0);
	Contact->SetNumberField(TEXT("minimum_gap_cm"), 0); Contact->SetNumberField(TEXT("maximum_gap_cm"), 1); Contact->SetNumberField(TEXT("maximum_sample_gap_s"), .075); Contact->SetBoolField(TEXT("sustained"), false);
	Definition->SetArrayField(TEXT("contacts"), {MakeShared<FJsonValueObject>(Contact)}); Definition->RemoveField(TEXT("alignment_budgets"));
	const FString ProfilePath = FPaths::ProjectSavedDir() / TEXT("PairedEvaluationControls") / (FGuid::NewGuid().ToString() + TEXT(".json"));
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(ProfilePath), true);
	FString Text; FJsonSerializer::Serialize(Definition.ToSharedRef(), TJsonWriterFactory<>::Create(&Text)); FFileHelper::SaveStringToFile(Text, *ProfilePath);
	auto* Subsystem = GEditor->GetEditorSubsystem<UPairedAnimationAnalysisSubsystem>(); FPairedAnimationPreviewModel Model; FString Error, Headless, Preview;
	if (!TestTrue(TEXT("Control profile loads"), Subsystem->LoadContactProfileIntoPreview(ProfilePath, Model, Error))) { AddError(Error); return false; }
	if (!TestTrue(TEXT("Headless profile evaluates"), Subsystem->EvaluateContactProfile(ProfilePath, FString(), Headless, Error))) { AddError(Error); return false; }
	if (!TestTrue(TEXT("Preview profile evaluates"), Subsystem->EvaluateContactProfileForPreview(ProfilePath, Model, Preview, Error))) { AddError(Error); return false; }
	const auto A = Read(Headless / TEXT("evaluation.json")); const auto B = Read(Preview / TEXT("evaluation.json"));
	const auto& ARows = A->GetArrayField(TEXT("cases"))[0]->AsObject()->GetArrayField(TEXT("observations"));
	const auto& BRows = B->GetArrayField(TEXT("cases"))[0]->AsObject()->GetArrayField(TEXT("observations"));
	if (TestEqual(TEXT("Same observation count"), ARows.Num(), BRows.Num()))
	{
		for (int32 I = 0; I < ARows.Num(); ++I) { TestEqual(TEXT("Preview and headless gaps agree numerically"), ARows[I]->AsObject()->GetNumberField(TEXT("signed_gap_cm")), BRows[I]->AsObject()->GetNumberField(TEXT("signed_gap_cm"))); }
	}
	FPreviewScene Scene{FPreviewScene::ConstructionValues()};
	auto* Attacker = NewObject<UDebugSkelMeshComponent>(); auto* Victim = NewObject<UDebugSkelMeshComponent>();
	Scene.AddComponent(Attacker, FTransform::Identity); Scene.AddComponent(Victim, FTransform::Identity);
	if (!TestTrue(TEXT("Control time samples"), Subsystem->SampleContactPreviewPose(Model, .5, Attacker, Victim, Error))) { return false; }
	Model.VictimConfig.PositionOffset += Attacker->GetSocketLocation(TEXT("hand_l")) - Victim->GetSocketLocation(TEXT("spine_03"));
	auto Evaluate = [&](const TCHAR* Expected) {
		FString Directory;
		if (!TestTrue(TEXT("Adjusted preview evaluates"), Subsystem->EvaluateContactProfileForPreview(ProfilePath, Model, Directory, Error))) { AddError(Error); return; }
		TestEqual(TEXT("Controlled contact result"), Read(Directory / TEXT("evaluation.json"))->GetStringField(TEXT("status")), FString(Expected));
	};
	Evaluate(TEXT("pass"));
	Model.VictimConfig.PositionOffset += FVector(500, 0, 0); Evaluate(TEXT("fail"));
	Model.VictimConfig.PositionOffset -= FVector(500, 0, 0); Evaluate(TEXT("pass"));
	Model.VictimTimeOffset = .1;
	TestFalse(TEXT("Unsupported preview timing cannot masquerade as a matching report"), Subsystem->EvaluateContactProfileForPreview(ProfilePath, Model, Preview, Error));
	return true;
}
