// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "JoA_SeatTypes.generated.h"

/**
 * Link of one seat to a player. Empty until somebody
 * took the seat. Disconnected keeps the seat reserved
 * for its player until the reconnect window ends.
 */
UENUM(BlueprintType)
enum class ESeatConnection : uint8
{
	Empty,
	Connected,
	Disconnected
};

/**
 * Why the match ended. The HUD words a forfeit
 * differently from a lost board.
 */
UENUM(BlueprintType)
enum class EMatchEndReason : uint8
{
	None,
	Elimination,
	Stalemate,
	Forfeit,
	FlagCapture,
	Draw
};

/**
 * One pick in the throw that decides who moves
 * first. None means that seat has not picked.
 */
UENUM(BlueprintType)
enum class EOpeningThrow : uint8
{
	None,
	Rock,
	Paper,
	Scissors
};

/**
 * Replicated link state of one seat. The deadline is
 * server world time and is 0 unless the seat waits for
 * its player to come back.
 */
USTRUCT(BlueprintType)
struct FSeatLink
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Net")
	ESeatConnection State = ESeatConnection::Empty;

	UPROPERTY(BlueprintReadOnly, Category = "Net")
	double ReconnectDeadline = 0.0;
};
