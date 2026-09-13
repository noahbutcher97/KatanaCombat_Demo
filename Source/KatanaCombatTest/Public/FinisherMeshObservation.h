#pragma once
#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "AnimationCapture/AnimationCaptureMeshReference.h"

class UMeshComponent;
namespace FinisherMeshObservation
{
FString OutputRoot();
bool WriteJson(const FString& Path, const TSharedRef<FJsonObject>& Object);
FAnimationMeshEnrollment Enrollment(UMeshComponent* Mesh, const FString& Role);
TSharedRef<FJsonObject> Inventory(UMeshComponent* Mesh, const FString& Role);
}
