// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

// --- Engine ---
#include "GameFramework/Actor.h"

#include "JoA_Obstacle.generated.h"

class UStaticMeshComponent;

/**
 * Local cube for one blocked cell.
 * GridCells stores the height. This actor
 * is not replicated. Height 0 destroys it.
 */
UCLASS()
class JOA_ARENA_API AJoA_Obstacle : public AActor
{
	GENERATED_BODY()

public:
	AJoA_Obstacle();

	/* Height 2 stacks a second cube.
	 * Feet still use the summed height. */
	void ApplyHeight(
		uint8 Height,
		const FVector& CellLocation,
		float TileWorldSize);

	/* World Z from the cell to the cube top.
	 * A figure's feet sit here. */
	static float WorldZForHeight(uint8 Height);

	/* Height hit 0. Chips, then the caller
	 * destroys this actor. */
	void RemoveFromBoard(const FVector& CellLocation);

protected:
	UPROPERTY(VisibleAnywhere, Category = "Obstacle")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, Category = "Obstacle")
	TObjectPtr<UStaticMeshComponent> Mesh;

	UPROPERTY(VisibleAnywhere, Category = "Obstacle")
	TObjectPtr<UStaticMeshComponent> UpperMesh;

private:
	void PrepareCube(
		UStaticMeshComponent* Cube,
		UStaticMesh* CubeAsset);

	void PlaceCube(
		UStaticMeshComponent* Cube,
		float CenterZ,
		float XYScale,
		float ZScale,
		bool bShown);

	void SpawnRemovedLayers(
		const FVector& CellLocation,
		uint8 OldHeight,
		uint8 NewHeight);

	/* Drawn height. 0 means this actor
	 * has not shown a crate yet. */
	uint8 ShownHeight = 0;
};
