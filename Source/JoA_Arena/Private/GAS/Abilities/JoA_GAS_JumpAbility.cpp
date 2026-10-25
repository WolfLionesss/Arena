// Copyright Epic Games, Inc. All Rights Reserved.

#include "GAS/Abilities/JoA_GAS_JumpAbility.h"

// --- GAME ---
#include "Player/Units/JoA_Figure.h"

bool UJoA_GAS_JumpAbility::CanRunCommand(
	const AJoA_Figure* Figure,
	int32 CommandID) const
{
	(void)CommandID;
	return Figure && Figure->CanJump();
}

TArray<FKey> UJoA_GAS_JumpAbility::GetDefaultKeys(
	int32 CommandID) const
{
	(void)CommandID;
	return { EKeys::SpaceBar };
}

void UJoA_GAS_JumpAbility::ActivateAbility(
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
		Figure->ResolveJump();
	}

	EndAbility(
		Handle,
		ActorInfo,
		ActivationInfo,
		true,
		false);
}
