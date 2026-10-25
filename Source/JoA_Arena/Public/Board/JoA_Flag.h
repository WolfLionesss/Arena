// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

// --- Engine ---
#include "GameFramework/Actor.h"

#include "JoA_Flag.generated.h"

class AJoA_Figure;
class UStaticMesh;
class UStaticMeshComponent;

/**
 * Team flag made of a wooden pole and a banner.
 * M_Atlas gives one colour per component, so each
 * part is its own Static Mesh Component. The server
 * sets the seat, clients colour the banner in
 * OnRep_OwningPlayerIndex.
 * A grounded flag occupies a cell. Pickup attaches
 * it to the figure. Drop puts it back on that cell.
 */
UCLASS()
class JOA_ARENA_API AJoA_Flag : public AActor
{
	GENERATED_BODY()

public:
	AJoA_Flag();

	virtual void OnConstruction(
		const FTransform& Transform) override;

	virtual void PostInitializeComponents() override;

	virtual bool IsNetRelevantFor(
		const AActor* RealViewer,
		const AActor* ViewTarget,
		const FVector& SrcLocation) const override;

	/* Server only. FrenchSeat is blue, EnglishSeat
	 * is red. Any other value keeps the banner white. */
	void SetOwningPlayerIndex(int32 NewPlayerIndex);

	int32 GetOwningPlayerIndex() const
	{
		return OwningPlayerIndex;
	}

	int32 GetCellX() const
	{
		return CellX;
	}

	int32 GetCellY() const
	{
		return CellY;
	}

	AJoA_Figure* GetCarrier() const
	{
		return Carrier;
	}

	bool IsCarried() const
	{
		return Carrier != nullptr;
	}

	/* Server only. The cell this flag occupies,
	 * grounded or carried. */
	void SetBoardCell(int32 InX, int32 InY);

	/* Server only. Attaches to the figure. */
	void PickupBy(AJoA_Figure* Figure);

	/* Server only. Detaches onto CellX, CellY. */
	void DropOnBoard();

	/* Local attach. The client calls this when the
	 * holder or its cell replicates, because a
	 * teleport on the figure does not move a flag
	 * that never attached on that machine. */
	void AttachToHolder(AJoA_Figure* Figure);

	static AJoA_Flag* FindGroundedAt(
		const UObject* WorldContext,
		int32 InX,
		int32 InY);

	virtual void GetLifetimeReplicatedProps(
		TArray<FLifetimeProperty>& OutLifetimeProps
	) const override;

#if UE_WITH_IRIS
	virtual void RegisterReplicationFragments(
		UE::Net::FFragmentRegistrationContext& Context,
		UE::Net::EFragmentRegistrationFlags
			RegistrationFlags) override;
#endif

protected:
	virtual void BeginPlay() override;

	UFUNCTION()
	void OnRep_OwningPlayerIndex();

	UFUNCTION()
	void OnRep_Carrier();

	UFUNCTION()
	void OnRep_BoardCell();

	void ApplyColors();
	void ApplyCarryVisual();
	void PlaceOnBoard();

	/* BP_Flag can clear these on its CDO.
	 * Call after construction so the actor is
	 * still in the net list on spawn. */
	void EnsureReplication();

	UPROPERTY(VisibleAnywhere, Category = "Flag")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, Category = "Flag")
	TObjectPtr<UStaticMeshComponent> PoleMesh;

	UPROPERTY(VisibleAnywhere, Category = "Flag")
	TObjectPtr<UStaticMeshComponent> BannerMesh;

	/* Assign Wood and Banner in the Blueprint child. */
	UPROPERTY(EditDefaultsOnly, Category = "Flag")
	TObjectPtr<UStaticMesh> PoleAsset;

	UPROPERTY(EditDefaultsOnly, Category = "Flag")
	TObjectPtr<UStaticMesh> BannerAsset;

	UPROPERTY(
		ReplicatedUsing = OnRep_OwningPlayerIndex)
	int32 OwningPlayerIndex = INDEX_NONE;

	UPROPERTY(ReplicatedUsing = OnRep_BoardCell)
	int32 CellX = 0;

	UPROPERTY(ReplicatedUsing = OnRep_BoardCell)
	int32 CellY = 0;

	UPROPERTY(ReplicatedUsing = OnRep_Carrier)
	TObjectPtr<AJoA_Figure> Carrier;
};
