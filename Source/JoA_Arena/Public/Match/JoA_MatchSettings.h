// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

// --- Engine ---
#include "Engine/DeveloperSettings.h"

#include "JoA_MatchSettings.generated.h"

/**
 * Match rules both machines read. The server enforces
 * them. The client reads the same values to know how
 * long it keeps trying to get back into a match.
 */
UCLASS(Config = Game, DefaultConfig,
	meta = (DisplayName = "JoA Match"))
class JOA_ARENA_API UJoA_MatchSettings
	: public UDeveloperSettings
{
	GENERATED_BODY()

public:
	virtual FName GetCategoryName() const override;

	/* Seconds one turn lasts. When they run out the turn
	 * passes. 0 turns the limit off, as automated tests
	 * need. */
	UPROPERTY(
		Config,
		EditAnywhere,
		Category = "Turn",
		meta = (ClampMin = "0"))
	float TurnSeconds = 30.f;

	/* Seconds a seat is held for a player who lost the
	 * connection. After that the opponent wins. */
	UPROPERTY(
		Config,
		EditAnywhere,
		Category = "Net",
		meta = (ClampMin = "0"))
	float ReconnectGraceSeconds = 120.f;

	// Pause between two reconnect attempts of a client.
	UPROPERTY(
		Config,
		EditAnywhere,
		Category = "Net",
		meta = (ClampMin = "0.5"))
	float ClientRetryIntervalSeconds = 3.f;

	/* A client keeps trying this long after the grace
	 * window, in case the server clock ran slower. */
	UPROPERTY(
		Config,
		EditAnywhere,
		Category = "Net",
		meta = (ClampMin = "0"))
	float ClientRetryMarginSeconds = 15.f;

	/* The match stays paused until both seats have a
	 * player. Turn it off to test alone in one window. */
	UPROPERTY(Config, EditAnywhere, Category = "Net")
	bool bWaitForOpponent = true;
};
