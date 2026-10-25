// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

// --- GAME ---
#include "Player/Units/JoA_UnitTypes.h"

class AJoA_GridManager;
class USkeletalMeshComponent;
class UStaticMeshComponent;

/**
 * Free helpers shared by the AJoA_Figure source files.
 * Unity builds merge every .cpp of the module, so a copy
 * in two anonymous namespaces is C2084. Each helper has
 * one definition: here, or in JoA_FigureHelpers.cpp.
 */
namespace JoAFigure
{
	inline constexpr float StepStaminaCost = 1.f;

	/* Hit, clap, and cry must keep their clip.
	 * A board refresh slide must not play Run. */
	inline bool IsReactionAnim(EFigureAnim Anim)
	{
		switch (Anim)
		{
		case EFigureAnim::Attack:
		case EFigureAnim::Shoot:
		case EFigureAnim::Hit:
		case EFigureAnim::Dodge:
		case EFigureAnim::Clap:
		case EFigureAnim::Cheer:
		case EFigureAnim::Cry:
		case EFigureAnim::Heal:
		case EFigureAnim::Death:
		case EFigureAnim::PickUp:
		case EFigureAnim::Interact:
		case EFigureAnim::Jump:
		case EFigureAnim::Fall:
			return true;
		default:
			return false;
		}
	}

	inline FIntPoint ForwardDelta(EFacing Facing)
	{
		switch (Facing)
		{
		case EFacing::PositiveX:
			return FIntPoint(1, 0);
		case EFacing::NegativeX:
			return FIntPoint(-1, 0);
		case EFacing::PositiveY:
			return FIntPoint(0, 1);
		case EFacing::NegativeY:
			return FIntPoint(0, -1);
		case EFacing::PositiveXPositiveY:
			return FIntPoint(1, 1);
		case EFacing::PositiveXNegativeY:
			return FIntPoint(1, -1);
		case EFacing::NegativeXPositiveY:
			return FIntPoint(-1, 1);
		default:
			return FIntPoint(-1, -1);
		}
	}

	/* Clockwise seen from above, the order of rising
	 * yaw. One step to the right is +45 degrees. */
	inline EFacing RotateFacing(EFacing Facing, int32 Steps)
	{
		const EFacing Ring[] =
		{
			EFacing::PositiveX,
			EFacing::PositiveXPositiveY,
			EFacing::PositiveY,
			EFacing::NegativeXPositiveY,
			EFacing::NegativeX,
			EFacing::NegativeXNegativeY,
			EFacing::NegativeY,
			EFacing::PositiveXNegativeY
		};
		const int32 RingSize = UE_ARRAY_COUNT(Ring);
		int32 Index = 0;
		for (int32 Candidate = 0;
			Candidate < RingSize;
			++Candidate)
		{
			if (Ring[Candidate] == Facing)
			{
				Index = Candidate;
				break;
			}
		}

		const int32 Turned =
			(Index + Steps % RingSize + RingSize)
			% RingSize;
		return Ring[Turned];
	}

	inline FIntPoint StepDelta(
		EFacing Facing,
		EStepDirection Direction)
	{
		const FIntPoint Forward = ForwardDelta(Facing);
		switch (Direction)
		{
		case EStepDirection::Back:
			return FIntPoint(-Forward.X, -Forward.Y);
		case EStepDirection::Left:
			/* Yaw -90. Forward +X gives -Y, which is
			 * the left hand of a figure facing +X. */
			return FIntPoint(Forward.Y, -Forward.X);
		case EStepDirection::Right:
			return FIntPoint(-Forward.Y, Forward.X);
		default:
			return Forward;
		}
	}

	float PaletteRowFor(EUnitType Type);

	void MatchHeldScale(
		UStaticMeshComponent* Item,
		const USkeletalMeshComponent* Body);

	uint8 ObstacleHeightAt(
		const AJoA_GridManager* Grid,
		int32 X,
		int32 Y);
}
