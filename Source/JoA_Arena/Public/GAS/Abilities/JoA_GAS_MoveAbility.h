// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

// --- GAS ---
#include "GAS/Abilities/JoA_GAS_GameplayAbility.h"

#include "JoA_GAS_MoveAbility.generated.h"

/**
 * One step of one square, relative to the Facing of the
 * figure. The direction is the InputID of the spec,
 * an EStepDirection. The figure decides whether that
 * square is free. W is the HUD Go key on the
 * controller. A, S, and D stay on this ability.
 */
UCLASS()
class JOA_ARENA_API UJoA_GAS_MoveAbility
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
