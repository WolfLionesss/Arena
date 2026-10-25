// Copyright Epic Games, Inc. All Rights Reserved.

#include "Board/JoA_Obstacle.h"

// --- Engine ---
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"

// --- GAME ---
#include "Board/JoA_Atlas.h"
#include "Match/JoA_CookedContent.h"

// --- Components ---
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"

// --- Helpers ---
#include "UObject/ConstructorHelpers.h"

AJoA_Obstacle::AJoA_Obstacle()
{
	PrimaryActorTick.bCanEverTick = false;

	/* Local visual. GridCells replicates the height. */
	bReplicates = false;
	SetReplicateMovement(false);

	SceneRoot = CreateDefaultSubobject<USceneComponent>(
		TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(
		TEXT("Mesh"));
	Mesh->SetupAttachment(SceneRoot);
	UpperMesh = CreateDefaultSubobject<UStaticMeshComponent>(
		TEXT("UpperMesh"));
	UpperMesh->SetupAttachment(SceneRoot);

	static ConstructorHelpers::FObjectFinder<UStaticMesh>
		CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	UStaticMesh* CubeAsset = nullptr;
	if (CubeMesh.Succeeded())
	{
		CubeAsset = CubeMesh.Object;
	}
	PrepareCube(Mesh, CubeAsset);
	PrepareCube(UpperMesh, CubeAsset);
	UpperMesh->SetVisibility(false);
}

void AJoA_Obstacle::PrepareCube(
	UStaticMeshComponent* Cube,
	UStaticMesh* CubeAsset)
{
	if (!Cube)
	{
		return;
	}

	/* Height lives on the cell, not this mesh. */
	Cube->SetCollisionEnabled(
		ECollisionEnabled::NoCollision);
	Cube->SetGenerateOverlapEvents(false);
	Cube->SetCastShadow(false);
	Cube->SetVisibleInRayTracing(false);
	if (CubeAsset)
	{
		Cube->SetStaticMesh(CubeAsset);
	}
}

void AJoA_Obstacle::PlaceCube(
	UStaticMeshComponent* Cube,
	float CenterZ,
	float XYScale,
	float ZScale,
	bool bShown)
{
	if (!Cube)
	{
		return;
	}

	Cube->SetRelativeScale3D(
		FVector(XYScale, XYScale, ZScale));
	Cube->SetRelativeLocation(
		FVector(0.f, 0.f, CenterZ));
	Cube->SetVisibility(bShown);
	if (!bShown)
	{
		return;
	}

	/* White tile keeps the team tint off.
	 * Row 5 is one crate face. */
	JoAAtlas::ApplyPalette(
		Cube,
		JoAAtlas::WhiteTile,
		JoAAtlas::CrateRow);
}

void AJoA_Obstacle::SpawnRemovedLayers(
	const FVector& CellLocation,
	uint8 OldHeight,
	uint8 NewHeight)
{
	if (OldHeight <= NewHeight)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	static TWeakObjectPtr<UNiagaraSystem> Cached;
	static bool bLogged = false;
	UNiagaraSystem* Chips = Cached.Get();
	if (!Chips)
	{
		Chips = UJoA_CookedContent::LoadCrateChips(this);
		if (Chips)
		{
			Cached = Chips;
		}
		else if (!bLogged)
		{
			bLogged = true;
			UE_LOG(
				LogTemp,
				Warning,
				TEXT("Missing /Game/VFX/NS_CrateChips."));
		}
	}
	if (!Chips)
	{
		return;
	}

	/* A new crate raises height. Chips only
	 * fire for a layer that was on screen. */
	const float UnitZ = WorldZForHeight(1);
	uint8 Layer = OldHeight;
	while (Layer > NewHeight)
	{
		const float CenterZ =
			UnitZ * (static_cast<float>(Layer) - 0.5f);
		const FVector At(
			CellLocation.X,
			CellLocation.Y,
			CellLocation.Z + CenterZ);
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(
			World,
			Chips,
			At);
		--Layer;
	}
}

float AJoA_Obstacle::WorldZForHeight(uint8 Height)
{
	constexpr float CubeNativeSize = 100.f;
	constexpr float HeightOneZScale = 0.6f;
	return CubeNativeSize
		* HeightOneZScale
		* static_cast<float>(Height);
}

void AJoA_Obstacle::ApplyHeight(
	uint8 Height,
	const FVector& CellLocation,
	float TileWorldSize)
{
	if (!Mesh || !UpperMesh || Height == 0)
	{
		return;
	}

	if (Height < ShownHeight)
	{
		SpawnRemovedLayers(
			CellLocation,
			ShownHeight,
			Height);
	}
	ShownHeight = Height;

	/* One mesh scaled on Z stretches the face.
	 * Height 2 is a second cube on the first. */
	constexpr float CubeNativeSize = 100.f;
	constexpr float Footprint = 0.7f;
	const float UnitZ = WorldZForHeight(1);
	const float ZScale = UnitZ / CubeNativeSize;
	const float XYScale =
		(TileWorldSize * Footprint) / CubeNativeSize;

	SetActorScale3D(FVector::OneVector);
	SetActorLocation(CellLocation);
	PlaceCube(
		Mesh,
		UnitZ * 0.5f,
		XYScale,
		ZScale,
		true);
	PlaceCube(
		UpperMesh,
		UnitZ * 1.5f,
		XYScale,
		ZScale,
		Height >= 2);
}

void AJoA_Obstacle::RemoveFromBoard(
	const FVector& CellLocation)
{
	SpawnRemovedLayers(
		CellLocation,
		ShownHeight,
		0);
	ShownHeight = 0;
}
