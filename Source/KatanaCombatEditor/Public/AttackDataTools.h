// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "CombatTypes.h"
#include "AttackDataTools.generated.h"

class UAttackData;
class UAnimMontage;
class UAnimNotifyState_AttackPhase;
class UAnimNotifyState_ComboWindow;
class UAnimNotify_ToggleHitDetection;
struct FAnimNotifyEvent;

/**
 * Static utility functions for working with AttackData in the editor
 * Provides tools for timing calculation, notify generation, and validation
 * 
 * These are editor-only helper functions that designers can use via
 * the custom details panel or Blueprint utility widgets.
 * 
 * All functions are static and marked UFUNCTION for Blueprint exposure.
 * Can be called from:
 * - Custom details panel (AttackDataCustomization)
 * - Blueprint utility widgets
 * - Editor scripting
 * - Python automation
 * 
 * Note: All functions are editor-only. No runtime overhead.
 */
UCLASS()
class KATANACOMBATEDITOR_API UAttackDataTools : public UObject
{
    GENERATED_BODY()

public:
    // ============================================================================
    // TIMING CALCULATION
    // ============================================================================

    /**
     * Auto-calculate reasonable timing from montage/section length and attack type
     * Uses heuristics based on attack type:
     * - Light: 30% windup, 20% active, 50% recovery
     * - Heavy: 40% windup, 30% active, 30% recovery
     * - Special: 35% windup, 25% active, 40% recovery
     * 
     * @param AttackData - Attack to calculate timing for
     * @return True if timing was successfully calculated
     */
    UFUNCTION(BlueprintCallable, Category = "Attack Data Tools")
    static bool AutoCalculateTiming(UAttackData* AttackData);

    /**
     * Calculate timing based on existing AnimNotifyStates in montage
     * Reads AnimNotifyState_AttackPhase notifies and extracts timing
     * 
     * @param AttackData - Attack to extract timing from
     * @param OutWindup - Output windup duration (seconds)
     * @param OutActive - Output active duration (seconds)
     * @param OutRecovery - Output recovery duration (seconds)
     * @return True if valid notifies were found
     */
    UFUNCTION(BlueprintCallable, Category = "Attack Data Tools")
    static bool ExtractTimingFromNotifies(UAttackData* AttackData, float& OutWindup, float& OutActive, float& OutRecovery);

    /**
     * Get timing percentages for visual display
     * Converts absolute times to percentages of total section length
     * 
     * @param AttackData - Attack to analyze
     * @param OutWindupPercent - Windup as percent (0-1)
     * @param OutActivePercent - Active as percent (0-1)
     * @param OutRecoveryPercent - Recovery as percent (0-1)
     * @return True if timing could be calculated
     */
    UFUNCTION(BlueprintCallable, Category = "Attack Data Tools")
    static bool GetTimingPercentages(UAttackData* AttackData, float& OutWindupPercent, float& OutActivePercent, float& OutRecoveryPercent);

    // ============================================================================
    // ANIMNOTIFY GENERATION
    // ============================================================================

    /**
     * Generate AnimNotifyState_AttackPhase notifies in the target section
     * Creates Windup, Active, and Recovery notifies based on calculated timing
     * 
     * @param AttackData - Attack to generate notifies for
     * @return True if notifies were successfully generated
     */
    UFUNCTION(BlueprintCallable, Category = "Attack Data Tools")
    static bool GenerateAttackPhaseNotifies(UAttackData* AttackData);

    /**
     * Generate AnimNotify_ToggleHitDetection notifies for active phase
     * Places Enable at start of active phase, Disable at end
     * 
     * @param AttackData - Attack to generate notifies for
     * @return True if notifies were successfully generated
     */
    UFUNCTION(BlueprintCallable, Category = "Attack Data Tools")
    static bool GenerateHitDetectionNotifies(UAttackData* AttackData);

    /**
     * Generate AnimNotifyState_ComboWindow notify during recovery
     * Places window at start of recovery phase for combo input
     * 
     * @param AttackData - Attack to generate notify for
     * @return True if notify was successfully generated
     */
    UFUNCTION(BlueprintCallable, Category = "Attack Data Tools")
    static bool GenerateComboWindowNotify(UAttackData* AttackData);

    /**
     * Generate all notifies (AttackPhase + HitDetection + ComboWindow)
     * Convenience function that calls all generation functions
     * 
     * @param AttackData - Attack to generate notifies for
     * @return True if all notifies were successfully generated
     */
    UFUNCTION(BlueprintCallable, Category = "Attack Data Tools")
    static bool GenerateAllNotifies(UAttackData* AttackData);

    // ============================================================================
    // VALIDATION
    // ============================================================================

    /**
     * Validate montage section exists and is properly configured
     * Checks section name, montage reference, and section length
     * 
     * @param AttackData - Attack to validate
     * @param OutErrorMessage - Human-readable error message
     * @return True if section is valid
     */
    UFUNCTION(BlueprintCallable, Category = "Attack Data Tools")
    static bool ValidateMontageSection(UAttackData* AttackData, FText& OutErrorMessage);

    /**
     * Find other AttackData assets using the same montage section
     * Used to detect conflicts where multiple attacks use same section
     * 
     * @param AttackData - Attack to check conflicts for
     * @return Array of conflicting AttackData assets
     */
    UFUNCTION(BlueprintCallable, Category = "Attack Data Tools")
    static TArray<UAttackData*> FindSectionConflicts(UAttackData* AttackData);

    /**
     * Check if montage has valid AnimNotifyStates for timing
     * Validates that required notifies exist in the target section
     * 
     * @param AttackData - Attack to check
     * @return True if valid notifies exist
     */
    UFUNCTION(BlueprintCallable, Category = "Attack Data Tools")
    static bool HasValidNotifyTiming(UAttackData* AttackData);

    /**
     * Validate entire AttackData configuration
     * Comprehensive check of all settings and references
     * 
     * @param AttackData - Attack to validate
     * @param OutWarnings - Array of warning messages
     * @param OutErrors - Array of error messages
     * @return True if no errors (warnings are acceptable)
     */
    UFUNCTION(BlueprintCallable, Category = "Attack Data Tools")
    static bool ValidateAttackData(UAttackData* AttackData, TArray<FText>& OutWarnings, TArray<FText>& OutErrors);

    // ============================================================================
    // VISUALIZATION
    // ============================================================================

    /**
     * Get a preview string showing the timing layout
     * Returns formatted text like: "[Windup 0.3s][Active 0.2s][Recovery 0.5s]"
     * 
     * @param AttackData - Attack to preview
     * @return Formatted timing string
     */
    UFUNCTION(BlueprintCallable, Category = "Attack Data Tools")
    static FString GetTimingPreview(UAttackData* AttackData);

    /**
     * Get detailed timing info as structured data
     * Useful for custom UI visualizations
     * 
     * @param AttackData - Attack to analyze
     * @param OutPhaseTiming - Output timing structure
     * @return True if timing could be determined
     */
    UFUNCTION(BlueprintCallable, Category = "Attack Data Tools")
    static bool GetDetailedTiming(UAttackData* AttackData, FAttackPhaseTiming& OutPhaseTiming);

    // ============================================================================
    // MONTAGE UTILITIES
    // ============================================================================

    /**
     * Get all section names from a montage
     * 
     * @param Montage - Montage to query
     * @return Array of section names
     */
    UFUNCTION(BlueprintCallable, Category = "Attack Data Tools")
    static TArray<FName> GetMontageSections(UAnimMontage* Montage);

    /**
     * Get section length in seconds
     * 
     * @param Montage - Montage containing the section
     * @param SectionName - Section to query (NAME_None = entire montage)
     * @return Section length in seconds
     */
    UFUNCTION(BlueprintCallable, Category = "Attack Data Tools")
    static float GetSectionLength(UAnimMontage* Montage, FName SectionName);

    /**
     * Get section start time in seconds
     * 
     * @param Montage - Montage containing the section
     * @param SectionName - Section to query
     * @return Section start time in seconds
     */
    UFUNCTION(BlueprintCallable, Category = "Attack Data Tools")
    static float GetSectionStartTime(UAnimMontage* Montage, FName SectionName);

    /**
     * Check if section exists in montage
     * 
     * @param Montage - Montage to check
     * @param SectionName - Section name to look for
     * @return True if section exists
     */
    UFUNCTION(BlueprintCallable, Category = "Attack Data Tools")
    static bool DoesSectionExist(UAnimMontage* Montage, FName SectionName);

    // ============================================================================
    // ASSET DISCOVERY
    // ============================================================================

    /**
     * Find all AttackData assets in project
     * Useful for batch operations and conflict detection
     * 
     * @return Array of all AttackData assets
     */
    UFUNCTION(BlueprintCallable, Category = "Attack Data Tools")
    static TArray<UAttackData*> FindAllAttackDataAssets();

    /**
     * Find AttackData assets using specific montage
     * 
     * @param Montage - Montage to search for
     * @return Array of AttackData using this montage
     */
    UFUNCTION(BlueprintCallable, Category = "Attack Data Tools")
    static TArray<UAttackData*> FindAttackDataUsingMontage(UAnimMontage* Montage);

    /**
     * Find AttackData assets of specific type
     * 
     * @param AttackType - Type to filter by
     * @return Array of AttackData of this type
     */
    UFUNCTION(BlueprintCallable, Category = "Attack Data Tools")
    static TArray<UAttackData*> FindAttackDataByType(EAttackType AttackType);

    // ============================================================================
    // BATCH OPERATIONS
    // ============================================================================

    /**
     * Batch generate notifies for multiple AttackData assets
     * 
     * @param AttackDataArray - Assets to process
     * @param OutSuccessCount - Number of successful operations
     * @param OutFailureCount - Number of failed operations
     * @return True if at least one succeeded
     */
    UFUNCTION(BlueprintCallable, Category = "Attack Data Tools")
    static bool BatchGenerateNotifies(const TArray<UAttackData*>& AttackDataArray, int32& OutSuccessCount, int32& OutFailureCount);

    /**
     * Batch validate multiple AttackData assets
     * 
     * @param AttackDataArray - Assets to validate
     * @param OutValidAssets - Assets that passed validation
     * @param OutInvalidAssets - Assets that failed validation
     */
    UFUNCTION(BlueprintCallable, Category = "Attack Data Tools")
    static void BatchValidate(const TArray<UAttackData*>& AttackDataArray, TArray<UAttackData*>& OutValidAssets, TArray<UAttackData*>& OutInvalidAssets);

private:
    // ============================================================================
    // INTERNAL HELPERS
    // ============================================================================

    /** Add notify state to montage at specific time */
    static bool AddNotifyStateToMontage(UAnimMontage* Montage, float StartTime, float Duration, UAnimNotifyState* NotifyState, FName SectionName);

    /** Add notify to montage at specific time */
    static bool AddNotifyToMontage(UAnimMontage* Montage, float Time, UAnimNotify* Notify, FName SectionName);

    /** Remove all notifies of specific type from section */
    static void RemoveNotifiesOfType(UAnimMontage* Montage, FName SectionName, UClass* NotifyClass);

    /** Convert section-relative time to montage-absolute time */
    static float SectionTimeToMontageTime(UAnimMontage* Montage, FName SectionName, float SectionRelativeTime);

    /** Get default timing percentages for attack type */
    static void GetDefaultTimingPercentages(EAttackType AttackType, float& OutWindupPercent, float& OutActivePercent, float& OutRecoveryPercent);

    /** Mark montage as modified (for saving) */
    static void MarkMontageModified(UAnimMontage* Montage);

    /** Log editor message (for debugging tool usage) */
    static void LogToolMessage(const FString& Message, bool bIsWarning = false);
};