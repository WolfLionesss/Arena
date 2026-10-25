// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

// --- Engine ---
#include "GameFramework/Actor.h"

#include "JoA_GridManager.generated.h"

class AJoA_Obstacle;
class UStaticMeshComponent;

/** Chessboard is always 8x8.
 * Coordinates are 0-indexed (A1 = X0,Y0). */
inline constexpr int32 GridSize = 8;
inline constexpr int32 GridCellCount = GridSize * GridSize;

/**
 * One square on the arena board.
 * X/Y are chess indices 0-7.
 * ObstacleHeight is 0 (clear), 1, or 2.
 * Replicated inside GridCells.
 */
USTRUCT(BlueprintType)
struct JOA_ARENA_API FGridCell
{
	GENERATED_BODY()

	/** File index: 0 = A ... 7 = H. */
	UPROPERTY(
		EditAnywhere,
		BlueprintReadWrite,
		Category = "Grid")
	uint8 X = 0;

	/** Rank index: 0 = 1 ... 7 = 8. */
	UPROPERTY(
		EditAnywhere,
		BlueprintReadWrite,
		Category = "Grid")
	uint8 Y = 0;

	/** 0 = no obstacle, 1 or 2 = blocking height. */
	UPROPERTY(
		EditAnywhere,
		BlueprintReadWrite,
		Category = "Grid",
		meta = (ClampMin = "0", ClampMax = "2"))
	uint8 ObstacleHeight = 0;

	FGridCell() = default;

	FGridCell(uint8 InX, uint8 InY, uint8 InObstacleHeight)
		: X(FMath::Min<uint8>(InX, GridSize - 1))
		, Y(FMath::Min<uint8>(InY, GridSize - 1))
		, ObstacleHeight(FMath::Min<uint8>(InObstacleHeight, 2))
	{
	}

	bool operator==(const FGridCell& Other) const
	{
		return X == Other.X
			&& Y == Other.Y
			&& ObstacleHeight == Other.ObstacleHeight;
	}

	bool operator!=(const FGridCell& Other) const
	{
		return !(*this == Other);
	}

	/** 180° symmetry through the center.
	 * (X,Y) becomes (7-X, 7-Y).
	 * 4D (3,3) matches 5E (4,4). */
	FIntPoint GetMirroredCoordinates() const
	{
		return FIntPoint(
			GridSize - 1 - X,
			GridSize - 1 - Y);
	}

	/** Algebraic file and rank, e.g. D4.
	 * User-facing 4D is rank then file. */
	FString ToAlgebraic() const
	{
		const TCHAR FileChar =
			static_cast<TCHAR>(TEXT('A') + X);
		return FString::Printf(
			TEXT("%c%d"),
			FileChar,
			Y + 1);
	}

	int32 ToLinearIndex() const
	{
		return static_cast<int32>(Y) * GridSize
			+ static_cast<int32>(X);
	}

	/** Compact Iris/legacy net serialize.
	 * 3+3+2 bits per cell. */
	bool NetSerialize(
		FArchive& Ar,
		UPackageMap* Map,
		bool& bOutSuccess);
};

template <>
struct TStructOpsTypeTraits<FGridCell>
	: public TStructOpsTypeTraitsBase2<FGridCell>
{
	enum
	{
		WithNetSerializer = true,
		WithIdenticalViaEquality = true,
		WithCopy = true,
	};
};

/**
 * Server-authoritative 8x8 chessboard.
 * Grid cell data replicates via Iris.
 * Visuals rebuild locally from GridCells.
 */
UCLASS(Blueprintable)
class JOA_ARENA_API AJoA_GridManager : public AActor
{
	GENERATED_BODY()

public:
	AJoA_GridManager();

	virtual void BeginPlay() override;
	virtual void EndPlay(
		const EEndPlayReason::Type EndPlayReason
	) override;
	virtual void OnConstruction(
		const FTransform& Transform) override;
	virtual void GetLifetimeReplicatedProps(
		TArray<FLifetimeProperty>& OutLifetimeProps
	) const override;

#if UE_WITH_IRIS
	/**
	 * Iris registers replicated properties
	 * as fragments. Super keeps the actor
	 * protocol and this class together.
	 */
	virtual void RegisterReplicationFragments(
		UE::Net::FFragmentRegistrationContext& Context,
		UE::Net::EFragmentRegistrationFlags
			RegistrationFlags) override;
#endif

	/** Builds 64 cells and the floor.
	 * The server seeds obstacle data. */
	UFUNCTION(BlueprintCallable, Category = "Grid")
	void GenerateGrid();

	/**
	 * Server-only. Each occupied cell gets
	 * the same height on the mirrored square.
	 * 4D (X=3, Y=3) copies onto 5E (X=4, Y=4).
	 */
	UFUNCTION(BlueprintCallable, Category = "Grid")
	void GenerateMirroredObstacles();

	/* Menu count and seed replace BeginPlay.
	 * Each pair is mirrored, so both sides match.
	 * The lineup then clears the home squares. */
	void ReseedOpeningObstacles(
		int32 PairCount,
		int32 RandomSeed);

	UFUNCTION(BlueprintPure, Category = "Grid")
	bool GetCell(
		int32 InX,
		int32 InY,
		FGridCell& OutCell) const;

	UFUNCTION(BlueprintCallable, Category = "Grid")
	void SetObstacleHeight(
		int32 InX,
		int32 InY,
		uint8 InHeight);

	UFUNCTION(BlueprintPure, Category = "Grid")
	int32 GetCellIndex(int32 InX, int32 InY) const;

	UFUNCTION(BlueprintPure, Category = "Grid")
	FVector GetCellWorldLocation(
		int32 InX,
		int32 InY) const;

	/* Cell centre, raised to the obstacle top
	 * so a figure's feet sit on it. */
	UFUNCTION(BlueprintPure, Category = "Grid")
	FVector GetCellStandWorldLocation(
		int32 InX,
		int32 InY) const;

protected:
	UFUNCTION()
	void OnRep_GridCells();

	UFUNCTION()
	void OnRep_TileWorldSize();

	void InitializeGridData();
	void UpdateBoardVisual();
	void SyncObstacleActors();
	void SyncObstacleActor(int32 Index);
	void ClearObstacleActors();
	bool ShouldShowObstacleActors() const;
	void MarkGridCellsDirty();
	bool IsValidCoordinate(int32 InX, int32 InY) const;
	void SeedHalfBoardObstacles();

	UPROPERTY(VisibleAnywhere, Category = "Grid")
	TObjectPtr<USceneComponent> SceneRoot;

	/** Checker plane. One mesh for the whole board. */
	UPROPERTY(VisibleAnywhere, Category = "Grid|Visual")
	TObjectPtr<UStaticMeshComponent> BoardMesh;

	/** Local cubes. Height 0 destroys the actor. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<AJoA_Obstacle>> ObstacleActors;

	/**
	 * Authoritative board state. Iris replicates
	 * this array to every connection. Clients
	 * never mutate it.
	 */
	UPROPERTY(
		VisibleAnywhere,
		BlueprintReadOnly,
		ReplicatedUsing = OnRep_GridCells,
		Category = "Grid")
	TArray<FGridCell> GridCells;

	UPROPERTY(
		EditAnywhere,
		BlueprintReadOnly,
		ReplicatedUsing = OnRep_TileWorldSize,
		Category = "Grid",
		meta = (ClampMin = "1.0"))
	float TileWorldSize = 100.f;

	UPROPERTY(
		EditAnywhere,
		BlueprintReadOnly,
		Category = "Grid|Obstacles",
		meta = (ClampMin = "0", ClampMax = "10"))
	int32 ObstaclePairCount = 6;

	UPROPERTY(
		EditAnywhere,
		BlueprintReadOnly,
		Category = "Grid|Obstacles")
	int32 ObstacleRandomSeed = 1337;

	UPROPERTY(
		EditAnywhere,
		BlueprintReadOnly,
		Category = "Grid|Obstacles")
	bool bGenerateObstaclesOnBeginPlay = true;
};
