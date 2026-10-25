// Copyright Epic Games, Inc. All Rights Reserved.

#include "Player/Units/JoA_Figure.h"

// --- GAME ---
#include "Match/JoA_GameMode.h"
#include "Player/JoA_GameState.h"
#include "Board/JoA_GridManager.h"
#include "Player/Units/JoA_FigureHelpers.h"
#include "Player/Units/JoA_FigureRegistry.h"

// --- GAS ---
#include "GAS/JoA_GAS_StaminaSet.h"

bool AJoA_Figure::IsAllowedToAct() const
{
	if (bResolvePending)
	{
		return false;
	}

	const AJoA_GameState* State = AJoA_GameState::Get(this);
	return !State || State->CanFigureAct(this);
}

FIntPoint AJoA_Figure::GetStepTarget(
	EStepDirection Direction) const
{
	return FIntPoint(CellX, CellY)
		+ JoAFigure::StepDelta(Facing, Direction);
}

bool AJoA_Figure::CanStep(EStepDirection Direction) const
{
	if (bResolvePending)
	{
		return false;
	}

	/* Without a turn state nothing may move. Attacks
	 * stay allowed, so a level without it still works. */
	const AJoA_GameState* State = AJoA_GameState::Get(this);
	if (!State || !State->CanFigureMove(this))
	{
		return false;
	}

	const AJoA_GridManager* Grid =
		AJoA_GameMode::GetGridManager(this);
	const UJoA_FigureRegistry* Registry =
		UJoA_FigureRegistry::Get(this);
	if (!Grid || !Registry)
	{
		return false;
	}

	const FIntPoint Target = GetStepTarget(Direction);
	FGridCell Cell;
	if (!Grid->GetCell(Target.X, Target.Y, Cell))
	{
		return false;
	}

	/* Obstacles block a step. Climb with Jump.
	 * Leave with Fall. A figure on an obstacle
	 * cannot walk off it. */
	if (Cell.ObstacleHeight > 0
		|| JoAFigure::ObstacleHeightAt(Grid, CellX, CellY) > 0)
	{
		return false;
	}

	if (Registry->FindFigureAt(Target.X, Target.Y))
	{
		return false;
	}

	if (!UsesStamina())
	{
		return true;
	}

	const float StepCost =
		JoAFigure::StepStaminaCost + GetHeldFlagSpendPenalty();
	const UJoA_GAS_StaminaSet* StaminaSet =
		GetStaminaSet();
	return StaminaSet
		&& StaminaSet->GetStamina() >= StepCost;
}

bool AJoA_Figure::CanTurn() const
{
	const AJoA_GameState* State = AJoA_GameState::Get(this);
	return State && State->CanFigureAct(this);
}

bool AJoA_Figure::HasAnyAction(FString* OutReason) const
{
	if (!IsAllowedToAct())
	{
		return false;
	}

	if (CanPickupFlag() || CanDropFlag())
	{
		const AJoA_GameState* TurnState =
			AJoA_GameState::Get(this);
		const bool bAlreadyMoved = TurnState
			&& TurnState->GetActingFigure() == this
			&& TurnState->HasActingFigureMoved();
		if (!bAlreadyMoved)
		{
			if (OutReason)
			{
				*OutReason = TEXT("flag");
			}
			return true;
		}
	}

	for (int32 Step = 0; Step < 4; ++Step)
	{
		if (CanStep(static_cast<EStepDirection>(Step)))
		{
			if (OutReason)
			{
				*OutReason = FString::Printf(
					TEXT("step %d"),
					Step);
			}
			return true;
		}
	}

	const UJoA_FigureRegistry* Registry =
		UJoA_FigureRegistry::Get(this);
	TArray<AJoA_Figure*> Living;
	if (Registry)
	{
		Registry->CollectFigures(Living);
	}

	const FUnitStats Defaults =
		FUnitStats::GetDefaults(UnitType);
	for (int32 Turn = 0; Turn < 8; ++Turn)
	{
		const EFacing Candidate =
			JoAFigure::RotateFacing(Facing, Turn);
		for (const AJoA_Figure* Other : Living)
		{
			const bool bAttack =
				CanAttackTargetFacing(Other, Candidate);
			if (bAttack
				|| CanHealTargetFacing(Other, Candidate))
			{
				if (OutReason)
				{
					*OutReason = FString::Printf(
						TEXT("%s unit %d at %d,%d after"
							" %d turns"),
						bAttack ? TEXT("attack")
							: TEXT("heal"),
						static_cast<int32>(Other->UnitType),
						Other->CellX,
						Other->CellY,
						Turn);
				}
				return true;
			}
		}

		FIntPoint ObstacleCell = FIntPoint::ZeroValue;
		if (Defaults.bCanDestroyObstacles
			&& UJoA_CombatLibrary::FindFirstObstacleInFront(
				this,
				FIntPoint(CellX, CellY),
				Candidate,
				Defaults.Range,
				ObstacleCell))
		{
			if (OutReason)
			{
				*OutReason = TEXT("obstacle");
			}
			return true;
		}

		FIntPoint BuildCell = FIntPoint::ZeroValue;
		if (FindBuildableCellFacing(Candidate, BuildCell))
		{
			if (OutReason)
			{
				*OutReason = TEXT("build");
			}
			return true;
		}

		if (CanJumpFacing(Candidate))
		{
			if (OutReason)
			{
				*OutReason = TEXT("jump");
			}
			return true;
		}

		if (CanFallFacing(Candidate))
		{
			if (OutReason)
			{
				*OutReason = TEXT("fall");
			}
			return true;
		}
	}

	return false;
}

EFacing AJoA_Figure::RotateFacingBy(
	EFacing InFacing,
	int32 Steps)
{
	return JoAFigure::RotateFacing(InFacing, Steps);
}

int32 AJoA_Figure::FacingRingIndex(EFacing InFacing)
{
	for (int32 Index = 0; Index < 8; ++Index)
	{
		if (JoAFigure::RotateFacing(EFacing::PositiveX, Index)
			== InFacing)
		{
			return Index;
		}
	}

	return 0;
}

bool AJoA_Figure::IsDiagonalFacing(EFacing InFacing)
{
	return FacingRingIndex(InFacing) % 2 == 1;
}

bool AJoA_Figure::CanSkipTurn() const
{
	const AJoA_GameState* State = AJoA_GameState::Get(this);
	return State
		&& !IsDead()
		&& !bResolvePending
		&& State->IsPlayable()
		&& State->GetActiveSeat() == OwningPlayerIndex;
}

bool AJoA_Figure::CanRest() const
{
	const AJoA_GameState* State = AJoA_GameState::Get(this);
	return CanSkipTurn()
		&& State
		&& State->CanFigureAct(this)
		&& !State->HasActingFigureMoved();
}

bool AJoA_Figure::CanJump() const
{
	return CanJumpFacing(Facing);
}

bool AJoA_Figure::CanFall() const
{
	return CanFallFacing(Facing);
}

bool AJoA_Figure::CanJumpFacing(EFacing InFacing) const
{
	const FUnitStats Defaults =
		FUnitStats::GetDefaults(UnitType);
	if (!Defaults.bCanJump
		|| IsDead()
		|| !IsAllowedToAct())
	{
		return false;
	}

	const AJoA_GridManager* Grid =
		AJoA_GameMode::GetGridManager(this);
	const UJoA_FigureRegistry* Registry =
		UJoA_FigureRegistry::Get(this);
	if (!Grid || !Registry)
	{
		return false;
	}

	const FIntPoint Dest = FIntPoint(CellX, CellY)
		+ JoAFigure::StepDelta(InFacing, EStepDirection::Forward);
	FGridCell DestCell;
	if (!Grid->GetCell(Dest.X, Dest.Y, DestCell)
		|| DestCell.ObstacleHeight == 0)
	{
		return false;
	}

	const uint8 HereHeight =
		JoAFigure::ObstacleHeightAt(Grid, CellX, CellY);
	/* A promoted figure climbs two. Empty
	 * ground onto height 2 needs that. */
	const uint8 MaxRise = bPromoted ? 2 : 1;
	if (DestCell.ObstacleHeight > HereHeight + MaxRise
		|| DestCell.ObstacleHeight < HereHeight)
	{
		return false;
	}

	return !Registry->FindFigureAt(Dest.X, Dest.Y);
}

bool AJoA_Figure::CanFallFacing(EFacing InFacing) const
{
	const FUnitStats Defaults =
		FUnitStats::GetDefaults(UnitType);
	if (!Defaults.bCanJump
		|| IsDead()
		|| !IsAllowedToAct())
	{
		return false;
	}

	const AJoA_GridManager* Grid =
		AJoA_GameMode::GetGridManager(this);
	const UJoA_FigureRegistry* Registry =
		UJoA_FigureRegistry::Get(this);
	if (!Grid || !Registry)
	{
		return false;
	}

	const uint8 HereHeight =
		JoAFigure::ObstacleHeightAt(Grid, CellX, CellY);
	if (HereHeight == 0)
	{
		return false;
	}

	const FIntPoint Dest = FIntPoint(CellX, CellY)
		+ JoAFigure::StepDelta(InFacing, EStepDirection::Forward);
	FGridCell DestCell;
	if (!Grid->GetCell(Dest.X, Dest.Y, DestCell)
		|| DestCell.ObstacleHeight >= HereHeight)
	{
		return false;
	}

	return !Registry->FindFigureAt(Dest.X, Dest.Y);
}
