// Copyright Epic Games, Inc. All Rights Reserved.

#include "Commandlets/Operations/EnemyAIProofAssetsOperation.h"

#include "AI/EnemyCombatAIComponent.h"
#include "AI/EnemyCombatAIController.h"
#include "AI/EnemyCombatStateTreeTasks.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Characters/EnemyCharacter.h"
#include "Characters/PlayerCharacter.h"
#include "Commandlets/KatanaAssetMigrationRunner.h"
#include "Components/StateTreeAIComponent.h"
#include "Components/StateTreeAIComponentSchema.h"
#include "Data/AttackData.h"
#include "EnhancedActionKeyMapping.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "FileHelpers.h"
#include "InputAction.h"
#include "InputCoreTypes.h"
#include "InputMappingContext.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/PackageName.h"
#include "PropertyBindingPath.h"
#include "StateTree.h"
#include "StateTreeCompilerLog.h"
#include "StateTreeEditingSubsystem.h"
#include "StateTreeEditorData.h"
#include "StateTreeState.h"
#include "StateTreeTypes.h"
#include "UObject/Package.h"

const FString FEnemyAIProofAssetsOperation::OperationName = TEXT("EnemyAIProofAssets");

namespace
{
	const FString StateTreePackageName = TEXT("/Game/ProjectFiles/AI/ST_EnemyCombatProof");
	const FString ControllerBlueprintPackageName = TEXT("/Game/ProjectFiles/AI/BP_EnemyCombatAIController");
	const FString EnemyBlueprintPackageName = TEXT("/Game/ProjectFiles/Core/Actors/Character/BP_EnemyCharacter");
	const FString PlayerBlueprintPackageName = TEXT("/Game/ProjectFiles/Core/Actors/Character/BP_Player");
	const FString LevelPackageName = TEXT("/Game/ProjectFiles/Levels/Lvl_ThirdPerson1");
	const FString DefaultAttackPackageName = TEXT("/Game/ProjectFiles/Data/PDA/Attack/AttackData/Light/New/LightAttack_1");
	const FString BlockActionPackageName = TEXT("/Game/ProjectFiles/Input/Actions/IA_Block");
	const FString FallbackPlayerMappingContextPackageName = TEXT("/Game/ProjectFiles/Input/IMC_Combat");

	const FGuid ActorContextGuid(0x1D971B00, 0x28884FDE, 0xB5436802, 0x36984FD5);
	const FGuid AIControllerContextGuid(0xEDB3CD97, 0x95F94E0A, 0xBD15207B, 0x98645CDC);

	FString BuildObjectPath(const FString& PackageName)
	{
		const FString AssetName = FPackageName::GetLongPackageAssetName(PackageName);
		return FString::Printf(TEXT("%s.%s"), *PackageName, *AssetName);
	}

	FString BuildGeneratedClassPath(const FString& PackageName)
	{
		const FString AssetName = FPackageName::GetLongPackageAssetName(PackageName);
		return FString::Printf(TEXT("%s.%s_C"), *PackageName, *AssetName);
	}

	template <typename T>
	T* LoadObjectAtPackage(const FString& PackageName)
	{
		return Cast<T>(StaticLoadObject(T::StaticClass(), nullptr, *BuildObjectPath(PackageName)));
	}

	void AddChangedPackage(UObject* Object, FKatanaAssetMigrationRow& OutRow)
	{
		if (Object && Object->GetOutermost())
		{
			OutRow.ChangedPackages.AddUnique(Object->GetOutermost()->GetName());
		}
	}

	void AddDirtyPackage(UObject* Object, TSet<FString>& OutDirtyPackages)
	{
		if (Object && Object->GetOutermost() && Object->GetOutermost()->IsDirty())
		{
			OutDirtyPackages.Add(Object->GetOutermost()->GetName());
		}
	}

	bool IsApplyMode(EKatanaAssetMigrationMode Mode)
	{
		return Mode == EKatanaAssetMigrationMode::Apply || Mode == EKatanaAssetMigrationMode::ApplyAndSave;
	}

	UPackage* CreateOrLoadPackageForAsset(const FString& PackageName, FString& OutError)
	{
		if (!FPackageName::IsValidLongPackageName(PackageName))
		{
			OutError = FString::Printf(TEXT("Invalid package path: %s"), *PackageName);
			return nullptr;
		}

		UPackage* Package = CreatePackage(*PackageName);
		if (!Package)
		{
			OutError = FString::Printf(TEXT("Failed to create package: %s"), *PackageName);
			return nullptr;
		}

		Package->FullyLoad();
		return Package;
	}

	UInputAction* CreateInputActionAsset(
		const FString& PackageName,
		const EInputActionValueType ValueType,
		FKatanaAssetMigrationRow& OutRow)
	{
		UObject* ExistingObject = StaticLoadObject(UObject::StaticClass(), nullptr, *BuildObjectPath(PackageName));
		if (ExistingObject && !ExistingObject->IsA<UInputAction>())
		{
			OutRow.Errors.Add(FString::Printf(
				TEXT("Existing asset at %s is not a UInputAction"),
				*BuildObjectPath(PackageName)));
			return nullptr;
		}

		FString Error;
		UPackage* Package = CreateOrLoadPackageForAsset(PackageName, Error);
		if (!Package)
		{
			OutRow.Errors.Add(Error);
			return nullptr;
		}

		const FString AssetName = FPackageName::GetLongPackageAssetName(PackageName);
		UInputAction* Action = Cast<UInputAction>(ExistingObject);
		if (!Action)
		{
			Action = NewObject<UInputAction>(
				Package,
				FName(*AssetName),
				RF_Public | RF_Standalone | RF_Transactional);
			FAssetRegistryModule::AssetCreated(Action);
		}

		Action->Modify();
		Action->ValueType = ValueType;
		Action->MarkPackageDirty();
		OutRow.ChangedPackages.AddUnique(PackageName);
		return Action;
	}

	UInputMappingContext* CreateInputMappingContextAsset(const FString& PackageName, FKatanaAssetMigrationRow& OutRow)
	{
		UObject* ExistingObject = StaticLoadObject(UObject::StaticClass(), nullptr, *BuildObjectPath(PackageName));
		if (ExistingObject && !ExistingObject->IsA<UInputMappingContext>())
		{
			OutRow.Errors.Add(FString::Printf(
				TEXT("Existing asset at %s is not a UInputMappingContext"),
				*BuildObjectPath(PackageName)));
			return nullptr;
		}

		FString Error;
		UPackage* Package = CreateOrLoadPackageForAsset(PackageName, Error);
		if (!Package)
		{
			OutRow.Errors.Add(Error);
			return nullptr;
		}

		const FString AssetName = FPackageName::GetLongPackageAssetName(PackageName);
		UInputMappingContext* MappingContext = Cast<UInputMappingContext>(ExistingObject);
		if (!MappingContext)
		{
			MappingContext = NewObject<UInputMappingContext>(
				Package,
				FName(*AssetName),
				RF_Public | RF_Standalone | RF_Transactional);
			FAssetRegistryModule::AssetCreated(MappingContext);
		}

		MappingContext->Modify();
		MappingContext->MarkPackageDirty();
		OutRow.ChangedPackages.AddUnique(PackageName);
		return MappingContext;
	}

	bool HasUsableAttack(const UEnemyCombatAIComponent* CombatAI)
	{
		if (!CombatAI)
		{
			return false;
		}

		for (const FEnemyAttackConfig& AttackConfig : CombatAI->AvailableAttacks)
		{
			if (AttackConfig.AttackData)
			{
				return true;
			}
		}

		return false;
	}

	bool HasInputMapping(const UInputMappingContext* MappingContext, const UInputAction* Action, const FKey& Key)
	{
		if (!MappingContext || !Action)
		{
			return false;
		}

		for (const FEnhancedActionKeyMapping& Mapping : MappingContext->GetMappings())
		{
			if (Mapping.Action == Action && Mapping.Key == Key)
			{
				return true;
			}
		}

		return false;
	}

	bool EnsureInputMapping(
		UInputMappingContext* MappingContext,
		const UInputAction* Action,
		const FKey& Key,
		const bool bApply,
		FKatanaAssetMigrationRow& OutRow)
	{
		if (!MappingContext || !Action)
		{
			return false;
		}

		if (HasInputMapping(MappingContext, Action, Key))
		{
			return false;
		}

		OutRow.PlannedAdditions.Add(FString::Printf(
			TEXT("Map %s to %s in %s"),
			*Action->GetName(),
			*Key.ToString(),
			*MappingContext->GetPathName()));
		AddChangedPackage(MappingContext, OutRow);

		if (!bApply)
		{
			return true;
		}

		MappingContext->Modify();
		MappingContext->MapKey(Action, Key);
		MappingContext->MarkPackageDirty();
		return true;
	}

	bool EnsureInputMappingRemoved(
		UInputMappingContext* MappingContext,
		const UInputAction* Action,
		const FKey& Key,
		const bool bApply,
		FKatanaAssetMigrationRow& OutRow)
	{
		if (!MappingContext || !Action)
		{
			return false;
		}

		if (!HasInputMapping(MappingContext, Action, Key))
		{
			return false;
		}

		OutRow.PlannedRemovals.Add(FString::Printf(
			TEXT("Remove %s from %s in %s"),
			*Action->GetName(),
			*Key.ToString(),
			*MappingContext->GetPathName()));
		AddChangedPackage(MappingContext, OutRow);

		if (!bApply)
		{
			return true;
		}

		MappingContext->Modify();
		MappingContext->UnmapKey(Action, Key);
		MappingContext->MarkPackageDirty();
		return true;
	}

	bool SeedDefaultAttackIfMissing(
		UEnemyCombatAIComponent* CombatAI,
		UAttackData* DefaultAttack,
		const FString& OwnerLabel,
		const bool bApply,
		FKatanaAssetMigrationRow& OutRow)
	{
		if (!CombatAI)
		{
			OutRow.Errors.Add(FString::Printf(TEXT("%s does not have UEnemyCombatAIComponent"), *OwnerLabel));
			return false;
		}

		if (HasUsableAttack(CombatAI))
		{
			return false;
		}

		OutRow.PlannedAdditions.Add(FString::Printf(
			TEXT("Seed %s AvailableAttacks with %s and Single selection mode"),
			*OwnerLabel,
			*BuildObjectPath(DefaultAttackPackageName)));

		if (!bApply)
		{
			AddChangedPackage(CombatAI->GetOwner(), OutRow);
			return true;
		}

		if (!DefaultAttack)
		{
			OutRow.Errors.Add(FString::Printf(TEXT("Default attack did not load: %s"), *BuildObjectPath(DefaultAttackPackageName)));
			return false;
		}

		CombatAI->Modify();

		FEnemyAttackConfig AttackConfig;
		AttackConfig.AttackData = DefaultAttack;
		AttackConfig.SelectionWeight = 1.0f;
		AttackConfig.MinRange = 0.0f;
		AttackConfig.MaxRange = 250.0f;

		CombatAI->AvailableAttacks.Reset();
		CombatAI->AvailableAttacks.Add(AttackConfig);
		CombatAI->AttackSelectionMode = EEnemyAttackSelection::Single;
		CombatAI->ApproachConfig.AttackRange = FMath::Max(CombatAI->ApproachConfig.AttackRange, 250.0f);
		CombatAI->MarkPackageDirty();
		AddChangedPackage(CombatAI->GetOwner(), OutRow);
		return true;
	}

	bool IsStartLogicAutomatic(const UStateTreeAIComponent* Component)
	{
		const FBoolProperty* Property = FindFProperty<FBoolProperty>(UStateTreeComponent::StaticClass(), TEXT("bStartLogicAutomatically"));
		return !Property || Property->GetPropertyValue_InContainer(Component);
	}

	void BindActorContext(UStateTreeEditorData& EditorData, const FStateTreeEditorNode& Node, const FName TargetProperty)
	{
		EditorData.AddPropertyBinding(FPropertyBindingPath(ActorContextGuid), FPropertyBindingPath(Node.ID, TargetProperty));
	}

	void BindAIControllerContext(UStateTreeEditorData& EditorData, const FStateTreeEditorNode& Node, const FName TargetProperty)
	{
		EditorData.AddPropertyBinding(FPropertyBindingPath(AIControllerContextGuid), FPropertyBindingPath(Node.ID, TargetProperty));
	}

	bool DoesStateTreeMatchProof(const UStateTree* StateTree)
	{
		if (!StateTree || !StateTree->IsReadyToRun())
		{
			return false;
		}

#if WITH_EDITORONLY_DATA
		const UStateTreeEditorData* EditorData = Cast<UStateTreeEditorData>(StateTree->EditorData);
		if (!EditorData || !Cast<UStateTreeAIComponentSchema>(EditorData->Schema))
		{
			return false;
		}

		if (EditorData->SubTrees.Num() != 1 || !EditorData->SubTrees[0])
		{
			return false;
		}

		const UStateTreeState* Root = EditorData->SubTrees[0];
		const TArray<FName> ExpectedNames = {
			TEXT("AcquireTarget"),
			TEXT("RequestToken"),
			TEXT("Approach"),
			TEXT("ExecuteAttack"),
			TEXT("Recover")
		};

		if (Root->Children.Num() != ExpectedNames.Num())
		{
			return false;
		}

		for (int32 Index = 0; Index < ExpectedNames.Num(); ++Index)
		{
			if (!Root->Children[Index] || Root->Children[Index]->Name != ExpectedNames[Index])
			{
				return false;
			}
		}
#endif

		return true;
	}

	bool BuildProofStateTree(UStateTree* StateTree, FKatanaAssetMigrationRow& OutRow)
	{
		check(StateTree);

		StateTree->Modify();
		StateTree->ResetCompiled();

		UStateTreeEditorData* EditorData = NewObject<UStateTreeEditorData>(StateTree, UStateTreeEditorData::StaticClass(), NAME_None, RF_Transactional);
		EditorData->Schema = NewObject<UStateTreeAIComponentSchema>(EditorData, UStateTreeAIComponentSchema::StaticClass(), NAME_None, RF_Transactional);
		StateTree->EditorData = EditorData;

		UStateTreeState& Root = EditorData->AddRootState();
		Root.SelectionBehavior = EStateTreeStateSelectionBehavior::TrySelectChildrenInOrder;
		Root.TasksCompletion = EStateTreeTaskCompletionType::Any;

		UStateTreeState& AcquireTarget = Root.AddChildState(TEXT("AcquireTarget"));
		auto& AcquireTask = AcquireTarget.AddTask<FStateTreeSetEnemyCombatTargetTask>();
		BindActorContext(*EditorData, AcquireTask, GET_MEMBER_NAME_CHECKED(FStateTreeSetEnemyCombatTargetInstanceData, EnemyActor));
		AcquireTask.GetInstanceData().bUsePlayerPawnIfTargetUnset = true;

		UStateTreeState& RequestToken = Root.AddChildState(TEXT("RequestToken"));
		auto& RequestTask = RequestToken.AddTask<FStateTreeRequestEnemyAttackTokenTask>();
		BindActorContext(*EditorData, RequestTask, GET_MEMBER_NAME_CHECKED(FStateTreeRequestEnemyAttackTokenInstanceData, EnemyActor));
		RequestTask.GetInstanceData().MaxQueueWaitTime = 3.0f;

		UStateTreeState& Approach = Root.AddChildState(TEXT("Approach"));
		auto& ApproachTask = Approach.AddTask<FStateTreeApproachEnemyCombatTargetTask>();
		BindActorContext(*EditorData, ApproachTask, GET_MEMBER_NAME_CHECKED(FStateTreeEnemyCombatMoveInstanceData, EnemyActor));
		BindAIControllerContext(*EditorData, ApproachTask, GET_MEMBER_NAME_CHECKED(FStateTreeEnemyCombatMoveInstanceData, Controller));
		ApproachTask.GetInstanceData().UpdateInterval = 0.1f;

		UStateTreeState& ExecuteAttack = Root.AddChildState(TEXT("ExecuteAttack"));
		auto& ExecuteTask = ExecuteAttack.AddTask<FStateTreeExecuteEnemyAttackTask>();
		BindActorContext(*EditorData, ExecuteTask, GET_MEMBER_NAME_CHECKED(FStateTreeExecuteEnemyAttackInstanceData, EnemyActor));

		UStateTreeState& Recover = Root.AddChildState(TEXT("Recover"));
		auto& RecoverTask = Recover.AddTask<FStateTreeCircleEnemyCombatTargetTask>();
		BindActorContext(*EditorData, RecoverTask, GET_MEMBER_NAME_CHECKED(FStateTreeEnemyCombatMoveInstanceData, EnemyActor));
		BindAIControllerContext(*EditorData, RecoverTask, GET_MEMBER_NAME_CHECKED(FStateTreeEnemyCombatMoveInstanceData, Controller));
		RecoverTask.GetInstanceData().UpdateInterval = 0.25f;
		RecoverTask.GetInstanceData().CirclingAcceptanceRadius = 100.0f;

		AcquireTarget.AddTransition(EStateTreeTransitionTrigger::OnStateSucceeded, EStateTreeTransitionType::GotoState, &RequestToken);
		AcquireTarget.AddTransition(EStateTreeTransitionTrigger::OnStateFailed, EStateTreeTransitionType::GotoState, &Recover);
		RequestToken.AddTransition(EStateTreeTransitionTrigger::OnStateSucceeded, EStateTreeTransitionType::GotoState, &Approach);
		RequestToken.AddTransition(EStateTreeTransitionTrigger::OnStateFailed, EStateTreeTransitionType::GotoState, &Recover);
		Approach.AddTransition(EStateTreeTransitionTrigger::OnStateSucceeded, EStateTreeTransitionType::GotoState, &ExecuteAttack);
		Approach.AddTransition(EStateTreeTransitionTrigger::OnStateFailed, EStateTreeTransitionType::GotoState, &Recover);
		ExecuteAttack.AddTransition(EStateTreeTransitionTrigger::OnStateCompleted, EStateTreeTransitionType::GotoState, &Recover);

		FStateTreeTransition& RecoverTransition = Recover.AddTransition(EStateTreeTransitionTrigger::OnTick, EStateTreeTransitionType::GotoState, &AcquireTarget);
		RecoverTransition.bDelayTransition = true;
		RecoverTransition.DelayDuration = 0.6f;

		FStateTreeCompilerLog CompilerLog;
		if (!UStateTreeEditingSubsystem::CompileStateTree(StateTree, CompilerLog))
		{
			CompilerLog.DumpToLog(LogTemp);
			OutRow.Errors.Add(TEXT("Failed to compile generated enemy combat StateTree"));
			return false;
		}

		if (!StateTree->IsReadyToRun())
		{
			OutRow.Errors.Add(TEXT("Generated enemy combat StateTree compiled but is not ready to run"));
			return false;
		}

		StateTree->MarkPackageDirty();
		return true;
	}

	UStateTree* EnsureStateTree(EKatanaAssetMigrationMode Mode, FKatanaAssetMigrationRow& OutRow)
	{
		UStateTree* StateTree = LoadObjectAtPackage<UStateTree>(StateTreePackageName);
		const bool bNeedsStateTree = !DoesStateTreeMatchProof(StateTree);
		if (!bNeedsStateTree)
		{
			return StateTree;
		}

		OutRow.PlannedAdditions.Add(FString::Printf(TEXT("Create or update StateTree %s"), *BuildObjectPath(StateTreePackageName)));
		OutRow.ChangedPackages.AddUnique(StateTreePackageName);

		if (Mode == EKatanaAssetMigrationMode::Audit || Mode == EKatanaAssetMigrationMode::Plan)
		{
			return StateTree;
		}

		FString Error;
		UPackage* Package = CreateOrLoadPackageForAsset(StateTreePackageName, Error);
		if (!Package)
		{
			OutRow.Errors.Add(Error);
			return nullptr;
		}

		const FString AssetName = FPackageName::GetLongPackageAssetName(StateTreePackageName);
		if (!StateTree)
		{
			StateTree = NewObject<UStateTree>(Package, *AssetName, RF_Public | RF_Standalone | RF_Transactional);
			FAssetRegistryModule::AssetCreated(StateTree);
		}

		return BuildProofStateTree(StateTree, OutRow) ? StateTree : nullptr;
	}

	UBlueprint* EnsureControllerBlueprint(UStateTree* StateTree, EKatanaAssetMigrationMode Mode, FKatanaAssetMigrationRow& OutRow)
	{
		UBlueprint* Blueprint = LoadObjectAtPackage<UBlueprint>(ControllerBlueprintPackageName);
		const bool bMissingBlueprint = Blueprint == nullptr;
		if (Blueprint && Blueprint->GeneratedClass && !Blueprint->GeneratedClass->IsChildOf(AEnemyCombatAIController::StaticClass()))
		{
			OutRow.Errors.Add(FString::Printf(
				TEXT("Existing controller Blueprint has the wrong parent class: %s"),
				*BuildObjectPath(ControllerBlueprintPackageName)));
			return nullptr;
		}

		AEnemyCombatAIController* ControllerCDO = Blueprint && Blueprint->GeneratedClass
			? Cast<AEnemyCombatAIController>(Blueprint->GeneratedClass->GetDefaultObject())
			: nullptr;
		UEnemyStateTreeAIComponent* StateTreeComponent = ControllerCDO
			? Cast<UEnemyStateTreeAIComponent>(ControllerCDO->GetStateTreeAIComponent())
			: nullptr;

		const bool bNeedsStateTreeAssignment = !StateTreeComponent || StateTreeComponent->GetAssignedStateTree() != StateTree || !IsStartLogicAutomatic(StateTreeComponent);
		if (!bMissingBlueprint && !bNeedsStateTreeAssignment)
		{
			return Blueprint;
		}

		if (bMissingBlueprint)
		{
			OutRow.PlannedAdditions.Add(FString::Printf(
				TEXT("Create controller Blueprint %s from AEnemyCombatAIController"),
				*BuildObjectPath(ControllerBlueprintPackageName)));
		}
		if (bNeedsStateTreeAssignment)
		{
			OutRow.PlannedAdditions.Add(FString::Printf(
				TEXT("Assign StateTree %s to BP_EnemyCombatAIController StateTreeAIComponent"),
				*BuildObjectPath(StateTreePackageName)));
		}
		OutRow.ChangedPackages.AddUnique(ControllerBlueprintPackageName);

		if (Mode == EKatanaAssetMigrationMode::Audit || Mode == EKatanaAssetMigrationMode::Plan)
		{
			return Blueprint;
		}

		if (!StateTree)
		{
			OutRow.Errors.Add(TEXT("Cannot assign controller Blueprint without a generated StateTree"));
			return nullptr;
		}

		if (!Blueprint)
		{
			FString Error;
			UPackage* Package = CreateOrLoadPackageForAsset(ControllerBlueprintPackageName, Error);
			if (!Package)
			{
				OutRow.Errors.Add(Error);
				return nullptr;
			}

			const FString AssetName = FPackageName::GetLongPackageAssetName(ControllerBlueprintPackageName);
			Blueprint = FKismetEditorUtilities::CreateBlueprint(
				AEnemyCombatAIController::StaticClass(),
				Package,
				FName(*AssetName),
				BPTYPE_Normal,
				UBlueprint::StaticClass(),
				UBlueprintGeneratedClass::StaticClass());
			if (!Blueprint)
			{
				OutRow.Errors.Add(FString::Printf(TEXT("Failed to create controller Blueprint: %s"), *BuildObjectPath(ControllerBlueprintPackageName)));
				return nullptr;
			}
		}

		FKismetEditorUtilities::CompileBlueprint(Blueprint);
		ControllerCDO = Blueprint->GeneratedClass
			? Cast<AEnemyCombatAIController>(Blueprint->GeneratedClass->GetDefaultObject())
			: nullptr;
		StateTreeComponent = ControllerCDO
			? Cast<UEnemyStateTreeAIComponent>(ControllerCDO->GetStateTreeAIComponent())
			: nullptr;

		if (!ControllerCDO || !StateTreeComponent)
		{
			OutRow.Errors.Add(TEXT("Controller Blueprint did not produce an AEnemyCombatAIController CDO with UEnemyStateTreeAIComponent"));
			return nullptr;
		}

		Blueprint->Modify();
		ControllerCDO->Modify();
		StateTreeComponent->Modify();
		StateTreeComponent->SetStateTree(StateTree);
		StateTreeComponent->SetStartLogicAutomatically(true);
		FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
		Blueprint->MarkPackageDirty();
		return Blueprint;
	}

	UClass* ResolveControllerClass(UBlueprint* ControllerBlueprint)
	{
		if (ControllerBlueprint && ControllerBlueprint->GeneratedClass)
		{
			return ControllerBlueprint->GeneratedClass;
		}

		return Cast<UClass>(StaticLoadObject(UClass::StaticClass(), nullptr, *BuildGeneratedClassPath(ControllerBlueprintPackageName)));
	}

	bool EnsureEnemyBlueprintDefaults(
		UClass* ControllerClass,
		UAttackData* DefaultAttack,
		EKatanaAssetMigrationMode Mode,
		FKatanaAssetMigrationRow& OutRow)
	{
		UBlueprint* EnemyBlueprint = LoadObjectAtPackage<UBlueprint>(EnemyBlueprintPackageName);
		if (!EnemyBlueprint)
		{
			OutRow.Errors.Add(FString::Printf(TEXT("Enemy Blueprint did not load: %s"), *BuildObjectPath(EnemyBlueprintPackageName)));
			return false;
		}

		if (!EnemyBlueprint->GeneratedClass)
		{
			FKismetEditorUtilities::CompileBlueprint(EnemyBlueprint);
		}

		if (!EnemyBlueprint->GeneratedClass || !EnemyBlueprint->GeneratedClass->IsChildOf(AEnemyCharacter::StaticClass()))
		{
			OutRow.Errors.Add(FString::Printf(TEXT("Enemy Blueprint is not based on AEnemyCharacter: %s"), *BuildObjectPath(EnemyBlueprintPackageName)));
			return false;
		}

		AEnemyCharacter* EnemyCDO = Cast<AEnemyCharacter>(EnemyBlueprint->GeneratedClass->GetDefaultObject());
		UEnemyCombatAIComponent* CombatAI = EnemyCDO ? EnemyCDO->FindComponentByClass<UEnemyCombatAIComponent>() : nullptr;
		const bool bNeedsController = ControllerClass && EnemyCDO && EnemyCDO->AIControllerClass != ControllerClass;
		const bool bNeedsAutoPossess = EnemyCDO && EnemyCDO->AutoPossessAI != EAutoPossessAI::PlacedInWorldOrSpawned;
		const bool bNeedsAttack = !HasUsableAttack(CombatAI);
		const bool bNeedsChange = bNeedsController || bNeedsAutoPossess || bNeedsAttack;

		if (!bNeedsChange)
		{
			return true;
		}

		if (bNeedsController)
		{
			OutRow.PlannedAdditions.Add(TEXT("Set BP_EnemyCharacter default AIControllerClass to BP_EnemyCombatAIController"));
		}
		if (bNeedsAutoPossess)
		{
			OutRow.PlannedAdditions.Add(TEXT("Set BP_EnemyCharacter default AutoPossessAI to PlacedInWorldOrSpawned"));
		}
		if (bNeedsAttack)
		{
			OutRow.PlannedAdditions.Add(FString::Printf(TEXT("Seed BP_EnemyCharacter default attack with %s"), *BuildObjectPath(DefaultAttackPackageName)));
		}
		OutRow.ChangedPackages.AddUnique(EnemyBlueprintPackageName);

		if (Mode == EKatanaAssetMigrationMode::Audit || Mode == EKatanaAssetMigrationMode::Plan)
		{
			return true;
		}

		if (!EnemyCDO || !CombatAI)
		{
			OutRow.Errors.Add(TEXT("BP_EnemyCharacter CDO is missing AEnemyCharacter or UEnemyCombatAIComponent"));
			return false;
		}
		if (!ControllerClass)
		{
			OutRow.Errors.Add(TEXT("Cannot assign BP_EnemyCharacter AIControllerClass without controller Blueprint class"));
			return false;
		}

		EnemyBlueprint->Modify();
		EnemyCDO->Modify();
		if (bNeedsController)
		{
			EnemyCDO->AIControllerClass = ControllerClass;
		}
		if (bNeedsAutoPossess)
		{
			EnemyCDO->AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;
		}
		if (bNeedsAttack)
		{
			SeedDefaultAttackIfMissing(CombatAI, DefaultAttack, TEXT("BP_EnemyCharacter default"), true, OutRow);
		}

		FBlueprintEditorUtils::MarkBlueprintAsModified(EnemyBlueprint);
		EnemyBlueprint->MarkPackageDirty();
		return true;
	}

	bool EnsurePlayerBlockInput(EKatanaAssetMigrationMode Mode, FKatanaAssetMigrationRow& OutRow)
	{
		const bool bApply = IsApplyMode(Mode);
		UInputAction* BlockAction = LoadObjectAtPackage<UInputAction>(BlockActionPackageName);
		if (!BlockAction)
		{
			OutRow.PlannedAdditions.Add(FString::Printf(
				TEXT("Create Boolean input action %s"),
				*BuildObjectPath(BlockActionPackageName)));
			OutRow.ChangedPackages.AddUnique(BlockActionPackageName);
			if (bApply)
			{
				BlockAction = CreateInputActionAsset(BlockActionPackageName, EInputActionValueType::Boolean, OutRow);
				if (!BlockAction)
				{
					return false;
				}
			}
		}

		UBlueprint* PlayerBlueprint = LoadObjectAtPackage<UBlueprint>(PlayerBlueprintPackageName);
		if (!PlayerBlueprint)
		{
			OutRow.Errors.Add(FString::Printf(TEXT("Player Blueprint did not load: %s"), *BuildObjectPath(PlayerBlueprintPackageName)));
			return false;
		}

		if (!PlayerBlueprint->GeneratedClass)
		{
			FKismetEditorUtilities::CompileBlueprint(PlayerBlueprint);
		}

		if (!PlayerBlueprint->GeneratedClass || !PlayerBlueprint->GeneratedClass->IsChildOf(APlayerCharacter::StaticClass()))
		{
			OutRow.Errors.Add(FString::Printf(TEXT("Player Blueprint is not based on APlayerCharacter: %s"), *BuildObjectPath(PlayerBlueprintPackageName)));
			return false;
		}

		APlayerCharacter* PlayerCDO = Cast<APlayerCharacter>(PlayerBlueprint->GeneratedClass->GetDefaultObject());
		UInputMappingContext* MappingContext = PlayerCDO ? PlayerCDO->DefaultMappingContext.Get() : nullptr;
		if (!MappingContext)
		{
			MappingContext = LoadObjectAtPackage<UInputMappingContext>(FallbackPlayerMappingContextPackageName);
			if (!MappingContext)
			{
				OutRow.PlannedAdditions.Add(FString::Printf(
					TEXT("Create player input mapping context %s"),
					*BuildObjectPath(FallbackPlayerMappingContextPackageName)));
				OutRow.ChangedPackages.AddUnique(FallbackPlayerMappingContextPackageName);
				if (bApply)
				{
					MappingContext = CreateInputMappingContextAsset(FallbackPlayerMappingContextPackageName, OutRow);
					if (!MappingContext)
					{
						return false;
					}
				}
			}
		}

		if (!PlayerCDO)
		{
			OutRow.Errors.Add(TEXT("BP_Player CDO is missing"));
			return false;
		}

		if (bApply && (!BlockAction || !MappingContext))
		{
			OutRow.Errors.Add(TEXT("Apply mode could not resolve IA_Block or player input mapping context"));
			return false;
		}

		const bool bNeedsActionType = BlockAction && BlockAction->ValueType != EInputActionValueType::Boolean;
		const bool bNeedsBlockAction = !BlockAction || PlayerCDO->BlockAction != BlockAction;
		const bool bNeedsMappingContext = !MappingContext || PlayerCDO->DefaultMappingContext != MappingContext;
		const bool bNeedsMouseMapping = !BlockAction || !MappingContext || !HasInputMapping(MappingContext, BlockAction, EKeys::ThumbMouseButton);
		const bool bNeedsGamepadMapping = !BlockAction || !MappingContext || !HasInputMapping(MappingContext, BlockAction, EKeys::Gamepad_LeftShoulder);
		const bool bNeedsDeprecatedRightMouseRemoval = BlockAction && MappingContext && HasInputMapping(MappingContext, BlockAction, EKeys::RightMouseButton);
		const bool bNeedsChange = bNeedsActionType || bNeedsBlockAction || bNeedsMappingContext || bNeedsMouseMapping || bNeedsGamepadMapping || bNeedsDeprecatedRightMouseRemoval;

		if (!bNeedsChange)
		{
			return true;
		}

		if (bNeedsActionType)
		{
			OutRow.PlannedAdditions.Add(TEXT("Set IA_Block ValueType to Boolean"));
			OutRow.ChangedPackages.AddUnique(BlockActionPackageName);
		}
		if (bNeedsBlockAction)
		{
			OutRow.PlannedAdditions.Add(TEXT("Assign IA_Block to BP_Player BlockAction"));
			OutRow.ChangedPackages.AddUnique(PlayerBlueprintPackageName);
		}
		if (bNeedsMappingContext)
		{
			OutRow.PlannedAdditions.Add(FString::Printf(
				TEXT("Assign %s to BP_Player DefaultMappingContext"),
				MappingContext ? *MappingContext->GetPathName() : *BuildObjectPath(FallbackPlayerMappingContextPackageName)));
			OutRow.ChangedPackages.AddUnique(PlayerBlueprintPackageName);
		}

		if (!bApply)
		{
			const FString ActionLabel = BlockAction ? BlockAction->GetName() : BuildObjectPath(BlockActionPackageName);
			const FString MappingContextLabel = MappingContext ? MappingContext->GetPathName() : BuildObjectPath(FallbackPlayerMappingContextPackageName);
			if (bNeedsMouseMapping)
			{
				OutRow.PlannedAdditions.Add(FString::Printf(
					TEXT("Map %s to %s in %s"),
					*ActionLabel,
					*EKeys::ThumbMouseButton.ToString(),
					*MappingContextLabel));
				if (MappingContext)
				{
					AddChangedPackage(MappingContext, OutRow);
				}
				else
				{
					OutRow.ChangedPackages.AddUnique(FallbackPlayerMappingContextPackageName);
				}
			}
			if (bNeedsGamepadMapping)
			{
				OutRow.PlannedAdditions.Add(FString::Printf(
					TEXT("Map %s to %s in %s"),
					*ActionLabel,
					*EKeys::Gamepad_LeftShoulder.ToString(),
					*MappingContextLabel));
				if (MappingContext)
				{
					AddChangedPackage(MappingContext, OutRow);
				}
				else
				{
					OutRow.ChangedPackages.AddUnique(FallbackPlayerMappingContextPackageName);
				}
			}
			if (bNeedsDeprecatedRightMouseRemoval)
			{
				OutRow.PlannedRemovals.Add(FString::Printf(
					TEXT("Remove %s from %s in %s"),
					*ActionLabel,
					*EKeys::RightMouseButton.ToString(),
					*MappingContextLabel));
				if (MappingContext)
				{
					AddChangedPackage(MappingContext, OutRow);
				}
				else
				{
					OutRow.ChangedPackages.AddUnique(FallbackPlayerMappingContextPackageName);
				}
			}
			return true;
		}

		if (bNeedsActionType)
		{
			BlockAction->Modify();
			BlockAction->ValueType = EInputActionValueType::Boolean;
			BlockAction->MarkPackageDirty();
		}

		if (bNeedsBlockAction || bNeedsMappingContext)
		{
			PlayerBlueprint->Modify();
			PlayerCDO->Modify();
			if (bNeedsBlockAction)
			{
				PlayerCDO->BlockAction = BlockAction;
			}
			if (bNeedsMappingContext)
			{
				PlayerCDO->DefaultMappingContext = MappingContext;
			}
			FBlueprintEditorUtils::MarkBlueprintAsModified(PlayerBlueprint);
			PlayerBlueprint->MarkPackageDirty();
		}

		if (bNeedsMouseMapping)
		{
			EnsureInputMapping(MappingContext, BlockAction, EKeys::ThumbMouseButton, true, OutRow);
		}
		if (bNeedsGamepadMapping)
		{
			EnsureInputMapping(MappingContext, BlockAction, EKeys::Gamepad_LeftShoulder, true, OutRow);
		}
		if (bNeedsDeprecatedRightMouseRemoval)
		{
			EnsureInputMappingRemoved(MappingContext, BlockAction, EKeys::RightMouseButton, true, OutRow);
		}

		return true;
	}

	UWorld* LoadProofLevel(FKatanaAssetMigrationRow& OutRow)
	{
		FString MapFilename;
		if (!FPackageName::TryConvertLongPackageNameToFilename(LevelPackageName, MapFilename, FPackageName::GetMapPackageExtension()))
		{
			OutRow.Errors.Add(FString::Printf(TEXT("Failed to resolve level filename: %s"), *LevelPackageName));
			return nullptr;
		}

		UWorld* World = UEditorLoadingAndSavingUtils::LoadMap(MapFilename);
		if (!World)
		{
			OutRow.Errors.Add(FString::Printf(TEXT("Failed to load level: %s"), *LevelPackageName));
			return nullptr;
		}

		OutRow.bMapLoaded = true;
		return World;
	}

	bool EnsureLevelEnemyAssignments(
		UClass* ControllerClass,
		UAttackData* DefaultAttack,
		EKatanaAssetMigrationMode Mode,
		FKatanaAssetMigrationRow& OutRow)
	{
		UWorld* World = LoadProofLevel(OutRow);
		if (!World)
		{
			return false;
		}

		int32 EnemyCount = 0;
		int32 AttackReadyCount = 0;
		for (TActorIterator<AEnemyCharacter> It(World); It; ++It)
		{
			AEnemyCharacter* Enemy = *It;
			if (!Enemy || Enemy->IsTemplate())
			{
				continue;
			}

			++EnemyCount;
			UEnemyCombatAIComponent* CombatAI = Enemy->FindComponentByClass<UEnemyCombatAIComponent>();
			const bool bNeedsController = ControllerClass && Enemy->AIControllerClass != ControllerClass;
			const bool bNeedsAutoPossess = Enemy->AutoPossessAI != EAutoPossessAI::PlacedInWorldOrSpawned;
			const bool bNeedsAttack = !HasUsableAttack(CombatAI);

			if (HasUsableAttack(CombatAI))
			{
				++AttackReadyCount;
			}

			if (!bNeedsController && !bNeedsAutoPossess && !bNeedsAttack)
			{
				continue;
			}

			const FString EnemyLabel = FString::Printf(TEXT("level enemy %s"), *Enemy->GetPathName());
			if (bNeedsController)
			{
				OutRow.PlannedAdditions.Add(FString::Printf(TEXT("Assign %s AIControllerClass to BP_EnemyCombatAIController"), *EnemyLabel));
			}
			if (bNeedsAutoPossess)
			{
				OutRow.PlannedAdditions.Add(FString::Printf(TEXT("Assign %s AutoPossessAI to PlacedInWorldOrSpawned"), *EnemyLabel));
			}
			if (bNeedsAttack)
			{
				OutRow.PlannedAdditions.Add(FString::Printf(TEXT("Seed %s with a usable default attack"), *EnemyLabel));
			}
			AddChangedPackage(Enemy, OutRow);

			if (Mode == EKatanaAssetMigrationMode::Audit || Mode == EKatanaAssetMigrationMode::Plan)
			{
				continue;
			}

			Enemy->Modify();
			if (bNeedsController)
			{
				Enemy->AIControllerClass = ControllerClass;
			}
			if (bNeedsAutoPossess)
			{
				Enemy->AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;
			}
			if (bNeedsAttack)
			{
				if (SeedDefaultAttackIfMissing(CombatAI, DefaultAttack, EnemyLabel, true, OutRow))
				{
					++AttackReadyCount;
				}
			}
			Enemy->MarkPackageDirty();
		}

		if (EnemyCount == 0)
		{
			OutRow.Errors.Add(FString::Printf(TEXT("%s loaded but no AEnemyCharacter actors were found"), *LevelPackageName));
			return false;
		}

		OutRow.Warnings.Add(FString::Printf(TEXT("Level load proof: %s loaded with %d AEnemyCharacter actors, %d already had usable attacks before seeding"),
			*LevelPackageName,
			EnemyCount,
			AttackReadyCount));

		return true;
	}
}

void FEnemyAIProofAssetsOperation::SnapshotInitiallyDirtyPackages(TSet<FString>& OutDirtyPackages)
{
	AddDirtyPackage(LoadObjectAtPackage<UStateTree>(StateTreePackageName), OutDirtyPackages);
	AddDirtyPackage(LoadObjectAtPackage<UBlueprint>(ControllerBlueprintPackageName), OutDirtyPackages);
	AddDirtyPackage(LoadObjectAtPackage<UBlueprint>(EnemyBlueprintPackageName), OutDirtyPackages);
	AddDirtyPackage(LoadObjectAtPackage<UBlueprint>(PlayerBlueprintPackageName), OutDirtyPackages);
	AddDirtyPackage(LoadObjectAtPackage<UAttackData>(DefaultAttackPackageName), OutDirtyPackages);
	AddDirtyPackage(LoadObjectAtPackage<UInputAction>(BlockActionPackageName), OutDirtyPackages);
	AddDirtyPackage(LoadObjectAtPackage<UInputMappingContext>(FallbackPlayerMappingContextPackageName), OutDirtyPackages);

	FKatanaAssetMigrationRow IgnoredRow;
	if (UWorld* World = LoadProofLevel(IgnoredRow))
	{
		AddDirtyPackage(World, OutDirtyPackages);
		for (TActorIterator<AEnemyCharacter> It(World); It; ++It)
		{
			AddDirtyPackage(*It, OutDirtyPackages);
		}
	}
}

bool FEnemyAIProofAssetsOperation::Run(EKatanaAssetMigrationMode Mode, FKatanaAssetMigrationRow& OutRow) const
{
	OutRow = FKatanaAssetMigrationRow();
	OutRow.InputTarget = OperationName;
	OutRow.PackageName = LevelPackageName;
	OutRow.ObjectPath = BuildObjectPath(LevelPackageName);
	OutRow.AssetClass = TEXT("EnemyAIProofAssets");
	OutRow.bLoaded = true;

	const bool bSuccess = RunInternal(Mode, OutRow);
	if (!bSuccess || OutRow.Errors.Num() > 0)
	{
		OutRow.Status = EKatanaAssetMigrationStatus::Failed;
		return false;
	}

	if (OutRow.ChangedPackages.Num() == 0)
	{
		OutRow.Status = EKatanaAssetMigrationStatus::Unchanged;
	}
	else
	{
		OutRow.Status = (Mode == EKatanaAssetMigrationMode::Audit || Mode == EKatanaAssetMigrationMode::Plan)
			? EKatanaAssetMigrationStatus::WouldChange
			: EKatanaAssetMigrationStatus::Changed;
	}

	return true;
}

bool FEnemyAIProofAssetsOperation::RunInternal(EKatanaAssetMigrationMode Mode, FKatanaAssetMigrationRow& OutRow) const
{
	UAttackData* DefaultAttack = LoadObjectAtPackage<UAttackData>(DefaultAttackPackageName);
	if (!DefaultAttack)
	{
		OutRow.Errors.Add(FString::Printf(TEXT("Default attack did not load: %s"), *BuildObjectPath(DefaultAttackPackageName)));
		return false;
	}

	UStateTree* StateTree = EnsureStateTree(Mode, OutRow);
	if ((Mode == EKatanaAssetMigrationMode::Apply || Mode == EKatanaAssetMigrationMode::ApplyAndSave) && !StateTree)
	{
		return false;
	}

	UBlueprint* ControllerBlueprint = EnsureControllerBlueprint(StateTree, Mode, OutRow);
	if ((Mode == EKatanaAssetMigrationMode::Apply || Mode == EKatanaAssetMigrationMode::ApplyAndSave) && !ControllerBlueprint)
	{
		return false;
	}

	UClass* ControllerClass = ResolveControllerClass(ControllerBlueprint);
	if ((Mode == EKatanaAssetMigrationMode::Apply || Mode == EKatanaAssetMigrationMode::ApplyAndSave) && !ControllerClass)
	{
		OutRow.Errors.Add(FString::Printf(TEXT("Controller generated class did not load: %s"), *BuildGeneratedClassPath(ControllerBlueprintPackageName)));
		return false;
	}

	if (!EnsureEnemyBlueprintDefaults(ControllerClass, DefaultAttack, Mode, OutRow))
	{
		return false;
	}

	if (!EnsurePlayerBlockInput(Mode, OutRow))
	{
		return false;
	}

	if (!EnsureLevelEnemyAssignments(ControllerClass, DefaultAttack, Mode, OutRow))
	{
		return false;
	}

	return true;
}
