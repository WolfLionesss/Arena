// Copyright Epic Games, Inc. All Rights Reserved.

#include "Player/JoA_PlayerState.h"

// --- GAME ---
#include "Player/Units/JoA_Figure.h"
#include "Player/Units/JoA_FigureRegistry.h"

// --- Net ---
#include "Net/Core/PushModel/PushModel.h"
#include "Net/UnrealNetwork.h"

#if UE_WITH_IRIS
#include "Iris/ReplicationSystem/ReplicationFragment.h"
#endif

AJoA_PlayerState::AJoA_PlayerState()
{
	/* Both players need the combo target so the client
	 * can show the focused figure. */
	bReplicates = true;
	SetReplicateMovement(false);
}

void AJoA_PlayerState::GetLifetimeReplicatedProps(
	TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	// Push model for these fields.
	/* Iris still replicates them through the actor
	 * fragment registered below. */
	FDoRepLifetimeParams Params;
	Params.bIsPushBased = true;
	DOREPLIFETIME_WITH_PARAMS_FAST(
		AJoA_PlayerState,
		ComboTarget,
		Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(
		AJoA_PlayerState,
		ComboAttackCount,
		Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(
		AJoA_PlayerState,
		Nation,
		Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(
		AJoA_PlayerState,
		SeatIndex,
		Params);
}

#if UE_WITH_IRIS
void AJoA_PlayerState::RegisterReplicationFragments(
	UE::Net::FFragmentRegistrationContext& Context,
	UE::Net::EFragmentRegistrationFlags RegistrationFlags)
{
	Super::RegisterReplicationFragments(
		Context,
		RegistrationFlags);
}
#endif

int32 AJoA_PlayerState::GetComboAttacksAgainst(
	const AJoA_Figure* Target) const
{
	/* Same figure, and still alive. Destroy() marks the
	 * actor pending kill before the UPROPERTY clears. */
	if (IsValid(Target) && Target == ComboTarget)
	{
		return ComboAttackCount;
	}

	return 0;
}

void AJoA_PlayerState::CommitComboAttack(
	AJoA_Figure* Target)
{
	if (!HasAuthority() || !IsValid(Target))
	{
		return;
	}

	const int32 Previous = GetComboAttacksAgainst(Target);
	ComboTarget = Target;
	ComboAttackCount = Previous + 1;

	MARK_PROPERTY_DIRTY_FROM_NAME(
		AJoA_PlayerState,
		ComboTarget,
		this);
	MARK_PROPERTY_DIRTY_FROM_NAME(
		AJoA_PlayerState,
		ComboAttackCount,
		this);
	ForceNetUpdate();
}

void AJoA_PlayerState::RestoreCombo(
	AJoA_Figure* Target,
	int32 Count)
{
	if (!HasAuthority())
	{
		return;
	}

	ComboTarget = Target;
	ComboAttackCount = Count;
	MARK_PROPERTY_DIRTY_FROM_NAME(
		AJoA_PlayerState,
		ComboTarget,
		this);
	MARK_PROPERTY_DIRTY_FROM_NAME(
		AJoA_PlayerState,
		ComboAttackCount,
		this);
	ForceNetUpdate();
}

void AJoA_PlayerState::OnRep_SeatIndex()
{
	/* Portraits built before the seat arrived sat
	 * in the opponent row and ignored clicks. */
	if (UJoA_FigureRegistry* Registry =
		UJoA_FigureRegistry::Get(this))
	{
		Registry->NotifyBoardChanged();
	}
}

void AJoA_PlayerState::SetSeatIndex(int32 InSeat)
{
	if (!HasAuthority())
	{
		return;
	}

	SeatIndex = InSeat;
	MARK_PROPERTY_DIRTY_FROM_NAME(
		AJoA_PlayerState,
		SeatIndex,
		this);
	ForceNetUpdate();
}

void AJoA_PlayerState::SetNation(ENation InNation)
{
	if (!HasAuthority())
	{
		return;
	}

	Nation = InNation;
	MARK_PROPERTY_DIRTY_FROM_NAME(
		AJoA_PlayerState,
		Nation,
		this);
	ForceNetUpdate();
}
