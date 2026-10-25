// Copyright Epic Games, Inc. All Rights Reserved.

#include "Player/Units/JoA_Figure.h"

// --- Engine ---
#include "Engine/World.h"
#include "TimerManager.h"

// --- GAME ---
#include "Match/JoA_GameMode.h"
#include "Board/JoA_Flag.h"
#include "Board/JoA_GridManager.h"
#include "Player/Units/JoA_FigureHelpers.h"
#include "Player/Units/JoA_FigureRegistry.h"

// --- GAS ---
#include "GAS/Abilities/JoA_GAS_BuildObstacleAbility.h"
#include "GAS/Abilities/JoA_GAS_FallAbility.h"
#include "GAS/Abilities/JoA_GAS_FlagAbility.h"
#include "GAS/Abilities/JoA_GAS_JumpAbility.h"
#include "GAS/Abilities/JoA_GAS_MoveAbility.h"
#include "GAS/Abilities/JoA_GAS_RestAbility.h"
#include "GAS/Abilities/JoA_GAS_SkipTurnAbility.h"
#include "GAS/Abilities/JoA_GAS_TurnAbility.h"
#include "GAS/JoA_GAS_AbilitySystemComponent.h"

namespace
{
	const float RestResourceGain = 1.f;
	const float FallFromTwoDamage = 1.f;
}

void AJoA_Figure::RequestStep(EStepDirection Direction)
{
	if (!AbilitySystemComponent || !CanStep(Direction))
	{
		return;
	}

	const FIntPoint Target =
		GetStepTarget(Direction);
	if (ShouldDeferFlagArrival(Target))
	{
		/* A predicted step slides, then the
		 * pickup snaps it. Stay put. */
		bDeferStepArrival = true;
		ArmVisualConfirm();
	}
	else
	{
		PredictMoveTo(Target);
	}
	AbilitySystemComponent->TryActivateAbilityByInputID(
		UJoA_GAS_MoveAbility::StaticClass(),
		static_cast<int32>(Direction));
}

void AJoA_Figure::RequestTurn(ETurnDirection Direction)
{
	if (!AbilitySystemComponent || !CanTurn())
	{
		return;
	}

	AbilitySystemComponent->TryActivateAbilityByInputID(
		UJoA_GAS_TurnAbility::StaticClass(),
		static_cast<int32>(Direction));
}

void AJoA_Figure::RequestSkipTurn()
{
	if (!AbilitySystemComponent || !CanSkipTurn())
	{
		return;
	}

	AbilitySystemComponent->TryActivateAbilityByInputID(
		UJoA_GAS_SkipTurnAbility::StaticClass(),
		0);
}

void AJoA_Figure::RequestRest()
{
	if (!AbilitySystemComponent || !CanRest())
	{
		return;
	}

	AbilitySystemComponent->TryActivateAbilityByInputID(
		UJoA_GAS_RestAbility::StaticClass(),
		0);
}

void AJoA_Figure::RequestJump()
{
	if (!AbilitySystemComponent || !CanJump())
	{
		return;
	}

	PredictMoveTo(
		GetStepTarget(EStepDirection::Forward));
	AbilitySystemComponent->TryActivateAbilityByInputID(
		UJoA_GAS_JumpAbility::StaticClass(),
		0);
}

void AJoA_Figure::RequestFall()
{
	if (!AbilitySystemComponent || !CanFall())
	{
		return;
	}

	/* PredictMoveTo treats a tall step as a jump.
	 * Off a crate that arc looks like flying.
	 * The fall clip has to be playing first, so
	 * the slide uses the drop. */
	if (GetNetMode() == NM_Client)
	{
		PlayBodyAnim(EFigureAnim::Fall);
	}

	PredictMoveTo(
		GetStepTarget(EStepDirection::Forward));
	AbilitySystemComponent->TryActivateAbilityByInputID(
		UJoA_GAS_FallAbility::StaticClass(),
		0);
}

void AJoA_Figure::RequestPickupFlag()
{
	if (!AbilitySystemComponent || !CanPickupFlag())
	{
		return;
	}

	AbilitySystemComponent->TryActivateAbilityByInputID(
		UJoA_GAS_FlagAbility::StaticClass(),
		static_cast<int32>(EFlagCommand::Pickup));
}

void AJoA_Figure::RequestDropFlag()
{
	if (!AbilitySystemComponent || !CanDropFlag())
	{
		return;
	}

	AbilitySystemComponent->TryActivateAbilityByInputID(
		UJoA_GAS_FlagAbility::StaticClass(),
		static_cast<int32>(EFlagCommand::Drop));
}

void AJoA_Figure::RequestBuildObstacle()
{
	FIntPoint Cell = FIntPoint::ZeroValue;
	if (!AbilitySystemComponent
		|| !FindBuildableCell(Cell))
	{
		return;
	}

	AbilitySystemComponent->TryActivateAbilityByInputID(
		UJoA_GAS_BuildObstacleAbility::StaticClass(),
		0);
}

void AJoA_Figure::ArmDeferred(float Seconds)
{
	bResolvePending = true;
	UWorld* World = GetWorld();
	if (!World)
	{
		CommitDeferred();
		return;
	}

	World->GetTimerManager().SetTimer(
		DeferredHandle,
		this,
		&AJoA_Figure::CommitDeferred,
		FMath::Max(0.05f, Seconds),
		false);
}

void AJoA_Figure::ClearDeferred()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(
			DeferredHandle);
	}

	Deferred = FDeferredResolveData();
	bResolvePending = false;
}

void AJoA_Figure::CommitDeferred()
{
	if (!HasAuthority()
		|| Deferred.Kind == EDeferredResolve::None)
	{
		return;
	}

	const FDeferredResolveData Data = Deferred;
	AJoA_Figure* Target = Data.Target.Get();
	AJoA_Flag* Flag = Data.Flag.Get();
	ClearDeferred();

	if (Data.Kind == EDeferredResolve::Attack && Target)
	{
		Target->ApplyVitalChange(
			Data.Damage,
			0.f,
			Data.TargetStamina,
			0.f);
		Target->MulticastCombatText(
			Data.TextKind,
			Data.TextAmount);
		/* Impact stands a sitter or a liar. Cry
		 * on this figure would put it back down. */
		Target->StandFromIncomingAttack();
		if (!Target->IsDead())
		{
			Target->MulticastBodyAnim(
				Data.bHit
					? EFigureAnim::Hit
					: EFigureAnim::Dodge);
		}

		/* Hit: we clap, they cry. Miss: we
		 * cry, their teammates cheer. Target
		 * already plays Hit or Dodge. */
		if (Data.bHit)
		{
			BroadcastGoodNews(
				this,
				Data.NewsSeat,
				this,
				Target);
		}
		else
		{
			BroadcastBadNews(
				this,
				Data.NewsSeat,
				this,
				Target);
		}
	}
	else if (Data.Kind == EDeferredResolve::Heal && Target)
	{
		Target->ApplyVitalChange(0.f, Data.Healing, 0.f, 0.f);
		ApplyVitalChange(0.f, 0.f, 0.f, Data.ManaSpend);
		Target->MulticastCombatText(
			Data.TextKind,
			Data.TextAmount);
	}
	else if (Data.Kind == EDeferredResolve::Destroy
		|| Data.Kind == EDeferredResolve::Build)
	{
		if (AJoA_GridManager* Grid =
			AJoA_GameMode::GetGridManager(this))
		{
			Grid->SetObstacleHeight(
				Data.ObstacleX,
				Data.ObstacleY,
				Data.ObstacleHeight);
		}
	}
	else if (Data.Kind == EDeferredResolve::Jump
		|| Data.Kind == EDeferredResolve::Fall)
	{
		if (Data.Kind == EDeferredResolve::Fall
			&& Data.Damage > 0.f)
		{
			ApplyVitalChange(Data.Damage, 0.f, 0.f, 0.f);
			MulticastCombatText(Data.TextKind, Data.TextAmount);
		}
		if (Data.bBadNews)
		{
			BroadcastBadNews(this, Data.NewsSeat, this);
		}
		TryAutoPickupFlag();
		NotifyFlagCarried();
	}
	else if (Data.Kind == EDeferredResolve::Pickup && Flag)
	{
		Flag->PickupBy(this);
		SetHeldFlag(Flag);
		if (Data.bGoodNews)
		{
			/* The picker keeps PickUp.
			 * Mates clap. The other seat cries. */
			BroadcastGoodNews(
				this,
				Data.NewsSeat,
				nullptr,
				this);
		}
		NotifyFlagCarried();
	}
	else if (Data.Kind == EDeferredResolve::Drop)
	{
		DropHeldFlag();
		bSuppressFlagAutoPickup = true;
	}

	/* Attack, heal, an obstacle hit, and a built
	 * obstacle end the turn. */
	if (Data.bEndTurn)
	{
		if (AJoA_GameMode* Mode = GetArenaGameMode())
		{
			Mode->NotifyFigureActed(this);
		}
	}
}

bool AJoA_Figure::ResolveStep(EStepDirection Direction)
{
	if (!HasAuthority() || !CanStep(Direction))
	{
		return false;
	}

	MarkActiveThisTurn();
	const FIntPoint Target = GetStepTarget(Direction);
	const bool bDeferArrival =
		ShouldDeferFlagArrival(Target);
	if (bDeferArrival)
	{
		bDeferStepArrival = true;
	}
	else
	{
		MulticastBodyAnim(EFigureAnim::Run);
	}
	SetBoardPlacement(Target.X, Target.Y, Facing);
	if (UsesStamina())
	{
		ApplyVitalChange(
			0.f,
			0.f,
			JoAFigure::StepStaminaCost,
			0.f);
	}
	TryAutoPickupFlag();
	NotifyFlagCarried();

	/* Pickup did not start. Walk now. The
	 * cell is already the flag square. */
	if (bDeferArrival
		&& !(bResolvePending
			&& Deferred.Kind == EDeferredResolve::Pickup))
	{
		MulticastBodyAnim(EFigureAnim::Run);
	}

	/* The figure is locked in. The player can still
	 * turn it and act, but cannot pick another. */
	if (AJoA_GameMode* Mode = GetArenaGameMode())
	{
		Mode->NotifyFigureMoved(this);
	}

	return true;
}

bool AJoA_Figure::ResolvePickupFlag()
{
	if (!HasAuthority() || !CanPickupFlag())
	{
		return false;
	}

	AJoA_Flag* Flag = FindPickupFlag();
	if (!Flag)
	{
		return false;
	}

	MarkActiveThisTurn();
	ClearDeferred();
	Deferred.Kind = EDeferredResolve::Pickup;
	Deferred.Flag = Flag;
	Deferred.bGoodNews =
		Flag->GetOwningPlayerIndex()
		!= OwningPlayerIndex;
	Deferred.NewsSeat = OwningPlayerIndex;
	bResolvePending = true;
	MulticastBodyAnim(EFigureAnim::PickUp);
	if (bDeferStepArrival)
	{
		if (UWorld* World = GetWorld())
		{
			const float Wait =
				BodyAnimSeconds(EFigureAnim::PickUp);
			World->GetTimerManager().SetTimer(
				VisualConfirmHandle,
				this,
				&AJoA_Figure::ReleaseDeferredArrival,
				Wait,
				false);
		}
		else
		{
			ReleaseDeferredArrival();
		}
	}
	ArmDeferred(
		BodyAnimSeconds(EFigureAnim::PickUp) * 0.85f);
	return true;
}

bool AJoA_Figure::ResolveDropFlag()
{
	if (!HasAuthority() || !CanDropFlag())
	{
		return false;
	}

	MarkActiveThisTurn();
	ClearDeferred();
	Deferred.Kind = EDeferredResolve::Drop;
	bResolvePending = true;
	MulticastBodyAnim(EFigureAnim::PickUp);
	ArmDeferred(
		BodyAnimSeconds(EFigureAnim::PickUp) * 0.85f);
	return true;
}

bool AJoA_Figure::ResolveTurn(ETurnDirection Direction)
{
	if (!HasAuthority() || !CanTurn())
	{
		return false;
	}

	const int32 Steps =
		Direction == ETurnDirection::Right ? 1 : -1;
	SetBoardPlacement(
		CellX,
		CellY,
		JoAFigure::RotateFacing(Facing, Steps));
	return true;
}

bool AJoA_Figure::ResolveSkipTurn()
{
	if (!HasAuthority() || !CanSkipTurn())
	{
		return false;
	}

	AJoA_GameMode* Mode = GetArenaGameMode();
	if (!Mode)
	{
		return false;
	}

	Mode->RequestEndTurn(OwningPlayerIndex);
	return true;
}

bool AJoA_Figure::ResolveRest()
{
	if (!HasAuthority() || !CanRest())
	{
		return false;
	}

	AJoA_GameMode* Mode = GetArenaGameMode();
	if (!Mode)
	{
		return false;
	}

	/* The selected figure only carries the ask.
	 * Every living figure of the seat is paid. */
	TArray<AJoA_Figure*> Own;
	if (UJoA_FigureRegistry* Registry =
		UJoA_FigureRegistry::Get(this))
	{
		Registry->CollectFiguresOfSeat(
			OwningPlayerIndex,
			Own);
	}

	for (AJoA_Figure* Ally : Own)
	{
		if (!IsValid(Ally))
		{
			continue;
		}

		Ally->ApplyVitalChange(
			0.f,
			0.f,
			0.f,
			0.f,
			RestResourceGain,
			RestResourceGain);
		Ally->ApplySeatRestPose();
	}

	Mode->RequestEndTurn(OwningPlayerIndex);
	return true;
}

bool AJoA_Figure::ResolveJump()
{
	if (!HasAuthority() || !CanJump())
	{
		return false;
	}

	const FIntPoint Dest =
		GetStepTarget(EStepDirection::Forward);
	MarkActiveThisTurn();
	ClearDeferred();
	Deferred.Kind = EDeferredResolve::Jump;
	Deferred.bEndTurn = true;
	bResolvePending = true;
	MulticastBodyAnim(EFigureAnim::Jump);
	SetBoardPlacement(Dest.X, Dest.Y, Facing);
	return true;
}

bool AJoA_Figure::ResolveFall()
{
	if (!HasAuthority() || !CanFall())
	{
		return false;
	}

	const AJoA_GridManager* Grid =
		AJoA_GameMode::GetGridManager(this);
	const uint8 FromHeight =
		JoAFigure::ObstacleHeightAt(Grid, CellX, CellY);
	const FIntPoint Dest =
		GetStepTarget(EStepDirection::Forward);
	const uint8 ToHeight =
		JoAFigure::ObstacleHeightAt(Grid, Dest.X, Dest.Y);
	MarkActiveThisTurn();
	ClearDeferred();
	Deferred.Kind = EDeferredResolve::Fall;
	Deferred.bEndTurn = true;
	bResolvePending = true;
	if (FromHeight >= 2
		&& ToHeight == 0
		&& !bPromoted)
	{
		Deferred.Damage = FallFromTwoDamage;
		Deferred.bBadNews = true;
		Deferred.NewsSeat = OwningPlayerIndex;
		Deferred.TextKind = ECombatTextKind::Hit;
		Deferred.TextAmount = FMath::RoundToInt(
			FallFromTwoDamage);
	}

	MulticastBodyAnim(EFigureAnim::Fall);
	SetBoardPlacement(Dest.X, Dest.Y, Facing);
	return true;
}
