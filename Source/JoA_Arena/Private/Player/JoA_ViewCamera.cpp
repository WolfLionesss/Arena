// Copyright Epic Games, Inc. All Rights Reserved.

#include "Player/JoA_ViewCamera.h"

// --- Engine ---
#include "Camera/CameraComponent.h"
#include "Engine/World.h"

namespace JoA_ViewCameraPrivate
{
	/* +X, behind French, looking back at the board. */
	const FVector FrenchLocation(1000.f, 0.f, 600.f);
	const FRotator FrenchRotation(-30.f, 180.f, 0.f);

	/* -X, behind English, looking back at the board. */
	const FVector EnglishLocation(-1000.f, 0.f, 600.f);
	const FRotator EnglishRotation(-30.f, 0.f, 0.f);
}

AJoA_ViewCamera::AJoA_ViewCamera()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = false;
	SetReplicateMovement(false);

	if (UCameraComponent* Cam = GetCameraComponent())
	{
		Cam->bUsePawnControlRotation = false;
		Cam->bConstrainAspectRatio = false;
	}
}

FTransform AJoA_ViewCamera::GetFrenchViewTransform()
{
	return FTransform(
		JoA_ViewCameraPrivate::FrenchRotation,
		JoA_ViewCameraPrivate::FrenchLocation);
}

FTransform AJoA_ViewCamera::GetEnglishViewTransform()
{
	return FTransform(
		JoA_ViewCameraPrivate::EnglishRotation,
		JoA_ViewCameraPrivate::EnglishLocation);
}

FTransform AJoA_ViewCamera::GetLocalViewTransform(
	const UObject* WorldContext)
{
	const UWorld* World = WorldContext
		? WorldContext->GetWorld()
		: nullptr;
	const bool bClient =
		World && World->GetNetMode() == NM_Client;
	return bClient
		? GetEnglishViewTransform()
		: GetFrenchViewTransform();
}

void AJoA_ViewCamera::ApplyToViewInfo(
	FMinimalViewInfo& OutResult,
	const UObject* WorldContext)
{
	const FTransform View =
		GetLocalViewTransform(WorldContext);
	OutResult.Location = View.GetLocation();
	OutResult.Rotation = View.Rotator();
	OutResult.FOV = 90.f;
	OutResult.bConstrainAspectRatio = false;
}

namespace
{
	void ApplyLockedView(
		ACameraActor* Camera,
		const FTransform& View)
	{
		if (USceneComponent* Root =
			Camera->GetRootComponent())
		{
			Root->SetMobility(EComponentMobility::Movable);
		}
		Camera->SetActorTransform(
			View,
			false,
			nullptr,
			ETeleportType::TeleportPhysics);

		/* The rendered view reads the component,
		 * not only the actor root. */
		if (UCameraComponent* Cam =
			Camera->GetCameraComponent())
		{
			Cam->SetMobility(EComponentMobility::Movable);
			Cam->bUsePawnControlRotation = false;
			Cam->SetWorldTransform(View);
		}
	}
}

void AJoA_ViewCamera::ApplyFrenchView()
{
	ApplyLockedView(this, GetFrenchViewTransform());
}

void AJoA_ViewCamera::ApplyEnglishView()
{
	ApplyLockedView(this, GetEnglishViewTransform());
}
