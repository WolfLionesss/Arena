// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

// --- GAS ---
#include "GameplayEffect.h"

#include "JoA_GAS_VitalEffect.generated.h"

/**
 * Instant carrier for UJoA_GAS_VitalExecution.
 * The spec supplies SetByCaller magnitudes.
 * No modifier touches Health, Stamina, or Mana.
 */
UCLASS()
class JOA_ARENA_API UJoA_GAS_VitalEffect
	: public UGameplayEffect
{
	GENERATED_BODY()

public:
	UJoA_GAS_VitalEffect();
};
