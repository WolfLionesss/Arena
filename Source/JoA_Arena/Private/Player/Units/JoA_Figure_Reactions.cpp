// Copyright Epic Games, Inc. All Rights Reserved.

#include "Player/Units/JoA_Figure.h"

// --- Engine ---
#include "Engine/World.h"
#include "EngineUtils.h"

void AJoA_Figure::SetForcedBenchStart(int32 WaveIfOne)
{
	ForcedBenchStart = WaveIfOne;
}

void AJoA_Figure::NotifyPicked()
{
	if (bIsDead)
	{
		return;
	}

	bTouchedThisTurn = true;
	UnusedOwnerTurns = 0;
	PlayOrMulticastBodyAnim(EFigureAnim::Idle);
}

void AJoA_Figure::HandleTurnEnded(int32 EndedSeat)
{
	if (!HasAuthority())
	{
		return;
	}

	if (bIsDead)
	{
		if (bSkipNextBenchAdvance)
		{
			bSkipNextBenchAdvance = false;
			bReactedThisTurn = false;
			return;
		}

		if (bReactedThisTurn)
		{
			bReactedThisTurn = false;
			return;
		}

		AdvanceBenchRest();
		return;
	}

	if (OwningPlayerIndex != EndedSeat)
	{
		return;
	}

	if (bRestedThisTurn)
	{
		bRestedThisTurn = false;
		bTouchedThisTurn = false;
		return;
	}

	if (bTouchedThisTurn)
	{
		bTouchedThisTurn = false;
		UnusedOwnerTurns = 0;
		return;
	}

	/* Clap, cheer, and cry stand the seat.
	 * Sit after that would replay the down
	 * clip on the next unused turn. */
	if (bReactedThisTurn)
	{
		bReactedThisTurn = false;
		UnusedOwnerTurns = 0;
		return;
	}

	UnusedOwnerTurns =
		FMath::Min(UnusedOwnerTurns + 1, 2);
	PlayRestFromUnused();
}

void AJoA_Figure::MarkActiveThisTurn()
{
	if (bIsDead)
	{
		return;
	}

	bTouchedThisTurn = true;
	UnusedOwnerTurns = 0;
}

void AJoA_Figure::StandFromIncomingAttack()
{
	if (bIsDead)
	{
		return;
	}

	/* A shot stands a sitter or a liar. Unused
	 * rest would put them back on the floor
	 * after cry or dodge. */
	UnusedOwnerTurns = 0;
}

void AJoA_Figure::ApplySeatRestPose()
{
	if (bIsDead)
	{
		return;
	}

	UnusedOwnerTurns =
		FMath::Min(UnusedOwnerTurns + 1, 2);
	bRestedThisTurn = true;
	PlayRestFromUnused();
}

void AJoA_Figure::PlayRestFromUnused()
{
	if (UnusedOwnerTurns >= 2)
	{
		/* Already on the floor. Another Lie
		 * would replay the down clip. */
		if (PlayingBodyAnim == EFigureAnim::Lie)
		{
			return;
		}

		PlayOrMulticastBodyAnim(EFigureAnim::Lie);
		return;
	}

	if (UnusedOwnerTurns >= 1)
	{
		PlayOrMulticastBodyAnim(EFigureAnim::Sit);
		return;
	}

	PlayOrMulticastBodyAnim(EFigureAnim::Idle);
}

void AJoA_Figure::AdvanceBenchRest()
{
	switch (BenchPhase)
	{
	case EBenchPhase::Lie:
		PlayBenchStart();
		break;
	case EBenchPhase::Sit:
		BenchPhase = EBenchPhase::Lie;
		MulticastBodyAnim(EFigureAnim::Lie);
		break;
	default:
		BenchPhase = EBenchPhase::Sit;
		MulticastBodyAnim(EFigureAnim::Sit);
		break;
	}
}

void AJoA_Figure::PlayBenchStart()
{
	const bool bWave = ForcedBenchStart < 0
		? FMath::RandHelper(2) == 1
		: ForcedBenchStart != 0;
	BenchPhase = bWave
		? EBenchPhase::Wave
		: EBenchPhase::Idle;
	MulticastBodyAnim(
		bWave
			? EFigureAnim::Wave
			: EFigureAnim::Idle);
}

void AJoA_Figure::PlayTeamNews(EFigureAnim Anim)
{
	bReactedThisTurn = true;
	PlayOrMulticastBodyAnim(Anim);
}

void AJoA_Figure::QueueFollowUp(EFigureAnim Anim)
{
	FollowUpAnim = Anim;
	bHasFollowUp = true;
	bReactedThisTurn = true;
}

void AJoA_Figure::BroadcastGoodNews(
	const UObject* WorldContext,
	int32 WinnerSeat,
	AJoA_Figure* DelayOn,
	AJoA_Figure* Skip)
{
	BroadcastTeamNews(
		WorldContext,
		WinnerSeat,
		EFigureAnim::Clap,
		EFigureAnim::Cry,
		DelayOn,
		Skip);
}

void AJoA_Figure::BroadcastBadNews(
	const UObject* WorldContext,
	int32 VictimSeat,
	AJoA_Figure* DelayOn,
	AJoA_Figure* Skip)
{
	BroadcastTeamNews(
		WorldContext,
		VictimSeat,
		EFigureAnim::Cry,
		EFigureAnim::Cheer,
		DelayOn,
		Skip);
}

void AJoA_Figure::BroadcastTeamNews(
	const UObject* WorldContext,
	int32 FirstSeat,
	EFigureAnim FirstAnim,
	EFigureAnim OtherAnim,
	AJoA_Figure* DelayOn,
	AJoA_Figure* Skip)
{
	UWorld* World = WorldContext
		? WorldContext->GetWorld()
		: nullptr;
	if (!World || FirstSeat == INDEX_NONE)
	{
		return;
	}

	for (TActorIterator<AJoA_Figure> It(World); It; ++It)
	{
		AJoA_Figure* Figure = *It;
		if (!Figure || Figure == Skip)
		{
			continue;
		}

		const EFigureAnim News =
			Figure->OwningPlayerIndex == FirstSeat
				? FirstAnim
				: OtherAnim;
		if (Figure == DelayOn)
		{
			/* Shoot can finish before the arrow
			 * lands. The client hears the cheer
			 * when this clip ends. */
			Figure->QueueFollowUp(News);
			continue;
		}

		Figure->PlayTeamNews(News);
	}
}

void AJoA_Figure::BroadcastSeatNews(
	const UObject* WorldContext,
	int32 Seat,
	EFigureAnim Anim,
	AJoA_Figure* Skip)
{
	UWorld* World = WorldContext
		? WorldContext->GetWorld()
		: nullptr;
	if (!World || Seat == INDEX_NONE)
	{
		return;
	}

	for (TActorIterator<AJoA_Figure> It(World); It; ++It)
	{
		AJoA_Figure* Figure = *It;
		if (!Figure
			|| Figure == Skip
			|| Figure->OwningPlayerIndex != Seat)
		{
			continue;
		}

		Figure->PlayTeamNews(Anim);
	}
}
