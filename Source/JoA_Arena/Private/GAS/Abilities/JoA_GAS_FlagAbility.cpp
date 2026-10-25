// Copyright Epic Games, Inc. All Rights Reserved.

#include "GAS/Abilities/JoA_GAS_FlagAbility.h"

// --- GAME ---
#include "Player/Units/JoA_Figure.h"

bool UJoA_GAS_FlagAbility::CanRunCommand(
	const AJoA_Figure* Figure,
	int32 CommandID) const
{
	if (!Figure)
	{
		return false;
	}

	switch (static_cast<EFlagCommand>(CommandID))
	{
	case EFlagCommand::Pickup:
		return Figure->CanPickupFlag();
	case EFlagCommand::Drop:
		return Figure->CanDropFlag();
	default:
		return false;
	}
}

TArray<FKey> UJoA_GAS_FlagAbility::GetDefaultKeys(
	int32 CommandID) const
{
	switch (static_cast<EFlagCommand>(CommandID))
	{
	case EFlagCommand::Pickup:
		return { EKeys::I };
	case EFlagCommand::Drop:
		return { EKeys::O };
	default:
		return TArray<FKey>();
	}
}

void UJoA_GAS_FlagAbility::ActivateAbility(
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
	if (Figure && Figure->HasAuthority())
	{
		if (CommandID
			== static_cast<int32>(EFlagCommand::Pickup))
		{
			Figure->ResolvePickupFlag();
		}
		else if (CommandID
			== static_cast<int32>(EFlagCommand::Drop))
		{
			Figure->ResolveDropFlag();
		}
	}

	EndAbility(
		Handle,
		ActorInfo,
		ActivationInfo,
		true,
		false);
}
