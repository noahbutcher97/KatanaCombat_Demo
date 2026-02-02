// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * PT-16: Centralized configuration for SPairedAnimationPreview.
 *
 * These values were previously hardcoded throughout PairedAnimationPreview.cpp.
 * Centralizing them here provides:
 * - Single source of truth for all magic numbers
 * - Easy tuning without searching through thousands of lines
 * - Potential future migration to UDeveloperSettings for Project Settings UI
 * - Reference from FPairedAnimationPreviewModel for default values
 *
 * Usage:
 *   #include "PairedAnimationPreviewConfig.h"
 *   float Dist = PairedAnimPreviewConfig::Character::DefaultDistance;
 *   FColor Color = PairedAnimPreviewConfig::Colors::ContactGood;
 */
namespace PairedAnimPreviewConfig
{
	// ========================================================================
	// CAMERA SETTINGS
	// ========================================================================
	// Note: FVector/FRotator constructors aren't constexpr in UE5, so use inline functions
	namespace Camera
	{
		inline FVector GetInitialLocation() { return FVector(-400.0f, 0.0f, 100.0f); }
		inline FRotator GetInitialRotation() { return FRotator(-10.0f, 0.0f, 0.0f); }
		constexpr int32 SpeedSetting = 3;
		constexpr float FocusDistance = 300.0f;
		constexpr float FocusHeightRatio = 0.3f;
	}

	// ========================================================================
	// CHARACTER SETUP
	// ========================================================================
	namespace Character
	{
		constexpr float MeshYawOffset = -90.0f;		// Skeletal mesh yaw offset from capsule
		constexpr float DefaultDistance = 150.0f;	// Default distance between characters
		constexpr float VictimFacingYaw = 180.0f;	// Victim faces attacker
	}

	// ========================================================================
	// SPATIAL RELATIONSHIP CONSTRAINTS
	// ========================================================================
	// Note: These values are also used in FSpatialRotationConstraint::CreateForRelationship()
	// If modifying, update both locations to stay in sync.
	namespace SpatialConstraints
	{
		constexpr float FacingYaw = 180.0f;			// Victim faces attacker
		constexpr float BehindYaw = 0.0f;			// Attacker behind victim
		constexpr float LeftSideYaw = 90.0f;		// Attacker on victim's left
		constexpr float RightSideYaw = -90.0f;		// Attacker on victim's right
		constexpr float DefaultTolerance = 30.0f;	// Rotation tolerance in degrees
	}

	// ========================================================================
	// DEBUG VISUALIZATION COLORS
	// ========================================================================
	namespace Colors
	{
		const FColor ContactBad = FColor::Red;
		const FColor ContactMediocre = FColor::Yellow;
		const FColor ContactGood = FColor::Green;
		const FColor CurrentConnection = FColor::Red;
		const FColor OptimalConnection = FColor::Cyan;
		const FColor ContactSphere = FColor::Cyan;
		const FColor PredictedContact = FColor(255, 200, 50);  // Gold
		const FColor JointViolation = FColor::Red;
		const FColor JointNormal = FColor::Green;
		const FColor Trajectory = FColor::Orange;
		const FColor WeaponLine = FColor::Red;
		const FColor WeaponStart = FColor::Green;
		const FColor WeaponEnd = FColor::Red;
		const FColor CenterOfMass = FColor::Orange;
		const FColor ForwardArrow = FColor::Cyan;
		const FColor VictimForwardArrow = FColor::Orange;
	}

	// ========================================================================
	// DEBUG VISUALIZATION SIZES
	// ========================================================================
	namespace Sizes
	{
		constexpr float ContactSphereRadius = 10.0f;
		constexpr float SmallSphereRadius = 5.0f;
		constexpr float MediumSphereRadius = 8.0f;
		constexpr float LargeSphereRadius = 16.0f;
		constexpr float ThinLineWidth = 1.0f;
		constexpr float MediumLineWidth = 2.0f;
		constexpr float ThickLineWidth = 3.0f;
		constexpr float ArrowSize = 15.0f;
	}

	// ========================================================================
	// ANALYSIS THRESHOLDS
	// ========================================================================
	namespace Analysis
	{
		constexpr float GoodContactThreshold = 0.7f;
		constexpr float MediocreContactThreshold = 0.4f;
		constexpr float HighConfidence = 0.9f;
		constexpr float ConfidenceAngleDivisor = 90.0f;
	}

	// ========================================================================
	// CONTACT TYPE WEIGHTS FOR MULTI-CONTACT ANALYSIS
	// ========================================================================
	// Note: These values are also used in FPairedAnimationPreviewModel::InitializeContactTypeWeights()
	// If modifying, update both locations to stay in sync.
	namespace Weights
	{
		constexpr float Head = 1.0f;
		constexpr float LeftHand = 0.8f;
		constexpr float RightHand = 1.2f;		// Weapon hand - higher priority
		constexpr float LeftFoot = 0.5f;
		constexpr float RightFoot = 0.5f;
		constexpr float Pelvis = 0.6f;
		constexpr float WeaponTip = 1.5f;		// Highest priority - impact point
		constexpr float WeaponMid = 1.0f;
		constexpr float WeaponBase = 0.7f;
	}

	// ========================================================================
	// MODEL DEFAULT VALUES
	// ========================================================================
	namespace Defaults
	{
		constexpr float Distance = 150.0f;
		constexpr float ContactThreshold = 50.0f;
		constexpr float PlaybackSpeed = 1.0f;
		constexpr int32 HistoryMaxSize = 50;
		constexpr int32 AnalysisSampleCount = 30;
		constexpr float VictimTimeOffsetMin = -2.0f;
		constexpr float VictimTimeOffsetMax = 2.0f;
	}

	// ========================================================================
	// OPTIMIZATION SETTINGS
	// ========================================================================
	namespace Optimization
	{
		constexpr float MinDistance = 50.0f;
		constexpr float MaxDistance = 400.0f;
		constexpr int32 DistanceSteps = 50;
		constexpr int32 RotationSteps = 36;
		constexpr float GoldenSectionTolerance = 1.0f;
	}
}
