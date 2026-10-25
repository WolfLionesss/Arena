// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

// --- GAS ---
#include "GAS/Abilities/JoA_GAS_GameplayAbility.h"

#include "JoA_GAS_FallAbility.generated.h"

/**
 * Step down from an obstacle onto the square
 * in front. Height 2 onto the ground costs
 * one health. Height 1, or 2 onto height 1,
 * costs nothing. Btn_Fall asks. No key. The
 * turn ends.
 */
UCLASS()
class JOA_ARENA_API UJoA_GAS_FallAbility
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
