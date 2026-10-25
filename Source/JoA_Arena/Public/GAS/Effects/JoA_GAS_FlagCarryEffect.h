// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

// --- GAS ---
#include "GameplayEffect.h"

#include "JoA_GAS_FlagCarryEffect.generated.h"

/**
 * Kept for a carried-flag state. Attack and Heal
 * stay at base. Combat is blocked on the figure
 * while it holds a flag.
 */
UCLASS()
class JOA_ARENA_API UJoA_GAS_FlagCarryEffect
	: public UGameplayEffect
{
	GENERATED_BODY()

public:
	UJoA_GAS_FlagCarryEffect();
};
