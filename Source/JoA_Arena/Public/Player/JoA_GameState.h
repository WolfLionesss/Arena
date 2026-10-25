// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

// --- Engine ---
#include "GameFramework/GameStateBase.h"

// --- GAME ---
#include "Player/JoA_SeatTypes.h"

#include "JoA_GameState.generated.h"

class AJoA_Figure;

/**
 * Replicated turn state. The GameMode owns the turn
 * order and writes here. Both players read it, so a
 * client can grey out a control before the server
 * would reject it.
 * One turn: pick a figure, optionally turn it, move
 * it once, then attack or heal, or skip. Moving or
 * acting commits that figure for the rest of the turn.
 * A turn has a time limit. The match is paused while a
 * seat has no player, so nobody acts and the clock
 * stands still.
 * Both seats pick rock, paper, or scissors before
 * the first turn. The winner of that throw moves
 * first. The match is not playable until then.
 */
UCLASS()
class JOA_ARENA_API AJoA_GameState : public AGameStateBase
{
	GENERATED_BODY()

public:
	AJoA_GameState();

	static AJoA_GameState* Get(const UObject* WorldContext);

	virtual void BeginPlay() override;

	virtual void GetLifetimeReplicatedProps(
		TArray<FLifetimeProperty>& OutLifetimeProps
	) const override;

#if UE_WITH_IRIS
	virtual void RegisterReplicationFragments(
		UE::Net::FFragmentRegistrationContext& Context,
		UE::Net::EFragmentRegistrationFlags
			RegistrationFlags) override;
#endif

	int32 GetActiveSeat() const
	{
		return ActiveSeat;
	}

	AJoA_Figure* GetActingFigure() const;

	bool HasActingFigureMoved() const
	{
		return bActingFigureMoved;
	}

	int32 GetWinnerSeat() const
	{
		return WinnerSeat;
	}

	bool IsMatchOver() const
	{
		return WinnerSeat != INDEX_NONE
			|| MatchEndReason == EMatchEndReason::Draw;
	}

	EMatchEndReason GetMatchEndReason() const
	{
		return MatchEndReason;
	}

	/* A seat has no player. Nothing may be played
	 * and the turn clock waits. */
	bool IsPaused() const
	{
		return bPaused;
	}

	/* Every action of a player asks this first. */
	bool IsPlayable() const
	{
		return !IsMatchOver()
			&& !bPaused
			&& bMatchStarted;
	}

	bool HasMatchStarted() const
	{
		return bMatchStarted;
	}

	bool AreBothSeatsConnected() const;

	bool HasSeatThrown(int32 Seat) const
	{
		return Seat >= 0
			&& Seat < 2
			&& (OpeningThrowMask & (1 << Seat)) != 0;
	}

	/* Both seats picked this. None until a tie,
	 * and none once that tie is replaced. */
	EOpeningThrow GetOpeningTiedThrow() const
	{
		const uint8 Code = OpeningThrowMask >> 2;
		if (Code == 0
			|| Code > static_cast<uint8>(
				EOpeningThrow::Scissors))
		{
			return EOpeningThrow::None;
		}

		return static_cast<EOpeningThrow>(Code);
	}

	int32 GetOpeningWinnerSeat() const
	{
		return OpeningWinnerSeat;
	}

	EOpeningThrow GetOpeningWinningThrow() const
	{
		return OpeningWinningThrow;
	}

	/* Server world time when the turn passes by itself.
	 * 0 while there is no limit or the match is paused. */
	double GetTurnDeadline() const
	{
		return TurnDeadline;
	}

	FSeatLink GetSeatLink(int32 Seat) const;

	// Seconds to a server world time. Negative once past.
	float SecondsUntil(double ServerTime) const;

	/* The seat on turn began it with no move and no
	 * action. It has to skip, and the HUD says why. */
	bool IsActiveSeatBlocked() const
	{
		return bActiveSeatBlocked;
	}

	/* The seat is on turn and has not committed a
	 * figure yet, so it may still pick another. */
	bool CanSeatSelect(int32 Seat) const;

	/* On turn, alive, and either nothing is committed
	 * or this is the committed figure. */
	bool CanFigureAct(const AJoA_Figure* Figure) const;

	// Acting is allowed and the one move is still unused.
	bool CanFigureMove(const AJoA_Figure* Figure) const;

	/* Server only. The opening throw is over, or a
	 * solo match skipped it. InSeat moves first. */
	void BeginMatch(
		int32 InSeat,
		EOpeningThrow InThrow);

	// Server only. Which seats have picked this round.
	void SetOpeningThrowMask(uint8 InMask);

	/* Server only. Stores who has picked and keeps
	 * a tied item in the high bits of the mask. */
	void SetOpeningPicks(bool bFrench, bool bEnglish);

	/* Server only. Both picked this item. Clears
	 * the picks and keeps the item for the draw line. */
	void SetOpeningTie(EOpeningThrow Throw);

	// Server only. Passes the turn and clears the commit.
	void SetActiveSeat(int32 InSeat);

	// Server only. Flags a turn that began with no option.
	void SetActiveSeatBlocked(bool bInBlocked);

	// Server only. The figure is locked in for this turn.
	void CommitActingFigure(
		AJoA_Figure* Figure,
		bool bMoved);

	// Server only. The match is decided.
	void SetWinnerSeat(
		int32 InSeat,
		EMatchEndReason Reason);

	// Server only. See IsPaused.
	void SetPaused(bool bInPaused);

	// Server only. See GetTurnDeadline.
	void SetTurnDeadline(double InDeadline);

	// Server only. One seat gained or lost its player.
	void SetSeatLink(
		int32 Seat,
		ESeatConnection InState,
		double InDeadline);

protected:
	UFUNCTION()
	void OnRep_TurnState();

	/* Logs a seat that joined or dropped. A client that
	 * arrives late or comes back reads it from here, which
	 * a one-off RPC could not give it. */
	UFUNCTION()
	void OnRep_SeatLinks();

	void NotifyTurnChanged();

	/* INDEX_NONE until the opening throw names a
	 * seat, or a solo match starts on French. */
	UPROPERTY(
		ReplicatedUsing = OnRep_TurnState,
		BlueprintReadOnly,
		Category = "Turn")
	int32 ActiveSeat = INDEX_NONE;

	UPROPERTY(
		ReplicatedUsing = OnRep_TurnState,
		BlueprintReadOnly,
		Category = "Turn")
	bool bMatchStarted = false;

	/* Bit 0 French, bit 1 English. Bits 2-3 are the
	 * tied item: 1 rock, 2 paper, 3 scissors. A tie
	 * clears bits 0-1 so both pick again. */
	UPROPERTY(
		ReplicatedUsing = OnRep_TurnState,
		BlueprintReadOnly,
		Category = "Turn")
	uint8 OpeningThrowMask = 0;

	UPROPERTY(
		ReplicatedUsing = OnRep_TurnState,
		BlueprintReadOnly,
		Category = "Turn")
	int32 OpeningWinnerSeat = INDEX_NONE;

	UPROPERTY(
		ReplicatedUsing = OnRep_TurnState,
		BlueprintReadOnly,
		Category = "Turn")
	EOpeningThrow OpeningWinningThrow =
		EOpeningThrow::None;

	UPROPERTY(
		ReplicatedUsing = OnRep_TurnState,
		BlueprintReadOnly,
		Category = "Turn")
	TObjectPtr<AJoA_Figure> ActingFigure;

	UPROPERTY(
		ReplicatedUsing = OnRep_TurnState,
		BlueprintReadOnly,
		Category = "Turn")
	bool bActingFigureMoved = false;

	UPROPERTY(
		ReplicatedUsing = OnRep_TurnState,
		BlueprintReadOnly,
		Category = "Turn")
	bool bActiveSeatBlocked = false;

	// INDEX_NONE while the match runs.
	UPROPERTY(
		ReplicatedUsing = OnRep_TurnState,
		BlueprintReadOnly,
		Category = "Turn")
	int32 WinnerSeat = INDEX_NONE;

	UPROPERTY(
		ReplicatedUsing = OnRep_TurnState,
		BlueprintReadOnly,
		Category = "Turn")
	EMatchEndReason MatchEndReason = EMatchEndReason::None;

	UPROPERTY(
		ReplicatedUsing = OnRep_TurnState,
		BlueprintReadOnly,
		Category = "Turn")
	bool bPaused = false;

	UPROPERTY(
		ReplicatedUsing = OnRep_TurnState,
		BlueprintReadOnly,
		Category = "Turn")
	double TurnDeadline = 0.0;

	// One entry per seat. French first.
	UPROPERTY(
		ReplicatedUsing = OnRep_SeatLinks,
		BlueprintReadOnly,
		Category = "Net")
	TArray<FSeatLink> SeatLinks;

	// Client only. The last link state already logged.
	ESeatConnection LoggedLinks[2] = {
		ESeatConnection::Empty,
		ESeatConnection::Empty
	};
};
