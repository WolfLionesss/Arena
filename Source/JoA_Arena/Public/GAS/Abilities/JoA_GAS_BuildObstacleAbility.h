// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

// --- GAS ---
#include "GAS/Abilities/JoA_GAS_GameplayAbility.h"

#include "JoA_GAS_BuildObstacleAbility.generated.h"

/**
 * Light infantry raises the square in front.
 * An empty square becomes height 1. Height 1
 * becomes 2. Height 2, a unit, a flag on the
 * square, or a flag in hand blocks it. The
 * turn ends. The button and the left click
 * ask. There is no key.
 */
UCLASS()
class JOA_ARENA_API UJoA_GAS_BuildObstacleAbility
	: public UJoA_GAS_GameplayAbility
{
	GENERATED_BODY()

protected:
	virtual bool CanRunCommand(
		const AJoA_Figure* Figure,
		int32 CommandID) const override;

	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData
	) override;
};
