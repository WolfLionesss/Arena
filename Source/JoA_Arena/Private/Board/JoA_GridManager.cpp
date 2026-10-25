// Copyright Epic Games, Inc. All Rights Reserved.

#include "Board/JoA_GridManager.h"

// --- Engine ---
#include "Engine/StaticMesh.h"
#include "Engine/World.h"

// --- GAME ---
#include "Board/JoA_Atlas.h"
#include "Board/JoA_Obstacle.h"
#include "Match/JoA_GameMode.h"
#include "Match/JoA_GameInstance.h"
#include "Player/Units/JoA_FigureRegistry.h"

// --- Components ---
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"

// --- Helpers ---
#include "UObject/ConstructorHelpers.h"

// --- Net ---
#include "Net/Core/PushModel/PushModel.h"
#include "Net/UnrealNetwork.h"
#include "UObject/CoreNet.h"

#if UE_WITH_IRIS
#include "Iris/ReplicationSystem/ReplicationFragment.h"
#endif


bool FGridCell::NetSerialize(
	FArchive& Ar,
	UPackageMap* Map,
	bool& bOutSuccess)
{
	(void)Map;
	Ar.SerializeBits(&X, 3);
	Ar.SerializeBits(&Y, 3);
	Ar.SerializeBits(&ObstacleHeight, 2);

	if (Ar.IsLoading())
	{
		X = FMath::Min<uint8>(X, GridSize - 1);
		Y = FMath::Min<uint8>(Y, GridSize - 1);
		ObstacleHeight = FMath::Min<uint8>(ObstacleHeight, 2);
	}

	bOutSuccess = true;
	return true;
}

AJoA_GridManager::AJoA_GridManager()
{
	PrimaryActorTick.bCanEverTick = false;

	/* Both players share one board.
	 * The actor stays relevant to each. */
	bReplicates = true;
	bAlwaysRelevant = true;
	bReplicateUsingRegisteredSubObjectList = true;
	SetReplicateMovement(false);
	SetNetUpdateFrequency(2.f);
	SetMinNetUpdateFrequency(1.f);

	SceneRoot = CreateDefaultSubobject<USceneComponent>(
		TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	BoardMesh =
		CreateDefaultSubobject<UStaticMeshComponent>(
			TEXT("BoardMesh"));
	BoardMesh->SetupAttachment(SceneRoot);
	BoardMesh->SetCollisionEnabled(
		ECollisionEnabled::NoCollision);
	BoardMesh->SetGenerateOverlapEvents(false);
	BoardMesh->SetCastShadow(false);
	BoardMesh->SetVisibleInRayTracing(false);
	/* Local visual. The board state is GridCells. */
	BoardMesh->SetIsReplicated(false);

	static ConstructorHelpers::FObjectFinder<UStaticMesh>
		PlaneMesh(
			TEXT("/Engine/BasicShapes/Plane.Plane"));
	if (PlaneMesh.Succeeded())
	{
		BoardMesh->SetStaticMesh(PlaneMesh.Object);
	}

	ObstacleActors.SetNum(GridCellCount);
}

void AJoA_GridManager::OnConstruction(
	const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	if (GridCells.Num() != GridCellCount)
	{
		InitializeGridData();
	}

	UpdateBoardVisual();
	SyncObstacleActors();
}

void AJoA_GridManager::BeginPlay()
{
	/* Size the board first. GenerateGrid after
	 * Super wiped heights set from Blueprint. */
	if (HasAuthority() && GridCells.Num() != GridCellCount)
	{
		InitializeGridData();
		MarkGridCellsDirty();
	}

	if (HasAuthority() && bGenerateObstaclesOnBeginPlay)
	{
		bool bHasObstacle = false;
		for (const FGridCell& Cell : GridCells)
		{
			if (Cell.ObstacleHeight > 0)
			{
				bHasObstacle = true;
				break;
			}
		}

		if (!bHasObstacle)
		{
			SeedHalfBoardObstacles();
			GenerateMirroredObstacles();
		}
	}

	Super::BeginPlay();
	UpdateBoardVisual();
	SyncObstacleActors();
}

void AJoA_GridManager::GetLifetimeReplicatedProps(
	TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	/* Push model. UHT still requires this
	 * registration for replicated properties. */
	FDoRepLifetimeParams Params;
	Params.bIsPushBased = true;
	DOREPLIFETIME_WITH_PARAMS_FAST(
		AJoA_GridManager,
		GridCells,
		Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(
		AJoA_GridManager,
		TileWorldSize,
		Params);
}

#if UE_WITH_IRIS
void AJoA_GridManager::RegisterReplicationFragments(
	UE::Net::FFragmentRegistrationContext& Context,
	UE::Net::EFragmentRegistrationFlags RegistrationFlags)
{
	/* Super builds Iris fragments from
	 * replicated properties on this class. */
	Super::RegisterReplicationFragments(
		Context,
		RegistrationFlags);
}
#endif

void AJoA_GridManager::GenerateGrid()
{
	InitializeGridData();

	if (HasAuthority())
	{
		MarkGridCellsDirty();
	}

	UpdateBoardVisual();
	SyncObstacleActors();
}

void AJoA_GridManager::GenerateMirroredObstacles()
{
	if (!HasAuthority())
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("AJoA_GridManager::")
			TEXT("GenerateMirroredObstacles skipped")
			TEXT(" on client (server-authoritative)."));
		return;
	}

	if (GridCells.Num() != GridCellCount)
	{
		InitializeGridData();
	}

	for (int32 Index = 0; Index < GridCells.Num(); ++Index)
	{
		const FGridCell& Cell = GridCells[Index];
		if (Cell.ObstacleHeight == 0)
		{
			continue;
		}

		const FIntPoint Mirror = Cell.GetMirroredCoordinates();
		const int32 MirrorIndex =
			GetCellIndex(Mirror.X, Mirror.Y);
		if (!GridCells.IsValidIndex(MirrorIndex))
		{
			continue;
		}

		/* Same height on the counterpart.
		 * An even board has no center cell. */
		GridCells[MirrorIndex].X =
			static_cast<uint8>(Mirror.X);
		GridCells[MirrorIndex].Y =
			static_cast<uint8>(Mirror.Y);
		GridCells[MirrorIndex].ObstacleHeight =
			Cell.ObstacleHeight;
	}

	MarkGridCellsDirty();
	SyncObstacleActors();
}

void AJoA_GridManager::ReseedOpeningObstacles(
	int32 PairCount,
	int32 RandomSeed)
{
	if (!HasAuthority())
	{
		return;
	}

	ObstaclePairCount = FMath::Clamp(
		PairCount,
		UJoA_GameInstance::MinOpeningObstaclePairs,
		UJoA_GameInstance::MaxOpeningObstaclePairs);
	ObstacleRandomSeed = FMath::Clamp(
		RandomSeed,
		UJoA_GameInstance::MinOpeningObstacleSeed,
		UJoA_GameInstance::MaxOpeningObstacleSeed);
	/* BeginPlay already drew the default
	 * crates. Clearing them here is not a
	 * player break, so it must not spawn chips. */
	ClearObstacleActors();
	SeedHalfBoardObstacles();
	GenerateMirroredObstacles();
}

bool AJoA_GridManager::GetCell(
	int32 InX,
	int32 InY,
	FGridCell& OutCell) const
{
	const int32 Index = GetCellIndex(InX, InY);
	if (!GridCells.IsValidIndex(Index))
	{
		return false;
	}

	OutCell = GridCells[Index];
	return true;
}

void AJoA_GridManager::SetObstacleHeight(
	int32 InX,
	int32 InY,
	uint8 InHeight)
{
	if (!HasAuthority())
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("SetObstacleHeight has no authority."));
		return;
	}

	const int32 Index = GetCellIndex(InX, InY);
	if (!GridCells.IsValidIndex(Index))
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("SetObstacleHeight bad cell %d,%d."),
			InX,
			InY);
		return;
	}

	const uint8 Height = FMath::Min<uint8>(InHeight, 2);
	GridCells[Index].ObstacleHeight = Height;
	/* MarkGridCellsDirty already tells the HUD.
	 * A second notify rebuilt the rows again. */
	SyncObstacleActor(Index);
	MarkGridCellsDirty();

	if (UWorld* World = GetWorld())
	{
		if (AJoA_GameMode* Mode =
			World->GetAuthGameMode<AJoA_GameMode>())
		{
			Mode->EvaluateBoardDraw();
		}
	}

	UE_LOG(
		LogTemp,
		Log,
		TEXT("Obstacle %d,%d height %d."),
		InX,
		InY,
		static_cast<int32>(Height));
}

int32 AJoA_GridManager::GetCellIndex(
	int32 InX,
	int32 InY) const
{
	if (!IsValidCoordinate(InX, InY))
	{
		return INDEX_NONE;
	}

	return InY * GridSize + InX;
}

FVector AJoA_GridManager::GetCellWorldLocation(
	int32 InX,
	int32 InY) const
{
	const float OriginOffset =
		(GridSize - 1) * 0.5f * TileWorldSize;
	const FVector Local(
		static_cast<float>(InX) * TileWorldSize
			- OriginOffset,
		static_cast<float>(InY) * TileWorldSize
			- OriginOffset,
		0.f);
	return GetActorTransform().TransformPosition(Local);
}

FVector AJoA_GridManager::GetCellStandWorldLocation(
	int32 InX,
	int32 InY) const
{
	FVector Location = GetCellWorldLocation(InX, InY);
	FGridCell Cell;
	if (GetCell(InX, InY, Cell))
	{
		Location.Z += AJoA_Obstacle::WorldZForHeight(
			Cell.ObstacleHeight);
	}

	return Location;
}

void AJoA_GridManager::OnRep_GridCells()
{
	UpdateBoardVisual();
	SyncObstacleActors();

	// A cleared obstacle opens a step in the controls.
	if (UJoA_FigureRegistry* Registry =
		UJoA_FigureRegistry::Get(this))
	{
		Registry->NotifyBoardChanged();
	}
}

void AJoA_GridManager::OnRep_TileWorldSize()
{
	UpdateBoardVisual();
	SyncObstacleActors();
}

void AJoA_GridManager::InitializeGridData()
{
	GridCells.Reset(GridCellCount);
	GridCells.SetNum(GridCellCount);

	for (int32 Y = 0; Y < GridSize; ++Y)
	{
		for (int32 X = 0; X < GridSize; ++X)
		{
			const int32 Index = Y * GridSize + X;
			GridCells[Index] = FGridCell(
				static_cast<uint8>(X),
				static_cast<uint8>(Y),
				0);
		}
	}
}

void AJoA_GridManager::EndPlay(
	const EEndPlayReason::Type EndPlayReason)
{
	const UWorld* World = GetWorld();
	const bool bTearingDown =
		World && World->bIsTearingDown;
	if (!bTearingDown)
	{
		ClearObstacleActors();
	}

	Super::EndPlay(EndPlayReason);
}

void AJoA_GridManager::UpdateBoardVisual()
{
	if (!BoardMesh)
	{
		return;
	}

	if (!BoardMesh->GetStaticMesh())
	{
		UStaticMesh* PlaneMesh = LoadObject<UStaticMesh>(
			nullptr,
			TEXT("/Engine/BasicShapes/Plane.Plane"));
		if (PlaneMesh)
		{
			BoardMesh->SetStaticMesh(PlaneMesh);
		}
	}

	/* Engine plane is 100 units across. */
	constexpr float PlaneNativeSize = 100.f;
	const float BoardSize =
		static_cast<float>(GridSize) * TileWorldSize;
	const float UniformScale =
		BoardSize / PlaneNativeSize;
	const FVector BoardScale(
		UniformScale,
		UniformScale,
		1.f);
	/* Sit just under the cells to avoid z-fight. */
	const FVector BoardOffset(0.f, 0.f, -1.f);
	BoardMesh->SetRelativeScale3D(BoardScale);
	BoardMesh->SetRelativeLocation(BoardOffset);
	JoAAtlas::ApplyTile(
		BoardMesh,
		JoAAtlas::CheckerTile);
}

void AJoA_GridManager::SyncObstacleActors()
{
	if (ObstacleActors.Num() != GridCellCount)
	{
		ObstacleActors.SetNum(GridCellCount);
	}

	for (int32 Index = 0; Index < GridCells.Num(); ++Index)
	{
		SyncObstacleActor(Index);
	}
}

void AJoA_GridManager::SyncObstacleActor(int32 Index)
{
	if (!GridCells.IsValidIndex(Index))
	{
		return;
	}

	if (ObstacleActors.Num() != GridCellCount)
	{
		ObstacleActors.SetNum(GridCellCount);
	}

	const FGridCell& Cell = GridCells[Index];
	TObjectPtr<AJoA_Obstacle>& Obstacle =
		ObstacleActors[Index];

	if (Cell.ObstacleHeight == 0)
	{
		if (IsValid(Obstacle))
		{
			const FVector CellLocation =
				GetCellWorldLocation(Cell.X, Cell.Y);
			Obstacle->RemoveFromBoard(CellLocation);
			Obstacle->Destroy();
		}
		Obstacle = nullptr;
		return;
	}

	if (!IsValid(Obstacle))
	{
		if (!ShouldShowObstacleActors())
		{
			return;
		}

		UWorld* World = GetWorld();
		if (!World)
		{
			return;
		}

		FActorSpawnParameters Params;
		Params.Owner = this;
		Params.ObjectFlags = RF_Transient;
		Params.SpawnCollisionHandlingOverride =
			ESpawnActorCollisionHandlingMethod::
				AlwaysSpawn;
		Obstacle = World->SpawnActor<AJoA_Obstacle>(
			AJoA_Obstacle::StaticClass(),
			FTransform::Identity,
			Params);
	}

	if (!Obstacle)
	{
		return;
	}

	const FVector CellLocation =
		GetCellWorldLocation(Cell.X, Cell.Y);
	Obstacle->ApplyHeight(
		Cell.ObstacleHeight,
		CellLocation,
		TileWorldSize);
}

void AJoA_GridManager::ClearObstacleActors()
{
	const int32 ActorCount = ObstacleActors.Num();
	for (int32 Index = 0; Index < ActorCount; ++Index)
	{
		TObjectPtr<AJoA_Obstacle>& Obstacle =
			ObstacleActors[Index];
		if (IsValid(Obstacle))
		{
			Obstacle->Destroy();
		}
		Obstacle = nullptr;
	}
}

bool AJoA_GridManager::ShouldShowObstacleActors() const
{
	const UWorld* World = GetWorld();
	if (!World || !World->IsGameWorld())
	{
		return false;
	}

	return GetNetMode() != NM_DedicatedServer;
}

void AJoA_GridManager::MarkGridCellsDirty()
{
	/* Push-model dirty bit. Iris sends GridCells
	 * after the server changes the board. */
	MARK_PROPERTY_DIRTY_FROM_NAME(
		AJoA_GridManager,
		GridCells,
		this);
	ForceNetUpdate();

	if (UJoA_FigureRegistry* Registry =
		UJoA_FigureRegistry::Get(this))
	{
		Registry->NotifyBoardChanged();
	}
}

bool AJoA_GridManager::IsValidCoordinate(
	int32 InX,
	int32 InY) const
{
	return InX >= 0 && InX < GridSize
		&& InY >= 0 && InY < GridSize;
}

void AJoA_GridManager::SeedHalfBoardObstacles()
{
	if (!HasAuthority())
	{
		return;
	}

	if (GridCells.Num() != GridCellCount)
	{
		InitializeGridData();
	}

	for (FGridCell& Cell : GridCells)
	{
		Cell.ObstacleHeight = 0;
	}

	FRandomStream Stream(ObstacleRandomSeed);
	int32 PlacedPairs = 0;
	int32 Safety = 0;

	while (PlacedPairs < ObstaclePairCount && Safety < 256)
	{
		++Safety;
		const int32 X = Stream.RandRange(0, GridSize - 1);
		const int32 Y = Stream.RandRange(0, GridSize - 1);
		const int32 Index = GetCellIndex(X, Y);
		const int32 MirrorIndex = GetCellIndex(
			GridSize - 1 - X,
			GridSize - 1 - Y);

		if (Index == INDEX_NONE
			|| MirrorIndex == INDEX_NONE
			|| Index == MirrorIndex)
		{
			continue;
		}

		/* Seed the lower index of each pair.
		 * Mirroring copies the other half. */
		if (Index > MirrorIndex)
		{
			continue;
		}

		if (GridCells[Index].ObstacleHeight != 0)
		{
			continue;
		}

		const uint8 Height =
			static_cast<uint8>(Stream.RandRange(1, 2));
		GridCells[Index].ObstacleHeight = Height;
		++PlacedPairs;
	}

	MarkGridCellsDirty();
}
