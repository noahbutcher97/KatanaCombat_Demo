// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "CombatTypes.h"

struct FAnimNotifyEventReference;

/** Resolve a runtime-stable notify identity from the exact event address in its source animation. */
KATANACOMBAT_API FAnimNotifyRuntimeSourceId ResolveRuntimeNotifySourceId(
	const FAnimNotifyEventReference& EventReference);

/** Resolve the montage instance carried by a runtime notify; missing context fails closed. */
KATANACOMBAT_API int32 ResolveRuntimeMontageInstanceId(
	const FAnimNotifyEventReference& EventReference);
