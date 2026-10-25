// Copyright Epic Games, Inc. All Rights Reserved.

#include "GAS/Abilities/JoA_GAS_MoveAbility.h"

// --- GAME ---
#include "Player/Units/JoA_Figure.h"
#include "Player/Units/JoA_UnitTypes.h"

bool UJoA_GAS_MoveAbility::CanRunCommand(
	const AJoA_Figure* Figure,
	int32 CommandID) const
{
	if (!Figure || CommandID < 0 || CommandID > 3)
	{
		return false;
	}

	return Figure->CanStep(
		static_cast<EStepDirection>(CommandID));
}

TArray<FKey> UJoA_GAS_MoveAbility::GetDefaultKeys(
	int32 CommandID) const
{
	switch (static_cast<EStepDirection>(CommandID))
	{
	case EStepDirection::Forward:
		/* W is the HUD Go key on the controller.
		 * A second map here would step twice. */
		return TArray<FKey>();
	case EStepDirection::Back:
		return { EKeys::S };
	case EStepDirection::Left:
		return { EKeys::A };
	case EStepDirection::Right:
		return { EKeys::D };
	default:
		return TArray<FKey>();
	}
}

void UJoA_GAS_MoveAbility::ActivateAbility(
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
		&& CommandID >= 0
		&& CommandID <= 3)
	{
		Figure->ResolveStep(
			static_cast<EStepDirection>(CommandID));
	}

	EndAbility(
		Handle,
		ActorInfo,
		ActivationInfo,
		true,
		false);
}
