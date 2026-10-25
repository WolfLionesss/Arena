// Copyright Epic Games, Inc. All Rights Reserved.

#include "Player/Units/JoA_Figure.h"

// --- Engine ---
#include "Engine/World.h"
#include "TimerManager.h"

// --- GAME ---
#include "Match/JoA_GameMode.h"
#include "Board/JoA_Flag.h"
#include "Board/JoA_GridManager.h"
#include "Player/JoA_Nation.h"
#include "Player/Units/JoA_FigureHelpers.h"
#include "Player/Units/JoA_FigureRegistry.h"

// --- Components ---
#include "Components/SkeletalMeshComponent.h"

// --- Net ---
#include "Net/Core/PushModel/PushModel.h"

namespace
{
	FRotator RotationForFacing(EFacing Facing)
	{
		switch (Facing)
		{
		case EFacing::NegativeX:
			return FRotator(0.f, 180.f, 0.f);
		case EFacing::PositiveY:
			return FRotator(0.f, 90.f, 0.f);
		case EFacing::NegativeY:
			return FRotator(0.f, -90.f, 0.f);
		case EFacing::PositiveXPositiveY:
			return FRotator(0.f, 45.f, 0.f);
		case EFacing::PositiveXNegativeY:
			return FRotator(0.f, -45.f, 0.f);
		case EFacing::NegativeXPositiveY:
			return FRotator(0.f, 135.f, 0.f);
		case EFacing::NegativeXNegativeY:
			return FRotator(0.f, -135.f, 0.f);
		default:
			return FRotator::ZeroRotator;
		}
	}

	/* A fallen figure stands one square outside the
	 * board edge, looking into the board. French stands
	 * on the high-Y side, the left of the French view.
	 * English stands on the low-Y side, on the right.
	 * Each unit type keeps its own square along the
	 * edge, so no two fallen figures overlap. */
	void GetFallenPlacement(
		int32 Seat,
		EUnitType Type,
		int32& OutX,
		int32& OutY,
		EFacing& OutFacing)
	{
		const int32 Slot = FMath::Clamp(
			static_cast<int32>(Type),
			0,
			GridSize / 2 - 1);
		if (Seat == FrenchSeat)
		{
			OutX = GridSize - 1 - Slot;
			OutY = GridSize;
			OutFacing = EFacing::NegativeY;
			return;
		}

		OutX = Slot;
		OutY = -1;
		OutFacing = EFacing::PositiveY;
	}
}

void AJoA_Figure::OnRep_BoardPlacement()
{
	ApplyBoardTransform();

	if (UJoA_FigureRegistry* Registry =
		UJoA_FigureRegistry::Get(this))
	{
		Registry->NotifyBoardChanged();
	}
}

void AJoA_Figure::HandleBoardChanged()
{
	ApplyBoardTransform();
}

void AJoA_Figure::ApplyBoardTransform()
{
	ApplyTeamColor();

	/* The replicated cell keeps the last square. The
	 * body stands at the edge once the figure is dead. */
	int32 ShownX = CellX;
	int32 ShownY = CellY;
	EFacing ShownFacing = Facing;
	if (bIsDead && bOnBench)
	{
		GetFallenPlacement(
			OwningPlayerIndex,
			UnitType,
			ShownX,
			ShownY,
			ShownFacing);
	}

	const FRotator Yaw = RotationForFacing(ShownFacing);

	/* Root stays at yaw 0. The body carries Facing,
	 * so the spawn transform cannot turn it back. */
	SetActorRotation(FRotator::ZeroRotator);
	if (Mesh)
	{
		/* The imported mesh faces sideways, so
		 * figures looked away from the board.
		 * The offset aligns the body with
		 * Facing. */
		const FRotator MeshYawOffset(0.f, -90.f, 0.f);
		Mesh->SetRelativeRotation(Yaw + MeshYawOffset);
		ApplyBodyScale();
	}

	AJoA_GridManager* Grid =
		AJoA_GameMode::GetGridManager(this);
	if (!Grid)
	{
		return;
	}

	const FVector CellLocation = bIsDead
		? Grid->GetCellWorldLocation(ShownX, ShownY)
		: Grid->GetCellStandWorldLocation(
			ShownX,
			ShownY);

	/* Pickup plays on the square just left.
	 * A refresh must not snap onto the flag. */
	if (!bIsDead && bDeferStepArrival)
	{
		return;
	}

	const bool bPickupHold =
		!bIsDead
		&& bPlacedOnBoard
		&& PlayingBodyAnim == EFigureAnim::PickUp
		&& (bSliding
			|| ShownX != AppliedCellX
			|| ShownY != AppliedCellY);
	if (bPickupHold)
	{
		if (bSliding)
		{
			RewindArrivalSlide();
		}
		bDeferStepArrival = true;
		return;
	}

	const bool bCellMoved =
		bPlacedOnBoard
		&& (ShownX != AppliedCellX
			|| ShownY != AppliedCellY);
	AppliedCellX = ShownX;
	AppliedCellY = ShownY;
	const bool bReaction =
		bHoldBodyAnim
		|| JoAFigure::IsReactionAnim(PlayingBodyAnim);
	const bool bAirMove =
		PlayingBodyAnim == EFigureAnim::Jump
		|| PlayingBodyAnim == EFigureAnim::Fall;
	if (bCellMoved
		&& (!bReaction || bAirMove))
	{
		/* Run is multicast for a real step.
		 * A refresh that only looks like a move
		 * must not play it on the client.
		 * Jump and fall are reactions too.
		 * A snap skips the clip, so the host
		 * only appears on the next cell. */
		StartSlide(CellLocation, false);
		KeepNetBodyAnim();
		return;
	}

	/* A second refresh runs in the same frame as
	 * the jump. Cancelling its slide snapped the
	 * figure to the cell, so it looked teleported. */
	if (bReaction && bSliding && !bAwaitingConfirm
		&& !bAirMove)
	{
		bSliding = false;
	}

	/* Same square. Snap. A slide here left the
	 * client off the cell and played Run. */
	if (bAwaitingConfirm)
	{
		return;
	}

	if (bSliding && SlideTo.Equals(CellLocation, 1.f))
	{
		return;
	}

	bSliding = false;
	if (!bHoldBodyAnim)
	{
		SetActorTickEnabled(false);
	}
	SnapToCell(CellLocation);
	KeepNetBodyAnim();
}

void AJoA_Figure::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!bSliding)
	{
		SetActorTickEnabled(false);
		return;
	}

	SlideElapsed += DeltaSeconds;
	const float Alpha = FMath::Clamp(
		SlideElapsed / SlideDuration,
		0.f,
		1.f);
	if (PlayingBodyAnim == EFigureAnim::Fall)
	{
		/* A linear lerp made the fall glide. The
		 * figure steps off the edge, then gravity
		 * speeds the drop up until it hits. */
		const float SideAlpha = FMath::Clamp(
			Alpha / 0.5f,
			0.f,
			1.f);
		FVector FallPos = FMath::Lerp(
			SlideFrom,
			SlideTo,
			SideAlpha);
		FallPos.Z = FMath::Lerp(
			SlideFrom.Z,
			SlideTo.Z,
			Alpha * Alpha);
		SetActorLocation(FallPos);
	}
	else if (PlayingBodyAnim == EFigureAnim::Jump)
	{
		/* A linear lerp made the jump glide. The
		 * clip crouches first and lands last, so
		 * the figure leaves the ground in the
		 * middle and follows an arc. */
		const float JumpArcHeight = 45.f;
		const float AirAlpha = FMath::Clamp(
			(Alpha - 0.25f) / 0.5f,
			0.f,
			1.f);
		FVector JumpPos = FMath::Lerp(
			SlideFrom,
			SlideTo,
			AirAlpha);
		JumpPos.Z += 4.f * JumpArcHeight
			* AirAlpha * (1.f - AirAlpha);
		SetActorLocation(JumpPos);
	}
	else
	{
		SetActorLocation(FMath::Lerp(
			SlideFrom,
			SlideTo,
			Alpha));
	}
	if (Alpha < 1.f)
	{
		return;
	}

	bSliding = false;
	if (!bHoldBodyAnim)
	{
		SetActorTickEnabled(false);
	}
	SnapToCell(SlideTo);
}

bool AJoA_Figure::CanSlideTo(const FVector& Target) const
{
	const UWorld* World = GetWorld();
	if (!World
		|| !World->IsGameWorld()
		|| GetNetMode() == NM_DedicatedServer
		|| bIsDead
		|| !bPlacedOnBoard)
	{
		return false;
	}

	const float Dist = FVector::Dist(
		GetActorLocation(),
		Target);
	if (Dist < 1.f)
	{
		return false;
	}

	const AJoA_GridManager* Grid =
		AJoA_GameMode::GetGridManager(this);
	if (!Grid)
	{
		return false;
	}

	const float Tile = FVector::Dist(
		Grid->GetCellWorldLocation(0, 0),
		Grid->GetCellWorldLocation(1, 0));
	return Dist <= Tile * 2.5f;
}

void AJoA_Figure::SnapToCell(const FVector& Target)
{
	if (HasActorBegunPlay())
	{
		bPlacedOnBoard = true;
	}
	SetActorLocation(Target);

	/* The flag is parented to the mesh. A snap
	 * leaves it at the previous world spot. */
	if (HeldFlag)
	{
		HeldFlag->AttachToHolder(this);
	}

	KeepNetBodyAnim();

	if (!bIsDead
		&& PlayingBodyAnim == EFigureAnim::Run)
	{
		PlayRestFromUnused();
	}

	if (bResolvePending
		&& (Deferred.Kind == EDeferredResolve::Jump
			|| Deferred.Kind == EDeferredResolve::Fall))
	{
		CommitDeferred();
	}
}

void AJoA_Figure::StartSlide(
	const FVector& Target,
	bool bPlayMoveClip)
{
	if (bAwaitingConfirm && SlideTo.Equals(Target, 1.f))
	{
		bAwaitingConfirm = false;
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(
				VisualConfirmHandle);
		}
	}
	else if (bAwaitingConfirm
		&& bSliding
		&& SlideFrom.Equals(Target, 1.f))
	{
		/* A board refresh still has the old cell.
		 * It must not pull a predicted step back. */
		return;
	}

	if (!CanSlideTo(Target))
	{
		bSliding = false;
		SetActorTickEnabled(false);
		SnapToCell(Target);
		return;
	}

	if (bSliding && SlideTo.Equals(Target, 1.f))
	{
		return;
	}

	/* A fresh slide may play its cue. The
	 * confirm of this slide must not. */
	bMoveCuePlayed = false;
	SlideFrom = GetActorLocation();
	SlideTo = Target;
	SlideElapsed = 0.f;
	const float Climb =
		FMath::Abs(SlideTo.Z - SlideFrom.Z);
	if (PlayingBodyAnim == EFigureAnim::Fall)
	{
		/* The landing clip is long. A drop of
		 * one or two steps takes well under it. */
		const float FallDropSeconds = 0.4f;
		SlideDuration = FMath::Min(
			BodyAnimSeconds(EFigureAnim::Fall),
			FallDropSeconds);
	}
	else if (PlayingBodyAnim == EFigureAnim::Jump
		|| (bPlayMoveClip && Climb > 20.f))
	{
		SlideDuration = BodyAnimSeconds(
			EFigureAnim::Jump);
		if (bPlayMoveClip
			&& PlayingBodyAnim != EFigureAnim::Jump
			&& PlayingBodyAnim != EFigureAnim::Fall)
		{
			PlayBodyAnim(EFigureAnim::Jump);
		}
	}
	else
	{
		SlideDuration = BodyAnimSeconds(
			EFigureAnim::Run);
		/* Only a predicted step asks for the
		 * clip. A board refresh passes false. */
		if (bPlayMoveClip
			&& !JoAFigure::IsReactionAnim(PlayingBodyAnim))
		{
			bArmedStepRun = true;
			PlayBodyAnim(EFigureAnim::Run);
			bArmedStepRun = false;
		}
	}

	bSliding = true;
	SetActorTickEnabled(true);
}

void AJoA_Figure::PredictMoveTo(const FIntPoint& Cell)
{
	if (GetNetMode() != NM_Client
		|| !IsLocallyControlled())
	{
		return;
	}

	const AJoA_GridManager* Grid =
		AJoA_GameMode::GetGridManager(this);
	if (!Grid)
	{
		return;
	}

	const FVector Target =
		Grid->GetCellStandWorldLocation(
			Cell.X,
			Cell.Y);
	bAwaitingConfirm = true;
	StartSlide(Target, true);
	ArmVisualConfirm();
}

void AJoA_Figure::ArmVisualConfirm()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	/* A rejected step does not replicate. This
	 * reads the server cell after a normal trip. */
	constexpr float ConfirmSeconds = 0.75f;
	World->GetTimerManager().SetTimer(
		VisualConfirmHandle,
		this,
		&AJoA_Figure::ConfirmVisual,
		ConfirmSeconds,
		false);
}

void AJoA_Figure::RewindArrivalSlide()
{
	if (bSliding)
	{
		SetActorLocation(SlideFrom);
		bSliding = false;
		if (!bHoldBodyAnim)
		{
			SetActorTickEnabled(false);
		}
	}

	bAwaitingConfirm = false;
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(
			VisualConfirmHandle);
	}

	/* The cell already changed. Forget the
	 * visual square so the walk still runs. */
	AppliedCellX = INDEX_NONE;
	AppliedCellY = INDEX_NONE;
}

void AJoA_Figure::ReleaseDeferredArrival()
{
	if (!HasAuthority() || !bDeferStepArrival)
	{
		return;
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(
			VisualConfirmHandle);
	}

	/* Every machine hears Run and walks.
	 * The step cue plays with that clip. */
	MulticastBodyAnim(EFigureAnim::Run);
}

void AJoA_Figure::ConfirmVisual()
{
	bAwaitingConfirm = false;
	if (bDeferStepArrival
		&& AppliedCellX == CellX
		&& AppliedCellY == CellY
		&& PlayingBodyAnim != EFigureAnim::PickUp
		&& NetBodyAnim != EFigureAnim::PickUp)
	{
		bDeferStepArrival = false;
	}
	ApplyBoardTransform();
}

void AJoA_Figure::SetBoardPlacement(
	int32 InX,
	int32 InY,
	EFacing InFacing)
{
	if (!HasAuthority())
	{
		return;
	}

	const int32 OldX = CellX;
	const int32 OldY = CellY;
	CellX = FMath::Clamp(InX, 0, GridSize - 1);
	CellY = FMath::Clamp(InY, 0, GridSize - 1);
	Facing = InFacing;
	MARK_PROPERTY_DIRTY_FROM_NAME(AJoA_Figure, CellX, this);
	MARK_PROPERTY_DIRTY_FROM_NAME(AJoA_Figure, CellY, this);
	MARK_PROPERTY_DIRTY_FROM_NAME(AJoA_Figure, Facing, this);
	ForceNetUpdate();
	ApplyBoardTransform();

	if (OldX != CellX || OldY != CellY)
	{
		bSuppressFlagAutoPickup = false;
		if (HeldFlag)
		{
			HeldFlag->SetBoardCell(CellX, CellY);
		}
		TryPromoteOnEnemyHome();
	}

	if (UJoA_FigureRegistry* Registry =
		UJoA_FigureRegistry::Get(this))
	{
		Registry->NotifyBoardChanged();
	}
}
