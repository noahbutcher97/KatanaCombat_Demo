// Copyright Epic Games, Inc. All Rights Reserved.

#include "Data/CombatFXData.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

DEFINE_LOG_CATEGORY(LogCombatFX);

UCombatFXData::UCombatFXData()
{
	// Default empty - designers populate pools in editor
}

const FImpactFXPool* UCombatFXData::GetPoolForAttackType(EAttackType AttackType) const
{
	return AttackTypePools.Find(AttackType);
}

const FImpactFXPool* UCombatFXData::GetPoolForSurface(ECombatSurfaceType SurfaceType) const
{
	return SurfacePools.Find(SurfaceType);
}

const FImpactFXPool* UCombatFXData::ResolvePool(
	EAttackType AttackType,
	bool bWasBlocked,
	ECombatSurfaceType SurfaceType) const
{
	// Priority 1: Surface-specific (when wired)
	if (SurfaceType != ECombatSurfaceType::Default)
	{
		if (const FImpactFXPool* SurfacePool = GetPoolForSurface(SurfaceType))
		{
			if (SurfacePool->HasSounds())
			{
				UE_LOG(LogCombatFX, Verbose, TEXT("[POOL] Resolved to surface pool (type: %d)"),
					static_cast<uint8>(SurfaceType));
				return SurfacePool;
			}
		}
	}

	// Priority 2: Blocked pool
	if (bWasBlocked && bUseBlockedPool && BlockedPool.HasSounds())
	{
		UE_LOG(LogCombatFX, Verbose, TEXT("[POOL] Resolved to blocked pool"));
		return &BlockedPool;
	}

	// Priority 3: Attack type pool
	const FImpactFXPool* TypePool = GetPoolForAttackType(AttackType);
	if (TypePool)
	{
		UE_LOG(LogCombatFX, Verbose, TEXT("[POOL] Resolved to attack type pool (type: %d)"),
			static_cast<uint8>(AttackType));
	}
	return TypePool;
}

#if WITH_EDITOR
EDataValidationResult UCombatFXData::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);

	// Warn if no attack type pools configured
	bool bHasAnyPools = false;
	for (const auto& Pair : AttackTypePools)
	{
		if (Pair.Value.HasSounds() || Pair.Value.HasVFX())
		{
			bHasAnyPools = true;
			break;
		}
	}

	if (!bHasAnyPools)
	{
		Context.AddWarning(FText::FromString(TEXT("No attack type pools configured with valid sounds or VFX")));
	}

	// Warn if blocked pool enabled but empty
	if (bUseBlockedPool && !BlockedPool.HasSounds() && !BlockedPool.HasVFX())
	{
		Context.AddWarning(FText::FromString(TEXT("bUseBlockedPool is enabled but BlockedPool has no valid sounds or VFX")));
	}

	return Result;
}
#endif
