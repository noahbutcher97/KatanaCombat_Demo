#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

/** Strict pose input for named capture fixtures, relative to the fixture's world origin. */
namespace CombatScenarioPlacement
{
inline bool ReadPose(const TSharedPtr<FJsonObject>& Json, FTransform& Out)
{
	const TArray<TSharedPtr<FJsonValue>>* Offset = nullptr;
	if (!Json || Json->Values.Num() != 2 || !Json->TryGetArrayField(TEXT("offset_cm"), Offset) || Offset->Num() != 3) { return false; }
	FVector Location;
	for (int32 Index = 0; Index < 3; ++Index)
	{
		const auto& Value = (*Offset)[Index];
		if (!Value || Value->Type != EJson::Number || !Value->TryGetNumber(Location[Index])
			|| !FMath::IsFinite(Location[Index]) || FMath::Abs(Location[Index]) > 3000) { return false; }
	}
	const auto YawValue = Json->TryGetField(TEXT("yaw_deg"));
	double Yaw;
	if (!YawValue || YawValue->Type != EJson::Number || !YawValue->TryGetNumber(Yaw)
		|| !FMath::IsFinite(Yaw) || FMath::Abs(Yaw) > 180) { return false; }
	Out = FTransform(FRotator(0, Yaw, 0), Location);
	return true;
}

inline bool Read(const TSharedPtr<FJsonObject>& Definition, const FString& Name, TMap<FString, FTransform>& Out)
{
	Out.Reset();
	if (Name == TEXT("default")) { return true; }
	const TSharedPtr<FJsonObject>* Placements = nullptr;
	const TSharedPtr<FJsonObject>* Selected = nullptr;
	if (!Definition || !Definition->TryGetObjectField(TEXT("placements"), Placements)
		|| !(*Placements)->TryGetObjectField(Name, Selected) || (*Selected)->Values.Num() != 2) { return false; }
	TMap<FString, FTransform> Parsed;
	for (const TCHAR* Role : {TEXT("Attacker"), TEXT("Victim")})
	{
		const TSharedPtr<FJsonObject>* PoseJson = nullptr;
		FTransform Pose;
		if (!(*Selected)->TryGetObjectField(Role, PoseJson) || !ReadPose(*PoseJson, Pose)) { return false; }
		Parsed.Add(Role, Pose);
	}
	Out = MoveTemp(Parsed);
	return true;
}

inline TArray<TSharedPtr<FJsonValue>> VectorJson(const FVector& Value)
{
	return {MakeShared<FJsonValueNumber>(Value.X), MakeShared<FJsonValueNumber>(Value.Y), MakeShared<FJsonValueNumber>(Value.Z)};
}
}
