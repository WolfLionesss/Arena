// Copyright Epic Games, Inc. All Rights Reserved.

#include "Player/Units/JoA_FigureHelpers.h"

// --- GAME ---
#include "Board/JoA_Atlas.h"
#include "Board/JoA_GridManager.h"

// --- Components ---
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"

namespace JoAFigure
{
	float PaletteRowFor(EUnitType Type)
	{
		switch (Type)
		{
		case EUnitType::LightInfantry:
			return JoAAtlas::BarbarianRow;
		case EUnitType::Medic:
			return JoAAtlas::MageRow;
		case EUnitType::Archer:
			return JoAAtlas::RangerRow;
		default:
			return JoAAtlas::KnightRow;
		}
	}

	void MatchHeldScale(
		UStaticMeshComponent* Item,
		const USkeletalMeshComponent* Body)
	{
		if (!Item || !Body)
		{
			return;
		}

		/* The hand bone still has the FBX scale
		 * of 100. Relative scale 1 is then about
		 * 50 in the world, so the prop covers the
		 * board camera. Match the body instead. */
		Item->SetAbsolute(false, false, true);
		Item->SetWorldScale3D(
			Body->GetRelativeScale3D());
	}

	uint8 ObstacleHeightAt(
		const AJoA_GridManager* Grid,
		int32 X,
		int32 Y)
	{
		FGridCell Cell;
		if (!Grid || !Grid->GetCell(X, Y, Cell))
		{
			return 0;
		}

		return Cell.ObstacleHeight;
	}
}
