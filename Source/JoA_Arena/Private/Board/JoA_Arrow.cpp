// Copyright Epic Games, Inc. All Rights Reserved.

#include "Board/JoA_Arrow.h"

// --- Engine ---
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"

// --- GAME ---
#include "Match/JoA_GameMode.h"
#include "Board/JoA_GridManager.h"
#include "Match/JoA_CookedContent.h"

// --- Components ---
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"

AJoA_Arrow::AJoA_Arrow()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
	bReplicates = false;
	SetReplicateMovement(false);

	SceneRoot = CreateDefaultSubobject<USceneComponent>(
		TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(
		TEXT("Mesh"));
	Mesh->SetupAttachment(SceneRoot);
	Mesh->SetCollisionEnabled(
		ECollisionEnabled::NoCollision);
	Mesh->SetCastShadow(false);
	Mesh->SetVisibleInRayTracing(false);
	Mesh->SetHiddenInGame(true);
	/* The shaft is on Y. Actor X is the flight,
	 * so this turns the tip toward the target. */
	Mesh->SetRelativeRotation(
		FRotator(0.f, -90.f, 0.f));
}

AJoA_Arrow* AJoA_Arrow::Get(UWorld* World)
{
	if (!World)
	{
		return nullptr;
	}

	TActorIterator<AJoA_Arrow> It(World);
	if (It)
	{
		return *It;
	}

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride =
		ESpawnActorCollisionHandlingMethod::
			AlwaysSpawn;
	return World->SpawnActor<AJoA_Arrow>(
		AJoA_Arrow::StaticClass(),
		FTransform(FVector(0.f, 0.f, -5000.f)),
		Params);
}

void AJoA_Arrow::Launch(
	const FVector& From,
	const FVector& To,
	bool bHaltAtEnd)
{
	UStaticMesh* Body =
		UJoA_CookedContent::LoadProp(
			this,
			TEXT("arrow_bow"));
	if (Mesh && Body)
	{
		if (Mesh->GetStaticMesh() != Body)
		{
			Mesh->SetStaticMesh(Body);
			Mesh->SetRelativeScale3D(FVector(0.5f));
		}

		Mesh->SetRelativeRotation(
			FRotator(0.f, -90.f, 0.f));
	}

	FlightFrom = From;
	FlightTo = To;
	bStopAtEnd = bHaltAtEnd;
	const float Dist = FVector::Dist(From, To);
	FlightSeconds = FMath::Max(
		0.12f,
		Dist / FlightSpeed);
	FlightElapsed = 0.f;
	bFlying = true;

	const FVector Delta = To - From;
	if (!Delta.IsNearlyZero())
	{
		SetActorRotation(Delta.Rotation());
	}
	SetActorLocation(From);
	SetActorHiddenInGame(false);
	if (Mesh)
	{
		Mesh->SetHiddenInGame(false);
		Mesh->SetVisibility(true);
	}
	SetActorTickEnabled(true);
}

void AJoA_Arrow::HideShot()
{
	bFlying = false;
	SetActorTickEnabled(false);
	SetActorHiddenInGame(true);
	if (Mesh)
	{
		Mesh->SetHiddenInGame(true);
		Mesh->SetVisibility(false);
	}
	SetActorLocation(FVector(0.f, 0.f, -5000.f));
}

void AJoA_Arrow::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!bFlying)
	{
		SetActorTickEnabled(false);
		return;
	}

	FlightElapsed += DeltaSeconds;
	const float Alpha = FMath::Clamp(
		FlightElapsed / FlightSeconds,
		0.f,
		1.f);
	SetActorLocation(FMath::Lerp(
		FlightFrom,
		FlightTo,
		Alpha));

	const bool bArrived = Alpha >= 1.f;
	if ((bStopAtEnd && bArrived) || IsOffBoard())
	{
		HideShot();
	}
}

bool AJoA_Arrow::IsOffBoard() const
{
	const AJoA_GridManager* Grid =
		AJoA_GameMode::GetGridManager(this);
	if (!Grid)
	{
		return false;
	}

	const FVector A = Grid->GetCellWorldLocation(0, 0);
	const FVector B = Grid->GetCellWorldLocation(
		GridSize - 1,
		GridSize - 1);
	const float Tile = FVector::Dist(
		A,
		Grid->GetCellWorldLocation(1, 0));
	const FVector Min(
		FMath::Min(A.X, B.X) - Tile,
		FMath::Min(A.Y, B.Y) - Tile,
		FMath::Min(A.Z, B.Z) - Tile);
	const FVector Max(
		FMath::Max(A.X, B.X) + Tile,
		FMath::Max(A.Y, B.Y) + Tile,
		FMath::Max(A.Z, B.Z) + Tile * 3.f);
	const FVector Loc = GetActorLocation();
	return Loc.X < Min.X
		|| Loc.X > Max.X
		|| Loc.Y < Min.Y
		|| Loc.Y > Max.Y
		|| Loc.Z < Min.Z
		|| Loc.Z > Max.Z;
}
