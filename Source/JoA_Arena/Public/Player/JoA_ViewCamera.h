// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

// --- Engine ---
#include "Camera/CameraActor.h"
#include "Camera/CameraTypes.h"

#include "JoA_ViewCamera.generated.h"

/**
 * Locked board camera. Actor transform is the view.
 * Place it in the map, or the local controller
 * spawns one. French stands on +X. English
 * stands on -X. Both pitches aim at the board.
 */
UCLASS(Blueprintable)
class JOA_ARENA_API AJoA_ViewCamera : public ACameraActor
{
	GENERATED_BODY()

public:
	AJoA_ViewCamera();

	static FTransform GetFrenchViewTransform();
	static FTransform GetEnglishViewTransform();

	/* French on a listen host. English on a
	 * connected client. */
	static FTransform GetLocalViewTransform(
		const UObject* WorldContext);

	static void ApplyToViewInfo(
		FMinimalViewInfo& OutResult,
		const UObject* WorldContext);

	UFUNCTION(BlueprintCallable, Category = "Arena Camera")
	void ApplyFrenchView();

	UFUNCTION(BlueprintCallable, Category = "Arena Camera")
	void ApplyEnglishView();
};
