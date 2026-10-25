// Copyright Epic Games, Inc. All Rights Reserved.

#include "GAS/Abilities/JoA_GAS_BuildObstacleAbility.h"

// --- GAME ---
#include "Player/Units/JoA_Figure.h"

bool UJoA_GAS_BuildObstacleAbility::CanRunCommand(
	const AJoA_Figure* Figure,
	int32 CommandID) const
{
	(void)CommandID;
	FIntPoint Cell = FIntPoint::ZeroValue;
	return Figure && Figure->FindBuildableCell(Cell);
}

void UJoA_GAS_BuildObstacleAbility::ActivateAbility(
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
		Figure->ResolveBuildObstacle();
	}

	EndAbility(
		Handle,
		ActorInfo,
		ActivationInfo,
		true,
		false);
}
