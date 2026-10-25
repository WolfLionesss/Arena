// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

// --- GAS ---
#include "GAS/Abilities/JoA_GAS_GameplayAbility.h"

#include "JoA_GAS_RestAbility.generated.h"

/**
 * Skips the turn when this seat has not stepped,
 * attacked, or healed. Every living figure of
 * that seat gains one stamina and one mana.
 * Btn_Rest asks through one figure. There is
 * no key.
 */
UCLASS()
class JOA_ARENA_API UJoA_GAS_RestAbility
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
