// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

// --- Engine ---
#include "GameFramework/PlayerState.h"

// --- GAME ---
#include "Player/JoA_Nation.h"

#include "JoA_PlayerState.generated.h"

class AJoA_Figure;

/**
 * Per-player combo memory, shared by that player's figures.
 * Also names the seat of the player. The seat is not the
 * position in PlayerArray, which shifts when a player
 * leaves.
 */
UCLASS()
class JOA_ARENA_API AJoA_PlayerState : public APlayerState
{
	GENERATED_BODY()

public:
	AJoA_PlayerState();

	virtual void GetLifetimeReplicatedProps(
		TArray<FLifetimeProperty>& OutLifetimeProps
	) const override;

#if UE_WITH_IRIS
	virtual void RegisterReplicationFragments(
		UE::Net::FFragmentRegistrationContext& Context,
		UE::Net::EFragmentRegistrationFlags
			RegistrationFlags) override;
#endif

	/* Earlier attacks against Target in this combo.
	 * Another target returns 0. */
	int32 GetComboAttacksAgainst(
		const AJoA_Figure* Target) const;

	/* Server only. Records this attack. A new target
	 * starts the combo again from 1. */
	void CommitComboAttack(AJoA_Figure* Target);

	AJoA_Figure* GetComboTarget() const
	{
		return ComboTarget;
	}

	int32 GetComboAttackCount() const
	{
		return ComboAttackCount;
	}

	/* Server only. A player who came back gets the combo
	 * that the dropped player state held. */
	void RestoreCombo(AJoA_Figure* Target, int32 Count);

	// Server only. Seat 0 is French. Seat 1 is English.
	void SetNation(ENation InNation);

	ENation GetNation() const
	{
		return Nation;
	}

	// INDEX_NONE until the GameMode gives a seat.
	int32 GetSeatIndex() const
	{
		return SeatIndex;
	}

	// Server only.
	void SetSeatIndex(int32 InSeat);

	/* Server only. The secret the server gave this
	 * player. Sent back on reconnect, for a player with
	 * no online identity. */
	const FString& GetJoinToken() const
	{
		return JoinToken;
	}

	void SetJoinToken(const FString& InToken)
	{
		JoinToken = InToken;
	}

protected:
	UPROPERTY(Replicated, BlueprintReadOnly,
		Category = "Combat")
	TObjectPtr<AJoA_Figure> ComboTarget;

	/* Attacks already committed against ComboTarget.
	 * The next attack reads this, then adds one. */
	UPROPERTY(Replicated, BlueprintReadOnly,
		Category = "Combat")
	int32 ComboAttackCount = 0;

	// French until AssignSeat says otherwise.
	UPROPERTY(Replicated, BlueprintReadOnly,
		Category = "Nation")
	ENation Nation = ENation::French;

	UPROPERTY(
		ReplicatedUsing = OnRep_SeatIndex,
		BlueprintReadOnly,
		Category = "Nation")
	int32 SeatIndex = INDEX_NONE;

	UFUNCTION()
	void OnRep_SeatIndex();

	// Not replicated. Only the server compares it.
	FString JoinToken;
};
