// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "JoA_Nation.generated.h"

/**
 * Match side. French is seat 0, blue, and +X.
 * English is seat 1, red, and -X.
 */
UENUM(BlueprintType)
enum class ENation : uint8
{
	French UMETA(DisplayName = "French"),
	English UMETA(DisplayName = "English")
};

inline constexpr int32 FrenchSeat = 0;
inline constexpr int32 EnglishSeat = 1;

inline ENation GetNation(int32 Seat)
{
	return Seat == FrenchSeat
		? ENation::French
		: ENation::English;
}

inline FLinearColor GetTeamColor(ENation Nation)
{
	return Nation == ENation::French
		? FLinearColor(FColor::FromHex(TEXT("52A3E6")))
		: FLinearColor(FColor::FromHex(TEXT("D14747")));
}

inline FLinearColor GetTeamColor(int32 Seat)
{
	return GetTeamColor(GetNation(Seat));
}
