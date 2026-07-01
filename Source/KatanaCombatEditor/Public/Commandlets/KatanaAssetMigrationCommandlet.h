// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "KatanaAssetMigrationCommandlet.generated.h"

UCLASS()
class KATANACOMBATEDITOR_API UKatanaAssetMigrationCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	UKatanaAssetMigrationCommandlet();

	virtual int32 Main(const FString& Params) override;
};
