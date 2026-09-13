#include "CombatTestHelpers.h"
#include "Core/PairedAnimationComponent.h"
#include "Core/TargetingComponent.h"
#include "Data/PairedAnimationData.h"
#include "Animation/AnimInstance.h"
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
