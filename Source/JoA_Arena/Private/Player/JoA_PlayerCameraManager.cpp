// Copyright Epic Games, Inc. All Rights Reserved.

#include "Player/JoA_PlayerCameraManager.h"

// --- GAME ---
#include "Player/JoA_ViewCamera.h"

// --- Components ---
#include "Components/SceneComponent.h"

AJoA_PlayerCameraManager::AJoA_PlayerCameraManager()
{
	/* Samples the view target each frame.
	 * The target itself is assigned once. */
	PrimaryActorTick.bCanEverTick = true;
}

void AJoA_PlayerCameraManager::UpdateCamera(
	float DeltaTime)
{
	Super::UpdateCamera(DeltaTime);

	/* Super skips the POV until a local player
	 * exists. When it already wrote this view,
	 * a second write only moves the actor again. */
	FMinimalViewInfo BoardView = GetCameraCacheView();
	const FVector CachedLocation = BoardView.Location;
	const FRotator CachedRotation = BoardView.Rotation;
	const float CachedFov = BoardView.FOV;
	AJoA_ViewCamera::ApplyToViewInfo(BoardView, this);

	const bool bCacheMatches =
		BoardView.Location.Equals(CachedLocation)
		&& BoardView.Rotation.Equals(CachedRotation)
		&& FMath::IsNearlyEqual(BoardView.FOV, CachedFov);
	if (!bCacheMatches)
	{
		FillCameraCache(BoardView);
	}

	/* GetActorLocation is private on the camera
	 * manager. The root is the same transform. */
	const USceneComponent* Root = GetRootComponent();
	const bool bActorMatches = Root
		&& BoardView.Location.Equals(
			Root->GetComponentLocation())
		&& BoardView.Rotation.Equals(
			Root->GetComponentRotation());
	if (!bActorMatches)
	{
		SetActorLocationAndRotation(
			BoardView.Location,
			BoardView.Rotation,
			false);
	}
}

void AJoA_PlayerCameraManager::UpdateViewTarget(
	FTViewTarget& OutVT,
	float DeltaTime)
{
	Super::UpdateViewTarget(OutVT, DeltaTime);

	/* CheckViewTarget puts the possessed figure
	 * back as the view target. Its eye height
	 * sits in the head. */
	AJoA_ViewCamera::ApplyToViewInfo(OutVT.POV, this);
	SetActorLocationAndRotation(
		OutVT.POV.Location,
		OutVT.POV.Rotation,
		false);
}
