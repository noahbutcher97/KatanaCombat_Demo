// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"

class AActor;
class USkeletalMeshComponent;
class USceneComponent;
class UWorld;
class FViewportClient;

/** A stable role within a recording. Points are skeletal bones or sockets, in centimetres. */
struct KATANACOMBATEDITOR_API FCombatCaptureParticipant
{
	FString Role;
	TWeakObjectPtr<AActor> Actor;
	/** Optional explicit mesh; otherwise the actor's first skeletal mesh is used. */
	TWeakObjectPtr<USkeletalMeshComponent> Mesh;
	TArray<FName> Points = {TEXT("root"), TEXT("pelvis"), TEXT("foot_l"), TEXT("foot_r"), TEXT("hand_l"), TEXT("hand_r")};
	/** Explicit owned components for selected sockets, e.g. an attached weapon mesh.
	 * Missing/destroyed overrides remain missing; never fall back to the skeleton. */
	TMap<FName, TWeakObjectPtr<USceneComponent>> PointSources;
};

struct KATANACOMBATEDITOR_API FCombatCaptureSettings
{
	FString Scenario = TEXT("ManualCombat");
	double SampleHz = 60.0;
	/** Zero disables PNG capture. Actual cadence is limited by game viewport draws. */
	double FrameHz = 5.0;
	/** Opt in to bounded asynchronous RGB acquisition; synchronous capture remains the default. */
	bool bUseAsyncReadback = false;
	/** Async-only full-resolution diagnostic view policy. Caller still owns AA and camera settings. */
	bool bUseAsyncDiagnosticResolution = false;
	double MaxWallSeconds = 60.0;
	int32 MaxSamples = 7200;
	int32 MaxFrames = 600;
	int32 MaxTelemetryRecordsPerActor = 20000;
	/** Data-file budget; the final diagnostic manifest is written even on exhaustion. */
	int64 MaxDataBytes = 512ll * 1024 * 1024;
	/** Run/scenario/source identities supplied by the repository runner. */
	TMap<FString, FString> Metadata;
};

/**
 * Observational, editor-only recorder shared by automation and ordinary PIE.
 * All methods run on the game thread. One active session is allowed because telemetry
 * switches are global. Never changes actors, cameras, animation settings, or assets.
 * JSONL streams are written incrementally; session.json is finalized by Stop/teardown.
 */
class KATANACOMBATEDITOR_API FCombatCaptureSession
{
public:
	FCombatCaptureSession();
	~FCombatCaptureSession();
	FCombatCaptureSession(const FCombatCaptureSession&) = delete;
	FCombatCaptureSession& operator=(const FCombatCaptureSession&) = delete;

	bool Start(UWorld* World, const FCombatCaptureSettings& Settings,
		TConstArrayView<FCombatCaptureParticipant> Participants, FString& OutError);
	bool Stop(const FString& Reason, FString& OutError);
	void Mark(const FString& Label);
	bool IsRecording() const;
	FString GetOutputDirectory() const;
	int32 GetSampleCount() const;
	int32 GetFrameCount() const;
	FString GetStopReason() const;

	static bool IsExpectedPIEViewportClient(const UWorld* World,
		const FViewportClient* DrawnClient, const FViewportClient* ExpectedClient);
	/** Console discovery uses deterministic role order; explicit roles are preferable in fixtures. */
	static TArray<FCombatCaptureParticipant> DiscoverParticipants(UWorld* World);

private:
	struct FImpl;
	TUniquePtr<FImpl> Impl;
};

namespace CombatCaptureCommands
{
	void Register();
	void Unregister();
	/** Also useful to automation that verifies the ordinary console entry point. */
	KATANACOMBATEDITOR_API FCombatCaptureSession* GetSession();
}
