// Copyright Epic Games, Inc. All Rights Reserved.

#include "Player/JoA_Pawn.h"

// --- Engine ---
#include "Camera/CameraTypes.h"

// --- GAME ---
#include "Player/JoA_ViewCamera.h"

// --- Components ---
#include "Components/SceneComponent.h"

AJoA_Pawn::AJoA_Pawn()
{
	PrimaryActorTick.bCanEverTick = false;

	/* Replicated for possession in listen-server and
	 * client play. The pawn itself does not move. */
	bReplicates = true;
	SetReplicateMovement(false);

	/* APawn defaults to 100 Hz. Nothing on this
	 * pawn replicates, so match the board at 2 Hz. */
	SetNetUpdateFrequency(2.f);
	SetMinNetUpdateFrequency(1.f);

	SceneRoot = CreateDefaultSubobject<USceneComponent>(
		TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;
	BaseEyeHeight = 0.f;
}

void AJoA_Pawn::CalcCamera(
	float DeltaTime,
	FMinimalViewInfo& OutResult)
{
	Super::CalcCamera(DeltaTime, OutResult);
	AJoA_ViewCamera::ApplyToViewInfo(OutResult, this);
}

void AJoA_Pawn::GetActorEyesViewPoint(
	FVector& OutLocation,
	FRotator& OutRotation) const
{
	const FTransform View =
		AJoA_ViewCamera::GetLocalViewTransform(this);
	OutLocation = View.GetLocation();
	OutRotation = View.Rotator();
}

FVector AJoA_Pawn::GetPawnViewLocation() const
{
	return AJoA_ViewCamera::GetLocalViewTransform(this)
		.GetLocation();
}
