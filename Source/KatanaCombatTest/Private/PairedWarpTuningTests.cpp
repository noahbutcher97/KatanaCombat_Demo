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
