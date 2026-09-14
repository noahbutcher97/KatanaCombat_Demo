// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

// Project notify semantics, shared by the transient authoring fixture and native
// evaluator. No montage names or choreography-specific times belong here.
namespace PairedSyncTuning
{
struct FSettings
{
	TOptional<double> Time;
	TOptional<bool> Nudge;
};

inline bool Number(const TSharedPtr<FJsonObject>& Object, const FString& Key, double& Out)
{
	const auto Value = Object ? Object->TryGetField(Key) : nullptr;
	return Value && Value->Type == EJson::Number && Value->TryGetNumber(Out) && FMath::IsFinite(Out);
}

inline bool Read(const TSharedPtr<FJsonObject>& Object, FSettings& Out)
{
	Out = {};
	if (!Object || Object->Values.IsEmpty()) { return false; }
	for (const auto& Field : Object->Values)
	{
		if (Field.Key == TEXT("time_s"))
		{
			double Time = 0;
			if (!Number(Object, Field.Key, Time) || Time < 0 || Time > 5) { return false; }
			Out.Time = Time;
		}
		else if (Field.Key == TEXT("nudge_enabled"))
		{
			bool Nudge = false;
			if (!Field.Value || Field.Value->Type != EJson::Boolean || !Field.Value->TryGetBool(Nudge)) { return false; }
			Out.Nudge = Nudge;
		}
		else { return false; }
	}
	return true;
}

inline TSharedRef<FJsonObject> Snapshot(double Start, double End, bool bNudge)
{
	auto Object = MakeShared<FJsonObject>();
	Object->SetNumberField(TEXT("time_s"), Start); Object->SetNumberField(TEXT("end_s"), End);
	Object->SetBoolField(TEXT("nudge_enabled"), bNudge);
	return Object;
}

inline bool ValidateOverride(const FSettings& Settings, const TSharedPtr<FJsonObject>& Row)
{
	const TSharedPtr<FJsonObject> *Before = nullptr, *After = nullptr;
	if (!Row || !Row->TryGetObjectField(TEXT("before"), Before) || !Row->TryGetObjectField(TEXT("after"), After)
		|| (*Before)->Values.Num() != 3 || (*After)->Values.Num() != 3) { return false; }
	double Start[2], End[2]; bool Nudge[2];
	const TSharedPtr<FJsonObject> Objects[] = {*Before, *After};
	for (int32 I = 0; I < 2; ++I)
	{
		const auto NudgeValue = Objects[I]->TryGetField(TEXT("nudge_enabled"));
		if (!Number(Objects[I], TEXT("time_s"), Start[I]) || !Number(Objects[I], TEXT("end_s"), End[I])
			|| !NudgeValue || NudgeValue->Type != EJson::Boolean || !NudgeValue->TryGetBool(Nudge[I])
			|| Start[I] < -.001 || End[I] > 5 || End[I] <= Start[I]) { return false; }
	}
	return FMath::IsNearlyEqual(Start[1], Settings.Time.Get(Start[0]), 1.e-5)
		&& FMath::IsNearlyEqual(End[1] - Start[1], End[0] - Start[0], 1.e-5)
		&& Nudge[1] == Settings.Nudge.Get(Nudge[0]);
}
}
