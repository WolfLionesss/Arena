// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

// --- GAS ---
#include "GameplayEffect.h"

#include "JoA_GAS_PromotionEffect.generated.h"

/**
 * Instant carrier for one promotion. Caps and
 * combat stats are written by the execution.
 * Current Health and the used resource fill
 * on the figure afterwards.
 */
UCLASS()
class JOA_ARENA_API UJoA_GAS_PromotionEffect
	: public UGameplayEffect
{
	GENERATED_BODY()

public:
	UJoA_GAS_PromotionEffect();
};
