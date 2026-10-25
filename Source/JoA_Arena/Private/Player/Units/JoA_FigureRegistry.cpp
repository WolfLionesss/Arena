// Copyright Epic Games, Inc. All Rights Reserved.

#include "Player/Units/JoA_FigureRegistry.h"

// --- Engine ---
#include "Algo/Sort.h"
#include "Engine/World.h"

// --- GAME ---
#include "Player/Units/JoA_Figure.h"

UJoA_FigureRegistry* UJoA_FigureRegistry::Get(
	const UObject* WorldContext)
{
	const UWorld* World = WorldContext
		? WorldContext->GetWorld()
		: nullptr;
	return World
		? World->GetSubsystem<UJoA_FigureRegistry>()
		: nullptr;
}

void UJoA_FigureRegistry::RegisterFigure(
	AJoA_Figure* Figure)
{
	if (!IsValid(Figure) || Figure->IsDead())
	{
		return;
	}

	if (Figures.Contains(Figure))
	{
		return;
	}

	Figures.Add(Figure);
	OnFigureRegistered.Broadcast(Figure);
	OnBoardStateChanged.Broadcast();
}

void UJoA_FigureRegistry::UnregisterFigure(
	AJoA_Figure* Figure)
{
	if (!Figure)
	{
		return;
	}

	if (Figures.RemoveSingle(Figure) == 0)
	{
		return;
	}

	OnFigureUnregistered.Broadcast(Figure);
	OnBoardStateChanged.Broadcast();
}

void UJoA_FigureRegistry::CollectFigures(
	TArray<AJoA_Figure*>& OutFigures) const
{
	for (const TWeakObjectPtr<AJoA_Figure>& Entry : Figures)
	{
		AJoA_Figure* Figure = Entry.Get();
		if (IsValid(Figure) && !Figure->IsDead())
		{
			OutFigures.Add(Figure);
		}
	}
}

void UJoA_FigureRegistry::CollectFiguresOfSeat(
	int32 Seat,
	TArray<AJoA_Figure*>& OutFigures) const
{
	const int32 FirstIndex = OutFigures.Num();
	for (const TWeakObjectPtr<AJoA_Figure>& Entry : Figures)
	{
		AJoA_Figure* Figure = Entry.Get();
		if (IsValid(Figure)
			&& !Figure->IsDead()
			&& Figure->OwningPlayerIndex == Seat)
		{
			OutFigures.Add(Figure);
		}
	}

	/* Replication order differs per machine. Keys 1 to 4
	 * must pick the same figure for both players. */
	Algo::Sort(
		MakeArrayView(
			OutFigures.GetData() + FirstIndex,
			OutFigures.Num() - FirstIndex),
		[](const AJoA_Figure* A, const AJoA_Figure* B)
		{
			return static_cast<uint8>(A->UnitType)
				< static_cast<uint8>(B->UnitType);
		});
}

AJoA_Figure* UJoA_FigureRegistry::FindFigureAt(
	int32 X,
	int32 Y) const
{
	for (const TWeakObjectPtr<AJoA_Figure>& Entry : Figures)
	{
		AJoA_Figure* Figure = Entry.Get();
		if (IsValid(Figure)
			&& !Figure->IsDead()
			&& Figure->CellX == X
			&& Figure->CellY == Y)
		{
			return Figure;
		}
	}

	return nullptr;
}

void UJoA_FigureRegistry::NotifyBoardChanged()
{
	OnBoardStateChanged.Broadcast();
}
