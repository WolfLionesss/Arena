// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

// --- GAS ---
#include "GAS/Abilities/JoA_GAS_GameplayAbility.h"

#include "JoA_GAS_FlagAbility.generated.h"

/**
 * Pickup or drop. The InputID of the granted spec
 * is this value. I picks up. O drops.
 */
UENUM()
enum class EFlagCommand : uint8
{
	Pickup,
	Drop
};

/**
 * Pickup or drop of a board flag. Left mouse on
 * the flag and right mouse anywhere go through
 * the controller, then this ability.
 */
UCLASS()
class JOA_ARENA_API UJoA_GAS_FlagAbility
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
