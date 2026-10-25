// Copyright Epic Games, Inc. All Rights Reserved.

#include "Board/JoA_Flag.h"

// --- Engine ---
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"

// --- GAME ---
#include "Match/JoA_GameMode.h"
#include "Board/JoA_Atlas.h"
#include "Board/JoA_GridManager.h"
#include "JoA_Arena.h"
#include "Player/JoA_Nation.h"
#include "Player/Units/JoA_Figure.h"
#include "Player/Units/JoA_FigureRegistry.h"

// --- Components ---
#include "Components/SceneComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"

// --- Helpers ---
#include "UObject/ConstructorHelpers.h"

// --- Net ---
#include "Net/UnrealNetwork.h"
#include "Net/Core/PushModel/PushModel.h"

#if UE_WITH_IRIS
#include "Iris/ReplicationSystem/ReplicationFragment.h"
#endif

namespace
{
	/* Left-hand fallback when the hand bone
	 * is missing. Old (16,44,90) sat at the
	 * shoulder and missed the palm. */
	const FVector CarryMeshOffset(-12.f, 8.f, 42.f);

	/* Left hand carries the flag so the
	 * stowed left-hand weapon stays free. */
	const FName HandBoneNames[] =
	{
		TEXT("Flag"),
		TEXT("handslot_l"),
		TEXT("hand_l"),
		TEXT("Hand_L"),
		TEXT("LeftHand")
	};

	FName FindHandAttachName(
		const USkeletalMeshComponent* Body)
	{
		if (!Body)
		{
			return NAME_None;
		}

		for (const FName& Name : HandBoneNames)
		{
			if (Body->DoesSocketExist(Name)
				|| Body->GetBoneIndex(Name)
					!= INDEX_NONE)
			{
				return Name;
			}
		}

		return NAME_None;
	}

	FVector FindHandCarryLocation(
		const USkeletalMeshComponent* Body)
	{
		if (!Body)
		{
			return CarryMeshOffset;
		}

		const FName Socket = FindHandAttachName(Body);
		if (Socket.IsNone())
		{
			return CarryMeshOffset;
		}

		/* Prefer the live bone pose so the pole
		 * sits in the palm. Flag is a socket on
		 * handslot_l, so fall through to socket. */
		const int32 BoneIndex =
			Body->GetBoneIndex(Socket);
		if (BoneIndex != INDEX_NONE)
		{
			const FTransform BoneWorld =
				Body->GetBoneTransform(
					BoneIndex);
			return Body->GetComponentTransform()
				.InverseTransformPosition(
					BoneWorld.GetLocation());
		}

		return Body->GetSocketTransform(
			Socket,
			RTS_Component).GetLocation();
	}
}

AJoA_Flag::AJoA_Flag()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	bAlwaysRelevant = true;
	bOnlyRelevantToOwner = false;
	bNetUseOwnerRelevancy = false;
	SetReplicateMovement(false);
	SetNetUpdateFrequency(10.f);
	SetMinNetUpdateFrequency(2.f);

	SceneRoot = CreateDefaultSubobject<USceneComponent>(
		TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	PoleMesh = CreateDefaultSubobject<UStaticMeshComponent>(
		TEXT("PoleMesh"));
	PoleMesh->SetupAttachment(SceneRoot);
	PoleMesh->SetCollisionEnabled(
		ECollisionEnabled::NoCollision);
	PoleMesh->SetCastShadow(false);
	PoleMesh->SetVisibleInRayTracing(false);

	BannerMesh =
		CreateDefaultSubobject<UStaticMeshComponent>(
			TEXT("BannerMesh"));
	BannerMesh->SetupAttachment(SceneRoot);
	BannerMesh->SetCollisionEnabled(
		ECollisionEnabled::NoCollision);
	BannerMesh->SetCastShadow(false);
	BannerMesh->SetVisibleInRayTracing(false);

	static ConstructorHelpers::FObjectFinder<UStaticMesh>
		WoodMesh(TEXT("/Game/_Assets/Wood.Wood"));
	if (WoodMesh.Succeeded())
	{
		PoleAsset = WoodMesh.Object;
	}
	static ConstructorHelpers::FObjectFinder<UStaticMesh>
		BannerAssetFinder(
			TEXT("/Game/_Assets/Banner.Banner"));
	if (BannerAssetFinder.Succeeded())
	{
		BannerAsset = BannerAssetFinder.Object;
	}
}

void AJoA_Flag::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	EnsureReplication();
}

void AJoA_Flag::PostInitializeComponents()
{
	Super::PostInitializeComponents();
	EnsureReplication();

	/* BP_Flag SCS may add DefaultSceneRoot and
	 * steal the actor root. Attach uses the root,
	 * so the C++ SceneRoot must stay it. */
	if (SceneRoot && RootComponent != SceneRoot)
	{
		SceneRoot->DetachFromComponent(
			FDetachmentTransformRules::
				KeepWorldTransform);
		SetRootComponent(SceneRoot);
	}
}

bool AJoA_Flag::IsNetRelevantFor(
	const AActor* RealViewer,
	const AActor* ViewTarget,
	const FVector& SrcLocation) const
{
	(void)RealViewer;
	(void)ViewTarget;
	(void)SrcLocation;
	return true;
}

void AJoA_Flag::EnsureReplication()
{
	bAlwaysRelevant = true;
	bOnlyRelevantToOwner = false;
	bNetUseOwnerRelevancy = false;
	SetReplicateMovement(false);
	SetNetUpdateFrequency(10.f);
	SetMinNetUpdateFrequency(2.f);
	if (HasAuthority())
	{
		SetReplicates(true);
	}
}

void AJoA_Flag::GetLifetimeReplicatedProps(
	TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	// Push model
	FDoRepLifetimeParams Params;
	Params.bIsPushBased = true;
	DOREPLIFETIME_WITH_PARAMS_FAST(
		AJoA_Flag,
		OwningPlayerIndex,
		Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(
		AJoA_Flag,
		CellX,
		Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(
		AJoA_Flag,
		CellY,
		Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(
		AJoA_Flag,
		Carrier,
		Params);
}

#if UE_WITH_IRIS
void AJoA_Flag::RegisterReplicationFragments(
	UE::Net::FFragmentRegistrationContext& Context,
	UE::Net::EFragmentRegistrationFlags RegistrationFlags)
{
	Super::RegisterReplicationFragments(
		Context,
		RegistrationFlags);
}
#endif

void AJoA_Flag::BeginPlay()
{
	Super::BeginPlay();
	EnsureReplication();

	if (PoleAsset)
	{
		PoleMesh->SetStaticMesh(PoleAsset);
	}
	if (BannerAsset)
	{
		BannerMesh->SetStaticMesh(BannerAsset);
	}

	ApplyColors();
	ApplyCarryVisual();

	UE_LOG(
		LogJoA_Net,
		Log,
		TEXT("Flag BeginPlay auth=%d seat=%d "
			"cell=%d,%d carrier=%s"),
		HasAuthority() ? 1 : 0,
		OwningPlayerIndex,
		CellX,
		CellY,
		*GetNameSafe(Carrier));
}

void AJoA_Flag::SetOwningPlayerIndex(int32 NewPlayerIndex)
{
	if (!HasAuthority() ||
		OwningPlayerIndex == NewPlayerIndex)
	{
		return;
	}

	OwningPlayerIndex = NewPlayerIndex;
	MARK_PROPERTY_DIRTY_FROM_NAME(
		AJoA_Flag,
		OwningPlayerIndex,
		this);
	ForceNetUpdate();
	ApplyColors();
}

void AJoA_Flag::OnRep_OwningPlayerIndex()
{
	ApplyColors();
}

void AJoA_Flag::OnRep_Carrier()
{
	ApplyCarryVisual();
	if (UJoA_FigureRegistry* Registry =
		UJoA_FigureRegistry::Get(this))
	{
		Registry->NotifyBoardChanged();
	}
}

void AJoA_Flag::OnRep_BoardCell()
{
	ApplyCarryVisual();
}

void AJoA_Flag::AttachToHolder(AJoA_Figure* Figure)
{
	if (!IsValid(Figure))
	{
		PlaceOnBoard();
		return;
	}

	USkeletalMeshComponent* Body =
		Figure->GetBodyMesh();
	USceneComponent* Parent = Body
		? static_cast<USceneComponent*>(Body)
		: Figure->GetRootComponent();

	/* Attach to the mesh, not a hand bone. A bone
	 * would tilt the pole. The mesh already turns
	 * with Facing, so the banner stays with it. */
	AttachToComponent(
		Parent,
		FAttachmentTransformRules::
			SnapToTargetNotIncludingScale);

	const FVector RelLoc(48.f, -87.f, 30.f);
	const FRotator CarryRot(-75.f, 90.f, 0.f);

	SetActorRelativeLocation(RelLoc);
	SetActorRelativeRotation(CarryRot);
}

void AJoA_Flag::SetBoardCell(int32 InX, int32 InY)
{
	if (!HasAuthority())
	{
		return;
	}

	const int32 ClampedX =
		FMath::Clamp(InX, 0, GridSize - 1);
	const int32 ClampedY =
		FMath::Clamp(InY, 0, GridSize - 1);
	if (CellX == ClampedX && CellY == ClampedY)
	{
		return;
	}

	CellX = ClampedX;
	CellY = ClampedY;
	MARK_PROPERTY_DIRTY_FROM_NAME(
		AJoA_Flag,
		CellX,
		this);
	MARK_PROPERTY_DIRTY_FROM_NAME(
		AJoA_Flag,
		CellY,
		this);
	ForceNetUpdate();
}

void AJoA_Flag::PickupBy(AJoA_Figure* Figure)
{
	if (!HasAuthority() || !IsValid(Figure) || Carrier)
	{
		return;
	}

	Carrier = Figure;
	MARK_PROPERTY_DIRTY_FROM_NAME(
		AJoA_Flag,
		Carrier,
		this);
	SetBoardCell(Figure->CellX, Figure->CellY);
	ApplyCarryVisual();
	ForceNetUpdate();
}

void AJoA_Flag::DropOnBoard()
{
	if (!HasAuthority())
	{
		return;
	}

	Carrier = nullptr;
	MARK_PROPERTY_DIRTY_FROM_NAME(
		AJoA_Flag,
		Carrier,
		this);
	ApplyCarryVisual();
	ForceNetUpdate();
}

AJoA_Flag* AJoA_Flag::FindGroundedAt(
	const UObject* WorldContext,
	int32 InX,
	int32 InY)
{
	UWorld* World = WorldContext
		? WorldContext->GetWorld()
		: nullptr;
	if (!World)
	{
		return nullptr;
	}

	for (TActorIterator<AJoA_Flag> It(World); It; ++It)
	{
		AJoA_Flag* Flag = *It;
		if (IsValid(Flag)
			&& !Flag->IsCarried()
			&& Flag->CellX == InX
			&& Flag->CellY == InY)
		{
			return Flag;
		}
	}

	return nullptr;
}

void AJoA_Flag::ApplyCarryVisual()
{
	AttachToHolder(Carrier);
}

void AJoA_Flag::PlaceOnBoard()
{
	DetachFromActor(
		FDetachmentTransformRules::KeepWorldTransform);

	AJoA_GridManager* Grid =
		AJoA_GameMode::GetGridManager(this);
	if (Grid)
	{
		SetActorLocation(
			Grid->GetCellWorldLocation(CellX, CellY));
	}

	const FRotator FlagRotation(
		0.f,
		OwningPlayerIndex == FrenchSeat ? 180.f : 0.f,
		0.f);
	SetActorRotation(FlagRotation);
	SetActorScale3D(FVector(0.5f));
}

void AJoA_Flag::ApplyColors()
{
	if (!PoleMesh || !BannerMesh)
	{
		return;
	}

	/* The pole reads the brown cell, palette row 4.
	 * Tile 0 is white, so the team tint stays off. */
	JoAAtlas::ApplyPalette(
		PoleMesh,
		JoAAtlas::WhiteTile,
		JoAAtlas::WoodRow);

	float TileIndex = JoAAtlas::WhiteTile;
	if (OwningPlayerIndex == FrenchSeat)
	{
		TileIndex = JoAAtlas::BlueTile;
	}
	else if (OwningPlayerIndex == EnglishSeat)
	{
		TileIndex = JoAAtlas::RedTile;
	}

	JoAAtlas::ApplyTile(BannerMesh, TileIndex);
}
