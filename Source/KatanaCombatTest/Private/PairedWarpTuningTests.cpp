// Copyright Epic Games, Inc. All Rights Reserved.
#include "Misc/AutomationTest.h"
#include "Analysis/PairedWarpTuning.h"
#include "Animation/AnimMontage.h"
#include "AnimNotifyState_MotionWarping.h"
#include "Serialization/JsonSerializer.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPairedWarpTuningEffectiveWindow,
	"KatanaCombat.Editor.PairedEvaluation.WarpTuning.EffectiveWindow", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPairedWarpTuningEffectiveWindow::RunTest(const FString& Parameters)
{
	auto* Montage = NewObject<UAnimMontage>(); Montage->SetCompositeLength(3);
	FAnimNotifyEvent Event; Event.NotifyStateClass = NewObject<UAnimNotifyState_MotionWarping>(Montage);
	Event.TriggerTimeOffset = .0001f; Event.EndTriggerTimeOffset = -.0001f;
	for (const FVector2D Window : {FVector2D(.0001, .18), FVector2D(.2, .43)})
	{
		TestTrue(TEXT("Effective times match despite nonzero trigger offsets"), PairedWarpTuning::SetEffectiveWindow(Event, Montage, Window.X, Window.Y));
		TestTrue(TEXT("Requested end is not shifted by the start offset"), FMath::IsNearlyEqual(Event.GetEndTriggerTime(), static_cast<float>(Window.Y), 1.e-6f));
		TestEqual(TEXT("Start offset preserved"), Event.TriggerTimeOffset, .0001f);
		TestEqual(TEXT("End offset preserved"), Event.EndTriggerTimeOffset, -.0001f);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPairedWarpTuningBounds,
	"KatanaCombat.Editor.PairedEvaluation.WarpTuning.RequestBounds", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPairedWarpTuningBounds::RunTest(const FString& Parameters)
{
	for (const FString& Json : {TEXT("{\"warp_tuning\":{\"victim_window_s\":[0.2,0.43],\"victim_offset_cm\":[65,0,0]}}"),
		TEXT("{\"warp_tuning\":{\"victim_window_s\":[0.43,0.2],\"victim_offset_cm\":[65,0,0]}}"),
		TEXT("{\"warp_tuning\":{\"victim_window_s\":[0.2,0.43],\"victim_offset_cm\":[201,0,0]}}"),
		TEXT("{\"warp_tuning\":{\"victim_window_s\":[true,0.43],\"victim_offset_cm\":[65,0,0]}}")})
	{
		TSharedPtr<FJsonObject> Context; FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), Context);
		TArray<double> Window, Offset;
		const bool bExpected = Json.Contains(TEXT("[0.2,0.43]")) && Json.Contains(TEXT("[65,0,0]"));
		TestEqual(TEXT("Accept supported request; reject reversed window, remote offset and boolean time"), PairedWarpTuning::Read(Context, Window, Offset), bExpected);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPairedWarpTuningFacing,
	"KatanaCombat.Editor.PairedEvaluation.WarpTuning.FacingProvenance", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPairedWarpTuningFacing::RunTest(const FString&)
{
	TSharedPtr<FJsonObject> Context;
	FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(FString(TEXT(R"({"warp_tuning":{"victim_window_s":[0.2,0.87],"victim_offset_cm":[50,0,0]}})"))), Context);
	TArray<TSharedPtr<FJsonValue>> Rows;
	const FString RowsJson = TEXT(R"([
		{"role":"Attacker","asset":"/Game/Attacker","notify_index":1,"notify_class":"AnimNotifyState_PairedAnimationCollision","property":"bDisableMovement","before":true,"after":false},
		{"role":"Victim","asset":"/Game/Victim","notify_index":1,"notify_class":"AnimNotifyState_PairedAnimationCollision","property":"bDisableMovement","before":true,"after":false},
		{"role":"Attacker","asset":"/Game/Attacker","notify_index":0,"notify_class":"AnimNotifyState_MotionWarping","property":"RootMotionModifier.bWarpTranslation","before":true,"after":false},
		{"role":"Victim","asset":"/Game/Victim","notify_index":0,"notify_class":"AnimNotifyState_MotionWarping","property":"TriggerWindowSeconds","before":[0.0001,0.873],"after":[0.2,0.87]},
		{"role":"Victim","asset":"/Game/Pair","notify_index":-1,"notify_class":"PairedAnimationData","property":"VictimWarpConfig.RelativeOffset","before":[50,0,0],"after":[50,0,0]}
	])");
	FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(RowsJson), Rows);
	auto Validate = [&]() { return PairedWarpTuning::Validate(Context, Rows, TEXT("/Game/Attacker"), TEXT("/Game/Victim"), TEXT("/Game/Pair")); };
	TestTrue(TEXT("Legacy request retains exactly five overrides"), Validate());
	auto Settings = Context->GetObjectField(TEXT("warp_tuning"));
	Settings->SetStringField(TEXT("victim_facing_policy"), TEXT("match-partner-heading"));
	TestFalse(TEXT("Requested facing must be recorded"), Validate());
	auto Row = MakeShared<FJsonObject>();
	Row->SetStringField(TEXT("role"), TEXT("Victim")); Row->SetStringField(TEXT("asset"), TEXT("/Game/Pair"));
	Row->SetNumberField(TEXT("notify_index"), -1); Row->SetStringField(TEXT("notify_class"), TEXT("PairedAnimationData"));
	Row->SetStringField(TEXT("property"), TEXT("VictimWarpConfig.FacingPolicy")); Row->SetStringField(TEXT("before"), TEXT("face-partner"));
	Rows.Add(MakeShared<FJsonValueObject>(Row));
	for (const auto Policy : {EPairedFacingPolicy::FacePartner, EPairedFacingPolicy::FaceAwayFromPartner, EPairedFacingPolicy::MatchPartnerHeading})
	{
		const FString Name = PairedWarpTuning::FacingName(Policy);
		Settings->SetStringField(TEXT("victim_facing_policy"), Name); Row->SetStringField(TEXT("after"), Name);
		TestTrue(TEXT("Each explicit facing policy is accepted with matching provenance"), Validate());
		Row->SetStringField(TEXT("after"), TEXT("unknown"));
		TestFalse(TEXT("Mismatching or unknown captured facing fails closed"), Validate());
	}
	Row->SetStringField(TEXT("after"), TEXT("match-partner-heading"));
	Row->SetStringField(TEXT("asset"), TEXT("/Game/Unrelated"));
	TestFalse(TEXT("Facing and offset must belong to the actual pair asset"), Validate());
	Row->SetStringField(TEXT("asset"), TEXT("/Game/Pair"));
	Settings->RemoveField(TEXT("victim_facing_policy"));
	TestFalse(TEXT("Unrequested facing override fails closed"), Validate());
	Settings->SetBoolField(TEXT("victim_facing_policy"), true);
	TestFalse(TEXT("Boolean policy is not silently defaulted"), Validate());
	Settings->RemoveField(TEXT("victim_facing_policy")); Rows.Pop();
	auto Sync = MakeShared<FJsonObject>(); Sync->SetNumberField(TEXT("time_s"), .6); Sync->SetBoolField(TEXT("nudge_enabled"), false);
	Settings->SetObjectField(TEXT("primary_sync"), Sync);
	TestFalse(TEXT("Sync tuning requires both role overrides"), Validate());
	for (const FString Role : {TEXT("Attacker"), TEXT("Victim")})
	{
		auto SyncRow = MakeShared<FJsonObject>();
		SyncRow->SetStringField(TEXT("role"), Role); SyncRow->SetStringField(TEXT("asset"), TEXT("/Game/") + Role);
		SyncRow->SetNumberField(TEXT("notify_index"), 2); SyncRow->SetStringField(TEXT("notify_class"), TEXT("AnimNotifyState_PairedAnimationSync"));
		SyncRow->SetStringField(TEXT("property"), TEXT("PrimarySyncSettings"));
		SyncRow->SetObjectField(TEXT("before"), PairedSyncTuning::Snapshot(.0001, .0801, true));
		SyncRow->SetObjectField(TEXT("after"), PairedSyncTuning::Snapshot(.6, .68, false));
		Rows.Add(MakeShared<FJsonValueObject>(SyncRow));
	}
	TestTrue(TEXT("Both exact sync overrides are accepted"), Validate());
	Rows.Last()->AsObject()->SetNumberField(TEXT("notify_index"), 1);
	TestFalse(TEXT("Sync cannot reuse collision notify identity"), Validate());
	Rows.Last()->AsObject()->SetNumberField(TEXT("notify_index"), 2);
	Rows.Last()->AsObject()->GetObjectField(TEXT("after"))->SetNumberField(TEXT("time_s"), .5);
	TestFalse(TEXT("Sync timing must match the request"), Validate());
	Rows.Last()->AsObject()->GetObjectField(TEXT("after"))->SetNumberField(TEXT("time_s"), .6);
	FPairedEntryConfig Entry; Entry.bEnabled = true;
	Settings->SetObjectField(TEXT("entry"), PairedEntryTuning::Snapshot(Entry));
	TestFalse(TEXT("Entry request needs its own recorded override"), Validate());
	auto EntryRow = MakeShared<FJsonObject>();
	EntryRow->SetStringField(TEXT("role"), TEXT("Victim")); EntryRow->SetStringField(TEXT("asset"), TEXT("/Game/Pair"));
	EntryRow->SetNumberField(TEXT("notify_index"), -1); EntryRow->SetStringField(TEXT("notify_class"), TEXT("PairedAnimationData"));
	EntryRow->SetStringField(TEXT("property"), TEXT("Entry"));
	EntryRow->SetObjectField(TEXT("before"), PairedEntryTuning::Snapshot(FPairedEntryConfig{}));
	EntryRow->SetObjectField(TEXT("after"), PairedEntryTuning::Snapshot(Entry)); Rows.Add(MakeShared<FJsonValueObject>(EntryRow));
	TestTrue(TEXT("Native evaluation accepts the exact entry pose and limits"), Validate());
	EntryRow->GetObjectField(TEXT("after"))->SetNumberField(TEXT("travel_budget_cm"), 151);
	TestFalse(TEXT("Changed entry budget cannot pass provenance"), Validate());
	EntryRow->SetObjectField(TEXT("after"), PairedEntryTuning::Snapshot(Entry));
	EntryRow->GetObjectField(TEXT("before"))->SetBoolField(TEXT("duration_s"), false);
	TestFalse(TEXT("Invalid before configuration cannot pass provenance"), Validate());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPairedSyncTuningIndependentFields,
	"KatanaCombat.Editor.PairedEvaluation.SyncTuning.IndependentFields", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPairedSyncTuningIndependentFields::RunTest(const FString&)
{
	auto Request = MakeShared<FJsonObject>(); PairedSyncTuning::FSettings Settings;
	TestFalse(TEXT("Empty sync request rejected"), PairedSyncTuning::Read(Request, Settings));
	Request->SetBoolField(TEXT("time_s"), true);
	TestFalse(TEXT("Boolean time rejected"), PairedSyncTuning::Read(Request, Settings));
	Request->SetNumberField(TEXT("time_s"), 6);
	TestFalse(TEXT("Out-of-range time rejected"), PairedSyncTuning::Read(Request, Settings));
	Request->SetNumberField(TEXT("time_s"), .6);
	TestTrue(TEXT("Time-only request accepted"), PairedSyncTuning::Read(Request, Settings));
	auto Row = MakeShared<FJsonObject>();
	Row->SetObjectField(TEXT("before"), PairedSyncTuning::Snapshot(.0001, .0801, true));
	Row->SetObjectField(TEXT("after"), PairedSyncTuning::Snapshot(.6, .68, true));
	TestTrue(TEXT("Time-only control preserves nudge and duration"), PairedSyncTuning::ValidateOverride(Settings, Row));
	Row->GetObjectField(TEXT("after"))->SetBoolField(TEXT("nudge_enabled"), false);
	TestFalse(TEXT("Unrequested nudge change rejected"), PairedSyncTuning::ValidateOverride(Settings, Row));
	Request->RemoveField(TEXT("time_s")); Request->SetBoolField(TEXT("nudge_enabled"), false);
	TestTrue(TEXT("Nudge-only request accepted"), PairedSyncTuning::Read(Request, Settings));
	TestFalse(TEXT("Unrequested timing change rejected"), PairedSyncTuning::ValidateOverride(Settings, Row));
	Row->SetObjectField(TEXT("after"), PairedSyncTuning::Snapshot(.0001, .0801, false));
	TestTrue(TEXT("Nudge-only control preserves exact trigger window"), PairedSyncTuning::ValidateOverride(Settings, Row));
	Row->GetObjectField(TEXT("after"))->SetNumberField(TEXT("nudge_enabled"), 0);
	TestFalse(TEXT("Numeric nudge provenance is not coerced to boolean"), PairedSyncTuning::ValidateOverride(Settings, Row));
	Request->SetBoolField(TEXT("unknown"), true);
	TestFalse(TEXT("Unknown sync settings rejected"), PairedSyncTuning::Read(Request, Settings));
	return true;
}
