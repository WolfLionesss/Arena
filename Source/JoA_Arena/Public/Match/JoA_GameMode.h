// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

// --- Engine ---
#include "Engine/TimerHandle.h"
#include "GameFramework/GameModeBase.h"

// --- GAME ---
#include "Player/JoA_SeatTypes.h"

#include "JoA_GameMode.generated.h"

class AJoA_Figure;
class AJoA_Flag;
class AJoA_GridManager;
class AJoA_PlayerState;
class APlayerController;

/**
 * Server record of one seat. The player state dies with
 * its connection, so what a returning player needs is
 * kept here: who the seat belongs to, and the combo.
 */
struct FSeatRecord
{
	ESeatConnection Link = ESeatConnection::Empty;

	// The listen server player. Nobody can take it over.
	bool bHost = false;

	// Online identity. Empty without an online subsystem.
	FString UniqueKey;

	// Secret the server gave this seat.
	FString Token;

	TWeakObjectPtr<AController> Controller;

	TWeakObjectPtr<AJoA_Figure> ComboTarget;
	int32 ComboAttackCount = 0;

	FTimerHandle ForfeitTimer;
};

/**
 * Server-authoritative match setup. The default pawn
 * has no movement. Each local PlayerController locks
 * the view to AJoA_ViewCamera.
 * AJoA_PlayerState keeps that player's attack combo.
 * Figure stats live on AJoA_Figure.
 * GridManager is the one board, cached when play starts.
 * French stands on +X, in blue.
 * English is the mirrored line on -X, in red.
 * Who moves first is the opening throw, not the
 * nation. A solo match with no opponent still
 * opens on French.
 * Two seats, no more. A seat is held for its player
 * when the connection drops, for a window set in
 * UJoA_MatchSettings. A player who does not come back
 * forfeits. Every turn has a time limit, and the match
 * waits while a seat has no player.
 */
UCLASS(Blueprintable)
class JOA_ARENA_API AJoA_GameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AJoA_GameMode();

	virtual void BeginPlay() override;

	virtual void PreLogin(
		const FString& Options,
		const FString& Address,
		const FUniqueNetIdRepl& UniqueId,
		FString& ErrorMessage) override;

	virtual FString InitNewPlayer(
		APlayerController* NewPlayerController,
		const FUniqueNetIdRepl& UniqueId,
		const FString& Options,
		const FString& Portal = TEXT("")) override;

	virtual void PostLogin(
		APlayerController* NewPlayer) override;

	virtual void Logout(AController* Exiting) override;

	/* Digits of a SteamSockets P2P address.
	 * An IP address returns false, so PIE and
	 * Join Game on 127.0.0.1 stay on the token. */
	static bool ParseSteamSocketId(
		const FString& Address,
		FString& OutSteamId);

	/* Seat for a SteamSockets peer. The same ID
	 * reclaims its seat. A token does not move
	 * that peer onto a seat held by another ID. */
	static int32 FindSteamPeerSeat(
		const FSeatRecord (&SeatRows)[2],
		const FString& SteamId,
		const FString& Token);

	/* The placed board, including BP_GridManager.
	 * Server reads the cache. A client has no
	 * GameMode, so it resolves the same one actor. */
	UFUNCTION(
		BlueprintPure,
		Category = "Board",
		meta = (WorldContext = "WorldContext"))
	static AJoA_GridManager* GetGridManager(
		const UObject* WorldContext);

	/* Server only. The figure moved, so it is locked in
	 * for this turn. Another figure cannot be picked. */
	void NotifyFigureMoved(AJoA_Figure* Figure);

	/* Server only. Attack, heal, or obstacle hit.
	 * That ends the turn. */
	void NotifyFigureActed(AJoA_Figure* Figure);

	/* Server only. A player may pass at any point of
	 * their own turn. Another seat is ignored. */
	void RequestEndTurn(int32 Seat);

	/* Server only. A seat with no living figure
	 * has lost. */
	void NotifyFigureDied(AJoA_Figure* Figure);

	/* Server only. Both knights are dead, nobody
	 * is promoted, and neither side can cross,
	 * shoot, or bring a flag home. */
	void EvaluateBoardDraw();

	/* Server only. Enemy flag on the carrier's
	 * home file wins the match. */
	void NotifyFlagCarried(AJoA_Figure* Carrier);

	/* Server only. A controller that drops its figure
	 * says which one it held, so the seat gets it back
	 * when the player returns. */
	void RememberSeatFigure(
		int32 Seat,
		AJoA_Figure* Figure);

	/* Server only. The player left on purpose. The other
	 * seat wins at once, with no reconnect window. */
	void RequestLeaveMatch(int32 Seat);

	/* Server only. One pick in the throw that names
	 * who moves first. A second pick from that seat
	 * is ignored until a tie resets the round. */
	void SubmitOpeningThrow(
		int32 Seat,
		EOpeningThrow Throw);

private:
	static AJoA_GridManager* FindGridManager(
		UWorld* World);

	void EndTurn();
	void NotifyFiguresTurnEnded(int32 Seat);

	/* After a move. Passes the turn when no figure of the
	 * active seat can step, attack, heal, or hit an
	 * obstacle, even after turning. */
	void EvaluateAutoPass();

	/* A turn is handed to the seat. With no option the
	 * turn is not passed. The seat is told to skip. A
	 * seat still stuck on its next turn loses. */
	void BeginTurn(int32 Seat);

	bool SeatHasAnyAction(int32 Seat, FString* OutReason) const;

	/* Server only. Drops the possession of that seat and
	 * remembers the figure it held. */
	void ClearSeatSelection(int32 Seat);

	/* Server only. Selects the figure the seat is bound to
	 * by the turn: the committed one, else the one it held
	 * when it last played. A seat that never played, or
	 * whose figure fell, gets its Heavy Knight, or else the
	 * first figure left. */
	void RestoreSeatSelection(int32 Seat);

	// Server only. One of each class per side.
	void SpawnOpeningLineup();

	/* Server only. One flag per seat, beside its Mage. */
	void SpawnTeamFlag(int32 CellX, int32 CellY, int32 Seat);

	void AssignOpeningSeats();
	void AssignSeat(APlayerController* Controller);

	bool BothSeatsConnected() const;

	/* Unpaused and both seats here: wait for the
	 * throw. Unpaused with an empty seat: French
	 * opens, no throw. */
	void TryStartOrWaitOpeningThrow();

	void StartMatch(
		int32 OpeningSeat,
		EOpeningThrow WinningThrow);

	/* The seat a joining player takes. A known token gets
	 * its seat back even while the old connection lingers.
	 * A matching id only when that seat is waiting. Else
	 * the first empty seat. INDEX_NONE when both seats
	 * belong to others. bSteamPeer uses the socket ID. */
	int32 FindSeatForJoin(
		const FString& UniqueKey,
		const FString& Token,
		bool bIsHost,
		bool bSteamPeer) const;

	void RegisterSeat(APlayerController* Joining);

	void HandleSeatJoined(
		APlayerController* Joining,
		AJoA_PlayerState* JoinState,
		int32 Seat,
		const FString& UniqueKey);

	/* The connection of the seat is gone. The seat stays
	 * reserved, the match pauses, and the forfeit timer
	 * starts. */
	void HandleSeatDropped(
		int32 Seat,
		const AJoA_PlayerState* DroppedState);

	// The player did not come back in time.
	void ForfeitSeat(int32 Seat);

	void DeclareWinner(int32 Seat, EMatchEndReason Reason);

	/* Paused while a seat is Disconnected, or Empty when
	 * the settings wait for the opponent. */
	void RefreshPause();

	/* The one place that decides if the turn clock runs,
	 * waits, or stops. Called after every change that
	 * could move it. */
	void ApplyTurnTimer();

	// A new turn gets the whole limit again.
	void RestartTurnTimer();

	void HandleTurnExpired();

	UPROPERTY(VisibleAnywhere, Category = "Board")
	TObjectPtr<AJoA_GridManager> GridManager;

	/* Blueprint child with the Wood and Banner meshes.
	 * Without it the flag has no meshes. */
	UPROPERTY(EditDefaultsOnly, Category = "Board")
	TSubclassOf<AJoA_Flag> FlagClass;

	bool bOpeningLineupSpawned = false;

	// Last figure each seat held at the end of its turn.
	TWeakObjectPtr<AJoA_Figure> LastSelected[2];

	// The seat began its last turn with no option.
	bool bSeatWasBlocked[2] = { false, false };

	FSeatRecord Seats[2];

	/* Server only. The replicated mask says who
	 * picked, not what they picked. */
	EOpeningThrow OpeningThrows[2] = {
		EOpeningThrow::None,
		EOpeningThrow::None
	};

	FTimerHandle TurnTimer;
};
