// Copyright Epic Games, Inc. All Rights Reserved.

#include "GAS/Abilities/JoA_GAS_FallAbility.h"

// --- GAME ---
#include "Player/Units/JoA_Figure.h"

bool UJoA_GAS_FallAbility::CanRunCommand(
	const AJoA_Figure* Figure,
	int32 CommandID) const
{
	(void)CommandID;
	return Figure && Figure->CanFall();
}

void UJoA_GAS_FallAbility::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(
		Handle,
		ActorInfo,
		ActivationInfo,
		TriggerEventData);

	AJoA_Figure* Figure = GetFigure(ActorInfo);
	if (Figure && Figure->HasAuthority())
	{
		Figure->ResolveFall();
	}

	EndAbility(
		Handle,
		ActorInfo,
		ActivationInfo,
		true,
		false);
}
