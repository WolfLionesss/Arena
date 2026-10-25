// Copyright Epic Games, Inc. All Rights Reserved.

#include "GAS/Abilities/JoA_GAS_SkipTurnAbility.h"

// --- GAME ---
#include "Player/Units/JoA_Figure.h"

bool UJoA_GAS_SkipTurnAbility::CanRunCommand(
	const AJoA_Figure* Figure,
	int32 CommandID) const
{
	(void)CommandID;
	return Figure && Figure->CanSkipTurn();
}

TArray<FKey> UJoA_GAS_SkipTurnAbility::GetDefaultKeys(
	int32 CommandID) const
{
	(void)CommandID;
	return TArray<FKey>();
}

void UJoA_GAS_SkipTurnAbility::ActivateAbility(
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
		Figure->ResolveSkipTurn();
	}

	EndAbility(
		Handle,
		ActorInfo,
		ActivationInfo,
		true,
		false);
}
