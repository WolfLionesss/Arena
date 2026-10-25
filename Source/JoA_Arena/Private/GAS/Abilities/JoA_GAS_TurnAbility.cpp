// Copyright Epic Games, Inc. All Rights Reserved.

#include "GAS/Abilities/JoA_GAS_TurnAbility.h"

// --- GAME ---
#include "Player/Units/JoA_Figure.h"
#include "Player/Units/JoA_UnitTypes.h"

bool UJoA_GAS_TurnAbility::CanRunCommand(
	const AJoA_Figure* Figure,
	int32 CommandID) const
{
	return Figure
		&& (CommandID == 0 || CommandID == 1)
		&& Figure->CanTurn();
}

TArray<FKey> UJoA_GAS_TurnAbility::GetDefaultKeys(
	int32 CommandID) const
{
	(void)CommandID;
	/* Q and E are the HUD turn keys on the
	 * controller. A second map here would
	 * turn twice. */
	return TArray<FKey>();
}

void UJoA_GAS_TurnAbility::ActivateAbility(
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
	const int32 CommandID = GetCommandID(Handle, ActorInfo);
	if (Figure
		&& Figure->HasAuthority()
		&& (CommandID == 0 || CommandID == 1))
	{
		Figure->ResolveTurn(
			static_cast<ETurnDirection>(CommandID));
	}

	EndAbility(
		Handle,
		ActorInfo,
		ActivationInfo,
		true,
		false);
}
