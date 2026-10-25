// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

// --- GAS ---
#include "GAS/Abilities/JoA_GAS_GameplayAbility.h"

#include "JoA_GAS_TurnAbility.generated.h"

/**
 * Turns the figure 45 degrees on the 8-way Facing ring.
 * The direction is the InputID of the spec, an
 * ETurnDirection. Turning is free and does not commit
 * the figure. Q and E are on the controller.
 */
UCLASS()
class JOA_ARENA_API UJoA_GAS_TurnAbility
	: public UJoA_GAS_GameplayAbility
{
	GENERATED_BODY()

protected:
	virtual bool CanRunCommand(
		const AJoA_Figure* Figure,
		int32 CommandID) const override;

	virtual TArray<FKey> GetDefaultKeys(
		int32 CommandID) const override;

	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData
	) override;
};
