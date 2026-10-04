// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"

class AActor;
class ABaseCombatCharacter;
class FJsonObject;
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

	/** Opt in to a PresentationCapture MP4 of the PIE viewport, recorded in the same session.
	 * Requires a rendering editor (-RenderOffScreen works; -NullRHI cannot record video).
	 * The clip is written under the bundle's video/ directory and joined by capture-link.json. */
	bool bRecordVideo = false;
	/** Requested video rate, 1-120. A scheduling target; the achieved cadence is recorded, not promised. */
	int32 VideoFramesPerSecond = 60;
	/** Output bounding box by height: 360 (640x360), 720 (1280x720) or 1080 (1920x1080).
	 * Only a viewport of exactly that size is recorded without scaling. */
	int32 VideoResolution = 720;
	/** Clip bound, 1-30 seconds; 0 uses MaxWallSeconds clamped to the recorder's 30 s limit. */
	int32 VideoSeconds = 0;
};

/** How capture-link.json reports a clip that the recorder ended without a stop request. */
struct KATANACOMBATEDITOR_API FCombatCaptureRecorderEnd
{
	/** `stopped_by_recorder_limit`, `stopped_by_recorder_error` or `stopped_by_recorder`. */
	FString Status;
	/** The link's video.stop_reason, prefixed `recorder_limit`, `recorder_error` or `recorder_stopped`. */
	FString Reason;
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
	/** Marker with a bounded JSON payload the recorder stores verbatim (native 0.4.0). */
	void Mark(const FString& Label, const TSharedPtr<FJsonObject>& Payload);
	/** Writes `contact` markers with the reaction-review payload when the attacker's weapon
	 * trace hits a non-character victim, when a committed defense contact from the attacker
	 * lands on the victim (character hits, blocks and parries, with their outcome), or when the
	 * attacker's paired animation reaches a sync point. Requires a recording session; the
	 * observer is released by Stop. */
	bool ObserveContacts(ABaseCombatCharacter* Attacker, const FString& AttackerRole,
		ABaseCombatCharacter* Victim, const FString& VictimRole, FString& OutError);
	/** Marks committed defense contacts between any two recorded combat participants, in
	 * either direction, with their roles and outcome. The console recorder uses it. */
	bool ObserveParticipantContacts(FString& OutError);
	/** Weapon-trace and paired-sync contact markers written so far by ObserveContacts. */
	void GetContactCounts(int32& OutWeapon, int32& OutPaired) const;
	/** Committed defense-contact markers (hits, blocks, parries) written so far. */
	int32 GetCommittedContactCount() const;
	bool IsRecording() const;
	FString GetOutputDirectory() const;
	int32 GetSampleCount() const;
	int32 GetFrameCount() const;
	FString GetStopReason() const;
	/** True once this session started a clip; stays true after Stop for inspection. */
	bool HasVideo() const;
	/** True while this session's clip is still recording or being finalized by its encoder, or while
	 * capture-link.json still awaits the outcome of a clip the recorder ended on its own. Video stops
	 * asynchronously: wait for this to clear before reading the MP4 or the final link. */
	bool IsVideoFinalizing() const;
	/** PresentationCapture's directory for this session's clip, or empty without video. */
	FString GetVideoDirectory() const;
	/** capture-link.json joining the clip to this bundle, or empty without video. */
	FString GetLinkPath() const;

	/** Classifies a clip the recorder ended without a stop request (its own bound, an encoder or write
	 * failure, or another recorder client's Stop) from the clip's finalized video-manifest.json:
	 * unreadable or incomplete is an error with the recorder's failure reason; complete and observed
	 * stopped at or after the capture epoch plus the clip bound is the limit; complete and earlier
	 * was stopped by another recorder client. An external stop within one engine tick of the bound
	 * is indistinguishable from the limit. Public so the classification is testable headless. */
	static FCombatCaptureRecorderEnd ClassifyRecorderEnd(bool bManifestRead, bool bComplete, const FString& FailureReason,
		double CaptureEpochSeconds, double ClipSeconds, double ObservedStopSeconds);

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
