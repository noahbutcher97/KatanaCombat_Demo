#pragma once
#include "Data/PairedAnimationTypes.h"
#include "Utilities/AlignmentMotionLibrary.h"
#include "Dom/JsonObject.h"
#include "Animation/AnimSequence.h"

namespace PairedEntryTuning
{
inline TSharedRef<FJsonObject> Snapshot(const FPairedEntryConfig& C)
{
	auto O=MakeShared<FJsonObject>(); O->SetBoolField(TEXT("enabled"),C.bEnabled);
	TArray<TSharedPtr<FJsonValue>> Offset;
	for(int I=0;I<3;++I) {Offset.Add(MakeShared<FJsonValueNumber>(C.VictimRelativeTransform.GetLocation()[I]));}
	O->SetArrayField(TEXT("victim_offset_cm"),Offset);
	O->SetNumberField(TEXT("victim_yaw_deg"),C.VictimRelativeTransform.Rotator().Yaw);
	O->SetNumberField(TEXT("duration_s"),C.Limits.Duration);
	O->SetNumberField(TEXT("translation_speed_cm_s"),C.Limits.TranslationSpeed);
	O->SetNumberField(TEXT("travel_budget_cm"),C.Limits.TravelBudget);
	O->SetNumberField(TEXT("turn_rate_deg_s"),C.Limits.TurnRate);
	O->SetNumberField(TEXT("turn_budget_deg"),C.Limits.TurnBudget);
	O->SetNumberField(TEXT("position_tolerance_cm"),C.Limits.PositionTolerance);
	O->SetNumberField(TEXT("yaw_tolerance_deg"),C.Limits.YawTolerance);
	O->SetStringField(TEXT("moving_role"), C.MovingRole == EPairedEntryMovingRole::Initiator ? TEXT("initiator") : TEXT("victim"));
	O->SetStringField(TEXT("movement_animation"), C.MovementAnimation ? C.MovementAnimation->GetPathName() : TEXT(""));
	O->SetStringField(TEXT("movement_slot"), C.MovementSlot.ToString());
	O->SetNumberField(TEXT("movement_play_rate"), C.MovementPlayRate);
	O->SetNumberField(TEXT("movement_blend_in_s"), C.MovementBlendIn);
	O->SetNumberField(TEXT("movement_blend_out_s"), C.MovementBlendOut);
	return O;
}

inline bool Read(const TSharedPtr<FJsonObject>& O,FPairedEntryConfig& C)
{
	C = FPairedEntryConfig();
	if(!O || (O->Values.Num()!=10 && O->Values.Num()!=16) || !O->TryGetBoolField(TEXT("enabled"),C.bEnabled)) {return false;}
	const TArray<TSharedPtr<FJsonValue>>* Values=nullptr;
	if(!O->TryGetArrayField(TEXT("victim_offset_cm"),Values) || Values->Num()!=3) {return false;}
	FVector Offset;
	for(int I=0;I<3;++I)
	{
		if((*Values)[I]->Type!=EJson::Number || !(*Values)[I]->TryGetNumber(Offset[I]) || !FMath::IsFinite(Offset[I])) {return false;}
	}
	const auto Number=[&](const TCHAR* Key,double& Out)
	{
		const auto Value=O->TryGetField(Key);
		return Value && Value->Type==EJson::Number && Value->TryGetNumber(Out) && FMath::IsFinite(Out);
	};
	double Yaw;
	if(!Number(TEXT("victim_yaw_deg"),Yaw) || Yaw < -180 || Yaw > 180) {return false;}
	C.VictimRelativeTransform=FTransform(FRotator(0,Yaw,0),Offset);
	const TCHAR* Keys[]={TEXT("duration_s"),TEXT("translation_speed_cm_s"),TEXT("travel_budget_cm"),TEXT("turn_rate_deg_s"),TEXT("turn_budget_deg"),TEXT("position_tolerance_cm"),TEXT("yaw_tolerance_deg")};
	float* Fields[]={&C.Limits.Duration,&C.Limits.TranslationSpeed,&C.Limits.TravelBudget,&C.Limits.TurnRate,&C.Limits.TurnBudget,&C.Limits.PositionTolerance,&C.Limits.YawTolerance};
	for(int I=0;I<7;++I) {double Value; if(!Number(Keys[I],Value)) {return false;} *Fields[I]=Value;}
	if (O->Values.Num() == 16)
	{
		FString Role, Animation, Slot; double Rate, BlendIn, BlendOut;
		if (!O->TryGetStringField(TEXT("moving_role"), Role) || (Role != TEXT("initiator") && Role != TEXT("victim"))
			|| !O->TryGetStringField(TEXT("movement_animation"), Animation)
			|| !O->TryGetStringField(TEXT("movement_slot"), Slot) || Slot.IsEmpty() || FName(*Slot).IsNone()
			|| !Number(TEXT("movement_play_rate"), Rate) || Rate < .01 || Rate > 10
			|| !Number(TEXT("movement_blend_in_s"), BlendIn) || BlendIn < 0 || BlendIn > 1
			|| !Number(TEXT("movement_blend_out_s"), BlendOut) || BlendOut < 0 || BlendOut > 1) { return false; }
		C.MovingRole = Role == TEXT("initiator") ? EPairedEntryMovingRole::Initiator : EPairedEntryMovingRole::Victim;
		if (!Animation.IsEmpty())
		{
			C.MovementAnimation = LoadObject<UAnimSequence>(nullptr, *Animation);
			if (!C.MovementAnimation || C.MovementAnimation->GetPathName() != Animation) { return false; }
		}
		C.MovementSlot = FName(*Slot); C.MovementPlayRate = Rate;
		C.MovementBlendIn = BlendIn; C.MovementBlendOut = BlendOut;
	}
	return AlignmentMotion::IsValid(C.Limits) && AlignmentMotion::IsValidGoal(C.VictimRelativeTransform);
}

inline bool ValidateOverride(const FPairedEntryConfig& Requested,const TSharedPtr<FJsonObject>& Row)
{
	const TSharedPtr<FJsonObject>* Before=nullptr; const TSharedPtr<FJsonObject>* After=nullptr;
	FPairedEntryConfig B,A;
	if(!Row->TryGetObjectField(TEXT("before"),Before) || !Row->TryGetObjectField(TEXT("after"),After)
		|| !Read(*Before,B) || !Read(*After,A) || A.bEnabled!=Requested.bEnabled
		|| !A.VictimRelativeTransform.Equals(Requested.VictimRelativeTransform,1.e-4)) {return false;}
	const auto Expected=Snapshot(Requested),Actual=Snapshot(A);
	for(const auto& Pair:Expected->Values)
	{
		if(Pair.Value->Type==EJson::Number && !FMath::IsNearlyEqual(Pair.Value->AsNumber(),Actual->GetNumberField(Pair.Key),1.e-4)) {return false;}
		if(Pair.Value->Type==EJson::String && Pair.Value->AsString()!=Actual->GetStringField(Pair.Key)) {return false;}
	}
	return true;
}
}
