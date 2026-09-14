#include "CombatTestHelpers.h"
#include "Core/PairedAnimationComponent.h"
#include "Core/TargetingComponent.h"
#include "Data/PairedAnimationData.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimSequence.h"
#include "Components/BoxComponent.h"
#include "Analysis/PairedEntryTuning.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"

namespace
{
struct FPairedEntryFixture
{
	UWorld* World = FCombatTestHelpers::CreateTestWorld();
	APlayerCharacter* Owner = FCombatTestHelpers::CreateTestPlayerCharacter(World);
	AEnemyCharacter* Victim = FCombatTestHelpers::CreateTestEnemyCharacter(World,FVector(140,0,0));
	UPairedAnimationData* Pair = nullptr;
	UAttackData* Attack = nullptr;
	bool Initialize()
	{
		const auto ConfigureMesh=[](ABaseCombatCharacter* Character,const TCHAR* Path)
		{
			UClass* Class=LoadClass<ABaseCombatCharacter>(nullptr,Path);
			if(!Class) {return false;}
			const auto* Defaults=Class->GetDefaultObject<ABaseCombatCharacter>();
			Character->GetMesh()->SetSkeletalMesh(Defaults->GetMesh()->GetSkeletalMeshAsset());
			Character->GetMesh()->SetAnimInstanceClass(Defaults->GetMesh()->GetAnimClass());
			return Character->GetMesh()->GetAnimInstance()!=nullptr;
		};
		if(!Owner || !Victim || !ConfigureMesh(Owner,TEXT("/Game/ProjectFiles/Core/Actors/Character/BP_Player.BP_Player_C"))
			|| !ConfigureMesh(Victim,TEXT("/Game/ProjectFiles/Core/Actors/Character/BP_EnemyCharacter.BP_EnemyCharacter_C"))) {return false;}
		auto* Source=LoadObject<UPairedAnimationData>(nullptr,TEXT("/Game/ProjectFiles/Data/PDA/Defense/GateA/DA_Finisher_GateA.DA_Finisher_GateA"));
		if(!Source) {return false;}
		Pair=DuplicateObject<UPairedAnimationData>(Source,GetTransientPackage());
		Pair->bApplySlowMotion=false; Pair->Entry.bEnabled=true;
		Pair->Entry.VictimRelativeTransform=FTransform(FRotator::ZeroRotator,FVector(80,0,0));
		Attack=FCombatTestHelpers::CreateTestAttack(EAttackType::Light); Attack->FinisherData=Pair;
		Owner->SetActorLocationAndRotation(FVector::ZeroVector,FRotator::ZeroRotator);
		Victim->SetActorLocationAndRotation(FVector(140,0,0),FRotator(0,180,0));
		Victim->CurrentHealth=1; Owner->TargetingComponent->SetCurrentTarget(Victim);
		return true;
	}
	bool Start() {return Owner->PairedAnimationComponent->TryExecuteFinisher(Attack);}
	bool ConfigureInitiator()
	{
		Pair->Entry.MovingRole = EPairedEntryMovingRole::Initiator;
		Pair->Entry.MovementAnimation = LoadObject<UAnimSequence>(nullptr,
			TEXT("/Game/Assets/Animations/KatanaAnimset/InPlace/WalkForward_InPlace.WalkForward_InPlace"));
		Pair->Entry.VictimRelativeTransform = FTransform(FRotator::ZeroRotator, FVector(100,0,0));
		Pair->Entry.Limits.TranslationSpeed = 122;
		Pair->Entry.Limits.Duration = .65f;
		Victim->SetActorLocationAndRotation(FVector(150,0,0), FRotator::ZeroRotator);
		return Pair->Entry.MovementAnimation != nullptr;
	}
	void Step(float Delta)
	{
		for(auto* Character : {static_cast<ABaseCombatCharacter*>(Owner),static_cast<ABaseCombatCharacter*>(Victim)})
		{
			Character->TargetingComponent->ResetAlignmentExecutionFrameForTesting();
			Character->TargetingComponent->TickComponent(Delta,LEVELTICK_All,nullptr);
		}
		Owner->PairedAnimationComponent->TickComponent(Delta,LEVELTICK_All,nullptr);
	}
	~FPairedEntryFixture() {if(Owner) {Owner->PairedAnimationComponent->CancelPairedAnimation(0);} FCombatTestHelpers::DestroyTestWorld(World);}
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPairedEntryReadiness,"KatanaCombat.PairedAnimation.Entry.PlaybackWaitsForReadiness",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPairedEntryReadiness::RunTest(const FString&)
{
	FPairedEntryFixture F;
	if(!TestTrue(TEXT("Real montage fixture initialized"),F.Initialize()) || !TestTrue(TEXT("Public finisher reserves entry"),F.Start())) {return false;}
	auto* Paired=F.Owner->PairedAnimationComponent.Get();
	TestTrue(TEXT("Entry is pending"),Paired->IsPreparingPairedEntry());
	TestFalse(TEXT("Attacker montage has not started"),F.Owner->GetMesh()->GetAnimInstance()->Montage_IsPlaying(F.Pair->AttackerMontage));
	TestFalse(TEXT("Victim montage has not started"),F.Victim->GetMesh()->GetAnimInstance()->Montage_IsPlaying(F.Pair->VictimMontage));
	Paired->HandlePairedSyncPoint(TEXT("Impact"),true); Paired->CompletePairedAnimation();
	TestEqual(TEXT("Premature sync/completion cannot damage"),F.Victim->CurrentHealth,1.f);
	TestTrue(TEXT("Premature completion cannot approve entry"),Paired->IsPreparingPairedEntry());
	for(int I=0; I<40 && Paired->IsPreparingPairedEntry(); ++I) {F.Step(1.f/60);}
	TestEqual(TEXT("Entry reached declared pose"),Paired->GetLastPairedEntryOutcome(),EAlignmentMotionOutcome::Reached);
	TestTrue(TEXT("Ready entry starts both real montages"),F.Owner->GetMesh()->GetAnimInstance()->Montage_IsPlaying(F.Pair->AttackerMontage)
		&& F.Victim->GetMesh()->GetAnimInstance()->Montage_IsPlaying(F.Pair->VictimMontage));
	TestTrue(TEXT("Victim entry location meets tolerance"),F.Victim->GetActorLocation().Equals(FVector(80,0,0),2.));
	TestTrue(TEXT("Victim entry yaw meets tolerance"),F.Victim->GetActorRotation().Equals(FRotator::ZeroRotator,3.));
	TestTrue(TEXT("Ready entry releases its alignment requests"),F.Owner->TargetingComponent->GetAlignmentRequestCountForTesting()==0
		&& F.Victim->TargetingComponent->GetAlignmentRequestCountForTesting()==0);
	TestNull(TEXT("Bounded preparation cannot be followed by a sync teleport"),Paired->GetOwnedSyncCorrectionTarget());
	TestFalse(TEXT("Supervisor tick stops after preparation"),Paired->IsComponentTickEnabled());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPairedEntryCancellation,"KatanaCombat.PairedAnimation.Entry.CancellationAndRetry",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPairedEntryCancellation::RunTest(const FString&)
{
	FPairedEntryFixture F; if(!TestTrue(TEXT("Fixture initialized"),F.Initialize())) {return false;}
	auto* Paired=F.Owner->PairedAnimationComponent.Get();
	const auto Movement=F.Victim->GetCharacterMovement()->MovementMode;
	for(int Attempt=0;Attempt<2;++Attempt)
	{
		TestTrue(TEXT("Request accepted without leaked predecessor ownership"),F.Start()); F.Step(.05f);
		const FVector Position=F.Victim->GetActorLocation();
		Paired->CancelPairedAnimation(0);
		TestEqual(TEXT("Cancellation is explicit"),Paired->GetLastPairedEntryOutcome(),EAlignmentMotionOutcome::Cancelled);
		TestFalse(TEXT("Cancellation releases pair"),Paired->IsPairedAnimationActive());
		TestFalse(TEXT("Cancellation releases input"),Paired->IsInputBlocked());
		TestFalse(TEXT("Victim reaction reservation released"),F.Victim->HitReactionComponent->IsInPairedAnimationState());
		TestEqual(TEXT("Victim movement mode restored"),F.Victim->GetCharacterMovement()->MovementMode,Movement);
		TestEqual(TEXT("Cancelled entry never applied damage"),F.Victim->CurrentHealth,1.f);
		F.Step(.5f); TestTrue(TEXT("Released requests cannot continue moving"),F.Victim->GetActorLocation().Equals(Position));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPairedEntryUnreachable,"KatanaCombat.PairedAnimation.Entry.UnreachableAndPreempted",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPairedEntryUnreachable::RunTest(const FString&)
{
	FPairedEntryFixture F; if(!TestTrue(TEXT("Fixture initialized"),F.Initialize())) {return false;}
	auto* Paired=F.Owner->PairedAnimationComponent.Get();
	F.Pair->Entry.Limits.TravelBudget=1;
	TestFalse(TEXT("Unreachable entry is rejected before takeover"),F.Start());
	TestFalse(TEXT("Rejected entry owns no input"),Paired->IsInputBlocked());
	TestFalse(TEXT("Rejected entry owns no victim state"),F.Victim->HitReactionComponent->IsInPairedAnimationState());
	F.Pair->Entry.Limits.TravelBudget=150;
	TestTrue(TEXT("Reachable replacement accepted"),F.Start());
	FAlignmentRequestSpec Spec; Spec.OwnerId=TEXT("CompetingAlignment"); Spec.OwnerGeneration=77;
	Spec.Executor=EAlignmentExecutor::CharacterMovement; Spec.MaximumTurnRate=90; Spec.RemainingTurnBudget=90;
	Spec.Priority=EDefenseAlignmentPriority::Terminal;
	const auto Other=F.Owner->TargetingComponent->AcquireAlignmentRequest(Spec);
	F.Step(.02f);
	TestFalse(TEXT("Preempted preparation cannot start playback"),Paired->IsPairedAnimationActive());
	FAlignmentRequestSpec Preserved;
	TestTrue(TEXT("Cleanup preserves the competing owner's request"),F.Owner->TargetingComponent->GetAlignmentRequestSpec(Other,Preserved));
	TestEqual(TEXT("Preemption did not damage the victim"),F.Victim->CurrentHealth,1.f);
	F.Owner->TargetingComponent->ReleaseAlignmentRequest(Other);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPairedEntryLiveGoal,"KatanaCombat.PairedAnimation.Entry.LiveGoalAndExhaustion",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPairedEntryLiveGoal::RunTest(const FString&)
{
	for (const bool Exhaust : {false,true})
	{
		FPairedEntryFixture F; if(!TestTrue(TEXT("Fixture initialized"),F.Initialize()) || !TestTrue(TEXT("Entry reserved"),F.Start())) {return false;}
		auto* Paired=F.Owner->PairedAnimationComponent.Get();
		if (!Exhaust)
		{
			// Executors report ready, then external movement changes the target before supervision.
			for (auto* Character : {static_cast<ABaseCombatCharacter*>(F.Owner),static_cast<ABaseCombatCharacter*>(F.Victim)})
			{
				Character->TargetingComponent->ResetAlignmentExecutionFrameForTesting();
				Character->TargetingComponent->TickComponent(.4f,LEVELTICK_All,nullptr);
			}
			F.Owner->AddActorWorldOffset(FVector(10,0,0));
			Paired->TickComponent(.4f,LEVELTICK_All,nullptr);
			TestTrue(TEXT("Stale readiness cannot approve a moved goal"),Paired->IsPreparingPairedEntry());
			TestFalse(TEXT("Moved goal still defers montage playback"),F.Owner->GetMesh()->GetAnimInstance()->Montage_IsPlaying(F.Pair->AttackerMontage));
		}
		else
		{
			F.Owner->SetActorLocation(FVector(1000,0,0)); F.Step(.6f);
			TestEqual(TEXT("Unreachable live goal exhausts the reservation"),Paired->GetLastPairedEntryOutcome(),EAlignmentMotionOutcome::Exhausted);
			TestFalse(TEXT("Exhaustion releases paired ownership"),Paired->IsPairedAnimationActive());
			TestFalse(TEXT("Exhaustion releases victim reaction ownership"),F.Victim->HitReactionComponent->IsInPairedAnimationState());
			TestEqual(TEXT("Exhaustion cannot commit damage"),F.Victim->CurrentHealth,1.f);
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPairedEntryInitiatorGoal,"KatanaCombat.PairedAnimation.Entry.InitiatorMovesVictimAnchors",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPairedEntryInitiatorGoal::RunTest(const FString&)
{
	for (const double Yaw : {0., 30.})
	{
		FPairedEntryFixture F;
		if (!TestTrue(TEXT("Fixture and reviewed clip load"), F.Initialize() && F.ConfigureInitiator())) { return false; }
		// Native inventory closes the Python reflection gap for protected notify data.
		TestEqual(TEXT("Reviewed approach clip has no gameplay notifies"), F.Pair->Entry.MovementAnimation->Notifies.Num(), 0);
		F.Victim->SetActorRotation(FRotator(0,Yaw,0));
		const FTransform Anchor = F.Victim->GetActorTransform();
		const FTransform Goal = F.Pair->Entry.VictimRelativeTransform.Inverse() * Anchor;
		if (!TestTrue(TEXT("Initiator entry accepted"), F.Start())) { return false; }
		for (int I=0; I<45 && F.Owner->PairedAnimationComponent->IsPreparingPairedEntry(); ++I)
		{
			F.Step(1.f/60);
			TestTrue(TEXT("Victim remains at accepted anchor throughout entry"), F.Victim->GetActorTransform().Equals(Anchor));
		}
		TestEqual(TEXT("Initiator entry reaches readiness"), F.Owner->PairedAnimationComponent->GetLastPairedEntryOutcome(), EAlignmentMotionOutcome::Reached);
		TestTrue(TEXT("Initiator resolves the inverse relative pose including heading"), F.Owner->GetActorLocation().Equals(Goal.GetLocation(),2)
			&& F.Owner->GetActorRotation().Equals(Goal.Rotator(),3));
		TestTrue(TEXT("Both paired montages start only after approach"), F.Owner->GetMesh()->GetAnimInstance()->Montage_IsPlaying(F.Pair->AttackerMontage)
			&& F.Victim->GetMesh()->GetAnimInstance()->Montage_IsPlaying(F.Pair->VictimMontage));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPairedEntryPresentationOwnership,"KatanaCombat.PairedAnimation.Entry.MovementPresentationOwnership",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPairedEntryPresentationOwnership::RunTest(const FString&)
{
	for (const bool bReplace : {false, true})
	{
		FPairedEntryFixture F;
		if (!TestTrue(TEXT("Fixture loads"), F.Initialize() && F.ConfigureInitiator()) || !TestTrue(TEXT("Entry starts"), F.Start())) { return false; }
		auto* Instance = F.Owner->GetMesh()->GetAnimInstance();
		UAnimMontage* Approach = Instance->GetCurrentActiveMontage();
		if (!TestNotNull(TEXT("Approach has an active instance"), Approach)) { return false; }
		TestTrue(TEXT("Entry uses a transient movement montage"), Approach != F.Pair->AttackerMontage && Approach->GetPackage() == GetTransientPackage());
		UAnimMontage* Replacement = nullptr;
		if (bReplace)
		{
			Replacement = UAnimMontage::CreateSlotAnimationAsDynamicMontage(F.Pair->Entry.MovementAnimation, TEXT("DefaultSlot"));
			TestTrue(TEXT("External replacement starts"), Instance->Montage_Play(Replacement) > 0);
		}
		else { Instance->Montage_Stop(0, Approach); }
		F.Step(.02f);
		TestEqual(TEXT("Lost approach ownership rejects readiness"), F.Owner->PairedAnimationComponent->GetLastPairedEntryOutcome(), EAlignmentMotionOutcome::Invalid);
		TestFalse(TEXT("Replacement or early stop releases the pair"), F.Owner->PairedAnimationComponent->IsPairedAnimationActive());
		TestFalse(TEXT("No early paired playback"), Instance->Montage_IsPlaying(F.Pair->AttackerMontage));
		TestEqual(TEXT("No early damage"), F.Victim->CurrentHealth, 1.f);
		if (bReplace) { TestTrue(TEXT("Entry cleanup preserves the replacement instance"), Instance->Montage_IsPlaying(Replacement)); }
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPairedEntryInitiatorObstruction,"KatanaCombat.PairedAnimation.Entry.InitiatorObstruction",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPairedEntryInitiatorObstruction::RunTest(const FString&)
{
	FPairedEntryFixture F;
	if (!TestTrue(TEXT("Fixture loads"), F.Initialize() && F.ConfigureInitiator())) { return false; }
	const FVector Anchor = F.Victim->GetActorLocation();
	const auto Movement = F.Owner->GetCharacterMovement()->MovementMode;
	if (!TestTrue(TEXT("Clear approach accepted before obstruction appears"), F.Start())) { return false; }
	// A wall present before public targeting can reject the target itself. Insert
	// this obstacle after acceptance to exercise the entry executor's swept failure.
	AActor* Wall = F.World->SpawnActor<AActor>();
	auto* Box = NewObject<UBoxComponent>(Wall); Wall->SetRootComponent(Box); Box->SetBoxExtent(FVector(3,100,200));
	Box->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics); Box->SetCollisionResponseToAllChannels(ECR_Block);
	Box->SetCollisionObjectType(ECC_WorldStatic); Box->RegisterComponent(); Wall->SetActorLocation(FVector(65,0,0));
	for (int I=0; I<45 && F.Owner->PairedAnimationComponent->IsPreparingPairedEntry(); ++I) { F.Step(1.f/60); }
	TestEqual(TEXT("Obstruction is retained"), F.Owner->PairedAnimationComponent->GetLastPairedEntryOutcome(), EAlignmentMotionOutcome::Blocked);
	TestTrue(TEXT("Initiator cannot cross the obstacle"), F.Owner->GetActorLocation().X < 50);
	TestTrue(TEXT("Victim stays anchored"), F.Victim->GetActorLocation().Equals(Anchor));
	TestFalse(TEXT("Obstruction releases input and participation"), F.Owner->PairedAnimationComponent->IsInputBlocked() || F.Victim->HitReactionComponent->IsInPairedAnimationState());
	TestEqual(TEXT("Movement mode restored"), F.Owner->GetCharacterMovement()->MovementMode, Movement);
	TestFalse(TEXT("No montage begins after obstruction"), F.Owner->GetMesh()->GetAnimInstance()->Montage_IsPlaying(F.Pair->AttackerMontage));
	TestEqual(TEXT("Obstruction never damages"), F.Victim->CurrentHealth, 1.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPairedEntryInitiatorCancelReady,"KatanaCombat.PairedAnimation.Entry.InitiatorCancellationAndReady",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPairedEntryInitiatorCancelReady::RunTest(const FString&)
{
	FPairedEntryFixture F;
	if (!TestTrue(TEXT("Fixture loads"), F.Initialize() && F.ConfigureInitiator())) { return false; }
	const auto Movement = F.Owner->GetCharacterMovement()->MovementMode;
	for (int Attempt=0; Attempt<2; ++Attempt)
	{
		if (!TestTrue(TEXT("Approach can be retried"), F.Start())) { return false; }
		F.Step(.05f);
		const FVector Position = F.Owner->GetActorLocation();
		F.Owner->PairedAnimationComponent->CancelPairedAnimation(0);
		F.Step(.2f);
		TestTrue(TEXT("Cancellation does not teleport or continue approach"), F.Owner->GetActorLocation().Equals(Position));
		TestEqual(TEXT("Cancellation restores initiator movement"), F.Owner->GetCharacterMovement()->MovementMode, Movement);
		TestFalse(TEXT("Cancellation releases input"), F.Owner->PairedAnimationComponent->IsInputBlocked());
		TestEqual(TEXT("Cancelled approach retains health"), F.Victim->CurrentHealth, 1.f);
	}
	F.Owner->GetMesh()->GetAnimInstance()->Montage_Stop(0);
	F.Owner->SetActorLocation(FVector(50,0,0));
	if (!TestTrue(TEXT("Ready pair accepted"), F.Start())) { return false; }
	TestNull(TEXT("Already-ready pair skips the approach montage"), F.Owner->GetMesh()->GetAnimInstance()->GetCurrentActiveMontage());
	F.Step(.01f);
	TestEqual(TEXT("Ready pair proceeds without displacement"), F.Owner->PairedAnimationComponent->GetLastPairedEntryOutcome(), EAlignmentMotionOutcome::Reached);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPairedEntryPresentationPreflight,"KatanaCombat.PairedAnimation.Entry.PresentationPreflight",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPairedEntryPresentationPreflight::RunTest(const FString&)
{
	for (int Mode=0; Mode<8; ++Mode)
	{
		FPairedEntryFixture F;
		if (!TestTrue(TEXT("Fixture loads"), F.Initialize() && F.ConfigureInitiator())) { return false; }
		if (Mode==0) { F.Pair->Entry.MovementAnimation = LoadObject<UAnimSequence>(nullptr,TEXT("/Game/Assets/Animations/KatanaAnimset/RootMotion/WalkForward_Root.WalkForward_Root")); }
		if (Mode==1) { F.Pair->Entry.MovementPlayRate = 10; }
		if (Mode==2) { F.Pair->Entry.MovingRole = static_cast<EPairedEntryMovingRole>(255); }
		if (Mode==3) { F.Pair->Entry.MovementSlot = TEXT("UnregisteredApproachSlot"); }
		if (Mode==4) { F.Pair->Entry.MovementStartTime = -1; }
		if (Mode==5) { F.Pair->Entry.MovementStartTime = F.Pair->Entry.MovementAnimation->GetPlayLength(); }
		if (Mode==6) { F.Pair->Entry.MovementStartTime = F.Pair->Entry.MovementAnimation->GetPlayLength() - .01f; }
		if (Mode==7) { F.Pair->Entry.MovementAnimation = nullptr; F.Pair->Entry.MovementStartTime = .1f; }
		TestFalse(TEXT("Invalid presentation rejects before takeover"), F.Start());
		TestFalse(TEXT("Rejected presentation owns no victim"), F.Victim->HitReactionComponent->IsInPairedAnimationState());
		TestFalse(TEXT("Rejected presentation owns no input"), F.Owner->PairedAnimationComponent->IsInputBlocked());
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPairedEntrySourcePhase,"KatanaCombat.PairedAnimation.Entry.SourcePhaseUsesAssetRate",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPairedEntrySourcePhase::RunTest(const FString&)
{
	for (const float AssetRate : {1.f, 2.f})
	{
		FPairedEntryFixture F;
		if (!TestTrue(TEXT("Fixture loads"), F.Initialize() && F.ConfigureInitiator())) { return false; }
		F.Pair->Entry.MovementAnimation = DuplicateObject<UAnimSequence>(F.Pair->Entry.MovementAnimation, GetTransientPackage());
		F.Pair->Entry.MovementAnimation->RateScale = AssetRate;
		F.Pair->Entry.MovementPlayRate = .5f;
		F.Pair->Entry.MovementStartTime = .2f;
		if (!TestTrue(TEXT("Nonzero source phase accepted"), F.Start())) { return false; }
		auto* Instance = F.Owner->GetMesh()->GetAnimInstance();
		TestTrue(TEXT("Active track starts at source seconds divided by asset rate"),
			FMath::IsNearlyEqual(Instance->Montage_GetPosition(Instance->GetCurrentActiveMontage()), .2f / AssetRate));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPairedEntryTuningRoundTrip,"KatanaCombat.PairedAnimation.Entry.PresentationTuningIdentity",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPairedEntryTuningRoundTrip::RunTest(const FString&)
{
	FPairedEntryFixture F;
	if (!TestTrue(TEXT("Fixture loads"), F.Initialize() && F.ConfigureInitiator())) { return false; }
	const auto Snapshot = PairedEntryTuning::Snapshot(F.Pair->Entry);
	FPairedEntryConfig Parsed;
	TestTrue(TEXT("Typed presentation round trips"), PairedEntryTuning::Read(Snapshot, Parsed));
	TestEqual(TEXT("Animation identity retained"), Parsed.MovementAnimation.Get(), F.Pair->Entry.MovementAnimation.Get());
	auto Row = MakeShared<FJsonObject>(); Row->SetObjectField(TEXT("before"), PairedEntryTuning::Snapshot({}));
	Row->SetObjectField(TEXT("after"), Snapshot);
	TestTrue(TEXT("Requested presentation matches recorded override"), PairedEntryTuning::ValidateOverride(F.Pair->Entry, Row));
	Snapshot->SetStringField(TEXT("moving_role"), TEXT("victim"));
	TestFalse(TEXT("Wrong moving role cannot validate"), PairedEntryTuning::ValidateOverride(F.Pair->Entry, Row));
	Snapshot->SetStringField(TEXT("moving_role"), TEXT("unknown"));
	TestFalse(TEXT("Unknown role cannot silently default"), PairedEntryTuning::Read(Snapshot, Parsed));
	for (const TCHAR* Key : {TEXT("moving_role"), TEXT("movement_animation"), TEXT("movement_slot"), TEXT("movement_play_rate"), TEXT("movement_blend_in_s"), TEXT("movement_blend_out_s"), TEXT("movement_start_time_s")}) { Snapshot->RemoveField(Key); }
	TestTrue(TEXT("Original ten-field entry settings remain readable"), PairedEntryTuning::Read(Snapshot, Parsed));
	TestEqual(TEXT("Original settings keep victim-moving behavior"), Parsed.MovingRole, EPairedEntryMovingRole::Victim);
	TestNull(TEXT("Original settings have no presentation clip"), Parsed.MovementAnimation.Get());
	return true;
}
