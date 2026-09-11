// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "PairedAnimationEvaluationCommandlet.generated.h"

UCLASS()
class KATANACOMBATEDITOR_API UPairedAnimationEvaluationCommandlet : public UCommandlet
{
	GENERATED_BODY()
public:
	UPairedAnimationEvaluationCommandlet();
	virtual int32 Main(const FString& Params) override;
};
