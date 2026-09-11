// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimTypes.h"
#include "Dom/JsonObject.h"

// Bounded diagnostic settings shared by the PIE fixture and native evaluator.
// The movement override and attacker translation-off control are fixed; only
// the victim's effective notify window and relative offset are varied.
namespace PairedWarpTuning
{
inline bool SetEffectiveWindow(FAnimNotifyEvent& Event, UAnimMontage* Montage, double Start, double End)
{
	Event.Link(Montage, Start - Event.TriggerTimeOffset);
	// Unreal adds the start trigger offset again in GetEndTriggerTime().
	Event.SetDuration(End - Event.EndTriggerTimeOffset - Event.GetTriggerTime());
	return FMath::IsNearlyEqual(static_cast<double>(Event.GetTriggerTime()), Start, 1.e-5)
		&& FMath::IsNearlyEqual(static_cast<double>(Event.GetEndTriggerTime()), End, 1.e-5);
}

inline bool Numbers(const TSharedPtr<FJsonObject>& Object, const FString& Key, int32 Count, TArray<double>& Out)
{
	const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
	if (!Object || !Object->TryGetArrayField(Key, Values) || Values->Num() != Count) { return false; }
	Out.Reset();
	for (const auto& Value : *Values)
	{
		double Number = 0;
		if (!Value || Value->Type != EJson::Number || !Value->TryGetNumber(Number) || !FMath::IsFinite(Number)) { return false; }
		Out.Add(Number);
	}
	return true;
}

inline bool Read(const TSharedPtr<FJsonObject>& Context, TArray<double>& Window, TArray<double>& Offset)
{
	const TSharedPtr<FJsonObject>* Settings = nullptr;
	return Context && Context->TryGetObjectField(TEXT("warp_tuning"), Settings) && (*Settings)->Values.Num() == 2
		&& Numbers(*Settings, TEXT("victim_window_s"), 2, Window) && Numbers(*Settings, TEXT("victim_offset_cm"), 3, Offset)
		&& Window[0] >= 0 && Window[1] <= 5 && Window[1] - Window[0] >= .02
		&& FMath::Abs(Offset[0]) <= 200 && FMath::Abs(Offset[1]) <= 200 && Offset[2] == 0;
}

inline bool Validate(const TSharedPtr<FJsonObject>& Context, const TArray<TSharedPtr<FJsonValue>>& Overrides,
	const FString& AttackerMontage, const FString& VictimMontage, const FString& PairAsset)
{
	TArray<double> Window, Offset;
	if (!Read(Context, Window, Offset) || Overrides.Num() != 5) { return false; }
	TSet<FString> Seen, NotifyIds;
	for (const auto& Value : Overrides)
	{
		const auto Row = Value && Value->Type == EJson::Object ? Value->AsObject() : nullptr;
		FString Role, Property, Class, Asset;
		double Index = -2;
		if (!Row || !Row->TryGetStringField(TEXT("role"), Role) || !Row->TryGetStringField(TEXT("property"), Property)
			|| !Row->TryGetStringField(TEXT("notify_class"), Class) || !Row->TryGetStringField(TEXT("asset"), Asset)
			|| !Row->TryGetNumberField(TEXT("notify_index"), Index) || !FMath::IsFinite(Index) || Index != FMath::FloorToDouble(Index)
			|| (Role != TEXT("Attacker") && Role != TEXT("Victim"))) { return false; }
		const FString Key = Role + TEXT("|") + Property;
		if (Seen.Contains(Key)) { return false; }
		Seen.Add(Key);
		const FString NotifyId = Role + TEXT("|") + Asset + FString::Printf(TEXT("|%.0f"), Index);
		if (NotifyIds.Contains(NotifyId)) { return false; }
		NotifyIds.Add(NotifyId);
		const bool bOffset = Role == TEXT("Victim") && Property == TEXT("VictimWarpConfig.RelativeOffset");
		if (Asset != (bOffset ? PairAsset : Role == TEXT("Attacker") ? AttackerMontage : VictimMontage)
			|| (bOffset ? Index != -1 : Index < 0)) { return false; }
		if (bOffset || (Role == TEXT("Victim") && Property == TEXT("TriggerWindowSeconds")))
		{
			TArray<double> Before, After;
			const auto& Expected = bOffset ? Offset : Window;
			if (Class != (bOffset ? TEXT("PairedAnimationData") : TEXT("AnimNotifyState_MotionWarping"))
				|| !Numbers(Row, TEXT("before"), Expected.Num(), Before) || !Numbers(Row, TEXT("after"), Expected.Num(), After)) { return false; }
			for (int32 I = 0; I < Expected.Num(); ++I) { if (!FMath::IsNearlyEqual(After[I], Expected[I], 1.e-5)) { return false; } }
			if (!bOffset && (Before[0] < 0 || Before[1] <= Before[0] || Before[1] > 5)) { return false; }
		}
		else
		{
			bool Before = false, After = true;
			const bool bMovement = Property == TEXT("bDisableMovement") && Class == TEXT("AnimNotifyState_PairedAnimationCollision");
			const bool bTranslation = Role == TEXT("Attacker") && Property == TEXT("RootMotionModifier.bWarpTranslation") && Class == TEXT("AnimNotifyState_MotionWarping");
			if ((!bMovement && !bTranslation) || !Row->TryGetBoolField(TEXT("before"), Before) || !Before
				|| !Row->TryGetBoolField(TEXT("after"), After) || After) { return false; }
		}
	}
	return Seen.Contains(TEXT("Attacker|bDisableMovement")) && Seen.Contains(TEXT("Victim|bDisableMovement"))
		&& Seen.Contains(TEXT("Attacker|RootMotionModifier.bWarpTranslation")) && Seen.Contains(TEXT("Victim|TriggerWindowSeconds"))
		&& Seen.Contains(TEXT("Victim|VictimWarpConfig.RelativeOffset"));
}
}
