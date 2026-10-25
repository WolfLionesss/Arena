// Copyright Epic Games, Inc. All Rights Reserved.

#include "Player/JoA_GameState.h"

// --- Engine ---
#include "Engine/World.h"

// --- GAME ---
#include "JoA_Arena.h"
#include "Match/JoA_GameInstance.h"
#include "Player/JoA_Nation.h"
#include "Player/JoA_PlayerController.h"
#include "Player/Units/JoA_Figure.h"
#include "Player/Units/JoA_FigureRegistry.h"

// --- Net ---
#include "Net/Core/PushModel/PushModel.h"
#include "Net/UnrealNetwork.h"

#if UE_WITH_IRIS
#include "Iris/ReplicationSystem/ReplicationFragment.h"
#endif

AJoA_GameState::AJoA_GameState()
{
	bReplicates = true;

	/* Actor default is 100 Hz. Turn state changes
	 * in bursts. ForceNetUpdate sends that burst. */
	SetNetUpdateFrequency(10.f);
	SetMinNetUpdateFrequency(2.f);
	SeatLinks.Init(FSeatLink(), 2);
}

AJoA_GameState* AJoA_GameState::Get(
	const UObject* WorldContext)
{
	const UWorld* World = WorldContext
		? WorldContext->GetWorld()
		: nullptr;
	return World
		? World->GetGameState<AJoA_GameState>()
		: nullptr;
}

void AJoA_GameState::BeginPlay()
{
	Super::BeginPlay();

	/* A client gets default values with no OnRep.
	 * The HUD still needs one pass to read them. */
	NotifyTurnChanged();
}

void AJoA_GameState::GetLifetimeReplicatedProps(
	TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	// Push model. Iris sends these through the fragment.
	FDoRepLifetimeParams Params;
	Params.bIsPushBased = true;
	DOREPLIFETIME_WITH_PARAMS_FAST(
		AJoA_GameState,
		ActiveSeat,
		Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(
		AJoA_GameState,
		bMatchStarted,
		Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(
		AJoA_GameState,
		OpeningThrowMask,
		Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(
		AJoA_GameState,
		OpeningWinnerSeat,
		Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(
		AJoA_GameState,
		OpeningWinningThrow,
		Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(
		AJoA_GameState,
		ActingFigure,
		Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(
		AJoA_GameState,
		bActingFigureMoved,
		Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(
		AJoA_GameState,
		WinnerSeat,
		Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(
		AJoA_GameState,
		MatchEndReason,
		Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(
		AJoA_GameState,
		bActiveSeatBlocked,
		Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(
		AJoA_GameState,
		bPaused,
		Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(
		AJoA_GameState,
		TurnDeadline,
		Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(
		AJoA_GameState,
		SeatLinks,
		Params);
}

#if UE_WITH_IRIS
void AJoA_GameState::RegisterReplicationFragments(
	UE::Net::FFragmentRegistrationContext& Context,
	UE::Net::EFragmentRegistrationFlags RegistrationFlags)
{
	Super::RegisterReplicationFragments(
		Context,
		RegistrationFlags);
}
#endif

AJoA_Figure* AJoA_GameState::GetActingFigure() const
{
	return ActingFigure;
}

FSeatLink AJoA_GameState::GetSeatLink(int32 Seat) const
{
	return SeatLinks.IsValidIndex(Seat)
		? SeatLinks[Seat]
		: FSeatLink();
}

float AJoA_GameState::SecondsUntil(double ServerTime) const
{
	return static_cast<float>(
		ServerTime - GetServerWorldTimeSeconds());
}

bool AJoA_GameState::AreBothSeatsConnected() const
{
	return GetSeatLink(FrenchSeat).State
			== ESeatConnection::Connected
		&& GetSeatLink(EnglishSeat).State
			== ESeatConnection::Connected;
}

void AJoA_GameState::BeginMatch(
	int32 InSeat,
	EOpeningThrow InThrow)
{
	if (!HasAuthority() || bMatchStarted)
	{
		return;
	}

	bMatchStarted = true;
	OpeningWinnerSeat = InSeat;
	OpeningWinningThrow = InThrow;
	MARK_PROPERTY_DIRTY_FROM_NAME(
		AJoA_GameState,
		bMatchStarted,
		this);
	MARK_PROPERTY_DIRTY_FROM_NAME(
		AJoA_GameState,
		OpeningWinnerSeat,
		this);
	MARK_PROPERTY_DIRTY_FROM_NAME(
		AJoA_GameState,
		OpeningWinningThrow,
		this);
	SetActiveSeat(InSeat);
}

void AJoA_GameState::SetOpeningThrowMask(uint8 InMask)
{
	if (!HasAuthority() || OpeningThrowMask == InMask)
	{
		return;
	}

	OpeningThrowMask = InMask;
	MARK_PROPERTY_DIRTY_FROM_NAME(
		AJoA_GameState,
		OpeningThrowMask,
		this);
	NotifyTurnChanged();
}

void AJoA_GameState::SetOpeningPicks(
	bool bFrench,
	bool bEnglish)
{
	/* Bits 2-3 name a tie from the previous
	 * round. The next picks must not wipe that,
	 * or the other seat loses the draw line. */
	uint8 Mask = OpeningThrowMask & 0xFC;
	if (bFrench)
	{
		Mask |= 1;
	}
	if (bEnglish)
	{
		Mask |= 2;
	}
	SetOpeningThrowMask(Mask);
}

void AJoA_GameState::SetOpeningTie(EOpeningThrow Throw)
{
	const uint8 Code = static_cast<uint8>(Throw);
	SetOpeningThrowMask(Code << 2);
}

bool AJoA_GameState::CanSeatSelect(int32 Seat) const
{
	return IsPlayable()
		&& Seat == ActiveSeat
		&& !IsValid(ActingFigure);
}

bool AJoA_GameState::CanFigureAct(
	const AJoA_Figure* Figure) const
{
	if (!IsValid(Figure)
		|| Figure->IsDead()
		|| !IsPlayable()
		|| Figure->OwningPlayerIndex != ActiveSeat)
	{
		return false;
	}

	return !IsValid(ActingFigure) || ActingFigure == Figure;
}

bool AJoA_GameState::CanFigureMove(
	const AJoA_Figure* Figure) const
{
	return CanFigureAct(Figure)
		&& !(ActingFigure == Figure && bActingFigureMoved);
}

void AJoA_GameState::SetActiveSeat(int32 InSeat)
{
	if (!HasAuthority())
	{
		return;
	}

	ActiveSeat = InSeat;
	ActingFigure = nullptr;
	bActingFigureMoved = false;
	bActiveSeatBlocked = false;
	MARK_PROPERTY_DIRTY_FROM_NAME(
		AJoA_GameState,
		bActiveSeatBlocked,
		this);
	MARK_PROPERTY_DIRTY_FROM_NAME(
		AJoA_GameState,
		ActiveSeat,
		this);
	MARK_PROPERTY_DIRTY_FROM_NAME(
		AJoA_GameState,
		ActingFigure,
		this);
	MARK_PROPERTY_DIRTY_FROM_NAME(
		AJoA_GameState,
		bActingFigureMoved,
		this);
	NotifyTurnChanged();
}

void AJoA_GameState::SetActiveSeatBlocked(bool bInBlocked)
{
	if (!HasAuthority() || bActiveSeatBlocked == bInBlocked)
	{
		return;
	}

	bActiveSeatBlocked = bInBlocked;
	MARK_PROPERTY_DIRTY_FROM_NAME(
		AJoA_GameState,
		bActiveSeatBlocked,
		this);
	NotifyTurnChanged();
}

void AJoA_GameState::CommitActingFigure(
	AJoA_Figure* Figure,
	bool bMoved)
{
	if (!HasAuthority())
	{
		return;
	}

	ActingFigure = Figure;
	bActingFigureMoved = bMoved;
	MARK_PROPERTY_DIRTY_FROM_NAME(
		AJoA_GameState,
		ActingFigure,
		this);
	MARK_PROPERTY_DIRTY_FROM_NAME(
		AJoA_GameState,
		bActingFigureMoved,
		this);
	NotifyTurnChanged();
}

void AJoA_GameState::SetWinnerSeat(
	int32 InSeat,
	EMatchEndReason Reason)
{
	if (!HasAuthority())
	{
		return;
	}

	WinnerSeat = InSeat;
	MatchEndReason = Reason;
	MARK_PROPERTY_DIRTY_FROM_NAME(
		AJoA_GameState,
		WinnerSeat,
		this);
	MARK_PROPERTY_DIRTY_FROM_NAME(
		AJoA_GameState,
		MatchEndReason,
		this);
	NotifyTurnChanged();
}

void AJoA_GameState::SetPaused(bool bInPaused)
{
	if (!HasAuthority() || bPaused == bInPaused)
	{
		return;
	}

	bPaused = bInPaused;
	MARK_PROPERTY_DIRTY_FROM_NAME(
		AJoA_GameState,
		bPaused,
		this);
	NotifyTurnChanged();
}

void AJoA_GameState::SetTurnDeadline(double InDeadline)
{
	if (!HasAuthority() || TurnDeadline == InDeadline)
	{
		return;
	}

	TurnDeadline = InDeadline;
	MARK_PROPERTY_DIRTY_FROM_NAME(
		AJoA_GameState,
		TurnDeadline,
		this);
	NotifyTurnChanged();
}

void AJoA_GameState::SetSeatLink(
	int32 Seat,
	ESeatConnection InState,
	double InDeadline)
{
	if (!HasAuthority() || !SeatLinks.IsValidIndex(Seat))
	{
		return;
	}

	SeatLinks[Seat].State = InState;
	SeatLinks[Seat].ReconnectDeadline = InDeadline;
	MARK_PROPERTY_DIRTY_FROM_NAME(
		AJoA_GameState,
		SeatLinks,
		this);
	NotifyTurnChanged();
}

void AJoA_GameState::OnRep_TurnState()
{
	NotifyTurnChanged();
}

void AJoA_GameState::OnRep_SeatLinks()
{
	const int32 Count = FMath::Min(SeatLinks.Num(), 2);
	for (int32 Seat = 0; Seat < Count; ++Seat)
	{
		const ESeatConnection Was = LoggedLinks[Seat];
		const ESeatConnection Now = SeatLinks[Seat].State;
		if (Was == Now)
		{
			continue;
		}

		LoggedLinks[Seat] = Now;
		const FString WasText =
			UEnum::GetValueAsString(Was);
		const FString NowText =
			UEnum::GetValueAsString(Now);
		if (Now == ESeatConnection::Disconnected)
		{
			UE_LOG(
				LogJoA_Net,
				Warning,
				TEXT("Seat %d link %s -> %s."),
				Seat,
				*WasText,
				*NowText);
		}
		else
		{
			UE_LOG(
				LogJoA_Net,
				Log,
				TEXT("Seat %d link %s -> %s."),
				Seat,
				*WasText,
				*NowText);
		}
	}

	NotifyTurnChanged();
}

void AJoA_GameState::NotifyTurnChanged()
{
	if (HasAuthority())
	{
		ForceNetUpdate();
	}

	/* A finished match has nothing to come back to. The
	 * client stops trying to rejoin it. */
	if (IsMatchOver())
	{
		if (UJoA_GameInstance* Instance =
			GetGameInstance<UJoA_GameInstance>())
		{
			Instance->ClearReconnectTicket();
		}
	}

	if (UJoA_FigureRegistry* Registry =
		UJoA_FigureRegistry::Get(this))
	{
		Registry->NotifyBoardChanged();
	}

	/* Rebuild can steal keys. QWE must work
	 * on the seat that just received the turn. */
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	for (FConstPlayerControllerIterator It =
			World->GetPlayerControllerIterator();
		It;
		++It)
	{
		AJoA_PlayerController* Controller =
			Cast<AJoA_PlayerController>(It->Get());
		if (Controller && Controller->IsLocalController())
		{
			Controller->FocusGameViewport();
		}
	}
}
