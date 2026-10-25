// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

// --- Engine ---
#include "Subsystems/WorldSubsystem.h"

#include "JoA_FigureRegistry.generated.h"

class AJoA_Figure;

DECLARE_MULTICAST_DELEGATE_OneParam(
	FOnFigureRegistryEvent,
	AJoA_Figure*);
DECLARE_MULTICAST_DELEGATE(FOnBoardStateChanged);

/**
 * Living figures of this world, on the server and on
 * every client. A figure joins when it starts and
 * leaves when it dies or ends play. The HUD listens
 * here, because figures replicate one by one and a
 * single loop at startup would miss late ones.
 */
UCLASS()
class JOA_ARENA_API UJoA_FigureRegistry
	: public UWorldSubsystem
{
	GENERATED_BODY()

public:
	static UJoA_FigureRegistry* Get(
		const UObject* WorldContext);

	void RegisterFigure(AJoA_Figure* Figure);
	void UnregisterFigure(AJoA_Figure* Figure);

	/* Living figures only. The order is not stable. */
	void CollectFigures(
		TArray<AJoA_Figure*>& OutFigures) const;

	/* One player's living figures, in unit type order.
	 * That order is the order of keys 1 to 4. */
	void CollectFiguresOfSeat(
		int32 Seat,
		TArray<AJoA_Figure*>& OutFigures) const;

	AJoA_Figure* FindFigureAt(int32 X, int32 Y) const;

	/* Placement, an obstacle, or the turn changed.
	 * Controls recompute what they allow. */
	void NotifyBoardChanged();

	FOnFigureRegistryEvent OnFigureRegistered;
	FOnFigureRegistryEvent OnFigureUnregistered;
	FOnBoardStateChanged OnBoardStateChanged;

private:
	TArray<TWeakObjectPtr<AJoA_Figure>> Figures;
};
