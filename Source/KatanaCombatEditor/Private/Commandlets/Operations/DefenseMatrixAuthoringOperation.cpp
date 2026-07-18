// Copyright Epic Games, Inc. All Rights Reserved.

#include "Commandlets/Operations/DefenseMatrixAuthoringOperation.h"

#include "AI/EnemyCombatAIComponent.h"
#include "Animation/AnimNotify_AttackPhaseTransition.h"
#include "Animation/AnimNotifyState_CounterWindow.h"
#include "Animation/AnimNotifyState_CombatWarp.h"
#include "Animation/AnimNotifyState_ParryWindow.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "Animation/AnimSequenceBase.h"
#include "AnimNotifyState_MotionWarping.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Characters/EnemyCharacter.h"
#include "Characters/PlayerCharacter.h"
#include "Commandlets/KatanaAssetMigrationRunner.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Data/AttackData.h"
#include "Data/CombatSettings.h"
#include "Data/DefenseConfiguration.h"
#include "Debug/DefenseMatrixProofDirector.h"
#include "DefenseAssetValidationService.h"
#include "Dom/JsonObject.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Engine/DirectionalLight.h"
#include "Engine/Level.h"
#include "Engine/SkyLight.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/WorldSettings.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Modules/ModuleManager.h"
#include "PackageTools.h"
#include "RootMotionModifier_SkewWarp.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/Package.h"
#include "UObject/UnrealType.h"
#include "UObject/UObjectHash.h"
#include "Utilities/CombatGameplayTags.h"

const FString FDefenseMatrixAuthoringOperation::OperationName =
	TEXT("DefenseMatrixAuthoring");

namespace
{
constexpr int32 RecipeVersion = 11;
constexpr float FloatTolerance = 0.001f;
constexpr float MatrixProofRadius = 185.0f;
constexpr float LowCenterProofRadius = 150.0f;
constexpr float LowCenterActiveStart = 0.34f;
constexpr float LowCenterRecoveryStart = 0.49f;

const FString ManifestPath = TEXT("Tools/Codex/manifests/defense-gate-b.json");
const FString SourceConfigurationPath =
	TEXT("/Game/ProjectFiles/Data/PDA/Defense/GateA/DA_DefenseConfiguration_GateA.DA_DefenseConfiguration_GateA");
const FString SourceCombatSettingsPath =
	TEXT("/Game/ProjectFiles/Data/PDA/Settings/DA_CombatSettings_Default.DA_CombatSettings_Default");
const FString SourcePlayerBlueprintPath =
	TEXT("/Game/ProjectFiles/Core/Actors/Character/BP_Player.BP_Player");
const FString SourceEnemyBlueprintPath =
	TEXT("/Game/ProjectFiles/Core/Actors/Character/BP_EnemyCharacter.BP_EnemyCharacter");
const FString RecoilSequencePath =
	TEXT("/Game/Assets/Animations/DynamicKatana/AS_Hit_Large_F_Seq.AS_Hit_Large_F_Seq");
const FString LowCenterSequencePath =
	TEXT("/Game/Assets/Animations/CombatMasterAnimBundle/Animations/DSAS_V1/Manny_UE5/RootMotion/Attack/CommonAttack/Anim_SAS_Stab1.Anim_SAS_Stab1");
const FString MiddleCenterSequencePath =
	TEXT("/Game/Assets/Animations/CombatMasterAnimBundle/Animations/DSAS_V1/Manny_UE5/RootMotion/Attack/CommonAttack/Anim_SAS_Stab1.Anim_SAS_Stab1");
const FString MiddleLeftSequencePath =
	TEXT("/Game/Assets/Animations/CombatMasterAnimBundle/Animations/DynamicAxeAnims/Manny_UE5/RootMotion/CommboAttack/Anim_DA_Combo_A2_RM.Anim_DA_Combo_A2_RM");
const FString LowLeftSequencePath =
	TEXT("/Game/Assets/Animations/CombatMasterAnimBundle/Animations/DSAS_V1/Manny_UE5/RootMotion/Attack/CommonAttack/Anim_SAS_Slash1.Anim_SAS_Slash1");
const FString FloorMeshPath = TEXT("/Engine/BasicShapes/Plane.Plane");

const FString RecoilMontagePackage =
	TEXT("/Game/ProjectFiles/Animation/Montages/Defense/GateB/AM_Recoil_Generic");
const FString LowMatrixMontagePackage =
	TEXT("/Game/ProjectFiles/Animation/Montages/Defense/GateB/AM_GateB_LowCenter");
const FString ContactMatrixMontagePackage =
	TEXT("/Game/ProjectFiles/Animation/Montages/Defense/GateB/AM_GateB_ContactMatrix");
const FString DefenseConfigurationPackage =
	TEXT("/Game/ProjectFiles/Data/PDA/Defense/GateB/DA_DefenseConfiguration_GateB");
const FString CombatSettingsPackage =
	TEXT("/Game/ProjectFiles/Data/PDA/Defense/GateB/DA_CombatSettings_DefenseMatrix");
const FString EnemyBlueprintPackage =
	TEXT("/Game/ProjectFiles/Levels/Test/DefenseMatrix/BP_DefenseMatrixEnemy");
const FString PlayerBlueprintPackage =
	TEXT("/Game/ProjectFiles/Levels/Test/DefenseMatrix/BP_DefenseMatrixPlayer");
const FString MapPackage =
	TEXT("/Game/ProjectFiles/Levels/Test/Lvl_DefenseMatrix");

const FString HighLeftAttackPackage =
	TEXT("/Game/ProjectFiles/Data/PDA/Defense/GateB/Attacks/DA_GateB_HighLeft");
const FString HighCenterAttackPackage =
	TEXT("/Game/ProjectFiles/Data/PDA/Defense/GateB/Attacks/DA_GateB_HighCenter");
const FString HighRightAttackPackage =
	TEXT("/Game/ProjectFiles/Data/PDA/Defense/GateB/Attacks/DA_GateB_HighRight");
const FString MiddleLeftAttackPackage =
	TEXT("/Game/ProjectFiles/Data/PDA/Defense/GateB/Attacks/DA_GateB_MiddleLeft");
const FString MiddleCenterAttackPackage =
	TEXT("/Game/ProjectFiles/Data/PDA/Defense/GateB/Attacks/DA_GateB_MiddleCenter");
const FString MiddleRightAttackPackage =
	TEXT("/Game/ProjectFiles/Data/PDA/Defense/GateB/Attacks/DA_GateB_MiddleRight");
const FString LowLeftAttackPackage =
	TEXT("/Game/ProjectFiles/Data/PDA/Defense/GateB/Attacks/DA_GateB_LowLeft");
const FString LowCenterAttackPackage =
	TEXT("/Game/ProjectFiles/Data/PDA/Defense/GateB/Attacks/DA_GateB_LowCenter");
const FString LowRightAttackPackage =
	TEXT("/Game/ProjectFiles/Data/PDA/Defense/GateB/Attacks/DA_GateB_LowRight");

const FName PlayerActorName = TEXT("DefenseMatrix_Player");
const FName LeftEnemyActorName = TEXT("DefenseMatrix_Enemy_Left");
const FName CenterEnemyActorName = TEXT("DefenseMatrix_Enemy_Center");
const FName RightEnemyActorName = TEXT("DefenseMatrix_Enemy_Right");
const FName DirectorActorName = TEXT("DefenseMatrix_Director");
const FName FloorActorName = TEXT("DefenseMatrix_Floor");
const FName DirectionalLightActorName = TEXT("DefenseMatrix_DirectionalLight");
const FName SkyLightActorName = TEXT("DefenseMatrix_SkyLight");
const FName SkyAtmosphereActorName = TEXT("DefenseMatrix_SkyAtmosphere");

const FName PlayerFixtureTag = TEXT("DefenseMatrix.Player");
const FName LeftAnchorTag = TEXT("DefenseMatrix.Anchor.Left");
const FName CenterAnchorTag = TEXT("DefenseMatrix.Anchor.Center");
const FName RightAnchorTag = TEXT("DefenseMatrix.Anchor.Right");

const FVector PlayerLocation(0.0f, 0.0f, 96.0f);
const FVector LeftEnemyLocation(220.0f, -180.0f, 96.0f);
const FVector CenterEnemyLocation(290.0f, 0.0f, 96.0f);
const FVector RightEnemyLocation(220.0f, 180.0f, 96.0f);

const FString LightAttack1Path =
	TEXT("/Game/ProjectFiles/Data/PDA/Attack/AttackData/Light/New/LightAttack_1.LightAttack_1");
const FString LightAttack2Path =
	TEXT("/Game/ProjectFiles/Data/PDA/Attack/AttackData/Light/New/LightAttack_2.LightAttack_2");
const FString LightAttack3Path =
	TEXT("/Game/ProjectFiles/Data/PDA/Attack/AttackData/Light/New/LightAttack_3.LightAttack_3");
const FString LightAttack4Path =
	TEXT("/Game/ProjectFiles/Data/PDA/Attack/AttackData/Light/New/LightAttack_4.LightAttack_4");
const FString LightAttack6Path =
	TEXT("/Game/ProjectFiles/Data/PDA/Attack/AttackData/Light/New/LightAttack_6.LightAttack_6");
const FString LightAttack8Path =
	TEXT("/Game/ProjectFiles/Data/PDA/Attack/AttackData/Light/New/LightAttack_8.LightAttack_8");
const FString LightAttack11Path =
	TEXT("/Game/ProjectFiles/Data/PDA/Attack/AttackData/Light/New/LightAttack_11.LightAttack_11");
const FString DirectionalAttackBPath =
	TEXT("/Game/ProjectFiles/Data/PDA/Attack/AttackData/DirectionalAttacks/New/DA_DirectionalAttack_B.DA_DirectionalAttack_B");

struct FDefenseMatrixAuthoringPlan : FDefenseMatrixAuthoringApprovalContract
{
	TArray<FString> Errors;
};

struct FCaseRecipe
{
	FName Name;
	FString AttackPath;
	FName AnchorTag;
	FTransform DefenderTransform = FTransform::Identity;
	FTransform AttackerTransform = FTransform::Identity;
	bool bBeginHeldGuard = true;
};

struct FAttackVariantRecipe
{
	FName Name;
	FString DestinationPackage;
	FString SourceAttackPath;
	EAttackHeight Height = EAttackHeight::Middle;
	EIncomingAttackLane Lane = EIncomingAttackLane::Center;
	ESwingDirection Swing = ESwingDirection::Horizontal;
	FName TargetBone = NAME_None;
	FName AnchorTag = NAME_None;
	float ProofRadius = MatrixProofRadius;
	bool bBlockInterruptible = false;
	bool bUseLowMatrixMontage = false;
	bool bUseLowCenterSection = false;
	bool bUseContactMatrixMontage = false;
	FString SequenceOverridePath;
	float ActiveStartOffset = 0.3f;
	float ActiveDuration = 0.2f;
};

FAttackVariantRecipe MakeContactMatrixRecipe(
	const FName Name,
	const FString& DestinationPackage,
	const FString& SourceAttackPath,
	const EAttackHeight Height,
	const EIncomingAttackLane Lane,
	const ESwingDirection Swing,
	const FName TargetBone,
	const FName AnchorTag,
	const float ActiveStartOffset,
	const float ActiveDuration,
	const float ProofRadius = MatrixProofRadius,
	const bool bBlockInterruptible = false,
	const FString& SequenceOverridePath = FString())
{
	FAttackVariantRecipe Recipe;
	Recipe.Name = Name;
	Recipe.DestinationPackage = DestinationPackage;
	Recipe.SourceAttackPath = SourceAttackPath;
	Recipe.Height = Height;
	Recipe.Lane = Lane;
	Recipe.Swing = Swing;
	Recipe.TargetBone = TargetBone;
	Recipe.AnchorTag = AnchorTag;
	Recipe.ProofRadius = ProofRadius;
	Recipe.bBlockInterruptible = bBlockInterruptible;
	Recipe.bUseContactMatrixMontage = true;
	Recipe.SequenceOverridePath = SequenceOverridePath;
	Recipe.ActiveStartOffset = ActiveStartOffset;
	Recipe.ActiveDuration = ActiveDuration;
	return Recipe;
}

FString BuildObjectPath(const FString& PackageName)
{
	const FString AssetName = FPackageName::GetLongPackageAssetName(PackageName);
	return FString::Printf(TEXT("%s.%s"), *PackageName, *AssetName);
}

FString MatrixFamilyName(const EAttackHeight Height)
{
	switch (Height)
	{
	case EAttackHeight::High:
		return TEXT("High");
	case EAttackHeight::Low:
		return TEXT("Low");
	case EAttackHeight::Middle:
	default:
		return TEXT("Middle");
	}
}

const FKatanaAssetAuthoringIdentity& GetIdentity()
{
	static const FKatanaAssetAuthoringIdentity Identity = {
		FDefenseMatrixAuthoringOperation::OperationName,
		ManifestPath,
		TEXT("B"),
		TEXT("GateBReviewedRecipeV11"),
		TEXT("DefenseMatrixAuthoringRecipe")};
	return Identity;
}

bool IsApplyMode(const EKatanaAssetMigrationMode Mode)
{
	return Mode == EKatanaAssetMigrationMode::Apply
		|| Mode == EKatanaAssetMigrationMode::ApplyAndSave;
}

template <typename TObjectType>
TObjectType* LoadObjectAtPath(const FString& ObjectPath)
{
	return Cast<TObjectType>(StaticLoadObject(
		TObjectType::StaticClass(), nullptr, *ObjectPath));
}

template <typename TObjectType>
TObjectType* LoadObjectAtPackage(const FString& PackageName)
{
	return LoadObjectAtPath<TObjectType>(BuildObjectPath(PackageName));
}

UObject* FindExistingAsset(const FString& ObjectPath)
{
	if (UObject* Loaded = FindObject<UObject>(nullptr, *ObjectPath))
	{
		return Loaded;
	}
	const FAssetData AssetData =
		FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"))
		.Get().GetAssetByObjectPath(FSoftObjectPath(ObjectPath));
	return AssetData.IsValid() ? AssetData.GetAsset() : nullptr;
}

UWorld* LoadWorldPackageFully(const FString& PackageName)
{
	UPackage* Package = FindPackage(nullptr, *PackageName);
	if (!Package)
	{
		Package = LoadPackage(nullptr, *PackageName, LOAD_None);
	}
	if (!Package)
	{
		return nullptr;
	}
	Package->FullyLoad();
	return UWorld::FindWorldInPackage(Package);
}

UPackage* CreateAssetPackage(const FString& PackageName, TArray<FString>& OutErrors)
{
	if (!FPackageName::IsValidLongPackageName(PackageName))
	{
		OutErrors.Add(FString::Printf(TEXT("invalid destination package: %s"),
			*PackageName));
		return nullptr;
	}
	UPackage* Package = CreatePackage(*PackageName);
	if (!Package)
	{
		OutErrors.Add(FString::Printf(TEXT("could not create destination package: %s"),
			*PackageName));
		return nullptr;
	}
	Package->FullyLoad();
	return Package;
}

bool NearlyEqual(const float Left, const float Right)
{
	return FMath::IsNearlyEqual(Left, Right, FloatTolerance);
}

TArray<FAttackVariantRecipe> BuildAttackVariantRecipes()
{
	return {
		MakeContactMatrixRecipe(TEXT("HighLeft"), HighLeftAttackPackage,
			LightAttack4Path, EAttackHeight::High, EIncomingAttackLane::Left,
			ESwingDirection::Horizontal, TEXT("head"), LeftAnchorTag,
			0.32f, 0.18f, MatrixProofRadius, true),
		MakeContactMatrixRecipe(TEXT("HighCenter"), HighCenterAttackPackage,
			DirectionalAttackBPath, EAttackHeight::High, EIncomingAttackLane::Center,
			ESwingDirection::Vertical, TEXT("head"), CenterAnchorTag,
			0.43f, 0.12f, MatrixProofRadius, true),
		MakeContactMatrixRecipe(TEXT("HighRight"), HighRightAttackPackage,
			LightAttack4Path, EAttackHeight::High, EIncomingAttackLane::Right,
			ESwingDirection::Horizontal, TEXT("head"), RightAnchorTag,
			0.405f, 0.095f, MatrixProofRadius, true),
		MakeContactMatrixRecipe(TEXT("MiddleLeft"), MiddleLeftAttackPackage,
			LightAttack8Path, EAttackHeight::Middle, EIncomingAttackLane::Left,
			ESwingDirection::Horizontal, TEXT("spine_03"), LeftAnchorTag,
			0.285f, 0.215f, MatrixProofRadius, false, MiddleLeftSequencePath),
		MakeContactMatrixRecipe(TEXT("MiddleCenter"), MiddleCenterAttackPackage,
			LightAttack3Path, EAttackHeight::Middle, EIncomingAttackLane::Center,
			ESwingDirection::Thrust, TEXT("spine_01"), CenterAnchorTag,
			0.34f, 0.15f, 150.0f, false, MiddleCenterSequencePath),
		MakeContactMatrixRecipe(TEXT("MiddleRight"), MiddleRightAttackPackage,
			LightAttack6Path, EAttackHeight::Middle, EIncomingAttackLane::Right,
			ESwingDirection::Horizontal, TEXT("spine_03"), RightAnchorTag,
			0.30f, 0.20f),
		MakeContactMatrixRecipe(TEXT("LowLeft"), LowLeftAttackPackage,
			LightAttack8Path, EAttackHeight::Low, EIncomingAttackLane::Left,
			ESwingDirection::Sweep, TEXT("pelvis"), LeftAnchorTag,
			0.41f, 0.29f, 150.0f, false, LowLeftSequencePath),
		{TEXT("LowCenter"), LowCenterAttackPackage, LightAttack2Path,
			EAttackHeight::Low, EIncomingAttackLane::Center,
			ESwingDirection::Thrust, TEXT("pelvis"), CenterAnchorTag,
			LowCenterProofRadius, false, true, true},
		{TEXT("LowRight"), LowRightAttackPackage, LightAttack1Path,
			EAttackHeight::Low, EIncomingAttackLane::Right,
			ESwingDirection::Sweep, TEXT("pelvis"), RightAnchorTag,
			MatrixProofRadius, false, true, false}};
}

TArray<FString> BuildMapAttackPaths()
{
	TArray<FString> Paths;
	for (const FAttackVariantRecipe& Recipe : BuildAttackVariantRecipes())
	{
		Paths.Add(BuildObjectPath(Recipe.DestinationPackage));
	}
	Paths.Add(LightAttack11Path);
	Paths.Add(LightAttack1Path);
	return Paths;
}

TArray<FCaseRecipe> BuildCaseRecipes()
{
	const auto AtRadius = [](const float Radius)
	{
		const FVector Location = PlayerLocation + FVector(Radius, 0.0f, 0.0f);
		return FTransform((PlayerLocation - Location).Rotation(), Location);
	};
	const FTransform DefenderTransform(FRotator::ZeroRotator, PlayerLocation);
	TArray<FCaseRecipe> Cases;
	for (const FAttackVariantRecipe& Recipe : BuildAttackVariantRecipes())
	{
		Cases.Add({FName(*FString::Printf(TEXT("NormalBlock%s"), *Recipe.Name.ToString())),
			BuildObjectPath(Recipe.DestinationPackage), Recipe.AnchorTag,
			DefenderTransform, AtRadius(Recipe.ProofRadius)});
	}
	Cases.Add({TEXT("UnblockableMiddleCenter"), LightAttack11Path, CenterAnchorTag,
		DefenderTransform, AtRadius(MatrixProofRadius)});
	Cases.Add({TEXT("PerfectParryGateARegression"), LightAttack1Path, CenterAnchorTag,
		DefenderTransform, AtRadius(MatrixProofRadius), false});
	return Cases;
}

struct FManifestCaseContract
{
	FString Name;
	FString AttackName;
	FString AttackPath;
	FString Outcome;
	FString Reason;
	FString AttackerResponse;
	FString Presentation;
	bool bHasPresentation = false;
};

TArray<FManifestCaseContract> BuildManifestCaseContracts()
{
	TArray<FManifestCaseContract> Contracts;
	for (const FAttackVariantRecipe& Recipe : BuildAttackVariantRecipes())
	{
		FString Presentation;
		if (Recipe.Height == EAttackHeight::High)
		{
			Presentation = TEXT("NormalBlockHighRecoil");
		}
		else if (Recipe.Height == EAttackHeight::Middle
			&& Recipe.Lane == EIncomingAttackLane::Center)
		{
			Presentation = TEXT("NormalBlockMiddleCenterContinue");
		}
		else if (Recipe.Height == EAttackHeight::Middle)
		{
			Presentation = TEXT("NormalBlockMiddleContinue");
		}
		else
		{
			Presentation = TEXT("NormalBlockLowContinue");
		}
		Contracts.Add({
			TEXT("NormalBlock") + Recipe.Name.ToString(),
			TEXT("GateB_") + Recipe.Name.ToString(),
			BuildObjectPath(Recipe.DestinationPackage),
			TEXT("NormalBlock"),
			TEXT("None"),
			Recipe.bBlockInterruptible ? TEXT("Recoil") : TEXT("Continue"),
			Presentation,
			true});
	}
	Contracts.Add({
		TEXT("UnblockableMiddleCenter"), TEXT("LightAttack_11"),
		LightAttack11Path, TEXT("UnblockableHit"), TEXT("Unblockable"),
		TEXT("Continue"), FString(), false});
	Contracts.Add({
		TEXT("PerfectParryGateARegression"), TEXT("LightAttack_1"),
		LightAttack1Path, TEXT("PerfectParry"), TEXT("None"),
		TEXT("ParryStagger"), TEXT("PerfectParryGateA"), true});
	Contracts.Sort([](const FManifestCaseContract& Left,
		const FManifestCaseContract& Right)
	{
		return Left.Name < Right.Name;
	});
	return Contracts;
}

bool ValidateManifestCatalogInternal(
	const FDefenseProofManifest& Manifest,
	TArray<FString>& OutErrors)
{
	const TArray<FManifestCaseContract> Contracts =
		BuildManifestCaseContracts();
	const TArray<FAttackVariantRecipe> AttackRecipes = BuildAttackVariantRecipes();
	if (Manifest.ExpectedCases.Num() != Contracts.Num())
	{
		OutErrors.Add(FString::Printf(
			TEXT("Gate B expected-case catalog count differs: manifest=%d recipe=%d"),
			Manifest.ExpectedCases.Num(), Contracts.Num()));
	}

	TSet<FString> SeenCaseNames;
	for (const FDefenseProofExpectedCaseEntry& ExpectedCase :
		Manifest.ExpectedCases)
	{
		if (SeenCaseNames.Contains(ExpectedCase.Name))
		{
			OutErrors.Add(TEXT("duplicate Gate B expected case: ")
				+ ExpectedCase.Name);
		}
		SeenCaseNames.Add(ExpectedCase.Name);
	}

	for (const FManifestCaseContract& Contract : Contracts)
	{
		const FDefenseProofExpectedCaseEntry* ExpectedCase =
			Manifest.ExpectedCases.FindByPredicate(
				[&Contract](const FDefenseProofExpectedCaseEntry& Candidate)
				{
					return Candidate.Name == Contract.Name;
				});
		if (!ExpectedCase)
		{
			OutErrors.Add(TEXT("manifest is missing Gate B expected case: ")
				+ Contract.Name);
			continue;
		}
		if (!ExpectedCase->bReviewed
			|| ExpectedCase->Attack != Contract.AttackName
			|| ExpectedCase->Outcome != Contract.Outcome
			|| ExpectedCase->Reason != Contract.Reason
			|| ExpectedCase->AttackerResponse != Contract.AttackerResponse
			|| ExpectedCase->bHasPresentation != Contract.bHasPresentation
			|| (Contract.bHasPresentation
				&& ExpectedCase->Presentation != Contract.Presentation))
		{
			OutErrors.Add(TEXT("manifest expected-case semantics differ from recipe: ")
				+ Contract.Name);
		}

		const FDefenseProofAttackEntry* Attack =
			Manifest.Attacks.FindByPredicate(
				[&Contract](const FDefenseProofAttackEntry& Candidate)
				{
					return Candidate.Name == Contract.AttackName;
				});
		if (!Attack || Attack->AttackData != Contract.AttackPath)
		{
			OutErrors.Add(TEXT("manifest case attack differs from recipe: ")
				+ Contract.Name);
		}
		const FAttackVariantRecipe* MatrixRecipe = AttackRecipes.FindByPredicate(
			[&Contract](const FAttackVariantRecipe& Recipe)
			{
				return BuildObjectPath(Recipe.DestinationPackage) == Contract.AttackPath;
			});
		if (MatrixRecipe)
		{
			const FString ExpectedFamily = MatrixFamilyName(MatrixRecipe->Height);
			if (!Attack || !Attack->bHasMatrixFamily
				|| Attack->MatrixFamily != ExpectedFamily)
			{
				OutErrors.Add(TEXT("manifest matrixFamily differs from recipe: ")
					+ Contract.Name);
			}
		}
		else if (Attack && Attack->bHasMatrixFamily)
		{
			OutErrors.Add(TEXT("non-matrix recipe attack declares matrixFamily: ")
				+ Contract.Name);
		}
		if (Contract.bHasPresentation)
		{
			const FDefenseProofPresentationEntry* Presentation =
				Manifest.Presentations.FindByPredicate(
					[&Contract](const FDefenseProofPresentationEntry& Candidate)
					{
						return Candidate.Name == Contract.Presentation;
					});
			if (!Presentation || !Presentation->bReviewed
				|| Presentation->Outcome != Contract.Outcome
				|| Presentation->AttackerResponse != Contract.AttackerResponse)
			{
				OutErrors.Add(TEXT("manifest case presentation differs from recipe: ")
					+ Contract.Name);
			}
		}
		if (!Manifest.ProofCases.Contains(Contract.Name))
		{
			OutErrors.Add(TEXT("proofCases is missing recipe case: ")
				+ Contract.Name);
		}
	}

	const TArray<FCaseRecipe> DirectorCases = BuildCaseRecipes();
	if (DirectorCases.Num() != Contracts.Num())
	{
		OutErrors.Add(TEXT("proof-director case count differs from manifest contract"));
	}
	TSet<FName> SeenDirectorCases;
	for (const FCaseRecipe& DirectorCase : DirectorCases)
	{
		if (SeenDirectorCases.Contains(DirectorCase.Name))
		{
			OutErrors.Add(TEXT("duplicate proof-director case: ")
				+ DirectorCase.Name.ToString());
		}
		SeenDirectorCases.Add(DirectorCase.Name);
		const FManifestCaseContract* Contract = Contracts.FindByPredicate(
			[&DirectorCase](const FManifestCaseContract& Candidate)
			{
				return Candidate.Name == DirectorCase.Name.ToString();
			});
		if (!Contract || Contract->AttackPath != DirectorCase.AttackPath)
		{
			OutErrors.Add(TEXT("proof-director attack differs from manifest contract: ")
				+ DirectorCase.Name.ToString());
		}
	}
	return OutErrors.IsEmpty();
}

bool PropertiesMatch(const UObject* Left, const UObject* Right)
{
	if (!Left || !Right || Left->GetClass() != Right->GetClass())
	{
		return false;
	}
	for (TFieldIterator<FProperty> It(Left->GetClass(),
		EFieldIteratorFlags::IncludeSuper); It; ++It)
	{
		const FProperty* Property = *It;
		if (Property->HasAnyPropertyFlags(
			CPF_Transient | CPF_DuplicateTransient | CPF_NonPIEDuplicateTransient))
		{
			continue;
		}
		if (!Property->Identical_InContainer(Left, Right, 0, PPF_DeepComparison))
		{
			return false;
		}
	}
	return true;
}

bool RecoilMontageMatches(const UAnimMontage* Montage, TArray<FString>* OutMismatches = nullptr)
{
	auto Mismatch = [OutMismatches](const FString& Detail)
	{
		if (OutMismatches)
		{
			OutMismatches->Add(FString::Printf(
				TEXT("Gate B recoil montage mismatch: %s"), *Detail));
		}
	};
	if (!Montage)
	{
		Mismatch(TEXT("asset is null"));
		return false;
	}
	bool bMatches = true;
	auto Require = [&bMatches, &Mismatch](const bool bCondition, const FString& Detail)
	{
		if (!bCondition)
		{
			bMatches = false;
			Mismatch(Detail);
		}
	};
	Require(Montage->SlotAnimTracks.Num() == 1,
		FString::Printf(TEXT("expected one slot track, found %d"), Montage->SlotAnimTracks.Num()));
	Require(Montage->CompositeSections.Num() == 1,
		FString::Printf(TEXT("expected one section, found %d"), Montage->CompositeSections.Num()));
	Require(Montage->Notifies.Num() == 1,
		FString::Printf(TEXT("expected one notify, found %d"), Montage->Notifies.Num()));
	Require(NearlyEqual(Montage->GetPlayLength(), 1.0f),
		FString::Printf(TEXT("expected length 1.0, found %.6f"), Montage->GetPlayLength()));
	if (Montage->SlotAnimTracks.Num() != 1
		|| Montage->CompositeSections.Num() != 1
		|| Montage->Notifies.Num() != 1)
	{
		return false;
	}
	Require(Montage->SlotAnimTracks[0].SlotName == TEXT("DefaultSlot"),
		FString::Printf(TEXT("expected DefaultSlot, found %s"),
			*Montage->SlotAnimTracks[0].SlotName.ToString()));
	Require(Montage->SlotAnimTracks[0].AnimTrack.AnimSegments.Num() == 1,
		FString::Printf(TEXT("expected one segment, found %d"),
			Montage->SlotAnimTracks[0].AnimTrack.AnimSegments.Num()));
	Require(Montage->CompositeSections[0].SectionName == TEXT("Recoil"),
		FString::Printf(TEXT("expected Recoil section, found %s"),
			*Montage->CompositeSections[0].SectionName.ToString()));
	Require(NearlyEqual(Montage->CompositeSections[0].GetTime(), 0.0f),
		FString::Printf(TEXT("expected section time 0.0, found %.6f"),
			Montage->CompositeSections[0].GetTime()));
	if (Montage->SlotAnimTracks[0].AnimTrack.AnimSegments.Num() != 1)
	{
		return false;
	}
	const FAnimSegment& Segment =
		Montage->SlotAnimTracks[0].AnimTrack.AnimSegments[0];
	Require(Segment.GetAnimReference()
		&& Segment.GetAnimReference()->GetPathName() == RecoilSequencePath,
		FString::Printf(TEXT("expected source %s, found %s"), *RecoilSequencePath,
			Segment.GetAnimReference() ? *Segment.GetAnimReference()->GetPathName() : TEXT("null")));
	Require(NearlyEqual(Segment.StartPos, 0.0f),
		FString::Printf(TEXT("expected segment start 0.0, found %.6f"), Segment.StartPos));
	Require(NearlyEqual(Segment.AnimStartTime, 0.0f),
		FString::Printf(TEXT("expected source start 0.0, found %.6f"), Segment.AnimStartTime));
	Require(NearlyEqual(Segment.AnimEndTime, 1.0f),
		FString::Printf(TEXT("expected source end 1.0, found %.6f"), Segment.AnimEndTime));
	Require(NearlyEqual(Segment.AnimPlayRate, 1.0f),
		FString::Printf(TEXT("expected play rate 1.0, found %.6f"), Segment.AnimPlayRate));
	Require(Segment.LoopingCount == 1,
		FString::Printf(TEXT("expected one loop, found %d"), Segment.LoopingCount));
	const FAnimNotifyEvent& Event = Montage->Notifies[0];
	const UAnimNotifyState_MotionWarping* Notify =
		Cast<UAnimNotifyState_MotionWarping>(Event.NotifyStateClass);
	const URootMotionModifier_SkewWarp* Warp = Notify
		? Cast<URootMotionModifier_SkewWarp>(Notify->RootMotionModifier) : nullptr;
	Require(Notify != nullptr, TEXT("notify is not a motion-warping state"));
	Require(Warp != nullptr, TEXT("notify modifier is not a skew warp"));
	if (Warp)
	{
		Require(Warp->WarpTargetName == TEXT("AttackerResponseTarget"),
			FString::Printf(TEXT("expected warp target AttackerResponseTarget, found %s"),
				*Warp->WarpTargetName.ToString()));
		Require(Warp->bWarpRotation, TEXT("rotation warping is disabled"));
		Require(!Warp->bWarpTranslation, TEXT("translation warping is enabled"));
	}
	Require(NearlyEqual(Event.GetTime(), 0.0f),
		FString::Printf(TEXT("expected notify time 0.0, found %.6f"), Event.GetTime()));
	Require(NearlyEqual(Event.GetDuration(), 0.65f),
		FString::Printf(TEXT("expected notify duration 0.65, found %.6f"), Event.GetDuration()));
	return bMatches;
}

bool ConfigureRecoilMontage(
	UAnimMontage* Montage,
	UAnimSequenceBase* Sequence,
	TArray<FString>& OutErrors)
{
	if (!Montage || !Sequence || !Sequence->GetSkeleton()
		|| Sequence->GetPlayLength() < 1.0f)
	{
		OutErrors.Add(TEXT("reviewed recoil sequence or one-second source range is unavailable"));
		return false;
	}
	Montage->SlotAnimTracks.Reset();
	Montage->CompositeSections.Reset();
	Montage->Notifies.Reset();
	Montage->SetSkeleton(Sequence->GetSkeleton());
	FSlotAnimationTrack Slot;
	Slot.SlotName = TEXT("DefaultSlot");
	FAnimSegment Segment;
	Segment.SetAnimReference(Sequence);
	Segment.StartPos = 0.0f;
	Segment.AnimStartTime = 0.0f;
	Segment.AnimEndTime = 1.0f;
	Segment.AnimPlayRate = 1.0f;
	Segment.LoopingCount = 1;
	Slot.AnimTrack.AnimSegments.Add(MoveTemp(Segment));
	Montage->SlotAnimTracks.Add(MoveTemp(Slot));
	Montage->SetCompositeLength(1.0f);
	FCompositeSection Section;
	Section.SectionName = TEXT("Recoil");
	Section.SetTime(0.0f);
	Montage->CompositeSections.Add(MoveTemp(Section));

	UAnimNotifyState_MotionWarping* MotionWarp =
		NewObject<UAnimNotifyState_MotionWarping>(Montage);
	URootMotionModifier_SkewWarp* Warp =
		NewObject<URootMotionModifier_SkewWarp>(MotionWarp);
	Warp->WarpTargetName = TEXT("AttackerResponseTarget");
	Warp->bWarpRotation = true;
	Warp->bWarpTranslation = false;
	MotionWarp->RootMotionModifier = Warp;
	FAnimNotifyEvent Event;
	Event.NotifyStateClass = MotionWarp;
	Event.SetTime(0.0f);
	Event.SetDuration(0.65f);
	Montage->Notifies.Add(MoveTemp(Event));
	Montage->SortNotifies();
	Montage->RefreshCacheData();
	return true;
}

UAnimMontage* CreateRecoilMontage(TArray<FString>& OutErrors)
{
	UAnimSequenceBase* Sequence = LoadObjectAtPath<UAnimSequenceBase>(RecoilSequencePath);
	UPackage* Package = CreateAssetPackage(RecoilMontagePackage, OutErrors);
	if (!Package)
	{
		return nullptr;
	}
	const FString AssetName = FPackageName::GetLongPackageAssetName(RecoilMontagePackage);
	UAnimMontage* Montage = NewObject<UAnimMontage>(
		Package, *AssetName, RF_Public | RF_Standalone | RF_Transactional);
	if (!ConfigureRecoilMontage(Montage, Sequence, OutErrors))
	{
		return nullptr;
	}
	FAssetRegistryModule::AssetCreated(Montage);
	Montage->MarkPackageDirty();
	return Montage;
}

void CopyPersistentProperties(UObject* Destination, const UObject* Source)
{
	check(Destination && Source && Destination->GetClass() == Source->GetClass());
	for (TFieldIterator<FProperty> It(Source->GetClass(),
		EFieldIteratorFlags::IncludeSuper); It; ++It)
	{
		const FProperty* Property = *It;
		if (!Property->HasAnyPropertyFlags(
			CPF_Transient | CPF_DuplicateTransient | CPF_NonPIEDuplicateTransient))
		{
			Property->CopyCompleteValue_InContainer(Destination, Source);
		}
	}
}

bool IsDefenseChainWindowEvent(const FAnimNotifyEvent& Event)
{
	return Event.NotifyStateClass
		&& (Event.NotifyStateClass->IsA(UAnimNotifyState_ParryWindow::StaticClass())
			|| Event.NotifyStateClass->IsA(UAnimNotifyState_CounterWindow::StaticClass()));
}

bool IsAttackPhaseTransitionEvent(const FAnimNotifyEvent& Event)
{
	return Event.Notify && Event.Notify->IsA(UAnimNotify_AttackPhaseTransition::StaticClass());
}

FAnimNotifyEvent DuplicateNotifyEvent(
	const FAnimNotifyEvent& Source,
	UAnimMontage* Destination,
	const float DestinationTime,
	const float DestinationDuration)
{
	FAnimNotifyEvent Copy = Source;
	Copy.Notify = Source.Notify
		? DuplicateObject<UAnimNotify>(Source.Notify, Destination) : nullptr;
	Copy.NotifyStateClass = Source.NotifyStateClass
		? DuplicateObject<UAnimNotifyState>(Source.NotifyStateClass, Destination) : nullptr;
	Copy.Guid = FGuid::NewGuid();
	Copy.TrackIndex = 0;
	Copy.Link(Destination, DestinationTime, 0);
	Copy.SetDuration(DestinationDuration);
	if (Copy.NotifyStateClass)
	{
		Copy.EndLink.Link(Destination, DestinationTime + DestinationDuration, 0);
	}
	return Copy;
}

FAnimNotifyEvent DuplicateNotifyEvent(
	const FAnimNotifyEvent& Source,
	UAnimMontage* Destination)
{
	return DuplicateNotifyEvent(
		Source, Destination, Source.GetTime(), Source.GetDuration());
}

void AddPhaseTransition(
	UAnimMontage* Montage,
	const EAttackPhase Phase,
	const float Time)
{
	UAnimNotify_AttackPhaseTransition* Transition =
		NewObject<UAnimNotify_AttackPhaseTransition>(Montage);
	Transition->TransitionToPhase = Phase;
	FAnimNotifyEvent Event;
	Event.Notify = Transition;
	Event.TrackIndex = 0;
	Event.Link(Montage, Time, 0);
	Event.TriggerTimeOffset = EAnimEventTriggerOffsets::OffsetBefore;
	Montage->Notifies.Add(MoveTemp(Event));
}

bool ConfigureLowMatrixMontage(
	UAnimMontage* Montage,
	const UAnimMontage* LowRightSource,
	UAnimSequenceBase* LowCenterSequence,
	TArray<FString>& OutErrors)
{
	if (!Montage || !LowRightSource || !LowCenterSequence
		|| !LowRightSource->GetSkeleton() || !LowCenterSequence->GetSkeleton()
		|| LowRightSource->GetSkeleton() != LowCenterSequence->GetSkeleton()
		|| LowRightSource->SlotAnimTracks.IsEmpty())
	{
		OutErrors.Add(TEXT("low-matrix montage sources are missing or use incompatible skeletons"));
		return false;
	}

	const float SourceLength = LowRightSource->GetPlayLength();
	const float LowCenterLength = LowCenterSequence->GetPlayLength();
	if (SourceLength <= 0.0f || LowCenterLength < LowCenterRecoveryStart)
	{
		OutErrors.Add(TEXT("low-matrix montage sources cannot supply the reviewed timing ranges"));
		return false;
	}

	Montage->SetSkeleton(LowRightSource->GetSkeleton());
	Montage->SlotAnimTracks = LowRightSource->SlotAnimTracks;
	Montage->CompositeSections = LowRightSource->CompositeSections;
	Montage->Notifies.Reset();
	Montage->AnimNotifyTracks.Reset();
	FAnimNotifyTrack Track;
	Track.TrackName = TEXT("DefenseProof");
	Montage->AnimNotifyTracks.Add(MoveTemp(Track));
	for (const FAnimNotifyEvent& SourceEvent : LowRightSource->Notifies)
	{
		if (!IsDefenseChainWindowEvent(SourceEvent))
		{
			Montage->Notifies.Add(DuplicateNotifyEvent(SourceEvent, Montage));
		}
	}

	FAnimSegment LowCenterSegment;
	LowCenterSegment.SetAnimReference(LowCenterSequence);
	LowCenterSegment.StartPos = SourceLength;
	LowCenterSegment.AnimStartTime = 0.0f;
	LowCenterSegment.AnimEndTime = LowCenterLength;
	LowCenterSegment.AnimPlayRate = 1.0f;
	LowCenterSegment.LoopingCount = 1;
	Montage->SlotAnimTracks[0].AnimTrack.AnimSegments.Add(MoveTemp(LowCenterSegment));
	Montage->SetCompositeLength(SourceLength + LowCenterLength);
	for (FCompositeSection& Section : Montage->CompositeSections)
	{
		Section.Link(Montage, Section.GetTime(), 0);
	}

	FCompositeSection LowCenterSection;
	LowCenterSection.SectionName = TEXT("LowCenter");
	LowCenterSection.Link(Montage, SourceLength, 0);
	Montage->CompositeSections.Add(MoveTemp(LowCenterSection));
	AddPhaseTransition(Montage, EAttackPhase::Active,
		SourceLength + LowCenterActiveStart);
	AddPhaseTransition(Montage, EAttackPhase::Recovery,
		SourceLength + LowCenterRecoveryStart);

	UAnimNotifyState_CombatWarp* CombatWarp =
		NewObject<UAnimNotifyState_CombatWarp>(Montage);
	CombatWarp->TargetWarpName = TEXT("AttackTarget");
	CombatWarp->RotationWarpName = TEXT("RotationTarget");
	CombatWarp->bEnableTranslationForTarget = false;
	if (URootMotionModifier_Warp* Warp =
		Cast<URootMotionModifier_Warp>(CombatWarp->RootMotionModifier))
	{
		Warp->WarpTargetName = TEXT("AttackTarget");
		Warp->bWarpRotation = true;
		Warp->bWarpTranslation = false;
	}
	else
	{
		OutErrors.Add(TEXT("combat-warp notify did not provide a warp modifier template"));
		return false;
	}
	FAnimNotifyEvent WarpEvent;
	WarpEvent.NotifyStateClass = CombatWarp;
	WarpEvent.TrackIndex = 0;
	WarpEvent.Link(Montage, SourceLength, 0);
	WarpEvent.SetDuration(LowCenterRecoveryStart);
	WarpEvent.EndLink.Link(
		Montage, SourceLength + LowCenterRecoveryStart, 0);
	Montage->Notifies.Add(MoveTemp(WarpEvent));
	Montage->SortNotifies();
	Montage->RefreshCacheData();
	return true;
}

void AppendCanonicalObjectFacts(
	const UObject* Object,
	const FString& Prefix,
	FString& OutFacts,
	TSet<const UObject*>& Visited)
{
	if (!Object)
	{
		OutFacts += Prefix + TEXT("=<null>\n");
		return;
	}
	if (Visited.Contains(Object))
	{
		OutFacts += Prefix + TEXT("=<cycle>\n");
		return;
	}
	Visited.Add(Object);
	OutFacts += FString::Printf(TEXT("%s.class=%s\n"),
		*Prefix, *Object->GetClass()->GetPathName());

	TArray<const FProperty*> Properties;
	for (TFieldIterator<FProperty> It(
		Object->GetClass(), EFieldIteratorFlags::IncludeSuper); It; ++It)
	{
		const FProperty* Property = *It;
		if (!Property->HasAnyPropertyFlags(
			CPF_Transient | CPF_DuplicateTransient | CPF_NonPIEDuplicateTransient
			| CPF_TextExportTransient | CPF_Deprecated | CPF_SkipSerialization
			| CPF_EditorOnly))
		{
			Properties.Add(Property);
		}
	}
	Properties.Sort([](const FProperty& Left, const FProperty& Right)
	{
		return Left.GetName() < Right.GetName();
	});
	for (const FProperty* Property : Properties)
	{
		const FString PropertyPrefix = Prefix + TEXT(".") + Property->GetName();
		const void* Value = Property->ContainerPtrToValuePtr<void>(Object);
		if (const FObjectPropertyBase* ObjectProperty =
			CastField<FObjectPropertyBase>(Property))
		{
			const UObject* Referenced = ObjectProperty->GetObjectPropertyValue(Value);
			if (Referenced && Referenced->IsIn(Object))
			{
				AppendCanonicalObjectFacts(
					Referenced, PropertyPrefix, OutFacts, Visited);
			}
			else
			{
				OutFacts += FString::Printf(TEXT("%s=%s\n"),
					*PropertyPrefix, *GetPathNameSafe(Referenced));
			}
			continue;
		}

		FString ExportedValue;
		Property->ExportTextItem_Direct(
			ExportedValue,
			Value,
			nullptr,
			const_cast<UObject*>(Object),
			PPF_None);
		OutFacts += FString::Printf(TEXT("%s=%s\n"),
			*PropertyPrefix, *ExportedValue);
	}
}

FString BuildLowMatrixMontageFacts(const UAnimMontage* Montage)
{
	if (!Montage)
	{
		return TEXT("null");
	}
	// Normalize below UE's serialized float jitter while retaining sub-frame timing drift.
	FString Facts = FString::Printf(TEXT("skeleton=%s|length=%.4f\n"),
		*GetPathNameSafe(Montage->GetSkeleton()), Montage->GetPlayLength());
	for (const FCompositeSection& Section : Montage->CompositeSections)
	{
		Facts += FString::Printf(TEXT("section=%s|%.4f\n"),
			*Section.SectionName.ToString(), Section.GetTime());
	}
	for (const FSlotAnimationTrack& Track : Montage->SlotAnimTracks)
	{
		Facts += FString::Printf(TEXT("slot=%s\n"), *Track.SlotName.ToString());
		for (const FAnimSegment& Segment : Track.AnimTrack.AnimSegments)
		{
			Facts += FString::Printf(
				TEXT("segment=%s|%.4f|%.4f|%.4f|%.4f|%d\n"),
				*GetPathNameSafe(Segment.GetAnimReference()), Segment.StartPos,
				Segment.AnimStartTime, Segment.AnimEndTime, Segment.AnimPlayRate,
				Segment.LoopingCount);
		}
	}
	for (const FAnimNotifyEvent& Event : Montage->Notifies)
	{
		Facts += FString::Printf(
			TEXT("notify=%s|%s|time=%.6f|trigger=%.6f|duration=%.6f|")
			TEXT("end_trigger=%.6f|trigger_offset=%.6f|end_offset=%.6f|")
			TEXT("weight=%.6f|name=%s|converted=%d|tick=%d|chance=%.6f|")
			TEXT("filter=%d|lod=%d|request_filter=%d|dedicated=%d|follower=%d|")
			TEXT("track=%d|link=%d|slot=%d|segment=%d|")
			TEXT("end_link=%d|end_slot=%d|end_segment=%d\n"),
			*GetPathNameSafe(Event.Notify ? Event.Notify->GetClass() : nullptr),
			*GetPathNameSafe(Event.NotifyStateClass
				? Event.NotifyStateClass->GetClass() : nullptr),
			Event.GetTime(), Event.GetTriggerTime(), Event.GetDuration(),
			Event.GetEndTriggerTime(), Event.TriggerTimeOffset,
			Event.EndTriggerTimeOffset, Event.TriggerWeightThreshold,
			*Event.NotifyName.ToString(), Event.bConvertedFromBranchingPoint,
			static_cast<int32>(Event.MontageTickType.GetValue()),
			Event.NotifyTriggerChance,
			static_cast<int32>(Event.NotifyFilterType.GetValue()),
			Event.NotifyFilterLOD, Event.bCanBeFilteredViaRequest,
			Event.bTriggerOnDedicatedServer, Event.bTriggerOnFollower,
			Event.TrackIndex, static_cast<int32>(Event.GetLinkMethod()),
			Event.GetSlotIndex(), Event.GetSegmentIndex(),
			static_cast<int32>(Event.EndLink.GetLinkMethod()),
			Event.EndLink.GetSlotIndex(), Event.EndLink.GetSegmentIndex());
		TSet<const UObject*> Visited;
		AppendCanonicalObjectFacts(
			Event.Notify, TEXT("notify_object"), Facts, Visited);
		AppendCanonicalObjectFacts(
			Event.NotifyStateClass, TEXT("notify_state_object"), Facts, Visited);
	}
	return Facts;
}

FString DescribeFirstFactMismatch(
	const FString& ActualFacts,
	const FString& ExpectedFacts)
{
	TArray<FString> ActualLines;
	TArray<FString> ExpectedLines;
	ActualFacts.ParseIntoArrayLines(ActualLines, false);
	ExpectedFacts.ParseIntoArrayLines(ExpectedLines, false);
	const int32 ComparedLineCount = FMath::Max(
		ActualLines.Num(), ExpectedLines.Num());
	for (int32 Index = 0; Index < ComparedLineCount; ++Index)
	{
		const FString Actual = ActualLines.IsValidIndex(Index)
			? ActualLines[Index] : TEXT("<missing>");
		const FString Expected = ExpectedLines.IsValidIndex(Index)
			? ExpectedLines[Index] : TEXT("<missing>");
		if (Actual != Expected)
		{
			return FString::Printf(
				TEXT("fact[%d] actual={%s} expected={%s}"),
				Index, *Actual, *Expected);
		}
	}
	return TEXT("facts differ without a line-level mismatch");
}

bool LowMatrixMontageMatches(
	const UAnimMontage* Montage,
	const UAnimMontage* LowRightSource,
	UAnimSequenceBase* LowCenterSequence,
	TArray<FString>* OutMismatches = nullptr)
{
	UAnimMontage* Expected = LowRightSource
		? DuplicateObject<UAnimMontage>(LowRightSource, GetTransientPackage())
		: nullptr;
	TArray<FString> Errors;
	if (!ConfigureLowMatrixMontage(
		Expected, LowRightSource, LowCenterSequence, Errors))
	{
		if (OutMismatches)
		{
			OutMismatches->Append(Errors);
		}
		return false;
	}
	const FString ActualFacts = BuildLowMatrixMontageFacts(Montage);
	const FString ExpectedFacts = BuildLowMatrixMontageFacts(Expected);
	const bool bMatches = ActualFacts == ExpectedFacts;
	if (!bMatches && OutMismatches)
	{
		OutMismatches->Add(TEXT("low-matrix ")
			+ DescribeFirstFactMismatch(ActualFacts, ExpectedFacts));
	}
	return bMatches;
}

UAnimMontage* CreateLowMatrixMontage(
	UAnimMontage* LowRightSource,
	UAnimSequenceBase* LowCenterSequence,
	TArray<FString>& OutErrors)
{
	UPackage* Package = CreateAssetPackage(LowMatrixMontagePackage, OutErrors);
	if (!Package || !LowRightSource)
	{
		return nullptr;
	}
	const FString AssetName = FPackageName::GetLongPackageAssetName(LowMatrixMontagePackage);
	UAnimMontage* Montage = Cast<UAnimMontage>(
		StaticDuplicateObject(LowRightSource, Package, *AssetName));
	if (!ConfigureLowMatrixMontage(Montage, LowRightSource, LowCenterSequence, OutErrors))
	{
		return nullptr;
	}
	Montage->SetFlags(RF_Public | RF_Standalone | RF_Transactional);
	FAssetRegistryModule::AssetCreated(Montage);
	Montage->MarkPackageDirty();
	return Montage;
}

bool AddContactMatrixRotationWarp(
	UAnimMontage* Montage,
	const float SectionStart,
	const float Duration,
	TArray<FString>& OutErrors)
{
	UAnimNotifyState_CombatWarp* CombatWarp =
		NewObject<UAnimNotifyState_CombatWarp>(Montage);
	CombatWarp->TargetWarpName = TEXT("AttackTarget");
	CombatWarp->RotationWarpName = TEXT("RotationTarget");
	CombatWarp->bEnableTranslationForTarget = false;
	URootMotionModifier_Warp* Warp =
		Cast<URootMotionModifier_Warp>(CombatWarp->RootMotionModifier);
	if (!Warp)
	{
		OutErrors.Add(TEXT("contact-matrix combat-warp notify did not provide a modifier template"));
		return false;
	}
	Warp->WarpTargetName = TEXT("AttackTarget");
	Warp->bWarpRotation = true;
	Warp->bWarpTranslation = false;

	FAnimNotifyEvent Event;
	Event.NotifyStateClass = CombatWarp;
	Event.TrackIndex = 0;
	Event.Link(Montage, SectionStart, 0);
	Event.SetDuration(Duration);
	Event.EndLink.Link(Montage, SectionStart + Duration, 0);
	Montage->Notifies.Add(MoveTemp(Event));
	return true;
}

bool AppendContactMatrixSection(
	UAnimMontage* Montage,
	const FAttackVariantRecipe& Recipe,
	float& InOutDestinationStart,
	TArray<FString>& OutErrors)
{
	UAttackData* SourceAttack = LoadObjectAtPath<UAttackData>(Recipe.SourceAttackPath);
	UAnimSequenceBase* SequenceOverride = Recipe.SequenceOverridePath.IsEmpty()
		? nullptr
		: LoadObjectAtPath<UAnimSequenceBase>(Recipe.SequenceOverridePath);
	UAnimMontage* SourceMontage = SourceAttack ? SourceAttack->AttackMontage.Get() : nullptr;
	if (!SourceAttack || (!SequenceOverride && !SourceMontage))
	{
		OutErrors.Add(FString::Printf(
			TEXT("contact-matrix animation source is unavailable for %s"),
			*Recipe.Name.ToString()));
		return false;
	}

	float SourceStart = 0.0f;
	float SourceEnd = SequenceOverride ? SequenceOverride->GetPlayLength() : 0.0f;
	if (!SequenceOverride)
	{
		SourceAttack->GetSectionTimeRange(SourceStart, SourceEnd);
	}
	const float SectionLength = SourceEnd - SourceStart;
	const float RecoveryStart = Recipe.ActiveStartOffset + Recipe.ActiveDuration;
	USkeleton* SourceSkeleton = SequenceOverride
		? SequenceOverride->GetSkeleton()
		: SourceMontage->GetSkeleton();
	if (!SourceSkeleton || SectionLength <= 0.0f
		|| RecoveryStart > SectionLength + KINDA_SMALL_NUMBER)
	{
		OutErrors.Add(FString::Printf(
			TEXT("contact-matrix timing or skeleton is invalid for %s (length %.3f, recovery %.3f)"),
			*Recipe.Name.ToString(), SectionLength, RecoveryStart));
		return false;
	}
	if (!Montage->GetSkeleton())
	{
		Montage->SetSkeleton(SourceSkeleton);
	}
	else if (Montage->GetSkeleton() != SourceSkeleton)
	{
		OutErrors.Add(FString::Printf(
			TEXT("contact-matrix source skeleton differs for %s"),
			*Recipe.Name.ToString()));
		return false;
	}

	FAnimTrack& DestinationTrack = Montage->SlotAnimTracks[0].AnimTrack;
	const float DestinationStart = InOutDestinationStart;
	if (SequenceOverride)
	{
		FAnimSegment Segment;
		Segment.SetAnimReference(SequenceOverride);
		Segment.StartPos = DestinationStart;
		Segment.AnimStartTime = 0.0f;
		Segment.AnimEndTime = SectionLength;
		Segment.AnimPlayRate = 1.0f;
		Segment.LoopingCount = 1;
		DestinationTrack.AnimSegments.Add(MoveTemp(Segment));
	}
	else
	{
		if (SourceMontage->SlotAnimTracks.Num() != 1
			|| SourceMontage->SlotAnimTracks[0].SlotName != TEXT("DefaultSlot"))
		{
			OutErrors.Add(FString::Printf(
				TEXT("contact-matrix source %s must have one DefaultSlot track"),
				*Recipe.Name.ToString()));
			return false;
		}
		int32 CopiedSegmentCount = 0;
		for (const FAnimSegment& SourceSegment :
			SourceMontage->SlotAnimTracks[0].AnimTrack.AnimSegments)
		{
			const float SegmentStart = SourceSegment.StartPos;
			const float SegmentEnd = SegmentStart + SourceSegment.GetLength();
			const float OverlapStart = FMath::Max(SourceStart, SegmentStart);
			const float OverlapEnd = FMath::Min(SourceEnd, SegmentEnd);
			if (OverlapEnd <= OverlapStart + KINDA_SMALL_NUMBER)
			{
				continue;
			}
			if (SourceSegment.LoopingCount != 1 || SourceSegment.AnimPlayRate <= 0.0f)
			{
				OutErrors.Add(FString::Printf(
					TEXT("contact-matrix source %s uses an unsupported loop or play rate"),
					*Recipe.Name.ToString()));
				return false;
			}
			FAnimSegment Copy = SourceSegment;
			Copy.StartPos = DestinationStart + (OverlapStart - SourceStart);
			Copy.AnimStartTime = SourceSegment.AnimStartTime
				+ (OverlapStart - SegmentStart) * SourceSegment.AnimPlayRate;
			Copy.AnimEndTime = Copy.AnimStartTime
				+ (OverlapEnd - OverlapStart) * SourceSegment.AnimPlayRate;
			Copy.LoopingCount = 1;
			DestinationTrack.AnimSegments.Add(MoveTemp(Copy));
			++CopiedSegmentCount;
		}
		if (CopiedSegmentCount == 0)
		{
			OutErrors.Add(FString::Printf(
				TEXT("contact-matrix source section has no animation segments for %s"),
				*Recipe.Name.ToString()));
			return false;
		}
	}

	const float DestinationEnd = DestinationStart + SectionLength;
	Montage->SetCompositeLength(DestinationEnd);
	FCompositeSection Section;
	Section.SectionName = Recipe.Name;
	Section.SetTime(DestinationStart);
	Montage->CompositeSections.Add(MoveTemp(Section));

	if (SequenceOverride)
	{
		if (!AddContactMatrixRotationWarp(
			Montage, DestinationStart, RecoveryStart, OutErrors))
		{
			return false;
		}
	}
	else
	{
		for (const FAnimNotifyEvent& SourceEvent : SourceMontage->Notifies)
		{
			const float SourceEventTime = SourceEvent.GetTriggerTime();
			if (SourceEventTime < SourceStart - KINDA_SMALL_NUMBER
				|| SourceEventTime >= SourceEnd - KINDA_SMALL_NUMBER
				|| IsDefenseChainWindowEvent(SourceEvent)
				|| IsAttackPhaseTransitionEvent(SourceEvent))
			{
				continue;
			}
			const float DestinationTime =
				DestinationStart + (SourceEventTime - SourceStart);
			const float Duration = FMath::Min(
				SourceEvent.GetDuration(), SourceEnd - SourceEventTime);
			Montage->Notifies.Add(DuplicateNotifyEvent(
				SourceEvent, Montage, DestinationTime, Duration));
		}
	}
	AddPhaseTransition(Montage, EAttackPhase::Active,
		DestinationStart + Recipe.ActiveStartOffset);
	AddPhaseTransition(Montage, EAttackPhase::Recovery,
		DestinationStart + RecoveryStart);
	InOutDestinationStart = DestinationEnd;
	return true;
}

bool ConfigureContactMatrixMontage(
	UAnimMontage* Montage,
	TArray<FString>& OutErrors)
{
	if (!Montage)
	{
		OutErrors.Add(TEXT("contact-matrix montage is null"));
		return false;
	}
	Montage->SlotAnimTracks.Reset();
	Montage->CompositeSections.Reset();
	Montage->Notifies.Reset();
	Montage->AnimNotifyTracks.Reset();
	FSlotAnimationTrack Slot;
	Slot.SlotName = TEXT("DefaultSlot");
	Montage->SlotAnimTracks.Add(MoveTemp(Slot));
	FAnimNotifyTrack NotifyTrack;
	NotifyTrack.TrackName = TEXT("DefenseProof");
	Montage->AnimNotifyTracks.Add(MoveTemp(NotifyTrack));

	float DestinationStart = 0.0f;
	for (const FAttackVariantRecipe& Recipe : BuildAttackVariantRecipes())
	{
		if (Recipe.bUseContactMatrixMontage
			&& !AppendContactMatrixSection(
				Montage, Recipe, DestinationStart, OutErrors))
		{
			return false;
		}
	}
	for (FCompositeSection& Section : Montage->CompositeSections)
	{
		Section.Link(Montage, Section.GetTime(), 0);
	}
	Montage->SortNotifies();
	Montage->RefreshCacheData();
	return true;
}

bool ContactMatrixMontageMatches(
	const UAnimMontage* Montage,
	TArray<FString>* OutMismatches = nullptr)
{
	UAnimMontage* Expected = NewObject<UAnimMontage>(GetTransientPackage());
	TArray<FString> Errors;
	if (!ConfigureContactMatrixMontage(Expected, Errors))
	{
		if (OutMismatches)
		{
			OutMismatches->Append(Errors);
		}
		return false;
	}
	const FString ActualFacts = BuildLowMatrixMontageFacts(Montage);
	const FString ExpectedFacts = BuildLowMatrixMontageFacts(Expected);
	const bool bMatches = ActualFacts == ExpectedFacts;
	if (!bMatches && OutMismatches)
	{
		OutMismatches->Add(TEXT("contact-matrix ")
			+ DescribeFirstFactMismatch(ActualFacts, ExpectedFacts));
	}
	return bMatches;
}

UAnimMontage* CreateContactMatrixMontage(TArray<FString>& OutErrors)
{
	UPackage* Package = CreateAssetPackage(ContactMatrixMontagePackage, OutErrors);
	if (!Package)
	{
		return nullptr;
	}
	const FString AssetName =
		FPackageName::GetLongPackageAssetName(ContactMatrixMontagePackage);
	UAnimMontage* Montage = NewObject<UAnimMontage>(
		Package, *AssetName, RF_Public | RF_Standalone | RF_Transactional);
	if (!ConfigureContactMatrixMontage(Montage, OutErrors))
	{
		return nullptr;
	}
	FAssetRegistryModule::AssetCreated(Montage);
	Montage->MarkPackageDirty();
	return Montage;
}

bool ConfigureAttackVariant(
	UAttackData* Attack,
	const UAttackData* Source,
	const FAttackVariantRecipe& Recipe,
	UAnimMontage* LowMatrixMontage,
	UAnimMontage* ContactMatrixMontage,
	TArray<FString>& OutErrors)
{
	if (!Attack || !Source
		|| (Recipe.bUseLowMatrixMontage && !LowMatrixMontage)
		|| (Recipe.bUseContactMatrixMontage && !ContactMatrixMontage))
	{
		OutErrors.Add(FString::Printf(TEXT("attack variant sources are unavailable for %s"),
			*Recipe.Name.ToString()));
		return false;
	}
	if (Attack != Source)
	{
		CopyPersistentProperties(Attack, Source);
	}
	Attack->AttackType = EAttackType::Light;
	Attack->NextComboAttack = nullptr;
	Attack->HeavyComboAttack = nullptr;
	Attack->DirectionalFollowUps.Reset();
	Attack->HeavyDirectionalFollowUps.Reset();
	Attack->bCanHold = false;
	Attack->CounterData = nullptr;
	Attack->FinisherData = nullptr;
	Attack->bHasCounterVariant = false;
	Attack->bCanTriggerFinisher = false;
	Attack->RequiredContextTags.Reset();
	Attack->AttackTags.Reset();
	Attack->AttackTags.AddTag(
		FGameplayTag::RequestGameplayTag(TEXT("Attack.Type.Light")));
	if (Recipe.bBlockInterruptible)
	{
		Attack->AttackTags.AddTag(
			KatanaCombatGameplayTags::AttackDefenseBlockInterruptible());
	}
	Attack->DefenseProfile.Height = Recipe.Height;
	Attack->DefenseProfile.NominalLane = Recipe.Lane;
	Attack->DefenseProfile.SwingShape = Recipe.Swing;
	Attack->DefenseProfile.SourceContactSocketOverride = NAME_None;
	Attack->DefenseProfile.DefenderTargetBoneFallback = Recipe.TargetBone;
	Attack->DefaultContactBone = Recipe.TargetBone;
	Attack->bUseSectionOnly = true;
	Attack->bJumpToSectionStart = true;
	if (Recipe.bUseContactMatrixMontage)
	{
		Attack->AttackMontage = ContactMatrixMontage;
		Attack->MontageSection = Recipe.Name;
		const int32 SectionIndex = ContactMatrixMontage->GetSectionIndex(Recipe.Name);
		if (SectionIndex == INDEX_NONE)
		{
			OutErrors.Add(FString::Printf(
				TEXT("contact-matrix montage is missing section %s"),
				*Recipe.Name.ToString()));
			return false;
		}
		float SectionStart = 0.0f;
		float SectionEnd = 0.0f;
		ContactMatrixMontage->GetSectionStartAndEndTime(
			SectionIndex, SectionStart, SectionEnd);
		Attack->ManualTiming.WindupDuration = Recipe.ActiveStartOffset;
		Attack->ManualTiming.ActiveDuration = Recipe.ActiveDuration;
		Attack->ManualTiming.RecoveryDuration = FMath::Max(
			0.0f, SectionEnd - SectionStart
				- Recipe.ActiveStartOffset - Recipe.ActiveDuration);
	}
	else if (Recipe.bUseLowMatrixMontage)
	{
		Attack->AttackMontage = LowMatrixMontage;
		if (Recipe.bUseLowCenterSection)
		{
			const int32 SectionIndex =
				LowMatrixMontage->GetSectionIndex(TEXT("LowCenter"));
			if (SectionIndex == INDEX_NONE)
			{
				OutErrors.Add(TEXT("low-matrix montage is missing its LowCenter section"));
				return false;
			}
			float SectionStart = 0.0f;
			float SectionEnd = 0.0f;
			LowMatrixMontage->GetSectionStartAndEndTime(
				SectionIndex, SectionStart, SectionEnd);
			Attack->MontageSection = TEXT("LowCenter");
			Attack->WarpConfig.bEnableWarp = true;
			Attack->WarpConfig.AlreadyFacingThreshold = 0.0f;
			Attack->WarpConfig.TargetWarpName = TEXT("AttackTarget");
			Attack->WarpConfig.RotationWarpName = TEXT("RotationTarget");
			Attack->WarpConfig.MaxWarpDistance = 0.0f;
			Attack->WarpConfig.MinWarpDistance = 200.0f;
			Attack->WarpConfig.TargetRelativeOffset = FVector::ZeroVector;
			Attack->WarpConfig.RotationSpeed = 180.0f;
			Attack->ManualTiming.WindupDuration = LowCenterActiveStart;
			Attack->ManualTiming.ActiveDuration =
				LowCenterRecoveryStart - LowCenterActiveStart;
			Attack->ManualTiming.RecoveryDuration = FMath::Max(
				0.0f, SectionEnd - SectionStart - LowCenterRecoveryStart);
		}
	}
	return true;
}

UAttackData* BuildExpectedAttackVariant(
	UAttackData* Source,
	const FAttackVariantRecipe& Recipe,
	UAnimMontage* LowMatrixMontage,
	UAnimMontage* ContactMatrixMontage,
	TArray<FString>& OutErrors)
{
	UAttackData* Expected = Source
		? DuplicateObject<UAttackData>(Source, GetTransientPackage()) : nullptr;
	return ConfigureAttackVariant(
		Expected, Source, Recipe, LowMatrixMontage, ContactMatrixMontage, OutErrors)
		? Expected : nullptr;
}

UAttackData* CreateAttackVariant(
	UAttackData* Source,
	const FAttackVariantRecipe& Recipe,
	UAnimMontage* LowMatrixMontage,
	UAnimMontage* ContactMatrixMontage,
	TArray<FString>& OutErrors)
{
	UPackage* Package = CreateAssetPackage(Recipe.DestinationPackage, OutErrors);
	if (!Package || !Source)
	{
		return nullptr;
	}
	const FString AssetName =
		FPackageName::GetLongPackageAssetName(Recipe.DestinationPackage);
	UAttackData* Attack = Cast<UAttackData>(
		StaticDuplicateObject(Source, Package, *AssetName));
	if (!ConfigureAttackVariant(
		Attack, Source, Recipe, LowMatrixMontage, ContactMatrixMontage, OutErrors))
	{
		return nullptr;
	}
	Attack->SetFlags(RF_Public | RF_Standalone | RF_Transactional);
	FAssetRegistryModule::AssetCreated(Attack);
	Attack->MarkPackageDirty();
	return Attack;
}

void ClearContactOverrides(FDefensePresentationPayload& Payload)
{
	Payload.SourceSocketOverride = NAME_None;
	Payload.TargetBoneOverride = NAME_None;
}

bool ConfigureGateBDefenseConfiguration(
	UDefenseConfiguration* Configuration,
	UAnimMontage* RecoilMontage,
	TArray<FString>& OutErrors)
{
	if (!Configuration || !RecoilMontage)
	{
		OutErrors.Add(TEXT("Gate B configuration requires its source config and recoil montage"));
		return false;
	}
	for (FDefensePresentationRow& Row : Configuration->DefenderPresentationRows)
	{
		ClearContactOverrides(Row.Payload);
	}
	for (FAttackerResponsePresentationRow& Row : Configuration->AttackerResponseRows)
	{
		ClearContactOverrides(Row.Payload);
	}
	Configuration->AttackerResponseRows.RemoveAll(
		[](const FAttackerResponsePresentationRow& Row)
		{
			return Row.RowName == TEXT("RecoilGeneric");
		});
	Configuration->BoneHeightRows.RemoveAll(
		[](const FDefenseBoneHeightRow& Row)
		{
			return Row.BoneName == TEXT("spine_01");
		});
	Configuration->BoneHeightRows.Add(
		FDefenseBoneHeightRow(TEXT("spine_01"), EAttackHeight::Middle));
	FAttackerResponsePresentationRow Recoil;
	Recoil.RowName = TEXT("RecoilGeneric");
	Recoil.Response = EAttackerResponse::Recoil;
	Recoil.Payload.Montage = RecoilMontage;
	Recoil.Payload.MontageSection = TEXT("Recoil");
	Recoil.Payload.BlendInSeconds = 0.10f;
	Recoil.Payload.BlendOutSeconds = 0.10f;
	Recoil.Payload.bEnableRotationWarp = true;
	Recoil.Payload.MaximumTranslation = 0.0f;
	Configuration->AttackerResponseRows.Add(MoveTemp(Recoil));
	return true;
}

UDefenseConfiguration* BuildExpectedConfiguration(
	UDefenseConfiguration* Source,
	UAnimMontage* RecoilMontage,
	TArray<FString>& OutErrors)
{
	UDefenseConfiguration* Expected = Source
		? DuplicateObject<UDefenseConfiguration>(Source, GetTransientPackage()) : nullptr;
	return ConfigureGateBDefenseConfiguration(Expected, RecoilMontage, OutErrors)
		? Expected : nullptr;
}

UDefenseConfiguration* CreateDefenseConfiguration(
	UDefenseConfiguration* Source,
	UAnimMontage* RecoilMontage,
	TArray<FString>& OutErrors)
{
	UPackage* Package = CreateAssetPackage(DefenseConfigurationPackage, OutErrors);
	if (!Package || !Source)
	{
		return nullptr;
	}
	const FString AssetName =
		FPackageName::GetLongPackageAssetName(DefenseConfigurationPackage);
	UDefenseConfiguration* Configuration = Cast<UDefenseConfiguration>(
		StaticDuplicateObject(Source, Package, *AssetName));
	if (!ConfigureGateBDefenseConfiguration(Configuration, RecoilMontage, OutErrors))
	{
		return nullptr;
	}
	Configuration->SetFlags(RF_Public | RF_Standalone | RF_Transactional);
	FAssetRegistryModule::AssetCreated(Configuration);
	Configuration->MarkPackageDirty();
	return Configuration;
}

UCombatSettings* BuildExpectedCombatSettings(
	UCombatSettings* Source,
	UDefenseConfiguration* Configuration)
{
	UCombatSettings* Expected = Source
		? DuplicateObject<UCombatSettings>(Source, GetTransientPackage()) : nullptr;
	if (Expected)
	{
		Expected->DefenseConfiguration = Configuration;
	}
	return Expected;
}

UCombatSettings* CreateCombatSettings(
	UCombatSettings* Source,
	UDefenseConfiguration* Configuration,
	TArray<FString>& OutErrors)
{
	UPackage* Package = CreateAssetPackage(CombatSettingsPackage, OutErrors);
	if (!Package || !Source || !Configuration)
	{
		return nullptr;
	}
	const FString AssetName = FPackageName::GetLongPackageAssetName(CombatSettingsPackage);
	UCombatSettings* Settings = Cast<UCombatSettings>(
		StaticDuplicateObject(Source, Package, *AssetName));
	if (!Settings)
	{
		OutErrors.Add(TEXT("could not duplicate the default combat settings"));
		return nullptr;
	}
	Settings->DefenseConfiguration = Configuration;
	Settings->SetFlags(RF_Public | RF_Standalone | RF_Transactional);
	FAssetRegistryModule::AssetCreated(Settings);
	Settings->MarkPackageDirty();
	return Settings;
}

bool BlueprintMatches(
	const UBlueprint* Blueprint,
	const UBlueprint* SourceBlueprint,
	const UCombatSettings* Settings,
	const bool bPlayer)
{
	if (!Blueprint || !SourceBlueprint || !SourceBlueprint->GeneratedClass
		|| Blueprint->ParentClass != SourceBlueprint->GeneratedClass
		|| !Blueprint->GeneratedClass)
	{
		return false;
	}
	const ABaseCombatCharacter* Default = Cast<ABaseCombatCharacter>(
		Blueprint->GeneratedClass->GetDefaultObject());
	if (!Default || Default->CombatSettings != Settings)
	{
		return false;
	}
	return bPlayer
		? Blueprint->GeneratedClass->IsChildOf(APlayerCharacter::StaticClass())
		: Blueprint->GeneratedClass->IsChildOf(AEnemyCharacter::StaticClass());
}

UBlueprint* CreateFixtureBlueprint(
	UBlueprint* SourceBlueprint,
	const FString& DestinationPackage,
	UCombatSettings* Settings,
	TArray<FString>& OutErrors)
{
	if (!SourceBlueprint || !SourceBlueprint->GeneratedClass || !Settings)
	{
		OutErrors.Add(FString::Printf(TEXT("fixture Blueprint source is unavailable for %s"),
			*DestinationPackage));
		return nullptr;
	}
	UPackage* Package = CreateAssetPackage(DestinationPackage, OutErrors);
	if (!Package)
	{
		return nullptr;
	}
	const FString AssetName = FPackageName::GetLongPackageAssetName(DestinationPackage);
	UBlueprint* Blueprint = FKismetEditorUtilities::CreateBlueprint(
		SourceBlueprint->GeneratedClass,
		Package,
		FName(*AssetName),
		BPTYPE_Normal,
		UBlueprint::StaticClass(),
		UBlueprintGeneratedClass::StaticClass());
	if (!Blueprint)
	{
		OutErrors.Add(FString::Printf(TEXT("could not create fixture Blueprint: %s"),
			*DestinationPackage));
		return nullptr;
	}
	FKismetEditorUtilities::CompileBlueprint(Blueprint);
	ABaseCombatCharacter* Default = Blueprint->GeneratedClass
		? Cast<ABaseCombatCharacter>(Blueprint->GeneratedClass->GetDefaultObject()) : nullptr;
	if (!Default)
	{
		OutErrors.Add(FString::Printf(TEXT("fixture Blueprint has no combat-character CDO: %s"),
			*DestinationPackage));
		return nullptr;
	}
	Blueprint->Modify();
	Default->Modify();
	Default->CombatSettings = Settings;
	FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
	Blueprint->MarkPackageDirty();
	return Blueprint;
}

template <typename TActorType>
TActorType* FindNamedActor(const UWorld* World, const FName Name)
{
	if (!World || !World->PersistentLevel)
	{
		return nullptr;
	}
	for (AActor* Actor : World->PersistentLevel->Actors)
	{
		if (TActorType* TypedActor = Cast<TActorType>(Actor);
			TypedActor && TypedActor->GetFName() == Name)
		{
			return TypedActor;
		}
	}
	return nullptr;
}

FRotator FacePlayer(const FVector& EnemyLocation)
{
	return (PlayerLocation - EnemyLocation).Rotation();
}

bool CasesMatch(const ADefenseMatrixProofDirector* Director)
{
	const TArray<FCaseRecipe> Expected = BuildCaseRecipes();
	if (!Director || Director->ProofMaxConcurrentAttackers != 2
		|| !Director->bAutoStartHandsOffCase
		|| Director->HandsOffCase != TEXT("NormalBlockMiddleCenter")
		|| Director->Cases.Num() != Expected.Num())
	{
		return false;
	}
	for (int32 Index = 0; Index < Expected.Num(); ++Index)
	{
		const FDefenseMatrixProofCase& Actual = Director->Cases[Index];
		if (Actual.CaseName != Expected[Index].Name
			|| !Actual.Attack
			|| Actual.Attack->GetPathName() != Expected[Index].AttackPath
			|| Actual.AttackerAnchorTag != Expected[Index].AnchorTag
			|| !Actual.bApplyDefenderTransform
			|| !Actual.DefenderTransform.Equals(
				Expected[Index].DefenderTransform, 0.1f)
			|| !Actual.bApplyAttackerTransform
			|| !Actual.AttackerTransform.Equals(
				Expected[Index].AttackerTransform, 0.1f)
			|| Actual.bBeginHeldGuard != Expected[Index].bBeginHeldGuard)
		{
			return false;
		}
	}
	return true;
}

bool EnemyActorMatches(
	const AEnemyCharacter* Enemy,
	const UClass* EnemyClass,
	const FVector& Location,
	const FName AnchorTag)
{
	const USceneComponent* Root = Enemy ? Enemy->GetRootComponent() : nullptr;
	if (!Enemy || !Root || Enemy->GetClass() != EnemyClass
		|| !Root->GetRelativeLocation().Equals(Location, 0.1f)
		|| !Root->GetRelativeRotation().Equals(FacePlayer(Location), 0.1f)
		|| !Enemy->ActorHasTag(AnchorTag)
		|| Enemy->AutoPossessAI != EAutoPossessAI::PlacedInWorldOrSpawned)
	{
		return false;
	}
	const UEnemyCombatAIComponent* CombatAI = Enemy->GetCombatAIComponent();
	if (!CombatAI
		|| !NearlyEqual(CombatAI->ApproachConfig.AttackRange, 350.0f)
		|| !NearlyEqual(CombatAI->ApproachConfig.ApproachTimeout, 5.0f)
		|| CombatAI->AttackSelectionMode != EEnemyAttackSelection::Sequential)
	{
		return false;
	}
	const TArray<FString> ExpectedAttacks = BuildMapAttackPaths();
	if (CombatAI->AvailableAttacks.Num() != ExpectedAttacks.Num())
	{
		return false;
	}
	for (int32 Index = 0; Index < ExpectedAttacks.Num(); ++Index)
	{
		const FEnemyAttackConfig& Config = CombatAI->AvailableAttacks[Index];
		if (!Config.AttackData
			|| Config.AttackData->GetPathName() != ExpectedAttacks[Index]
			|| !NearlyEqual(Config.SelectionWeight, 1.0f)
			|| !NearlyEqual(Config.MinRange, 0.0f)
			|| !NearlyEqual(Config.MaxRange, 350.0f))
		{
			return false;
		}
	}
	return true;
}

bool MapMatches(
	const UWorld* World,
	const UBlueprint* PlayerBlueprint,
	const UBlueprint* EnemyBlueprint,
	TArray<FString>* OutMismatches = nullptr)
{
	auto RecordMismatch = [OutMismatches](const FString& Message)
	{
		if (OutMismatches)
		{
			OutMismatches->Add(FString::Printf(TEXT("proof map mismatch: %s"), *Message));
		}
		return false;
	};
	bool bMatches = true;
	auto Check = [&bMatches, &RecordMismatch](const bool bCondition, const FString& Message)
	{
		if (!bCondition)
		{
			bMatches = RecordMismatch(Message) && bMatches;
		}
	};
	if (!World || !PlayerBlueprint || !PlayerBlueprint->GeneratedClass
		|| !EnemyBlueprint || !EnemyBlueprint->GeneratedClass)
	{
		return RecordMismatch(TEXT("world or fixture Blueprint class did not load"));
	}
	const APlayerCharacter* Player = FindNamedActor<APlayerCharacter>(World, PlayerActorName);
	const AEnemyCharacter* Left = FindNamedActor<AEnemyCharacter>(World, LeftEnemyActorName);
	const AEnemyCharacter* Center = FindNamedActor<AEnemyCharacter>(World, CenterEnemyActorName);
	const AEnemyCharacter* Right = FindNamedActor<AEnemyCharacter>(World, RightEnemyActorName);
	const ADefenseMatrixProofDirector* Director =
		FindNamedActor<ADefenseMatrixProofDirector>(World, DirectorActorName);
	const AStaticMeshActor* Floor = FindNamedActor<AStaticMeshActor>(World, FloorActorName);
	const ADirectionalLight* Directional =
		FindNamedActor<ADirectionalLight>(World, DirectionalLightActorName);
	const ASkyLight* Sky = FindNamedActor<ASkyLight>(World, SkyLightActorName);
	const ASkyAtmosphere* Atmosphere =
		FindNamedActor<ASkyAtmosphere>(World, SkyAtmosphereActorName);
	Check(Player != nullptr, TEXT("named player actor is missing"));
	if (Player)
	{
		const USceneComponent* PlayerRoot = Player->GetRootComponent();
		Check(Player->GetClass() == PlayerBlueprint->GeneratedClass,
			FString::Printf(TEXT("player class is %s"), *GetPathNameSafe(Player->GetClass())));
		Check(PlayerRoot != nullptr, TEXT("player root component is missing"));
		if (PlayerRoot)
		{
			Check(PlayerRoot->GetRelativeLocation().Equals(PlayerLocation, 0.1f),
				FString::Printf(TEXT("player location is %s"), *PlayerRoot->GetRelativeLocation().ToString()));
			Check(PlayerRoot->GetRelativeRotation().IsNearlyZero(0.1f),
				FString::Printf(TEXT("player rotation is %s"), *PlayerRoot->GetRelativeRotation().ToString()));
		}
		Check(Player->ActorHasTag(PlayerFixtureTag), TEXT("player fixture tag is missing"));
		Check(Player->AutoPossessPlayer == EAutoReceiveInput::Player0,
			FString::Printf(TEXT("player auto-possess value is %d"), static_cast<int32>(Player->AutoPossessPlayer)));
	}
	Check(EnemyActorMatches(Left, EnemyBlueprint->GeneratedClass,
		LeftEnemyLocation, LeftAnchorTag), TEXT("left enemy differs from recipe"));
	Check(EnemyActorMatches(Center, EnemyBlueprint->GeneratedClass,
		CenterEnemyLocation, CenterAnchorTag), TEXT("center enemy differs from recipe"));
	Check(EnemyActorMatches(Right, EnemyBlueprint->GeneratedClass,
		RightEnemyLocation, RightAnchorTag), TEXT("right enemy differs from recipe"));
	Check(CasesMatch(Director), TEXT("proof director case catalog differs from recipe"));
	Check(Floor != nullptr, TEXT("named floor actor is missing"));
	if (Floor)
	{
		const UStaticMeshComponent* FloorComponent = Floor->GetStaticMeshComponent();
		Check(FloorComponent != nullptr, TEXT("floor static-mesh component is missing"));
		if (FloorComponent)
		{
			Check(FloorComponent->GetStaticMesh() != nullptr, TEXT("floor mesh is missing"));
			if (FloorComponent->GetStaticMesh())
			{
				Check(FloorComponent->GetStaticMesh()->GetPathName() == FloorMeshPath,
					FString::Printf(TEXT("floor mesh is %s"), *FloorComponent->GetStaticMesh()->GetPathName()));
			}
		}
		const USceneComponent* FloorRoot = Floor->GetRootComponent();
		Check(FloorRoot != nullptr, TEXT("floor root component is missing"));
		if (FloorRoot)
		{
			Check(FloorRoot->GetRelativeScale3D().Equals(FVector(12.0f, 12.0f, 1.0f), 0.01f),
				FString::Printf(TEXT("floor scale is %s"), *FloorRoot->GetRelativeScale3D().ToString()));
		}
	}
	Check(Directional != nullptr, TEXT("named directional light is missing"));
	Check(Sky != nullptr, TEXT("named sky light is missing"));
	Check(Atmosphere != nullptr, TEXT("named sky atmosphere is missing"));
	int32 PlayerCount = 0;
	int32 EnemyCount = 0;
	for (const AActor* Actor : World->PersistentLevel->Actors)
	{
		PlayerCount += Actor && Actor->IsA<APlayerCharacter>();
		EnemyCount += Actor && Actor->IsA<AEnemyCharacter>();
	}
	Check(PlayerCount == 1,
		FString::Printf(TEXT("expected one player actor but found %d"), PlayerCount));
	Check(EnemyCount == 3,
		FString::Printf(TEXT("expected three enemy actors but found %d"), EnemyCount));
	return bMatches;
}

template <typename TActorType>
TActorType* SpawnNamedActor(
	UWorld* World,
	UClass* ActorClass,
	const FName Name,
	const FVector& Location,
	const FRotator& Rotation)
{
	FActorSpawnParameters Params;
	Params.Name = Name;
	Params.OverrideLevel = World->PersistentLevel;
	Params.SpawnCollisionHandlingOverride =
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Params.ObjectFlags = RF_Transactional;
	return Cast<TActorType>(World->SpawnActor(ActorClass, &Location, &Rotation, Params));
}

bool ConfigureEnemyActor(
	AEnemyCharacter* Enemy,
	const FName AnchorTag,
	const TArray<UAttackData*>& Attacks,
	TArray<FString>& OutErrors)
{
	UEnemyCombatAIComponent* CombatAI = Enemy ? Enemy->GetCombatAIComponent() : nullptr;
	if (!Enemy || !CombatAI)
	{
		OutErrors.Add(TEXT("spawned defense-matrix enemy has no combat AI component"));
		return false;
	}
	Enemy->Modify();
	CombatAI->Modify();
	Enemy->Tags = {AnchorTag};
	Enemy->AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;
	CombatAI->AvailableAttacks.Reset();
	for (UAttackData* Attack : Attacks)
	{
		FEnemyAttackConfig Config;
		Config.AttackData = Attack;
		Config.SelectionWeight = 1.0f;
		Config.MinRange = 0.0f;
		Config.MaxRange = 350.0f;
		CombatAI->AvailableAttacks.Add(MoveTemp(Config));
	}
	CombatAI->AttackSelectionMode = EEnemyAttackSelection::Sequential;
	CombatAI->ApproachConfig.AttackRange = 350.0f;
	CombatAI->ApproachConfig.ApproachTimeout = 5.0f;
	return true;
}

UWorld* CreateProofMap(
	UBlueprint* PlayerBlueprint,
	UBlueprint* EnemyBlueprint,
	TArray<FString>& OutErrors)
{
	if (!PlayerBlueprint || !PlayerBlueprint->GeneratedClass
		|| !EnemyBlueprint || !EnemyBlueprint->GeneratedClass)
	{
		OutErrors.Add(TEXT("fixture Blueprints must compile before map authoring"));
		return nullptr;
	}
	TArray<UAttackData*> Attacks;
	for (const FString& Path : BuildMapAttackPaths())
	{
		Attacks.Add(LoadObjectAtPath<UAttackData>(Path));
	}
	if (Attacks.Contains(nullptr))
	{
		OutErrors.Add(TEXT("one or more defense-matrix AttackData assets did not load"));
		return nullptr;
	}
	UStaticMesh* FloorMesh = LoadObjectAtPath<UStaticMesh>(FloorMeshPath);
	if (!FloorMesh)
	{
		OutErrors.Add(TEXT("engine proof-floor mesh did not load"));
		return nullptr;
	}
	UPackage* Package = CreateAssetPackage(MapPackage, OutErrors);
	if (!Package)
	{
		return nullptr;
	}
	const FName WorldName(*FPackageName::GetLongPackageAssetName(MapPackage));
	UWorld* World = UWorld::CreateWorld(
		EWorldType::Editor, false, WorldName, Package, false);
	if (!World)
	{
		OutErrors.Add(TEXT("could not create the defense-matrix world"));
		return nullptr;
	}
	World->SetFlags(RF_Public | RF_Standalone);
	FAssetRegistryModule::AssetCreated(World);

	AStaticMeshActor* Floor = SpawnNamedActor<AStaticMeshActor>(
		World, AStaticMeshActor::StaticClass(), FloorActorName,
		FVector::ZeroVector, FRotator::ZeroRotator);
	ADirectionalLight* Directional = SpawnNamedActor<ADirectionalLight>(
		World, ADirectionalLight::StaticClass(), DirectionalLightActorName,
		FVector(0.0f, 0.0f, 500.0f), FRotator(-45.0f, -30.0f, 0.0f));
	ASkyLight* Sky = SpawnNamedActor<ASkyLight>(
		World, ASkyLight::StaticClass(), SkyLightActorName,
		FVector(0.0f, 0.0f, 300.0f), FRotator::ZeroRotator);
	ASkyAtmosphere* Atmosphere = SpawnNamedActor<ASkyAtmosphere>(
		World, ASkyAtmosphere::StaticClass(), SkyAtmosphereActorName,
		FVector::ZeroVector, FRotator::ZeroRotator);
	APlayerCharacter* Player = SpawnNamedActor<APlayerCharacter>(
		World, PlayerBlueprint->GeneratedClass, PlayerActorName,
		PlayerLocation, FRotator::ZeroRotator);
	AEnemyCharacter* Left = SpawnNamedActor<AEnemyCharacter>(
		World, EnemyBlueprint->GeneratedClass, LeftEnemyActorName,
		LeftEnemyLocation, FacePlayer(LeftEnemyLocation));
	AEnemyCharacter* Center = SpawnNamedActor<AEnemyCharacter>(
		World, EnemyBlueprint->GeneratedClass, CenterEnemyActorName,
		CenterEnemyLocation, FacePlayer(CenterEnemyLocation));
	AEnemyCharacter* Right = SpawnNamedActor<AEnemyCharacter>(
		World, EnemyBlueprint->GeneratedClass, RightEnemyActorName,
		RightEnemyLocation, FacePlayer(RightEnemyLocation));
	ADefenseMatrixProofDirector* Director =
		SpawnNamedActor<ADefenseMatrixProofDirector>(
			World, ADefenseMatrixProofDirector::StaticClass(), DirectorActorName,
			FVector(0.0f, 0.0f, 50.0f), FRotator::ZeroRotator);
	if (!Floor || !Directional || !Sky || !Atmosphere || !Player
		|| !Left || !Center || !Right || !Director)
	{
		OutErrors.Add(TEXT("one or more required defense-matrix actors failed to spawn"));
		return nullptr;
	}

	Floor->SetActorScale3D(FVector(12.0f, 12.0f, 1.0f));
	Floor->GetStaticMeshComponent()->SetStaticMesh(FloorMesh);
	Floor->GetStaticMeshComponent()->SetMobility(EComponentMobility::Static);
	Directional->GetLightComponent()->SetIntensity(5.0f);
	Sky->GetLightComponent()->SetIntensity(1.0f);
	Sky->GetLightComponent()->SetMobility(EComponentMobility::Movable);
	Player->Tags = {PlayerFixtureTag};
	Player->AutoPossessPlayer = EAutoReceiveInput::Player0;
	if (!ConfigureEnemyActor(Left, LeftAnchorTag, Attacks, OutErrors)
		|| !ConfigureEnemyActor(Center, CenterAnchorTag, Attacks, OutErrors)
		|| !ConfigureEnemyActor(Right, RightAnchorTag, Attacks, OutErrors))
	{
		return nullptr;
	}
	Director->ProofMaxConcurrentAttackers = 2;
	Director->bAutoStartHandsOffCase = true;
	Director->HandsOffCase = TEXT("NormalBlockMiddleCenter");
	Director->Cases.Reset();
	for (const FCaseRecipe& Recipe : BuildCaseRecipes())
	{
		FDefenseMatrixProofCase Case;
		Case.CaseName = Recipe.Name;
		Case.Attack = LoadObjectAtPath<UAttackData>(Recipe.AttackPath);
		Case.AttackerAnchorTag = Recipe.AnchorTag;
		Case.bApplyDefenderTransform = true;
		Case.DefenderTransform = Recipe.DefenderTransform;
		Case.bApplyAttackerTransform = true;
		Case.AttackerTransform = Recipe.AttackerTransform;
		Case.bBeginHeldGuard = Recipe.bBeginHeldGuard;
		Director->Cases.Add(MoveTemp(Case));
	}

#if WITH_EDITOR
	Floor->SetActorLabel(TEXT("Defense Matrix Floor"));
	Directional->SetActorLabel(TEXT("Defense Matrix Key Light"));
	Sky->SetActorLabel(TEXT("Defense Matrix Sky Light"));
	Atmosphere->SetActorLabel(TEXT("Defense Matrix Atmosphere"));
	Player->SetActorLabel(TEXT("Defense Matrix Player"));
	Left->SetActorLabel(TEXT("Defense Matrix Attacker Left"));
	Center->SetActorLabel(TEXT("Defense Matrix Attacker Center"));
	Right->SetActorLabel(TEXT("Defense Matrix Attacker Right"));
	Director->SetActorLabel(TEXT("Defense Matrix Director"));
#endif

	World->UpdateWorldComponents(true, false);
	World->MarkPackageDirty();
	return World;
}

bool UpdateProofMap(
	UWorld* World,
	UBlueprint* PlayerBlueprint,
	UBlueprint* EnemyBlueprint,
	TArray<FString>& OutErrors)
{
	if (!World || !PlayerBlueprint || !PlayerBlueprint->GeneratedClass
		|| !EnemyBlueprint || !EnemyBlueprint->GeneratedClass)
	{
		OutErrors.Add(TEXT("proof map update requires the world and compiled fixture Blueprints"));
		return false;
	}
	APlayerCharacter* Player = FindNamedActor<APlayerCharacter>(World, PlayerActorName);
	AEnemyCharacter* Left = FindNamedActor<AEnemyCharacter>(World, LeftEnemyActorName);
	AEnemyCharacter* Center = FindNamedActor<AEnemyCharacter>(World, CenterEnemyActorName);
	AEnemyCharacter* Right = FindNamedActor<AEnemyCharacter>(World, RightEnemyActorName);
	ADefenseMatrixProofDirector* Director =
		FindNamedActor<ADefenseMatrixProofDirector>(World, DirectorActorName);
	if (!Player || !Left || !Center || !Right || !Director
		|| Player->GetClass() != PlayerBlueprint->GeneratedClass
		|| Left->GetClass() != EnemyBlueprint->GeneratedClass
		|| Center->GetClass() != EnemyBlueprint->GeneratedClass
		|| Right->GetClass() != EnemyBlueprint->GeneratedClass)
	{
		OutErrors.Add(TEXT("proof map named fixture actors or classes differ from the update recipe"));
		return false;
	}

	TArray<UAttackData*> Attacks;
	for (const FString& Path : BuildMapAttackPaths())
	{
		Attacks.Add(LoadObjectAtPath<UAttackData>(Path));
	}
	if (Attacks.Contains(nullptr))
	{
		OutErrors.Add(TEXT("one or more V4 defense-matrix attacks did not load for map update"));
		return false;
	}

	World->Modify();
	Player->Modify();
	Player->SetActorTransform(
		FTransform(FRotator::ZeroRotator, PlayerLocation), false, nullptr,
		ETeleportType::TeleportPhysics);
	Player->Tags = {PlayerFixtureTag};
	Player->AutoPossessPlayer = EAutoReceiveInput::Player0;
	for (const auto& Entry : TArray<TTuple<AEnemyCharacter*, FVector, FName>>{
		{Left, LeftEnemyLocation, LeftAnchorTag},
		{Center, CenterEnemyLocation, CenterAnchorTag},
		{Right, RightEnemyLocation, RightAnchorTag}})
	{
		AEnemyCharacter* Enemy = Entry.Get<0>();
		const FVector& Location = Entry.Get<1>();
		Enemy->SetActorTransform(
			FTransform(FacePlayer(Location), Location), false, nullptr,
			ETeleportType::TeleportPhysics);
		if (!ConfigureEnemyActor(Enemy, Entry.Get<2>(), Attacks, OutErrors))
		{
			return false;
		}
	}

	Director->Modify();
	Director->ProofMaxConcurrentAttackers = 2;
	Director->bAutoStartHandsOffCase = true;
	Director->HandsOffCase = TEXT("NormalBlockMiddleCenter");
	Director->Cases.Reset();
	for (const FCaseRecipe& Recipe : BuildCaseRecipes())
	{
		FDefenseMatrixProofCase Case;
		Case.CaseName = Recipe.Name;
		Case.Attack = LoadObjectAtPath<UAttackData>(Recipe.AttackPath);
		Case.AttackerAnchorTag = Recipe.AnchorTag;
		Case.bApplyDefenderTransform = true;
		Case.DefenderTransform = Recipe.DefenderTransform;
		Case.bApplyAttackerTransform = true;
		Case.AttackerTransform = Recipe.AttackerTransform;
		Case.bBeginHeldGuard = Recipe.bBeginHeldGuard;
		if (!Case.Attack)
		{
			OutErrors.Add(FString::Printf(TEXT("proof case attack did not load: %s"),
				*Recipe.AttackPath));
			return false;
		}
		Director->Cases.Add(MoveTemp(Case));
	}
	World->UpdateWorldComponents(true, false);
	World->MarkPackageDirty();
	return MapMatches(World, PlayerBlueprint, EnemyBlueprint, &OutErrors);
}

void AddPlannedPackage(
	FDefenseMatrixAuthoringPlan& Plan,
	const FString& Change,
	const FString& PackageName,
	const FString& PackageRole,
	const FString& PlannedAction = TEXT("Create"))
{
	Plan.ProposedChanges.Add(Change);
	FKatanaAssetMigrationPackageLedgerEntry Entry;
	Entry.PackageName = PackageName;
	Entry.PackageRole = PackageRole;
	const UPackage* ExistingPackage = FindPackage(nullptr, *PackageName);
	Entry.bInitiallyDirty = ExistingPackage && ExistingPackage->IsDirty();
	Entry.PlannedAction = PlannedAction;
	Plan.PackageLedger.Add(MoveTemp(Entry));
}

void CollectObjectPaths(const TSharedPtr<FJsonValue>& Value, TSet<FString>& OutPaths)
{
	if (!Value.IsValid())
	{
		return;
	}
	switch (Value->Type)
	{
	case EJson::String:
	{
		const FString Candidate = Value->AsString();
		if (Candidate.StartsWith(TEXT("/Game/"))
			&& FPackageName::IsValidObjectPath(Candidate))
		{
			OutPaths.Add(Candidate);
		}
		break;
	}
	case EJson::Array:
		for (const TSharedPtr<FJsonValue>& Child : Value->AsArray())
		{
			CollectObjectPaths(Child, OutPaths);
		}
		break;
	case EJson::Object:
		for (const auto& Pair : Value->AsObject()->Values)
		{
			CollectObjectPaths(Pair.Value, OutPaths);
		}
		break;
	default:
		break;
	}
}

TArray<FString> BuildSourceIdentities(TArray<FString>& OutErrors)
{
	FString Json;
	const FString ResolvedManifest =
		FKatanaAssetMigrationRunner::ResolveProjectRelativeFilePath(ManifestPath);
	if (!FFileHelper::LoadFileToString(Json, *ResolvedManifest))
	{
		OutErrors.Add(FString::Printf(TEXT("could not read Gate B manifest: %s"),
			*ResolvedManifest));
		return {};
	}
	TSharedPtr<FJsonValue> RootValue;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
	if (!FJsonSerializer::Deserialize(Reader, RootValue) || !RootValue.IsValid())
	{
		OutErrors.Add(TEXT("could not parse Gate B manifest for approval dependencies"));
		return {};
	}
	TSet<FString> Paths;
	CollectObjectPaths(RootValue, Paths);
	for (const FString& Destination :
		FDefenseMatrixAuthoringOperation::GetDestinationPackageNames())
	{
		Paths.Remove(BuildObjectPath(Destination));
	}
	Paths.Add(SourceConfigurationPath);
	Paths.Add(SourceCombatSettingsPath);
	Paths.Add(SourcePlayerBlueprintPath);
	Paths.Add(SourceEnemyBlueprintPath);
	Paths.Add(RecoilSequencePath);
	Paths.Add(LowCenterSequencePath);
	Paths.Add(MiddleLeftSequencePath);
	Paths.Add(MiddleCenterSequencePath);
	Paths.Add(LowLeftSequencePath);
	for (const FAttackVariantRecipe& Recipe : BuildAttackVariantRecipes())
	{
		Paths.Add(Recipe.SourceAttackPath);
		if (const UAttackData* SourceAttack =
			LoadObjectAtPath<UAttackData>(Recipe.SourceAttackPath))
		{
			if (SourceAttack->AttackMontage)
			{
				Paths.Add(SourceAttack->AttackMontage->GetPathName());
			}
		}
	}
	TArray<FString> Result = Paths.Array();
	Result.Sort();
	return Result;
}

FString BuildContactMatrixSourceFacts()
{
	TSet<FString> SeenMontages;
	FString Facts;
	for (const FAttackVariantRecipe& Recipe : BuildAttackVariantRecipes())
	{
		if (!Recipe.bUseContactMatrixMontage || !Recipe.SequenceOverridePath.IsEmpty())
		{
			continue;
		}
		UAttackData* SourceAttack = LoadObjectAtPath<UAttackData>(Recipe.SourceAttackPath);
		UAnimMontage* SourceMontage = SourceAttack
			? SourceAttack->AttackMontage.Get() : nullptr;
		const FString Path = GetPathNameSafe(SourceMontage);
		if (!SourceMontage || SeenMontages.Contains(Path))
		{
			continue;
		}
		SeenMontages.Add(Path);
		Facts += TEXT("source=") + Path + TEXT("\n");
		Facts += BuildLowMatrixMontageFacts(SourceMontage);
	}
	return Facts;
}

FString BuildRecipeFacts(TArray<FString>& OutErrors)
{
	FString Facts = FString::Printf(
		TEXT("source_config=%s\nsource_settings=%s\nsource_player=%s\nsource_enemy=%s\nrecoil=%s|0.0|1.0|Recoil|AttackerResponseTarget|rotation=1|translation=0|warp=0.0..0.65\n"),
		*SourceConfigurationPath, *SourceCombatSettingsPath,
		*SourcePlayerBlueprintPath, *SourceEnemyBlueprintPath, *RecoilSequencePath);
	Facts += FString::Printf(
		TEXT("low_matrix=%s|source_right=%s|source_center=%s|strip_parry=1|strip_counter=1|section=LowCenter|active=%.3f|recovery=%.3f|combat_warp=rotation_only\n"),
		*LowMatrixMontagePackage, *LightAttack1Path, *LowCenterSequencePath,
		LowCenterActiveStart, LowCenterRecoveryStart);
	UAttackData* LowRightSourceAttack = LoadObjectAtPath<UAttackData>(LightAttack1Path);
	UAnimMontage* LowRightSourceMontage = LowRightSourceAttack
		? LowRightSourceAttack->AttackMontage.Get() : nullptr;
	UAnimSequenceBase* LowCenterSequence =
		LoadObjectAtPath<UAnimSequenceBase>(LowCenterSequencePath);
	UAnimSequenceBase* MiddleCenterSequence =
		LoadObjectAtPath<UAnimSequenceBase>(MiddleCenterSequencePath);
	UAnimSequenceBase* LowLeftSequence =
		LoadObjectAtPath<UAnimSequenceBase>(LowLeftSequencePath);
	UAnimMontage* ExpectedLowMatrixMontage = LowRightSourceMontage
		? DuplicateObject<UAnimMontage>(LowRightSourceMontage, GetTransientPackage())
		: nullptr;
	TArray<FString> LowMatrixErrors;
	if (!ConfigureLowMatrixMontage(ExpectedLowMatrixMontage,
		LowRightSourceMontage, LowCenterSequence, LowMatrixErrors))
	{
		for (const FString& Error : LowMatrixErrors)
		{
			OutErrors.Add(TEXT("recipe facts: ") + Error);
		}
		Facts += TEXT("low_matrix_canonical=null\n");
	}
	else
	{
		Facts += TEXT("low_matrix_canonical_begin\n");
		Facts += BuildLowMatrixMontageFacts(ExpectedLowMatrixMontage);
		Facts += TEXT("low_matrix_canonical_end\n");
	}
	Facts += FString::Printf(
		TEXT("contact_matrix=%s|middle_left=%s|middle_center=%s|low_left=%s|strip_parry=1|strip_counter=1|isolated_sections=1\n"),
		*ContactMatrixMontagePackage, *MiddleLeftSequencePath,
		*MiddleCenterSequencePath, *LowLeftSequencePath);
	UAnimMontage* ExpectedContactMatrixMontage =
		NewObject<UAnimMontage>(GetTransientPackage());
	TArray<FString> ContactMatrixErrors;
	if (!ConfigureContactMatrixMontage(
		ExpectedContactMatrixMontage, ContactMatrixErrors))
	{
		for (const FString& Error : ContactMatrixErrors)
		{
			OutErrors.Add(TEXT("recipe facts: ") + Error);
		}
		Facts += TEXT("contact_matrix_canonical=null\n");
	}
	else
	{
		Facts += TEXT("contact_matrix_canonical_begin\n");
		Facts += BuildLowMatrixMontageFacts(ExpectedContactMatrixMontage);
		Facts += TEXT("contact_matrix_canonical_end\n");
	}
	for (const FAttackVariantRecipe& Recipe : BuildAttackVariantRecipes())
	{
		Facts += FString::Printf(
			TEXT("attack=%s|source=%s|family=%s|height=%d|lane=%d|swing=%d|target=%s|tags=Attack.Type.Light%s|combo=0|hold=0|paired=0|low_matrix=%d|low_center=%d|contact_matrix=%d|sequence=%s|active=%.3f|duration=%.3f\n"),
			*Recipe.DestinationPackage, *Recipe.SourceAttackPath,
			*MatrixFamilyName(Recipe.Height),
			static_cast<int32>(Recipe.Height), static_cast<int32>(Recipe.Lane),
			static_cast<int32>(Recipe.Swing), *Recipe.TargetBone.ToString(),
			Recipe.bBlockInterruptible
				? TEXT(",Attack.Defense.BlockInterruptible") : TEXT(""),
			Recipe.bUseLowMatrixMontage, Recipe.bUseLowCenterSection,
			Recipe.bUseContactMatrixMontage, *Recipe.SequenceOverridePath,
			Recipe.ActiveStartOffset, Recipe.ActiveDuration);
	}
	Facts += TEXT("config=GateAClone|clear_contact_overrides=1|bone_height=spine_01:Middle|add=RecoilGeneric:Recoil\n");
	Facts += TEXT("settings=DefaultClone|defense=GateB\n");
	Facts += TEXT("map=non_partitioned|player=1|enemies=3|max_attackers=2|hands_off=NormalBlockMiddleCenter\n");
	Facts += FString::Printf(TEXT("player=%s|0,0,96|yaw=0|tag=%s\n"),
		*PlayerBlueprintPackage, *PlayerFixtureTag.ToString());
	for (const auto& Entry : TArray<TTuple<FName, FVector, FName>>{
		{LeftEnemyActorName, LeftEnemyLocation, LeftAnchorTag},
		{CenterEnemyActorName, CenterEnemyLocation, CenterAnchorTag},
		{RightEnemyActorName, RightEnemyLocation, RightAnchorTag}})
	{
		Facts += FString::Printf(TEXT("enemy=%s|%.3f,%.3f,%.3f|tag=%s|range=350\n"),
			*Entry.Get<0>().ToString(), Entry.Get<1>().X, Entry.Get<1>().Y,
			Entry.Get<1>().Z, *Entry.Get<2>().ToString());
	}
	for (const FCaseRecipe& Case : BuildCaseRecipes())
	{
		const FVector DefenderLocation = Case.DefenderTransform.GetLocation();
		const FRotator DefenderRotation = Case.DefenderTransform.Rotator();
		const FVector CaseLocation = Case.AttackerTransform.GetLocation();
		const FRotator CaseRotation = Case.AttackerTransform.Rotator();
		Facts += FString::Printf(
			TEXT("case=%s|%s|%s|defender=%.3f,%.3f,%.3f|defender_yaw=%.3f|attacker=%.3f,%.3f,%.3f|attacker_yaw=%.3f|guard=%d\n"),
			*Case.Name.ToString(), *Case.AttackPath, *Case.AnchorTag.ToString(),
			DefenderLocation.X, DefenderLocation.Y, DefenderLocation.Z,
			DefenderRotation.Yaw,
			CaseLocation.X, CaseLocation.Y, CaseLocation.Z,
			CaseRotation.Yaw, Case.bBeginHeldGuard);
	}
	for (const FManifestCaseContract& Contract : BuildManifestCaseContracts())
	{
		Facts += FString::Printf(
			TEXT("case_contract=%s|attack=%s|path=%s|outcome=%s|reason=%s|")
			TEXT("response=%s|presentation=%s|has_presentation=%d\n"),
			*Contract.Name, *Contract.AttackName, *Contract.AttackPath,
			*Contract.Outcome, *Contract.Reason, *Contract.AttackerResponse,
			*Contract.Presentation, Contract.bHasPresentation);
	}
	return Facts;
}

FDefenseMatrixAuthoringPlan BuildPlan(const bool bRejectDirtyPackages = true)
{
	FDefenseMatrixAuthoringPlan Plan;
	Plan.RecipeVersion = RecipeVersion;
	FDefenseProofManifest Manifest;
	TArray<FString> ManifestErrors;
	if (!FDefenseAssetValidationService::LoadManifestFile(
		FKatanaAssetMigrationRunner::ResolveProjectRelativeFilePath(ManifestPath),
		Manifest,
		ManifestErrors))
	{
		Plan.Errors.Append(ManifestErrors);
	}
	else
	{
		ValidateManifestCatalogInternal(Manifest, Plan.Errors);
	}
	UDefenseConfiguration* SourceConfiguration =
		LoadObjectAtPath<UDefenseConfiguration>(SourceConfigurationPath);
	UCombatSettings* SourceSettings =
		LoadObjectAtPath<UCombatSettings>(SourceCombatSettingsPath);
	UBlueprint* SourcePlayer = LoadObjectAtPath<UBlueprint>(SourcePlayerBlueprintPath);
	UBlueprint* SourceEnemy = LoadObjectAtPath<UBlueprint>(SourceEnemyBlueprintPath);
	UAnimSequenceBase* RecoilSequence =
		LoadObjectAtPath<UAnimSequenceBase>(RecoilSequencePath);
	UAnimSequenceBase* LowCenterSequence =
		LoadObjectAtPath<UAnimSequenceBase>(LowCenterSequencePath);
	UAnimSequenceBase* MiddleLeftSequence =
		LoadObjectAtPath<UAnimSequenceBase>(MiddleLeftSequencePath);
	UAnimSequenceBase* MiddleCenterSequence =
		LoadObjectAtPath<UAnimSequenceBase>(MiddleCenterSequencePath);
	UAnimSequenceBase* LowLeftSequence =
		LoadObjectAtPath<UAnimSequenceBase>(LowLeftSequencePath);
	UAttackData* LowRightSourceAttack = LoadObjectAtPath<UAttackData>(LightAttack1Path);
	UAnimMontage* LowRightSourceMontage = LowRightSourceAttack
		? LowRightSourceAttack->AttackMontage.Get() : nullptr;
	if (!SourceConfiguration)
	{
		Plan.Errors.Add(TEXT("Gate A source defense configuration did not load"));
	}
	if (!SourceSettings)
	{
		Plan.Errors.Add(TEXT("default source combat settings did not load"));
	}
	if (!SourcePlayer || !SourcePlayer->GeneratedClass
		|| !SourcePlayer->GeneratedClass->IsChildOf(APlayerCharacter::StaticClass()))
	{
		Plan.Errors.Add(TEXT("source player Blueprint did not load as APlayerCharacter"));
	}
	if (!SourceEnemy || !SourceEnemy->GeneratedClass
		|| !SourceEnemy->GeneratedClass->IsChildOf(AEnemyCharacter::StaticClass()))
	{
		Plan.Errors.Add(TEXT("source enemy Blueprint did not load as AEnemyCharacter"));
	}
	if (!RecoilSequence || !RecoilSequence->GetSkeleton()
		|| RecoilSequence->GetPlayLength() < 1.0f)
	{
		Plan.Errors.Add(TEXT("reviewed recoil source cannot supply its one-second recipe"));
	}
	if (!LowCenterSequence || !LowCenterSequence->GetSkeleton()
		|| LowCenterSequence->GetPlayLength() < LowCenterRecoveryStart
		|| !LowRightSourceMontage || !LowRightSourceMontage->GetSkeleton()
		|| LowCenterSequence->GetSkeleton() != LowRightSourceMontage->GetSkeleton())
	{
		Plan.Errors.Add(TEXT("reviewed low-matrix animation sources are unavailable or incompatible"));
	}
	if (!MiddleLeftSequence || !MiddleLeftSequence->GetSkeleton()
		|| !MiddleCenterSequence || !MiddleCenterSequence->GetSkeleton()
		|| !LowLeftSequence || !LowLeftSequence->GetSkeleton()
		|| (LowRightSourceMontage
			&& (MiddleLeftSequence->GetSkeleton() != LowRightSourceMontage->GetSkeleton()
				|| MiddleCenterSequence->GetSkeleton() != LowRightSourceMontage->GetSkeleton()
				|| LowLeftSequence->GetSkeleton() != LowRightSourceMontage->GetSkeleton())))
	{
		Plan.Errors.Add(TEXT("reviewed contact-matrix sequence sources are unavailable or incompatible"));
	}
	for (const FAttackVariantRecipe& Recipe : BuildAttackVariantRecipes())
	{
		if (!LoadObjectAtPath<UAttackData>(Recipe.SourceAttackPath))
		{
			Plan.Errors.Add(FString::Printf(TEXT("attack variant source did not load: %s"),
				*Recipe.SourceAttackPath));
		}
	}

	UAnimMontage* RecoilMontage = Cast<UAnimMontage>(
		FindExistingAsset(BuildObjectPath(RecoilMontagePackage)));
	if (!FindExistingAsset(BuildObjectPath(RecoilMontagePackage)))
	{
		AddPlannedPackage(Plan,
			FString::Printf(TEXT("CreateRecoilMontage|%s"), *RecoilMontagePackage),
			RecoilMontagePackage, TEXT("AttackerResponseMontage"));
	}
	else if (!RecoilMontage)
	{
		Plan.Errors.Add(TEXT("existing Gate B recoil asset is not an animation montage"));
	}
	else
	{
		RecoilMontageMatches(RecoilMontage, &Plan.Errors);
	}

	UObject* ExistingLowMatrixObject =
		FindExistingAsset(BuildObjectPath(LowMatrixMontagePackage));
	UAnimMontage* LowMatrixMontage = Cast<UAnimMontage>(ExistingLowMatrixObject);
	if (!ExistingLowMatrixObject)
	{
		AddPlannedPackage(Plan,
			FString::Printf(TEXT("CreateLowMatrixMontage|%s"), *LowMatrixMontagePackage),
			LowMatrixMontagePackage, TEXT("AttackMontage"));
	}
	else if (!LowMatrixMontage)
	{
		Plan.Errors.Add(TEXT("existing Gate B low-matrix asset is not an animation montage"));
	}
	else
	{
		TArray<FString> Mismatches;
		if (!LowMatrixMontageMatches(
			LowMatrixMontage, LowRightSourceMontage, LowCenterSequence, &Mismatches))
		{
			AddPlannedPackage(Plan,
				FString::Printf(TEXT("UpdateLowMatrixMontage|%s|%s"),
					*LowMatrixMontagePackage, *FString::Join(Mismatches, TEXT("|"))),
				LowMatrixMontagePackage, TEXT("AttackMontage"), TEXT("Update"));
		}
	}

	UAnimMontage* ExpectedLowMatrixMontage = nullptr;
	if (LowRightSourceMontage && LowCenterSequence)
	{
		ExpectedLowMatrixMontage = DuplicateObject<UAnimMontage>(
			LowRightSourceMontage, GetTransientPackage());
		TArray<FString> ExpectedErrors;
		if (!ConfigureLowMatrixMontage(ExpectedLowMatrixMontage,
			LowRightSourceMontage, LowCenterSequence, ExpectedErrors))
		{
			Plan.Errors.Append(ExpectedErrors);
			ExpectedLowMatrixMontage = nullptr;
		}
	}

	UAnimMontage* ExpectedContactMatrixMontage =
		NewObject<UAnimMontage>(GetTransientPackage());
	TArray<FString> ContactMatrixErrors;
	if (!ConfigureContactMatrixMontage(
		ExpectedContactMatrixMontage, ContactMatrixErrors))
	{
		Plan.Errors.Append(ContactMatrixErrors);
		ExpectedContactMatrixMontage = nullptr;
	}
	UObject* ExistingContactMatrixObject =
		FindExistingAsset(BuildObjectPath(ContactMatrixMontagePackage));
	UAnimMontage* ContactMatrixMontage =
		Cast<UAnimMontage>(ExistingContactMatrixObject);
	if (!ExistingContactMatrixObject)
	{
		AddPlannedPackage(Plan,
			FString::Printf(TEXT("CreateContactMatrixMontage|%s"),
				*ContactMatrixMontagePackage),
			ContactMatrixMontagePackage, TEXT("AttackMontage"));
	}
	else if (!ContactMatrixMontage)
	{
		Plan.Errors.Add(TEXT("existing Gate B contact-matrix asset is not an animation montage"));
	}
	else
	{
		TArray<FString> Mismatches;
		if (!ContactMatrixMontageMatches(ContactMatrixMontage, &Mismatches))
		{
			AddPlannedPackage(Plan,
				FString::Printf(TEXT("UpdateContactMatrixMontage|%s|%s"),
					*ContactMatrixMontagePackage,
					*FString::Join(Mismatches, TEXT("|"))),
				ContactMatrixMontagePackage, TEXT("AttackMontage"), TEXT("Update"));
		}
	}
	for (const FAttackVariantRecipe& Recipe : BuildAttackVariantRecipes())
	{
		UAttackData* SourceAttack = LoadObjectAtPath<UAttackData>(Recipe.SourceAttackPath);
		UObject* ExistingObject = FindExistingAsset(BuildObjectPath(Recipe.DestinationPackage));
		UAttackData* ExistingAttack = Cast<UAttackData>(ExistingObject);
		if (!ExistingObject)
		{
			AddPlannedPackage(Plan,
				FString::Printf(TEXT("CreateAttackVariant|%s"), *Recipe.DestinationPackage),
				Recipe.DestinationPackage, TEXT("AttackDataVariant"));
			continue;
		}
		if (!ExistingAttack)
		{
			Plan.Errors.Add(FString::Printf(
				TEXT("existing Gate B attack variant has the wrong asset class: %s"),
				*Recipe.DestinationPackage));
			continue;
		}
		TArray<FString> ExpectedErrors;
		UAttackData* Expected = BuildExpectedAttackVariant(
			SourceAttack, Recipe, ExpectedLowMatrixMontage,
			ExpectedContactMatrixMontage, ExpectedErrors);
		if (Expected && Recipe.bUseLowMatrixMontage && LowMatrixMontage)
		{
			Expected->AttackMontage = LowMatrixMontage;
		}
		if (Expected && Recipe.bUseContactMatrixMontage && ContactMatrixMontage)
		{
			Expected->AttackMontage = ContactMatrixMontage;
		}
		Plan.Errors.Append(ExpectedErrors);
		if (!Expected || !PropertiesMatch(ExistingAttack, Expected))
		{
			AddPlannedPackage(Plan,
				FString::Printf(TEXT("UpdateAttackVariant|%s"), *Recipe.DestinationPackage),
				Recipe.DestinationPackage, TEXT("AttackDataVariant"), TEXT("Update"));
		}
	}

	UDefenseConfiguration* Configuration = Cast<UDefenseConfiguration>(
		FindExistingAsset(BuildObjectPath(DefenseConfigurationPackage)));
	if (!FindExistingAsset(BuildObjectPath(DefenseConfigurationPackage)))
	{
		AddPlannedPackage(Plan,
			FString::Printf(TEXT("CreateDefenseConfiguration|%s"),
				*DefenseConfigurationPackage),
			DefenseConfigurationPackage, TEXT("DefenseConfiguration"));
	}
	else
	{
		TArray<FString> ExpectedErrors;
		UDefenseConfiguration* Expected = BuildExpectedConfiguration(
			SourceConfiguration, RecoilMontage, ExpectedErrors);
		Plan.Errors.Append(ExpectedErrors);
		if (!Configuration || !Expected || !PropertiesMatch(Configuration, Expected))
		{
			AddPlannedPackage(Plan,
				FString::Printf(TEXT("UpdateDefenseConfiguration|%s"),
					*DefenseConfigurationPackage),
				DefenseConfigurationPackage, TEXT("DefenseConfiguration"), TEXT("Update"));
		}
	}

	UCombatSettings* Settings = Cast<UCombatSettings>(
		FindExistingAsset(BuildObjectPath(CombatSettingsPackage)));
	if (!FindExistingAsset(BuildObjectPath(CombatSettingsPackage)))
	{
		AddPlannedPackage(Plan,
			FString::Printf(TEXT("CreateCombatSettings|%s"), *CombatSettingsPackage),
			CombatSettingsPackage, TEXT("CombatSettings"));
	}
	else
	{
		UCombatSettings* Expected = BuildExpectedCombatSettings(SourceSettings, Configuration);
		if (!Settings || !Expected || !PropertiesMatch(Settings, Expected))
		{
			Plan.Errors.Add(TEXT("existing Gate B combat settings differ from the reviewed recipe"));
		}
	}

	UBlueprint* PlayerBlueprint = Cast<UBlueprint>(
		FindExistingAsset(BuildObjectPath(PlayerBlueprintPackage)));
	if (!FindExistingAsset(BuildObjectPath(PlayerBlueprintPackage)))
	{
		AddPlannedPackage(Plan,
			FString::Printf(TEXT("CreateDerivedPlayerBlueprint|%s"), *PlayerBlueprintPackage),
			PlayerBlueprintPackage, TEXT("PlayerBlueprint"));
	}
	else if (!BlueprintMatches(PlayerBlueprint, SourcePlayer, Settings, true))
	{
		Plan.Errors.Add(TEXT("existing Gate B player Blueprint differs from the reviewed recipe"));
	}

	UBlueprint* EnemyBlueprint = Cast<UBlueprint>(
		FindExistingAsset(BuildObjectPath(EnemyBlueprintPackage)));
	if (!FindExistingAsset(BuildObjectPath(EnemyBlueprintPackage)))
	{
		AddPlannedPackage(Plan,
			FString::Printf(TEXT("CreateDerivedEnemyBlueprint|%s"), *EnemyBlueprintPackage),
			EnemyBlueprintPackage, TEXT("EnemyBlueprint"));
	}
	else if (!BlueprintMatches(EnemyBlueprint, SourceEnemy, Settings, false))
	{
		Plan.Errors.Add(TEXT("existing Gate B enemy Blueprint differs from the reviewed recipe"));
	}

	UWorld* World = LoadWorldPackageFully(MapPackage);
	if (!FPackageName::DoesPackageExist(MapPackage) && !World)
	{
		AddPlannedPackage(Plan,
			FString::Printf(TEXT("CreateDefenseMatrixMap|%s"), *MapPackage),
			MapPackage, TEXT("ProofMap"));
	}
	else if (!World)
	{
		Plan.Errors.Add(TEXT("existing Gate B proof map package did not fully load as a world"));
	}
	else
	{
		TArray<FString> Mismatches;
		if (!MapMatches(World, PlayerBlueprint, EnemyBlueprint, &Mismatches))
		{
			AddPlannedPackage(Plan,
				FString::Printf(TEXT("UpdateDefenseMatrixMap|%s|%s"),
					*MapPackage, *FString::Join(Mismatches, TEXT("|"))),
				MapPackage, TEXT("ProofMap"), TEXT("Update"));
		}
	}

	Plan.ProposedChanges.Sort();
	Plan.PackageLedger.Sort([](const auto& Left, const auto& Right)
	{
		return Left.PackageName < Right.PackageName;
	});
	Plan.RecipeFactsHash = FKatanaAssetAuthoringApprovalService::HashText(
		BuildRecipeFacts(Plan.Errors));
	const TArray<FString> SourceIdentities = BuildSourceIdentities(Plan.Errors);
	FKatanaAssetAuthoringApprovalService::BuildPackageStateHash(
		SourceIdentities, TEXT("source"), true, bRejectDirtyPackages,
		Plan.SourceStateHash, Plan.SourceStateCount, Plan.Errors);
	FKatanaAssetAuthoringApprovalService::BuildPackageStateHash(
		FDefenseMatrixAuthoringOperation::GetDestinationPackageNames(),
		TEXT("destination"), false, bRejectDirtyPackages,
		Plan.DestinationStateHash, Plan.DestinationStateCount, Plan.Errors);
	FKatanaAssetAuthoringApprovalService::BuildManifestHash(
		ManifestPath, Plan.ManifestHash, Plan.Errors);
	Plan.Errors.Sort();
	Plan.Fingerprint = FKatanaAssetAuthoringApprovalService::ComputeFingerprint(
		GetIdentity(), Plan);
	return Plan;
}

bool PreflightApplyPlan(
	const FDefenseMatrixAuthoringPlan& Plan,
	TArray<FString>& OutErrors)
{
	TSet<FString> DestinationPackages(
		FDefenseMatrixAuthoringOperation::GetDestinationPackageNames());
	TSet<FString> PlannedPackages;
	for (const FKatanaAssetMigrationPackageLedgerEntry& Entry : Plan.PackageLedger)
	{
		if (!DestinationPackages.Contains(Entry.PackageName))
		{
			OutErrors.Add(TEXT("approved ledger contains a non-recipe package: ")
				+ Entry.PackageName);
			continue;
		}
		if (PlannedPackages.Contains(Entry.PackageName))
		{
			OutErrors.Add(TEXT("approved ledger contains a duplicate package: ")
				+ Entry.PackageName);
			continue;
		}
		PlannedPackages.Add(Entry.PackageName);
		UObject* Existing = FindExistingAsset(BuildObjectPath(Entry.PackageName));
		const bool bPackageExists =
			FPackageName::DoesPackageExist(Entry.PackageName);
		if (Entry.PlannedAction == TEXT("Create"))
		{
			if (Existing || bPackageExists)
			{
				OutErrors.Add(TEXT("approved Create destination now exists: ")
					+ Entry.PackageName);
			}
		}
		else if (Entry.PlannedAction == TEXT("Update"))
		{
			if (!Existing || !bPackageExists)
			{
				OutErrors.Add(TEXT("approved Update destination no longer exists: ")
					+ Entry.PackageName);
			}
			const bool bUpdateSupported =
				Entry.PackageName == LowMatrixMontagePackage
				|| Entry.PackageName == ContactMatrixMontagePackage
				|| Entry.PackageName == DefenseConfigurationPackage
				|| Entry.PackageName == MapPackage
				|| BuildAttackVariantRecipes().ContainsByPredicate(
					[&Entry](const FAttackVariantRecipe& Recipe)
					{
						return Recipe.DestinationPackage == Entry.PackageName;
					});
			if (!bUpdateSupported)
			{
				OutErrors.Add(TEXT("approved Update is unsupported for recipe package: ")
					+ Entry.PackageName);
			}
		}
		else
		{
			OutErrors.Add(TEXT("approved ledger action is neither Create nor Update: ")
				+ Entry.PackageName);
		}
	}
	if (Plan.ProposedChanges.Num() != Plan.PackageLedger.Num())
	{
		OutErrors.Add(TEXT(
			"approved proposed-change count differs from the package ledger"));
	}

	UDefenseConfiguration* SourceConfiguration =
		LoadObjectAtPath<UDefenseConfiguration>(SourceConfigurationPath);
	UCombatSettings* SourceSettings =
		LoadObjectAtPath<UCombatSettings>(SourceCombatSettingsPath);
	UBlueprint* SourcePlayer = LoadObjectAtPath<UBlueprint>(SourcePlayerBlueprintPath);
	UBlueprint* SourceEnemy = LoadObjectAtPath<UBlueprint>(SourceEnemyBlueprintPath);
	UAnimSequenceBase* RecoilSequence =
		LoadObjectAtPath<UAnimSequenceBase>(RecoilSequencePath);
	UAnimSequenceBase* LowCenterSequence =
		LoadObjectAtPath<UAnimSequenceBase>(LowCenterSequencePath);
	UAttackData* LowRightSourceAttack =
		LoadObjectAtPath<UAttackData>(LightAttack1Path);
	UAnimMontage* LowRightSourceMontage = LowRightSourceAttack
		? LowRightSourceAttack->AttackMontage.Get() : nullptr;
	if (!SourceConfiguration || !SourceSettings
		|| !SourcePlayer || !SourcePlayer->GeneratedClass
		|| !SourceEnemy || !SourceEnemy->GeneratedClass
		|| !RecoilSequence || !LowCenterSequence || !LowRightSourceMontage)
	{
		OutErrors.Add(TEXT("Gate B source dependencies changed before Apply"));
		return false;
	}

	UAnimMontage* ExpectedRecoil =
		NewObject<UAnimMontage>(GetTransientPackage());
	ConfigureRecoilMontage(ExpectedRecoil, RecoilSequence, OutErrors);
	UAnimMontage* ExpectedLow =
		DuplicateObject<UAnimMontage>(LowRightSourceMontage, GetTransientPackage());
	ConfigureLowMatrixMontage(
		ExpectedLow, LowRightSourceMontage, LowCenterSequence, OutErrors);
	UAnimMontage* ExpectedContact =
		NewObject<UAnimMontage>(GetTransientPackage());
	ConfigureContactMatrixMontage(ExpectedContact, OutErrors);

	TMap<FString, UAttackData*> ExpectedAttacks;
	for (const FAttackVariantRecipe& Recipe : BuildAttackVariantRecipes())
	{
		UAttackData* SourceAttack =
			LoadObjectAtPath<UAttackData>(Recipe.SourceAttackPath);
		UAttackData* Expected = BuildExpectedAttackVariant(
			SourceAttack, Recipe, ExpectedLow, ExpectedContact, OutErrors);
		if (Expected)
		{
			ExpectedAttacks.Add(
				BuildObjectPath(Recipe.DestinationPackage), Expected);
		}
	}
	UDefenseConfiguration* ExpectedConfiguration =
		BuildExpectedConfiguration(
			SourceConfiguration, ExpectedRecoil, OutErrors);
	UCombatSettings* ExpectedSettings =
		BuildExpectedCombatSettings(SourceSettings, ExpectedConfiguration);
	if (!ExpectedConfiguration || !ExpectedSettings)
	{
		OutErrors.Add(TEXT(
			"Gate B configuration/settings dry construction failed"));
	}
	if (!SourcePlayer->GeneratedClass->IsChildOf(APlayerCharacter::StaticClass())
		|| !SourceEnemy->GeneratedClass->IsChildOf(AEnemyCharacter::StaticClass()))
	{
		OutErrors.Add(TEXT("Gate B fixture Blueprint source classes changed"));
	}
	if (!LoadObjectAtPath<UStaticMesh>(FloorMeshPath))
	{
		OutErrors.Add(TEXT("Gate B proof floor dependency did not load"));
	}
	for (const FString& AttackPath : BuildMapAttackPaths())
	{
		if (!ExpectedAttacks.Contains(AttackPath)
			&& !LoadObjectAtPath<UAttackData>(AttackPath))
		{
			OutErrors.Add(TEXT("Gate B map attack dependency did not load: ")
				+ AttackPath);
		}
	}

	const FKatanaAssetMigrationPackageLedgerEntry* MapEntry =
		Plan.PackageLedger.FindByPredicate([](const auto& Entry)
		{
			return Entry.PackageName == MapPackage;
		});
	if (MapEntry && MapEntry->PlannedAction == TEXT("Update"))
	{
		if (Plan.PackageLedger.ContainsByPredicate([](const auto& Entry)
			{
				return (Entry.PackageName == PlayerBlueprintPackage
					|| Entry.PackageName == EnemyBlueprintPackage)
					&& Entry.PlannedAction == TEXT("Create");
			}))
		{
			OutErrors.Add(TEXT(
				"an existing proof map cannot be updated while fixture Blueprints are being created"));
		}
		UBlueprint* PlayerBlueprint = LoadObjectAtPackage<UBlueprint>(
			PlayerBlueprintPackage);
		UBlueprint* EnemyBlueprint = LoadObjectAtPackage<UBlueprint>(
			EnemyBlueprintPackage);
		UWorld* World = LoadWorldPackageFully(MapPackage);
		const APlayerCharacter* Player =
			FindNamedActor<APlayerCharacter>(World, PlayerActorName);
		const AEnemyCharacter* Left =
			FindNamedActor<AEnemyCharacter>(World, LeftEnemyActorName);
		const AEnemyCharacter* Center =
			FindNamedActor<AEnemyCharacter>(World, CenterEnemyActorName);
		const AEnemyCharacter* Right =
			FindNamedActor<AEnemyCharacter>(World, RightEnemyActorName);
		const ADefenseMatrixProofDirector* Director =
			FindNamedActor<ADefenseMatrixProofDirector>(World, DirectorActorName);
		if (!PlayerBlueprint || !PlayerBlueprint->GeneratedClass
			|| !EnemyBlueprint || !EnemyBlueprint->GeneratedClass
			|| !World || !Player || !Left || !Center || !Right || !Director
			|| Player->GetClass() != PlayerBlueprint->GeneratedClass
			|| Left->GetClass() != EnemyBlueprint->GeneratedClass
			|| Center->GetClass() != EnemyBlueprint->GeneratedClass
			|| Right->GetClass() != EnemyBlueprint->GeneratedClass)
		{
			OutErrors.Add(TEXT(
				"existing proof map cannot satisfy the non-destructive update preconditions"));
		}
	}
	return OutErrors.IsEmpty();
}

bool RollbackFailedApply(
	const FDefenseMatrixAuthoringPlan& Plan,
	TArray<FString>& OutErrors)
{
	TArray<UPackage*> PackagesToReload;
	TArray<UPackage*> PackagesToUnload;
	for (const FKatanaAssetMigrationPackageLedgerEntry& Entry : Plan.PackageLedger)
	{
		UPackage* Package = FindPackage(nullptr, *Entry.PackageName);
		if (!Package)
		{
			continue;
		}
		TArray<UObject*> Objects;
		GetObjectsWithPackage(Package, Objects, true);
		for (UObject* Object : Objects)
		{
			if (UBlueprint* Blueprint = Cast<UBlueprint>(Object))
			{
				Blueprint->ClearEditorReferences();
			}
		}
		if (FPackageName::DoesPackageExist(Entry.PackageName))
		{
			PackagesToReload.AddUnique(Package);
		}
		else
		{
			for (UObject* Object : Objects)
			{
				if (Object && Object->HasAnyFlags(RF_Standalone))
				{
					FAssetRegistryModule::AssetDeleted(Object);
				}
			}
			Package->SetDirtyFlag(false);
			PackagesToUnload.AddUnique(Package);
		}
	}

	bool bSucceeded = true;
	if (!PackagesToReload.IsEmpty())
	{
		FText ReloadError;
		if (!UPackageTools::ReloadPackages(
			PackagesToReload,
			ReloadError,
			EReloadPackagesInteractionMode::AssumePositive))
		{
			bSucceeded = false;
			OutErrors.Add(FString::Printf(
				TEXT("failed to reload Gate B packages after Apply failure: %s"),
				*ReloadError.ToString()));
		}
	}
	if (!PackagesToUnload.IsEmpty())
	{
		FText UnloadError;
		if (!UPackageTools::UnloadPackages(
			PackagesToUnload, UnloadError, true))
		{
			bSucceeded = false;
			OutErrors.Add(FString::Printf(
				TEXT("failed to unload new Gate B packages after Apply failure: %s"),
				*UnloadError.ToString()));
		}
	}
	return bSucceeded;
}

bool ApplyPlan(
	const FDefenseMatrixAuthoringPlan& Plan,
	TSet<FString>& OutChangedPackages,
	TArray<FString>& OutErrors)
{
	const auto FindPlannedEntry = [&Plan](const FString& PackageName)
		-> const FKatanaAssetMigrationPackageLedgerEntry*
	{
		return Plan.PackageLedger.FindByPredicate(
			[&PackageName](const FKatanaAssetMigrationPackageLedgerEntry& Entry)
			{
				return Entry.PackageName == PackageName;
			});
	};
	UDefenseConfiguration* SourceConfiguration =
		LoadObjectAtPath<UDefenseConfiguration>(SourceConfigurationPath);
	UCombatSettings* SourceSettings =
		LoadObjectAtPath<UCombatSettings>(SourceCombatSettingsPath);
	UBlueprint* SourcePlayer = LoadObjectAtPath<UBlueprint>(SourcePlayerBlueprintPath);
	UBlueprint* SourceEnemy = LoadObjectAtPath<UBlueprint>(SourceEnemyBlueprintPath);
	UAnimSequenceBase* LowCenterSequence =
		LoadObjectAtPath<UAnimSequenceBase>(LowCenterSequencePath);
	UAttackData* LowRightSourceAttack = LoadObjectAtPath<UAttackData>(LightAttack1Path);
	UAnimMontage* LowRightSourceMontage = LowRightSourceAttack
		? LowRightSourceAttack->AttackMontage.Get() : nullptr;
	const FString LowRightSourceFactsBefore =
		BuildLowMatrixMontageFacts(LowRightSourceMontage);
	const FString ContactMatrixSourceFactsBefore =
		BuildContactMatrixSourceFacts();

	UAnimMontage* RecoilMontage = Cast<UAnimMontage>(
		FindExistingAsset(BuildObjectPath(RecoilMontagePackage)));
	if (!RecoilMontage)
	{
		RecoilMontage = CreateRecoilMontage(OutErrors);
		if (RecoilMontage)
		{
			OutChangedPackages.Add(RecoilMontagePackage);
		}
	}

	UAnimMontage* LowMatrixMontage = Cast<UAnimMontage>(
		FindExistingAsset(BuildObjectPath(LowMatrixMontagePackage)));
	if (OutErrors.IsEmpty())
	{
		if (const FKatanaAssetMigrationPackageLedgerEntry* Entry =
			FindPlannedEntry(LowMatrixMontagePackage))
		{
			if (Entry->PlannedAction == TEXT("Create") && !LowMatrixMontage)
			{
				LowMatrixMontage = CreateLowMatrixMontage(
					LowRightSourceMontage, LowCenterSequence, OutErrors);
			}
			else if (Entry->PlannedAction == TEXT("Update") && LowMatrixMontage)
			{
				LowMatrixMontage->Modify();
				if (ConfigureLowMatrixMontage(
					LowMatrixMontage, LowRightSourceMontage,
					LowCenterSequence, OutErrors))
				{
					LowMatrixMontage->MarkPackageDirty();
				}
			}
			else
			{
				OutErrors.Add(FString::Printf(
					TEXT("approved low-matrix action cannot be applied: %s|%s"),
					*Entry->PlannedAction, *LowMatrixMontagePackage));
			}
			if (OutErrors.IsEmpty())
			{
				OutChangedPackages.Add(LowMatrixMontagePackage);
			}
		}
	}
	if (OutErrors.IsEmpty())
	{
		const FString SourceFactsAfterMontage =
			BuildLowMatrixMontageFacts(LowRightSourceMontage);
		if (SourceFactsAfterMontage != LowRightSourceFactsBefore)
		{
			OutErrors.Add(TEXT("low-right source montage changed during montage authoring: ")
				+ DescribeFirstFactMismatch(
					SourceFactsAfterMontage, LowRightSourceFactsBefore));
		}
	}

	UAnimMontage* ContactMatrixMontage = Cast<UAnimMontage>(
		FindExistingAsset(BuildObjectPath(ContactMatrixMontagePackage)));
	if (OutErrors.IsEmpty())
	{
		if (const FKatanaAssetMigrationPackageLedgerEntry* Entry =
			FindPlannedEntry(ContactMatrixMontagePackage))
		{
			if (Entry->PlannedAction == TEXT("Create") && !ContactMatrixMontage)
			{
				ContactMatrixMontage = CreateContactMatrixMontage(OutErrors);
			}
			else if (Entry->PlannedAction == TEXT("Update") && ContactMatrixMontage)
			{
				ContactMatrixMontage->Modify();
				if (ConfigureContactMatrixMontage(ContactMatrixMontage, OutErrors))
				{
					ContactMatrixMontage->MarkPackageDirty();
				}
			}
			else
			{
				OutErrors.Add(FString::Printf(
					TEXT("approved contact-matrix action cannot be applied: %s|%s"),
					*Entry->PlannedAction, *ContactMatrixMontagePackage));
			}
			if (OutErrors.IsEmpty())
			{
				OutChangedPackages.Add(ContactMatrixMontagePackage);
			}
		}
	}
	if (OutErrors.IsEmpty())
	{
		const FString SourceFactsAfterMontage = BuildContactMatrixSourceFacts();
		if (SourceFactsAfterMontage != ContactMatrixSourceFactsBefore)
		{
			OutErrors.Add(TEXT("contact-matrix source montage changed during montage authoring: ")
				+ DescribeFirstFactMismatch(
					SourceFactsAfterMontage, ContactMatrixSourceFactsBefore));
		}
	}

	for (const FAttackVariantRecipe& Recipe : BuildAttackVariantRecipes())
	{
		if (!OutErrors.IsEmpty())
		{
			break;
		}
		const FKatanaAssetMigrationPackageLedgerEntry* Entry =
			FindPlannedEntry(Recipe.DestinationPackage);
		if (!Entry)
		{
			continue;
		}
		UAttackData* SourceAttack =
			LoadObjectAtPath<UAttackData>(Recipe.SourceAttackPath);
		UAttackData* Attack = Cast<UAttackData>(
			FindExistingAsset(BuildObjectPath(Recipe.DestinationPackage)));
		if (Entry->PlannedAction == TEXT("Create") && !Attack)
		{
			Attack = CreateAttackVariant(
				SourceAttack, Recipe, LowMatrixMontage,
				ContactMatrixMontage, OutErrors);
		}
		else if (Entry->PlannedAction == TEXT("Update") && Attack)
		{
			Attack->Modify();
			if (ConfigureAttackVariant(
				Attack, SourceAttack, Recipe, LowMatrixMontage,
				ContactMatrixMontage, OutErrors))
			{
				Attack->MarkPackageDirty();
			}
		}
		else
		{
			OutErrors.Add(FString::Printf(
				TEXT("approved attack-variant action cannot be applied: %s|%s"),
				*Entry->PlannedAction, *Recipe.DestinationPackage));
		}
		if (OutErrors.IsEmpty())
		{
			OutChangedPackages.Add(Recipe.DestinationPackage);
		}
	}
	if (OutErrors.IsEmpty())
	{
		const FString SourceFactsAfterVariants =
			BuildLowMatrixMontageFacts(LowRightSourceMontage);
		if (SourceFactsAfterVariants != LowRightSourceFactsBefore)
		{
			OutErrors.Add(TEXT("low-right source montage changed during attack variant authoring: ")
				+ DescribeFirstFactMismatch(
					SourceFactsAfterVariants, LowRightSourceFactsBefore));
		}
		const FString ContactSourceFactsAfterVariants =
			BuildContactMatrixSourceFacts();
		if (ContactSourceFactsAfterVariants != ContactMatrixSourceFactsBefore)
		{
			OutErrors.Add(TEXT("contact-matrix source montage changed during attack variant authoring: ")
				+ DescribeFirstFactMismatch(
					ContactSourceFactsAfterVariants, ContactMatrixSourceFactsBefore));
		}
	}

	UDefenseConfiguration* Configuration = Cast<UDefenseConfiguration>(
		FindExistingAsset(BuildObjectPath(DefenseConfigurationPackage)));
	if (OutErrors.IsEmpty())
	{
		if (const FKatanaAssetMigrationPackageLedgerEntry* Entry =
			FindPlannedEntry(DefenseConfigurationPackage))
		{
			if (Entry->PlannedAction == TEXT("Create") && !Configuration)
			{
				Configuration = CreateDefenseConfiguration(
					SourceConfiguration, RecoilMontage, OutErrors);
			}
			else if (Entry->PlannedAction == TEXT("Update") && Configuration)
			{
				Configuration->Modify();
				if (ConfigureGateBDefenseConfiguration(
					Configuration, RecoilMontage, OutErrors))
				{
					Configuration->MarkPackageDirty();
				}
			}
			else
			{
				OutErrors.Add(FString::Printf(
					TEXT("approved defense-configuration action cannot be applied: %s|%s"),
					*Entry->PlannedAction, *DefenseConfigurationPackage));
			}
			if (Configuration && OutErrors.IsEmpty())
			{
				OutChangedPackages.Add(DefenseConfigurationPackage);
			}
		}
	}
	UCombatSettings* Settings = Cast<UCombatSettings>(
		FindExistingAsset(BuildObjectPath(CombatSettingsPackage)));
	if (OutErrors.IsEmpty() && !Settings)
	{
		Settings = CreateCombatSettings(SourceSettings, Configuration, OutErrors);
		if (Settings)
		{
			OutChangedPackages.Add(CombatSettingsPackage);
		}
	}
	UBlueprint* PlayerBlueprint = Cast<UBlueprint>(
		FindExistingAsset(BuildObjectPath(PlayerBlueprintPackage)));
	if (OutErrors.IsEmpty() && !PlayerBlueprint)
	{
		PlayerBlueprint = CreateFixtureBlueprint(
			SourcePlayer, PlayerBlueprintPackage, Settings, OutErrors);
		if (PlayerBlueprint)
		{
			OutChangedPackages.Add(PlayerBlueprintPackage);
		}
	}
	UBlueprint* EnemyBlueprint = Cast<UBlueprint>(
		FindExistingAsset(BuildObjectPath(EnemyBlueprintPackage)));
	if (OutErrors.IsEmpty() && !EnemyBlueprint)
	{
		EnemyBlueprint = CreateFixtureBlueprint(
			SourceEnemy, EnemyBlueprintPackage, Settings, OutErrors);
		if (EnemyBlueprint)
		{
			OutChangedPackages.Add(EnemyBlueprintPackage);
		}
	}
	if (OutErrors.IsEmpty())
	{
		UWorld* World = LoadWorldPackageFully(MapPackage);
		if (const FKatanaAssetMigrationPackageLedgerEntry* Entry =
			FindPlannedEntry(MapPackage))
		{
			bool bChanged = false;
			if (Entry->PlannedAction == TEXT("Create") && !World)
			{
				bChanged = CreateProofMap(
					PlayerBlueprint, EnemyBlueprint, OutErrors) != nullptr;
			}
			else if (Entry->PlannedAction == TEXT("Update") && World)
			{
				bChanged = UpdateProofMap(
					World, PlayerBlueprint, EnemyBlueprint, OutErrors);
			}
			else
			{
				OutErrors.Add(FString::Printf(
					TEXT("approved proof-map action cannot be applied: %s|%s"),
					*Entry->PlannedAction, *MapPackage));
			}
			if (bChanged && OutErrors.IsEmpty())
			{
				OutChangedPackages.Add(MapPackage);
			}
		}
	}

	TSet<FString> PlannedPackages;
	for (const FKatanaAssetMigrationPackageLedgerEntry& Entry : Plan.PackageLedger)
	{
		PlannedPackages.Add(Entry.PackageName);
	}
	if (!OutChangedPackages.Difference(PlannedPackages).IsEmpty()
		|| !PlannedPackages.Difference(OutChangedPackages).IsEmpty())
	{
		OutErrors.Add(TEXT("actual authored packages differ from the approved package ledger"));
	}
	return OutErrors.IsEmpty();
}
}

TArray<FString> FDefenseMatrixAuthoringOperation::GetDestinationPackageNames()
{
	TArray<FString> Packages = {
		RecoilMontagePackage,
		LowMatrixMontagePackage,
		ContactMatrixMontagePackage,
		DefenseConfigurationPackage,
		CombatSettingsPackage,
		EnemyBlueprintPackage,
		PlayerBlueprintPackage,
		MapPackage};
	for (const FAttackVariantRecipe& Recipe : BuildAttackVariantRecipes())
	{
		Packages.Add(Recipe.DestinationPackage);
	}
	Packages.Sort();
	return Packages;
}

bool FDefenseMatrixAuthoringOperation::ValidateManifestCatalog(
	const FDefenseProofManifest& Manifest,
	TArray<FString>& OutErrors)
{
	return ValidateManifestCatalogInternal(Manifest, OutErrors);
}

FString FDefenseMatrixAuthoringOperation::ComputeApprovalFingerprint(
	const FDefenseMatrixAuthoringApprovalContract& Contract)
{
	return FKatanaAssetAuthoringApprovalService::ComputeFingerprint(
		GetIdentity(), Contract);
}

bool FDefenseMatrixAuthoringOperation::BuildCurrentApprovalContract(
	FDefenseMatrixAuthoringApprovalContract& OutContract,
	TArray<FString>& OutErrors)
{
	const FDefenseMatrixAuthoringPlan Plan = BuildPlan();
	OutContract = Plan;
	OutErrors.Append(Plan.Errors);
	return Plan.Errors.IsEmpty();
}

bool FDefenseMatrixAuthoringOperation::ValidateApprovedPlanJson(
	const FString& Json,
	const FString& ApprovedPlanFingerprint,
	const FDefenseMatrixAuthoringApprovalContract& CurrentContract,
	TArray<FString>& OutErrors)
{
	return FKatanaAssetAuthoringApprovalService::ValidateApprovedPlanJson(
		GetIdentity(), Json, ApprovedPlanFingerprint, CurrentContract, OutErrors);
}

bool FDefenseMatrixAuthoringOperation::Run(
	const FKatanaAssetMigrationOptions& Options,
	FKatanaAssetMigrationReport& OutReport) const
{
	FDefenseMatrixAuthoringPlan Plan = BuildPlan();
	if (!Plan.Errors.IsEmpty() || !IsApplyMode(Options.Mode))
	{
		FKatanaAssetAuthoringApprovalService::PopulateReport(
			GetIdentity(), Plan, Options.Mode, Plan.Errors,
			GetDestinationPackageNames().Num(), nullptr, OutReport);
		return Plan.Errors.IsEmpty();
	}

	TArray<FString> Errors;
	if (!FKatanaAssetAuthoringApprovalService::ValidateApprovedPlanBinding(
		GetIdentity(), Options, Plan, Errors))
	{
		Plan.Errors.Append(Errors);
		FKatanaAssetAuthoringApprovalService::PopulateReport(
			GetIdentity(), Plan, Options.Mode, Plan.Errors,
			GetDestinationPackageNames().Num(), nullptr, OutReport);
		return false;
	}
	if (!Options.bAllowDirtyPackages
		&& Plan.PackageLedger.ContainsByPredicate([](const auto& Entry)
		{
			return Entry.bInitiallyDirty;
		}))
	{
		Plan.Errors.Add(TEXT("approved authoring plan contains an initially dirty package"));
		FKatanaAssetAuthoringApprovalService::PopulateReport(
			GetIdentity(), Plan, Options.Mode, Plan.Errors,
			GetDestinationPackageNames().Num(), nullptr, OutReport);
		return false;
	}
	if (!PreflightApplyPlan(Plan, Errors))
	{
		Plan.Errors.Append(Errors);
		FKatanaAssetAuthoringApprovalService::PopulateReport(
			GetIdentity(), Plan, Options.Mode, Plan.Errors,
			GetDestinationPackageNames().Num(), nullptr, OutReport);
		return false;
	}

	TSet<FString> ChangedPackages;
	if (!ApplyPlan(Plan, ChangedPackages, Errors))
	{
		const bool bRolledBack = RollbackFailedApply(Plan, Errors);
		if (bRolledBack)
		{
			ChangedPackages.Reset();
			Errors.Add(TEXT(
				"Gate B in-memory authoring changes were rolled back after Apply failure"));
		}
		Plan.Errors.Append(Errors);
		FKatanaAssetAuthoringApprovalService::PopulateReport(
			GetIdentity(), Plan, Options.Mode, Plan.Errors,
			GetDestinationPackageNames().Num(), &ChangedPackages, OutReport);
		return false;
	}
	const FDefenseMatrixAuthoringPlan PostApplyPlan = BuildPlan(false);
	if (!PostApplyPlan.Errors.IsEmpty() || !PostApplyPlan.ProposedChanges.IsEmpty())
	{
		Plan.Errors.Add(TEXT("post-apply Gate B authoring validation was not idempotent"));
		for (const FString& ProposedChange : PostApplyPlan.ProposedChanges)
		{
			Plan.Errors.Add(FString::Printf(
				TEXT("post-apply residual change: %s"), *ProposedChange));
		}
		Plan.Errors.Append(PostApplyPlan.Errors);
		TArray<FString> RollbackErrors;
		const bool bRolledBack = RollbackFailedApply(Plan, RollbackErrors);
		Plan.Errors.Append(RollbackErrors);
		if (bRolledBack)
		{
			ChangedPackages.Reset();
			Plan.Errors.Add(TEXT(
				"Gate B in-memory authoring changes were rolled back after post-apply validation failure"));
		}
		FKatanaAssetAuthoringApprovalService::PopulateReport(
			GetIdentity(), Plan, Options.Mode, Plan.Errors,
			GetDestinationPackageNames().Num(), &ChangedPackages, OutReport);
		return false;
	}

	FKatanaAssetAuthoringApprovalService::PopulateReport(
		GetIdentity(), Plan, Options.Mode, Plan.Errors,
		GetDestinationPackageNames().Num(), &ChangedPackages, OutReport);
	for (FKatanaAssetMigrationPackageLedgerEntry& Entry : OutReport.PackageLedger)
	{
		if (ChangedPackages.Contains(Entry.PackageName))
		{
			Entry.ActualAction = Entry.PlannedAction == TEXT("Create")
				? TEXT("Created") : TEXT("Updated");
		}
		else
		{
			Entry.ActualAction = TEXT("Missing");
		}
	}
	return true;
}
