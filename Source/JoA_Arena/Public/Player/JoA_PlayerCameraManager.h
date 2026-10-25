// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

// --- Engine ---
#include "Camera/PlayerCameraManager.h"

#include "JoA_PlayerCameraManager.generated.h"

/**
 * Camera manager for the local player.
 * Possession makes a figure the view target.
 * Its eye height sits in the head, so the
 * rendered view is always the board camera.
 */
UCLASS()
class JOA_ARENA_API AJoA_PlayerCameraManager
	: public APlayerCameraManager
{
	GENERATED_BODY()

public:
	AJoA_PlayerCameraManager();

	virtual void UpdateCamera(
		float DeltaTime) override;
	virtual void UpdateViewTarget(
		FTViewTarget& OutVT,
		float DeltaTime) override;
};
