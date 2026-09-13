#include "Misc/AutomationTest.h"
#include "CombatScenarioPlacement.h"
#include "CombatScenarioGrounding.h"
#include "CombatTestHelpers.h"
#include "Components/BoxComponent.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatScenarioPlacementValidationTest,
	"KatanaCombat.Capture.ScenarioPlacement.InputValidation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCombatScenarioPlacementValidationTest::RunTest(const FString& Parameters)
{
	auto Pose = MakeShared<FJsonObject>();
	Pose->SetArrayField(TEXT("offset_cm"), CombatScenarioPlacement::VectorJson(FVector(100, 20, 0)));
	Pose->SetNumberField(TEXT("yaw_deg"), -180);
	FTransform Parsed;
	TestTrue(TEXT("Valid upright pose is accepted"), CombatScenarioPlacement::ReadPose(Pose, Parsed));
	TestTrue(TEXT("Placement retains its declared translation"), Parsed.GetLocation().Equals(FVector(100, 20, 0)));
	Pose->SetBoolField(TEXT("yaw_deg"), false);
	TestFalse(TEXT("Boolean yaw is not coerced to a number"), CombatScenarioPlacement::ReadPose(Pose, Parsed));
	Pose->SetNumberField(TEXT("yaw_deg"), 181);
	TestFalse(TEXT("Out-of-domain heading is rejected"), CombatScenarioPlacement::ReadPose(Pose, Parsed));
	Pose->SetNumberField(TEXT("yaw_deg"), 0);
	Pose->SetArrayField(TEXT("offset_cm"), CombatScenarioPlacement::VectorJson(FVector(3001, 0, 0)));
	TestFalse(TEXT("Unbounded fixture coordinates are rejected"), CombatScenarioPlacement::ReadPose(Pose, Parsed));
	Pose->SetArrayField(TEXT("offset_cm"), CombatScenarioPlacement::VectorJson(FVector(100, 0, 0)));
	auto Roles = MakeShared<FJsonObject>(); Roles->SetObjectField(TEXT("Attacker"), Pose); Roles->SetObjectField(TEXT("Victim"), Pose);
	auto Placements = MakeShared<FJsonObject>(); Placements->SetObjectField(TEXT("rear"), Roles);
	auto Definition = MakeShared<FJsonObject>(); Definition->SetObjectField(TEXT("placements"), Placements);
	TMap<FString, FTransform> Poses;
	TestTrue(TEXT("Complete named roles resolve"), CombatScenarioPlacement::Read(Definition, TEXT("rear"), Poses));
	TestEqual(TEXT("Both roles resolved"), Poses.Num(), 2);
	Roles->RemoveField(TEXT("Victim"));
	TestFalse(TEXT("Partial setup is rejected"), CombatScenarioPlacement::Read(Definition, TEXT("rear"), Poses));
	TestTrue(TEXT("A rejected setup cannot retain a previous pose"), Poses.IsEmpty());
	TestTrue(TEXT("Default selector preserves existing setup"), CombatScenarioPlacement::Read(Definition, TEXT("default"), Poses));
	TestTrue(TEXT("Default adds no pose overrides"), Poses.IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatScenarioGroundingTest,
	"KatanaCombat.Capture.ScenarioPlacement.FloorPreparation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCombatScenarioGroundingTest::RunTest(const FString&)
{
	for (int32 Mode=0; Mode<6; ++Mode)
	{
		UWorld* World = FCombatTestHelpers::CreateTestWorld();
		AActor* Support = World->SpawnActor<AActor>();
		auto* Box = NewObject<UBoxComponent>(Support); Support->SetRootComponent(Box);
		Box->SetBoxExtent(FVector(200,200,10)); Box->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		Box->SetCollisionResponseToAllChannels(ECR_Block); Box->SetCollisionObjectType(ECC_WorldStatic); Box->RegisterComponent();
		const float FloorHeight = Mode==1 ? 50 : 0;
		Support->SetActorLocation(FVector(0,0,FloorHeight-10));
		if (Mode==2) { Box->SetCollisionEnabled(ECollisionEnabled::NoCollision); }
		if (Mode==3) { Support->SetActorRotation(FRotator(65,0,0)); }
		ACharacter* Character = World->SpawnActor<ACharacter>();
		const float HalfHeight = Mode==1 ? 110 : 88;
		Character->GetCapsuleComponent()->SetCapsuleSize(30, HalfHeight);
		Character->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
		Character->SetActorLocation(FVector(0,0,FloorHeight+HalfHeight+(Mode==4 ? -5 : 8)), false, nullptr, ETeleportType::TeleportPhysics);
		const FVector Requested = Character->GetActorLocation();
		auto Evidence = MakeShared<FJsonObject>();
		const bool bPrepared = CombatScenarioGrounding::Prepare(Character, Mode==5 ? 1 : 20, Evidence);
		if (Mode<2)
		{
			TestTrue(TEXT("Live capsule grounds on the actual floor"), bPrepared);
			TestTrue(TEXT("Height includes measured capsule and walking clearance"), FMath::IsNearlyEqual(Character->GetActorLocation().Z, FloorHeight+HalfHeight+2.15, .02));
			const FVector Grounded = Character->GetActorLocation();
			Character->GetCharacterMovement()->DisableMovement();
			Character->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
			TestTrue(TEXT("Movement restoration has no setup-induced height step"), Character->GetActorLocation().Equals(Grounded, .01));
		}
		else
		{
			TestFalse(TEXT("Missing, steep, penetrating or over-budget floor rejects"), bPrepared);
			TestTrue(FString::Printf(TEXT("Rejected preparation retains requested position (case %d)"), Mode), Character->GetActorLocation().Equals(Requested, .01));
		}
		FCombatTestHelpers::DestroyTestWorld(World);
	}
	return true;
}
