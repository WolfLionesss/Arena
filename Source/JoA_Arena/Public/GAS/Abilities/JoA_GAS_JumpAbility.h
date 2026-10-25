// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

// --- GAS ---
#include "GAS/Abilities/JoA_GAS_GameplayAbility.h"

#include "JoA_GAS_JumpAbility.generated.h"

/**
 * Climb onto the obstacle in front. Height may
 * rise by one. Height 2 from the ground is
 * refused. HeavyKnight never has this. Space
 * and Btn_Jump ask. The turn ends.
 */
UCLASS()
class JOA_ARENA_API UJoA_GAS_JumpAbility
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
