// Copyright Epic Games, Inc. All Rights Reserved.

#include "Player/Units/JoA_CombatLibrary.h"

// --- GAME ---
#include "Match/JoA_GameMode.h"
#include "Board/JoA_GridManager.h"

namespace
{
	bool IsOnBoard(const FIntPoint& Cell)
	{
		return Cell.X >= 0
			&& Cell.X < GridSize
			&& Cell.Y >= 0
			&& Cell.Y < GridSize;
	}

	FIntPoint StepForFacing(EFacing Facing)
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
		case EFacing::NegativeXNegativeY:
			return FIntPoint(-1, -1);
		default:
			return FIntPoint(1, 0);
		}
	}

	enum class ECellVisit : uint8
	{
		Continue,
		Stop
	};

	bool StepsOnFacing(
		FIntPoint Origin,
		FIntPoint Cell,
		EFacing Facing,
		int32& OutSteps)
	{
		const FIntPoint Step = StepForFacing(Facing);
		const int32 DeltaX = Cell.X - Origin.X;
		const int32 DeltaY = Cell.Y - Origin.Y;
		if (Step.X != 0 && Step.Y != 0)
		{
			const bool bOff =
				DeltaX * Step.X <= 0
				|| DeltaY * Step.Y <= 0
				|| FMath::Abs(DeltaX)
					!= FMath::Abs(DeltaY);
			if (bOff)
			{
				return false;
			}

			OutSteps = FMath::Abs(DeltaX);
			return true;
		}

		if (Step.X != 0)
		{
			if (DeltaY != 0 || DeltaX * Step.X <= 0)
			{
				return false;
			}

			OutSteps = FMath::Abs(DeltaX);
			return true;
		}

		if (DeltaX != 0 || DeltaY * Step.Y <= 0)
		{
			return false;
		}

		OutSteps = FMath::Abs(DeltaY);
		return true;
	}

	template <typename Func>
	void VisitCellsInFront(
		const UObject* WorldContext,
		FIntPoint Origin,
		EFacing Facing,
		int32 MaxSteps,
		EReachPolicy ReachPolicy,
		Func&& Accept)
	{
		const AJoA_GridManager* Grid =
			AJoA_GameMode::GetGridManager(WorldContext);
		if (!Grid || MaxSteps <= 0 || !IsOnBoard(Origin))
		{
			return;
		}

		const FIntPoint Step = StepForFacing(Facing);
		FIntPoint Cursor = Origin;

		for (int32 Distance = 1;
			Distance <= MaxSteps;
			++Distance)
		{
			Cursor += Step;
			if (!IsOnBoard(Cursor))
			{
				return;
			}

			FGridCell Cell;
			if (!Grid->GetCell(
				Cursor.X,
				Cursor.Y,
				Cell))
			{
				return;
			}

			if (Cell.ObstacleHeight == 0)
			{
				if (Accept(Cursor) == ECellVisit::Stop)
				{
					return;
				}
				continue;
			}

			if (ReachPolicy == EReachPolicy::Archer)
			{
				/* A figure on height 1 or 2 is a
				 * target. Height 1 also opens the
				 * next square. Height 2 does not. */
				if (Accept(Cursor) == ECellVisit::Stop)
				{
					return;
				}

				if (Cell.ObstacleHeight == 1)
				{
					const FIntPoint Behind =
						Cursor + Step;
					FGridCell BehindCell;
					if ((Distance + 1) <= MaxSteps
						&& IsOnBoard(Behind)
						&& Grid->GetCell(
							Behind.X,
							Behind.Y,
							BehindCell))
					{
						Accept(Behind);
					}
				}
				return;
			}

			const bool bHeightOne =
				ReachPolicy
					== EReachPolicy::IncludeHeightOne
				&& Cell.ObstacleHeight == 1;
			if (bHeightOne)
			{
				/* The obstacle square itself is the
				 * last legal attack or heal target. */
				Accept(Cursor);
			}
			return;
		}
	}
}

float UJoA_CombatLibrary::CalculateHitChance(
	float Attack,
	float Defense,
	int32 PreviousComboAttacks)
{
	const int32 Previous = FMath::Max(PreviousComboAttacks, 0);
	float Chance = 0.f;

	if (Attack > Defense)
	{
		/* 2 vs 1 is halfway to an undefended target,
		 * so 50%. 2 vs 0 is 100%. */
		Chance = ((Attack - Defense) / Attack) * 100.f;
		Chance += static_cast<float>(Previous) * 25.f;
	}
	else if (Attack < Defense)
	{
		/* No ratio check. The first attempt is 25%,
		 * so the fourth is 100%. */
		Chance = static_cast<float>(Previous + 1) * 25.f;
	}
	else
	{
		/* Equal stats miss on the first attempt.
		 * The next attempt on this figure is 25%. */
		Chance = static_cast<float>(Previous) * 25.f;
	}

	return FMath::Clamp(Chance, 0.f, 100.f);
}

void UJoA_CombatLibrary::GetStaminaCosts(
	float Attack,
	float Defense,
	float& OutAttackerCost,
	float& OutDefenderCost)
{
	if (Attack > Defense)
	{
		OutAttackerCost = 1.f;
		OutDefenderCost = 1.f;
	}
	else if (Attack < Defense)
	{
		OutAttackerCost = 2.f;
		OutDefenderCost = 0.f;
	}
	else
	{
		OutAttackerCost = 2.f;
		OutDefenderCost = 1.f;
	}
}

TArray<FIntPoint> UJoA_CombatLibrary::CollectCellsInFront(
	const UObject* WorldContext,
	FIntPoint Origin,
	EFacing Facing,
	int32 MaxSteps,
	EReachPolicy ReachPolicy)
{
	TArray<FIntPoint> Cells;
	VisitCellsInFront(
		WorldContext,
		Origin,
		Facing,
		MaxSteps,
		ReachPolicy,
		[&Cells](const FIntPoint& Cell)
		{
			Cells.Add(Cell);
			return ECellVisit::Continue;
		});
	return Cells;
}

bool UJoA_CombatLibrary::IsCellInFront(
	const UObject* WorldContext,
	FIntPoint Origin,
	FIntPoint Cell,
	EFacing Facing,
	int32 MaxSteps,
	EReachPolicy ReachPolicy)
{
	int32 Steps = 0;
	if (MaxSteps <= 0
		|| !StepsOnFacing(Origin, Cell, Facing, Steps)
		|| Steps > MaxSteps)
	{
		return false;
	}

	/* The tail past this square cannot hold it. */
	bool bFound = false;
	VisitCellsInFront(
		WorldContext,
		Origin,
		Facing,
		Steps,
		ReachPolicy,
		[&bFound, Cell](const FIntPoint& Visited)
		{
			if (Visited != Cell)
			{
				return ECellVisit::Continue;
			}

			bFound = true;
			return ECellVisit::Stop;
		});
	return bFound;
}

bool UJoA_CombatLibrary::FindFirstObstacleInFront(
	const UObject* WorldContext,
	FIntPoint Origin,
	EFacing Facing,
	int32 MaxSteps,
	FIntPoint& OutCell)
{
	const AJoA_GridManager* Grid =
		AJoA_GameMode::GetGridManager(WorldContext);
	if (!Grid || MaxSteps <= 0 || !IsOnBoard(Origin))
	{
		return false;
	}

	const FIntPoint Step = StepForFacing(Facing);
	FIntPoint Cursor = Origin;
	for (int32 Distance = 1; Distance <= MaxSteps; ++Distance)
	{
		Cursor += Step;
		if (!IsOnBoard(Cursor))
		{
			return false;
		}

		FGridCell Cell;
		if (!Grid->GetCell(Cursor.X, Cursor.Y, Cell))
		{
			return false;
		}

		if (Cell.ObstacleHeight == 0)
		{
			continue;
		}

		OutCell = Cursor;
		return true;
	}

	return false;
}
