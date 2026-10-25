// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

// --- GAS ---
#include "GAS/Abilities/JoA_GAS_GameplayAbility.h"

#include "JoA_GAS_SkipTurnAbility.generated.h"

/**
 * Passes the turn without acting. A player may skip at
 * any point of their own turn, before or after a move.
 * It is granted on every figure, so the selected figure
 * carries it. There is no key. Btn_SkipTurn asks.
 */
UCLASS()
class JOA_ARENA_API UJoA_GAS_SkipTurnAbility
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
