// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

/**
 * Editor module for Katana Combat System
 * Provides optional visual tools for designers to work with AttackData assets
 * 
 * Features:
 * - Custom details panel for AttackData with visual timeline
 * - Timing calculation tools
 * - AnimNotify generation tools
 * - Montage section validation
 * 
 * This module is completely optional. The combat system works perfectly
 * without it - this just provides convenience tools for designers.
 */
class FKatanaCombatEditorModule : public IModuleInterface
{
public:
    /** IModuleInterface implementation */
    virtual void StartupModule() override;
    virtual void ShutdownModule() override;

private:
    /** Register custom details customizations */
    void RegisterCustomizations();

    /** Unregister custom details customizations */
    void UnregisterCustomizations();
};