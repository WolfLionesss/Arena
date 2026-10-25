// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

// --- Engine ---
#include "GameFramework/Actor.h"

#include "JoA_FigureMarker.generated.h"

class AJoA_Figure;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class UProceduralMeshComponent;
class UStaticMeshComponent;

/**
 * What a figure marker says about its figure.
 */
enum class EFigureMarkerKind : uint8
{
	None,
	Selected,
	Attackable,
	Healable,

	// Ring on a free square the figure can step to.
	StepTarget,

	/* Ring on a square where an obstacle can be
	 * destroyed or raised. Brown. */
	Obstacle,

	// Ring under a flag the figure can pick up.
	FlagPickup
};

/**
 * Local-only marker on a figure. A ring on the square and
 * an inverted cone above the head share one color:
 * yellow for Selected, red for Attackable and light green
 * for Healable. It never replicates. Each player
 * controller spawns its own, so the opponent does not
 * see another player's selection.
 */
UCLASS(NotBlueprintable)
class JOA_ARENA_API AJoA_FigureMarker : public AActor
{
	GENERATED_BODY()

public:
	AJoA_FigureMarker();

	virtual void PostInitializeComponents() override;

	void ShowOn(
		AJoA_Figure* Figure,
		EFigureMarkerKind Kind);

	/* A ring alone, on a square. Used for StepTarget and
	 * Obstacle. The cone stays hidden. */
	void ShowOnCell(
		const FVector& Location,
		EFigureMarkerKind Kind);

	void HideMarker();

	EFigureMarkerKind GetKind() const
	{
		return CurrentKind;
	}

private:
	void ApplyColor(const FLinearColor& Color);
	void BuildRing();

	UPROPERTY()
	TObjectPtr<UProceduralMeshComponent> Ring;

	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> Cone;

	UPROPERTY()
	TObjectPtr<UMaterialInterface> UnlitBase;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> RingMaterial;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> ConeMaterial;

	EFigureMarkerKind CurrentKind = EFigureMarkerKind::None;
};
