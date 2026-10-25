// Copyright Epic Games, Inc. All Rights Reserved.

#include "Match/JoA_GameMode.h"

// --- Engine ---
#include "CoreGlobals.h"
#include "Engine/NetConnection.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/GameSession.h"
#include "GameFramework/GameStateBase.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"

// --- GAME ---
#include "Player/JoA_GameState.h"
#include "Player/JoA_PlayerController.h"
#include "Player/JoA_PlayerState.h"
#include "Player/JoA_Pawn.h"
#include "Board/JoA_Flag.h"
#include "Board/JoA_GridManager.h"
#include "JoA_Arena.h"
#include "Match/JoA_GameInstance.h"
#include "Match/JoA_MatchSettings.h"
#include "Player/JoA_Nation.h"
#include "Player/Units/JoA_CombatLibrary.h"
#include "Player/Units/JoA_Figure.h"
#include "Player/Units/JoA_FigureRegistry.h"
#include "Player/Units/JoA_UnitTypes.h"

// --- Net ---
#include "Net/Core/PushModel/PushModel.h"

namespace
{
	struct FOpeningSquare
	{
		EUnitType Type;
		int32 X;
		int32 Y;
		EFacing Facing;
		int32 PlayerIndex;
	};

	/* French camera looks along -X, so the
	 * near edge is X = 7. Left to right is
	 * Y = 7,6,5,4,3,2,1,0. Squares 3 to 6
	 * are Y 5,4,3,2. French faces inward.
	 * English is the copy, facing back. */
	const FOpeningSquare OpeningLine[] =
	{
		{ EUnitType::HeavyKnight, 7, 5,
			EFacing::NegativeX, FrenchSeat },
		{ EUnitType::LightInfantry, 7, 4,
			EFacing::NegativeX, FrenchSeat },
		{ EUnitType::Archer, 7, 3,
			EFacing::NegativeX, FrenchSeat },
		{ EUnitType::Medic, 7, 2,
			EFacing::NegativeX, FrenchSeat },
		{ EUnitType::HeavyKnight, 0, 2,
			EFacing::PositiveX, EnglishSeat },
		{ EUnitType::LightInfantry, 0, 3,
			EFacing::PositiveX, EnglishSeat },
		{ EUnitType::Archer, 0, 4,
			EFacing::PositiveX, EnglishSeat },
		{ EUnitType::Medic, 0, 5,
			EFacing::PositiveX, EnglishSeat },
	};

	bool IsValidSeat(int32 Seat)
	{
		return Seat == FrenchSeat || Seat == EnglishSeat;
	}

	int32 OtherSeat(int32 Seat)
	{
		return Seat == FrenchSeat
			? EnglishSeat
			: FrenchSeat;
	}

	FString UniqueKeyOf(const FUniqueNetIdRepl& UniqueId)
	{
		return UniqueId.IsValid()
			? UniqueId.ToString()
			: FString();
	}

	const TCHAR* ReconnectTokenOption =
		TEXT("ReconnectToken");

	/* Account SteamID64 is 17 digits. A port or
	 * an IP octet is shorter, so it stays out. */
	const int32 MinSteamIdDigits = 16;

	struct FSeatReach
	{
		bool Cell[GridSize][GridSize];
	};

	const FIntPoint ReachDeltas[] = {
		FIntPoint(1, 0),
		FIntPoint(-1, 0),
		FIntPoint(0, 1),
		FIntPoint(0, -1),
		FIntPoint(1, 1),
		FIntPoint(1, -1),
		FIntPoint(-1, 1),
		FIntPoint(-1, -1)
	};

	/* Unpromoted climb. Height 2 is entered only
	 * from height 1. Ground cannot step onto it. */
	void FloodFigure(
		const AJoA_GridManager* Grid,
		const AJoA_Figure* Figure,
		FSeatReach& Out)
	{
		FMemory::Memzero(Out.Cell, sizeof(Out.Cell));
		if (!Grid || !Figure)
		{
			return;
		}

		TArray<FIntPoint> Open;
		const int32 StartX = Figure->CellX;
		const int32 StartY = Figure->CellY;
		if (StartX < 0 || StartY < 0
			|| StartX >= GridSize
			|| StartY >= GridSize)
		{
			return;
		}

		Out.Cell[StartX][StartY] = true;
		Open.Add(FIntPoint(StartX, StartY));
		for (int32 Index = 0; Index < Open.Num(); ++Index)
		{
			const FIntPoint At = Open[Index];
			FGridCell HereCell;
			if (!Grid->GetCell(At.X, At.Y, HereCell))
			{
				continue;
			}

			const uint8 Here = HereCell.ObstacleHeight;
			for (const FIntPoint& Delta : ReachDeltas)
			{
				const int32 X = At.X + Delta.X;
				const int32 Y = At.Y + Delta.Y;
				if (X < 0 || Y < 0
					|| X >= GridSize || Y >= GridSize
					|| Out.Cell[X][Y])
				{
					continue;
				}

				FGridCell DestCell;
				if (!Grid->GetCell(X, Y, DestCell))
				{
					continue;
				}

				const uint8 Dest = DestCell.ObstacleHeight;
				const int32 Rise = static_cast<int32>(Here) + 1;
				const bool bStep = Here == 0 && Dest == 0;
				const bool bJump = Dest > 0
					&& Dest >= Here
					&& Dest <= Rise;
				const bool bFall = Here > 0 && Dest < Here;
				if (!bStep && !bJump && !bFall)
				{
					continue;
				}

				Out.Cell[X][Y] = true;
				Open.Add(FIntPoint(X, Y));
			}
		}
	}

	bool ReachContainsHome(
		const FSeatReach& Reach,
		int32 Seat)
	{
		const int32 HomeX = Seat == FrenchSeat
			? GridSize - 1
			: 0;
		for (int32 Y = 0; Y < GridSize; ++Y)
		{
			if (Reach.Cell[HomeX][Y])
			{
				return true;
			}
		}
		return false;
	}

	/* Enemy flag and this seat's home file are both
	 * in the same walk, so the flag can be scored. */
	bool CanBringFlagHome(
		UWorld* World,
		int32 Seat,
		const FSeatReach& Reach)
	{
		if (!World || !ReachContainsHome(Reach, Seat))
		{
			return false;
		}

		for (TActorIterator<AJoA_Flag> It(World); It; ++It)
		{
			const int32 X = It->GetCellX();
			const int32 Y = It->GetCellY();
			if (It->GetOwningPlayerIndex() == Seat
				|| X < 0 || Y < 0
				|| X >= GridSize || Y >= GridSize)
			{
				continue;
			}

			if (Reach.Cell[X][Y])
			{
				return true;
			}
		}
		return false;
	}

	bool CanStrikeReach(
		const UObject* World,
		const FSeatReach& FromReach,
		const FSeatReach& EnemyReach,
		int32 MaxSteps,
		EReachPolicy Policy)
	{
		for (int32 FromX = 0; FromX < GridSize; ++FromX)
		{
			for (int32 FromY = 0; FromY < GridSize; ++FromY)
			{
				if (!FromReach.Cell[FromX][FromY])
				{
					continue;
				}

				const FIntPoint From(FromX, FromY);
				for (int32 Turn = 0; Turn < 8; ++Turn)
				{
					const EFacing Facing =
						AJoA_Figure::RotateFacingBy(
							EFacing::PositiveX,
							Turn);
					for (int32 X = 0; X < GridSize; ++X)
					{
						for (int32 Y = 0; Y < GridSize; ++Y)
						{
							if (!EnemyReach.Cell[X][Y])
							{
								continue;
							}

							if (UJoA_CombatLibrary::IsCellInFront(
								World,
								From,
								FIntPoint(X, Y),
								Facing,
								MaxSteps,
								Policy))
							{
								return true;
							}
						}
					}
				}
			}
		}
		return false;
	}

	bool IsSplitDraw(const AJoA_GameMode* Mode)
	{
		UWorld* World = Mode ? Mode->GetWorld() : nullptr;
		const AJoA_GridManager* Grid =
			AJoA_GameMode::GetGridManager(Mode);
		if (!World || !Grid)
		{
			return false;
		}

		TArray<AJoA_Figure*> Living;
		bool bKnight = false;
		bool bPromoted = false;
		bool bFrench = false;
		bool bEnglish = false;
		for (TActorIterator<AJoA_Figure> It(World); It; ++It)
		{
			AJoA_Figure* Figure = *It;
			if (!IsValid(Figure) || Figure->IsDead())
			{
				continue;
			}

			if (Figure->UnitType == EUnitType::HeavyKnight)
			{
				bKnight = true;
			}
			if (Figure->IsPromoted())
			{
				bPromoted = true;
			}
			if (Figure->OwningPlayerIndex == FrenchSeat)
			{
				bFrench = true;
			}
			else if (
				Figure->OwningPlayerIndex == EnglishSeat)
			{
				bEnglish = true;
			}
			Living.Add(Figure);
		}

		if (bKnight || bPromoted || !bFrench || !bEnglish)
		{
			return false;
		}

		TArray<FSeatReach> Reaches;
		Reaches.SetNum(Living.Num());
		for (int32 Index = 0; Index < Living.Num(); ++Index)
		{
			FloodFigure(Grid, Living[Index], Reaches[Index]);
			if (CanBringFlagHome(
				World,
				Living[Index]->OwningPlayerIndex,
				Reaches[Index]))
			{
				return false;
			}
		}

		for (int32 Index = 0; Index < Living.Num(); ++Index)
		{
			const AJoA_Figure* Attacker = Living[Index];
			const FUnitStats Defaults =
				FUnitStats::GetDefaults(Attacker->UnitType);
			if (!Defaults.bCanAttack)
			{
				continue;
			}

			const int32 MaxSteps =
				Defaults.ReachPolicy == EReachPolicy::Archer
					? GridSize - 1
					: Defaults.Range;
			for (int32 Other = 0;
				Other < Living.Num();
				++Other)
			{
				if (Living[Other]->OwningPlayerIndex
					== Attacker->OwningPlayerIndex)
				{
					continue;
				}

				if (CanStrikeReach(
					Mode,
					Reaches[Index],
					Reaches[Other],
					MaxSteps,
					Defaults.ReachPolicy))
				{
					return false;
				}
			}
		}
		return true;
	}
}

bool AJoA_GameMode::ParseSteamSocketId(
	const FString& Address,
	FString& OutSteamId)
{
	FString Rest = Address.TrimStartAndEnd();
	if (Rest.StartsWith(
			TEXT("steam."),
			ESearchCase::IgnoreCase))
	{
		Rest.RightChopInline(6);
	}

	int32 Colon = INDEX_NONE;
	if (Rest.FindChar(TEXT(':'), Colon))
	{
		const FString Left = Rest.Left(Colon);
		const FString Port = Rest.Mid(Colon + 1);
		if (!Left.IsNumeric() || !Port.IsNumeric())
		{
			return false;
		}
		Rest = Left;
	}

	if (!Rest.IsNumeric()
		|| Rest.Len() < MinSteamIdDigits)
	{
		return false;
	}

	OutSteamId = Rest;
	return true;
}

int32 AJoA_GameMode::FindSteamPeerSeat(
	const FSeatRecord (&SeatRows)[2],
	const FString& SteamId,
	const FString& Token)
{
	if (SteamId.IsEmpty())
	{
		return INDEX_NONE;
	}

	for (int32 Seat = 0; Seat < 2; ++Seat)
	{
		const FSeatRecord& Record = SeatRows[Seat];
		if (Record.Link == ESeatConnection::Empty
			|| Record.bHost)
		{
			continue;
		}

		if (Record.UniqueKey == SteamId)
		{
			return Seat;
		}

		/* A seat taken over IP has no Steam ID.
		 * Its token still belongs to that player. */
		if (Record.UniqueKey.IsEmpty()
			&& !Token.IsEmpty()
			&& Record.Token == Token)
		{
			return Seat;
		}
	}

	for (int32 Seat = 0; Seat < 2; ++Seat)
	{
		if (SeatRows[Seat].Link == ESeatConnection::Empty)
		{
			return Seat;
		}
	}

	return INDEX_NONE;
}

AJoA_GameMode::AJoA_GameMode()
{
	DefaultPawnClass = AJoA_Pawn::StaticClass();
	GameStateClass = AJoA_GameState::StaticClass();
	PlayerControllerClass =
		AJoA_PlayerController::StaticClass();
	PlayerStateClass = AJoA_PlayerState::StaticClass();
}

void AJoA_GameMode::BeginPlay()
{
	Super::BeginPlay();
	GridManager = FindGridManager(GetWorld());

	/* Next tick, after obstacle seeding.
	 * Home squares are then cleared. */
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	World->GetTimerManager().SetTimerForNextTick(
		this,
		&AJoA_GameMode::SpawnOpeningLineup);
}

void AJoA_GameMode::PreLogin(
	const FString& Options,
	const FString& Address,
	const FUniqueNetIdRepl& UniqueId,
	FString& ErrorMessage)
{
	Super::PreLogin(
		Options,
		Address,
		UniqueId,
		ErrorMessage);
	if (!ErrorMessage.IsEmpty())
	{
		return;
	}

	/* Two seats, and a seat belongs to its player until
	 * the forfeit. A third player, or a stranger while the
	 * other seat waits for its player, is turned away. */
	const FString Token = UGameplayStatics::ParseOption(
		Options,
		ReconnectTokenOption);
	FString SteamId;
	const bool bSteamPeer = ParseSteamSocketId(
		Address,
		SteamId);
	FString Key = UniqueKeyOf(UniqueId);
	if (bSteamPeer)
	{
		/* Login writes whatever id the client wants.
		 * The socket address is the SteamID64 Steam
		 * signed. Those two have to be the same. */
		if (Key != SteamId)
		{
			ErrorMessage = TEXT(
				"Join through the Steam invite.");
			UE_LOG(
				LogJoA_Net,
				Warning,
				TEXT("Join from %s refused: %s"),
				*Address,
				*ErrorMessage);
			return;
		}

		const UJoA_GameInstance* Instance =
			GetGameInstance<UJoA_GameInstance>();
		const FString Refusal = Instance
			? Instance->SteamPeerRefusal(SteamId)
			: FString(TEXT(
				"Join through the Steam invite."));
		if (!Refusal.IsEmpty())
		{
			ErrorMessage = Refusal;
			UE_LOG(
				LogJoA_Net,
				Warning,
				TEXT("Join from %s refused: %s"),
				*Address,
				*ErrorMessage);
			return;
		}
		Key = SteamId;
	}

	if (FindSeatForJoin(
			Key,
			Token,
			false,
			bSteamPeer) != INDEX_NONE)
	{
		return;
	}

	/* A finished match keeps its seats as they were. The
	 * client shows this text, so it says why the join
	 * does nothing. */
	const AJoA_GameState* JoinGameState =
		GetGameState<AJoA_GameState>();
	const bool bOver =
		JoinGameState && JoinGameState->IsMatchOver();
	ErrorMessage = bOver
		? TEXT("Match is over.")
		: TEXT("Match is full.");
	UE_LOG(
		LogJoA_Net,
		Warning,
		TEXT("Join from %s refused: %s"),
		*Address,
		*ErrorMessage);
}

FString AJoA_GameMode::InitNewPlayer(
	APlayerController* NewPlayerController,
	const FUniqueNetIdRepl& UniqueId,
	const FString& Options,
	const FString& Portal)
{
	const FString Result = Super::InitNewPlayer(
		NewPlayerController,
		UniqueId,
		Options,
		Portal);

	/* PostLogin has no options. The token is kept on the
	 * player state until the seat is chosen. */
	AJoA_PlayerState* JoinState = NewPlayerController
		? NewPlayerController->GetPlayerState<
			AJoA_PlayerState>()
		: nullptr;
	if (JoinState)
	{
		JoinState->SetJoinToken(
			UGameplayStatics::ParseOption(
				Options,
				ReconnectTokenOption));
	}

	return Result;
}

void AJoA_GameMode::PostLogin(
	APlayerController* NewPlayer)
{
	Super::PostLogin(NewPlayer);
	RegisterSeat(NewPlayer);
	AssignSeat(NewPlayer);
}

void AJoA_GameMode::Logout(AController* Exiting)
{
	/* The player state is still alive here. The engine
	 * destroys it right after Logout returns. */
	const APlayerController* Leaving =
		Cast<APlayerController>(Exiting);
	const AJoA_PlayerState* LeavingState = Leaving
		? Leaving->GetPlayerState<AJoA_PlayerState>()
		: nullptr;
	const int32 Seat = LeavingState
		? LeavingState->GetSeatIndex()
		: INDEX_NONE;

	/* A replaced connection logs out after the seat went
	 * to its new connection. That one must not drop it. */
	if (IsValidSeat(Seat)
		&& Seats[Seat].Controller.Get() == Exiting)
	{
		HandleSeatDropped(Seat, LeavingState);
	}

	Super::Logout(Exiting);
}

int32 AJoA_GameMode::FindSeatForJoin(
	const FString& UniqueKey,
	const FString& Token,
	bool bIsHost,
	bool bSteamPeer) const
{
	if (bSteamPeer)
	{
		return FindSteamPeerSeat(
			Seats,
			UniqueKey,
			Token);
	}

	if (!bIsHost)
	{
		for (int32 Seat = 0; Seat < 2; ++Seat)
		{
			const FSeatRecord& Record = Seats[Seat];
			if (Record.Link == ESeatConnection::Empty
				|| Record.bHost)
			{
				continue;
			}

			const bool bSameToken = !Token.IsEmpty()
				&& Record.Token == Token;
			if (bSameToken)
			{
				return Seat;
			}

			/* The NULL net id is the same on every process
			 * of this machine. A third window would take a
			 * connected seat. The id only reclaims a seat
			 * that is waiting for its player. */
			const bool bSameId = !UniqueKey.IsEmpty()
				&& Record.UniqueKey == UniqueKey
				&& Record.Link
					== ESeatConnection::Disconnected;
			if (bSameId)
			{
				return Seat;
			}
		}
	}

	for (int32 Seat = 0; Seat < 2; ++Seat)
	{
		if (Seats[Seat].Link == ESeatConnection::Empty)
		{
			return Seat;
		}
	}

	return INDEX_NONE;
}

void AJoA_GameMode::RegisterSeat(
	APlayerController* Joining)
{
	AJoA_PlayerState* JoinState = Joining
		? Joining->GetPlayerState<AJoA_PlayerState>()
		: nullptr;
	if (!JoinState || !GetGameState<AJoA_GameState>())
	{
		return;
	}

	FString Key = UniqueKeyOf(JoinState->GetUniqueId());
	bool bSteamPeer = false;
	if (UNetConnection* Conn = Joining->GetNetConnection())
	{
		FString SteamId;
		if (ParseSteamSocketId(
				Conn->LowLevelGetRemoteAddress(),
				SteamId))
		{
			bSteamPeer = true;
			Key = SteamId;
		}
	}

	const int32 Seat = FindSeatForJoin(
		Key,
		JoinState->GetJoinToken(),
		Joining->IsLocalController(),
		bSteamPeer);
	if (Seat == INDEX_NONE)
	{
		UE_LOG(
			LogJoA_Net,
			Warning,
			TEXT("No seat for %s. Kicked."),
			*JoinState->GetPlayerName());
		if (GameSession)
		{
			GameSession->KickPlayer(
				Joining,
				FText::FromString(TEXT("Match is full.")));
		}
		return;
	}

	/* The player came back before the server saw the old
	 * connection time out. That connection goes now. It
	 * lets go of its figure first, because the engine kick
	 * destroys the pawn of the controller it kicks. */
	APlayerController* StalePC = Cast<APlayerController>(
		Seats[Seat].Controller.Get());
	if (StalePC && StalePC != Joining)
	{
		UE_LOG(
			LogJoA_Net,
			Warning,
			TEXT("Seat %d: old connection replaced."),
			Seat);

		AJoA_Figure* Held =
			Cast<AJoA_Figure>(StalePC->GetPawn());
		if (Held)
		{
			RememberSeatFigure(Seat, Held);
			StalePC->UnPossess();
		}

		if (GameSession)
		{
			GameSession->KickPlayer(
				StalePC,
				FText::FromString(
					TEXT("Replaced by a new connection.")));
		}
	}

	HandleSeatJoined(Joining, JoinState, Seat, Key);
}

void AJoA_GameMode::HandleSeatJoined(
	APlayerController* Joining,
	AJoA_PlayerState* JoinState,
	int32 Seat,
	const FString& UniqueKey)
{
	AJoA_GameState* State = GetGameState<AJoA_GameState>();
	UWorld* World = GetWorld();
	if (!State || !World || !IsValidSeat(Seat))
	{
		return;
	}

	FSeatRecord& Record = Seats[Seat];
	const bool bRejoin =
		Record.Link == ESeatConnection::Disconnected;
	FTimerManager& Timers = World->GetTimerManager();
	Timers.ClearTimer(Record.ForfeitTimer);

	Record.Link = ESeatConnection::Connected;
	Record.Controller = Joining;
	Record.bHost = Joining->IsLocalController();
	Record.UniqueKey = UniqueKey;
	if (Record.Token.IsEmpty())
	{
		Record.Token = FGuid::NewGuid().ToString();
	}

	JoinState->SetSeatIndex(Seat);
	JoinState->SetNation(GetNation(Seat));
	if (bRejoin)
	{
		JoinState->RestoreCombo(
			Record.ComboTarget.Get(),
			Record.ComboAttackCount);
	}

	State->SetSeatLink(
		Seat,
		ESeatConnection::Connected,
		0.0);
	UE_LOG(
		LogJoA_Net,
		Log,
		TEXT("Seat %d %s by %s."),
		Seat,
		bRejoin ? TEXT("rejoined") : TEXT("taken"),
		*JoinState->GetPlayerName());

	/* The host has nothing to reconnect to. A client keeps
	 * the ticket in its game instance, which outlives the
	 * world it loses with the connection. */
	if (!Record.bHost)
	{
		AJoA_PlayerController* Remote =
			Cast<AJoA_PlayerController>(Joining);
		if (Remote)
		{
			Remote->ClientStoreReconnectTicket(
				Record.Token,
				Seat);
		}
	}

	RefreshPause();
}

void AJoA_GameMode::HandleSeatDropped(
	int32 Seat,
	const AJoA_PlayerState* DroppedState)
{
	AJoA_GameState* State = GetGameState<AJoA_GameState>();
	UWorld* World = GetWorld();
	if (!State
		|| !World
		|| World->bIsTearingDown
		|| !IsValidSeat(Seat)
		|| Seats[Seat].Link != ESeatConnection::Connected)
	{
		return;
	}

	FSeatRecord& Record = Seats[Seat];
	if (DroppedState)
	{
		Record.ComboTarget = DroppedState->GetComboTarget();
		Record.ComboAttackCount =
			DroppedState->GetComboAttackCount();
	}

	Record.Controller.Reset();
	Record.Link = ESeatConnection::Disconnected;

	if (State->IsMatchOver())
	{
		State->SetSeatLink(
			Seat,
			ESeatConnection::Disconnected,
			0.0);
		UE_LOG(
			LogJoA_Net,
			Log,
			TEXT("Seat %d left after the match."),
			Seat);
		return;
	}

	const float Grace =
		GetDefault<UJoA_MatchSettings>()->
			ReconnectGraceSeconds;
	State->SetSeatLink(
		Seat,
		ESeatConnection::Disconnected,
		State->GetServerWorldTimeSeconds() + Grace);
	UE_LOG(
		LogJoA_Net,
		Warning,
		TEXT("Seat %d lost its connection. Held %.0f s."),
		Seat,
		Grace);

	if (Grace <= 0.f)
	{
		ForfeitSeat(Seat);
		return;
	}

	World->GetTimerManager().SetTimer(
		Record.ForfeitTimer,
		FTimerDelegate::CreateUObject(
			this,
			&AJoA_GameMode::ForfeitSeat,
			Seat),
		Grace,
		false);
	RefreshPause();
}

void AJoA_GameMode::ForfeitSeat(int32 Seat)
{
	const AJoA_GameState* State =
		GetGameState<AJoA_GameState>();
	if (!State
		|| State->IsMatchOver()
		|| !IsValidSeat(Seat)
		|| Seats[Seat].Link == ESeatConnection::Connected)
	{
		return;
	}

	const int32 Winner = OtherSeat(Seat);
	UE_LOG(
		LogJoA_Net,
		Warning,
		TEXT("Seat %d did not come back. Seat %d wins."),
		Seat,
		Winner);
	DeclareWinner(Winner, EMatchEndReason::Forfeit);
}

void AJoA_GameMode::RequestLeaveMatch(int32 Seat)
{
	if (!IsValidSeat(Seat))
	{
		return;
	}

	const int32 Winner = OtherSeat(Seat);
	UE_LOG(
		LogJoA_Net,
		Warning,
		TEXT("Seat %d left on purpose. Seat %d wins."),
		Seat,
		Winner);
	DeclareWinner(Winner, EMatchEndReason::Forfeit);
}

void AJoA_GameMode::RememberSeatFigure(
	int32 Seat,
	AJoA_Figure* Figure)
{
	if (IsValidSeat(Seat))
	{
		LastSelected[Seat] = Figure;
	}
}

void AJoA_GameMode::DeclareWinner(
	int32 Seat,
	EMatchEndReason Reason)
{
	AJoA_GameState* State = GetGameState<AJoA_GameState>();
	UWorld* World = GetWorld();
	if (!State || !World || State->IsMatchOver())
	{
		return;
	}

	State->SetWinnerSeat(Seat, Reason);
	for (FSeatRecord& Record : Seats)
	{
		World->GetTimerManager().ClearTimer(
			Record.ForfeitTimer);
	}

	ApplyTurnTimer();
}

void AJoA_GameMode::RefreshPause()
{
	AJoA_GameState* State = GetGameState<AJoA_GameState>();
	if (!State)
	{
		return;
	}

	const bool bWait = GetDefault<UJoA_MatchSettings>()->
		bWaitForOpponent;
	bool bPause = false;
	bool bDropped = false;
	for (const FSeatRecord& Record : Seats)
	{
		bDropped = bDropped
			|| Record.Link == ESeatConnection::Disconnected;
		if (Record.Link == ESeatConnection::Disconnected
			|| (bWait
				&& Record.Link == ESeatConnection::Empty))
		{
			bPause = true;
		}
	}

	const bool bWasPaused = State->IsPaused();
	State->SetPaused(bPause);
	ApplyTurnTimer();

	if (bWasPaused && !bPause)
	{
		UE_LOG(
			LogJoA_Net,
			Log,
			TEXT("Both seats have a player."));
	}
	else if (!bWasPaused && bPause)
	{
		/* Only a dropped player is worth a warning. An empty
		 * seat at the start is the normal wait. */
		if (bDropped)
		{
			UE_LOG(
				LogJoA_Net,
				Warning,
				TEXT("Match paused. A seat is offline."));
		}
		else
		{
			UE_LOG(
				LogJoA_Net,
				Log,
				TEXT("Match paused. Waiting for a player."));
		}
	}

	TryStartOrWaitOpeningThrow();
}

bool AJoA_GameMode::BothSeatsConnected() const
{
	return Seats[FrenchSeat].Link
			== ESeatConnection::Connected
		&& Seats[EnglishSeat].Link
			== ESeatConnection::Connected;
}

void AJoA_GameMode::TryStartOrWaitOpeningThrow()
{
	AJoA_GameState* State = GetGameState<AJoA_GameState>();
	if (!State
		|| State->HasMatchStarted()
		|| State->IsMatchOver()
		|| State->IsPaused())
	{
		return;
	}

	if (BothSeatsConnected())
	{
		UE_LOG(
			LogJoA_Net,
			Log,
			TEXT("Both seats are in. Opening throw."));
		return;
	}

	/* Solo, or wait-for-opponent is off. French
	 * opens with no throw. */
	StartMatch(FrenchSeat, EOpeningThrow::None);
}

void AJoA_GameMode::StartMatch(
	int32 OpeningSeat,
	EOpeningThrow WinningThrow)
{
	AJoA_GameState* State = GetGameState<AJoA_GameState>();
	if (!State || State->HasMatchStarted())
	{
		return;
	}

	if (!IsValidSeat(OpeningSeat))
	{
		OpeningSeat = FrenchSeat;
	}

	UE_LOG(
		LogJoA_Net,
		Log,
		TEXT("Match starts. Seat %d moves first."),
		OpeningSeat);
	State->BeginMatch(OpeningSeat, WinningThrow);
	RestoreSeatSelection(OpeningSeat);
	ApplyTurnTimer();
}

void AJoA_GameMode::SubmitOpeningThrow(
	int32 Seat,
	EOpeningThrow Throw)
{
	AJoA_GameState* State = GetGameState<AJoA_GameState>();
	if (!State
		|| State->HasMatchStarted()
		|| State->IsMatchOver()
		|| !IsValidSeat(Seat)
		|| Throw == EOpeningThrow::None
		|| OpeningThrows[Seat] != EOpeningThrow::None)
	{
		return;
	}

	OpeningThrows[Seat] = Throw;
	State->SetOpeningPicks(
		OpeningThrows[FrenchSeat] != EOpeningThrow::None,
		OpeningThrows[EnglishSeat] != EOpeningThrow::None);

	/* Kept while the other seat is empty.
	 * Resolve only after that seat throws. */
	if (!BothSeatsConnected())
	{
		return;
	}

	const int32 Opponent = OtherSeat(Seat);
	const EOpeningThrow OtherThrow = OpeningThrows[Opponent];
	if (OtherThrow == EOpeningThrow::None)
	{
		return;
	}

	if (OtherThrow == Throw)
	{
		OpeningThrows[FrenchSeat] = EOpeningThrow::None;
		OpeningThrows[EnglishSeat] = EOpeningThrow::None;
		State->SetOpeningTie(Throw);
		UE_LOG(
			LogJoA_Net,
			Log,
			TEXT("Opening throw tied. Both pick again."));
		return;
	}

	const bool bSeatWins =
		(Throw == EOpeningThrow::Rock
			&& OtherThrow == EOpeningThrow::Scissors)
		|| (Throw == EOpeningThrow::Paper
			&& OtherThrow == EOpeningThrow::Rock)
		|| (Throw == EOpeningThrow::Scissors
			&& OtherThrow == EOpeningThrow::Paper);
	const int32 Winner = bSeatWins ? Seat : Opponent;
	StartMatch(Winner, OpeningThrows[Winner]);
}

void AJoA_GameMode::ApplyTurnTimer()
{
	AJoA_GameState* State = GetGameState<AJoA_GameState>();
	UWorld* World = GetWorld();
	if (!State || !World)
	{
		return;
	}

	FTimerManager& Timers = World->GetTimerManager();
	const float Limit =
		GetDefault<UJoA_MatchSettings>()->TurnSeconds;
	if (State->IsMatchOver() || Limit <= 0.f)
	{
		Timers.ClearTimer(TurnTimer);
		State->SetTurnDeadline(0.0);
		return;
	}

	/* A paused match keeps the time the seat had left.
	 * Waiting for the opponent or the opening throw
	 * costs nobody a turn. */
	if (State->IsPaused() || !State->HasMatchStarted())
	{
		if (Timers.IsTimerActive(TurnTimer))
		{
			Timers.PauseTimer(TurnTimer);
		}
		State->SetTurnDeadline(0.0);
		return;
	}

	if (Timers.IsTimerPaused(TurnTimer))
	{
		Timers.UnPauseTimer(TurnTimer);
	}
	else if (!Timers.IsTimerActive(TurnTimer))
	{
		Timers.SetTimer(
			TurnTimer,
			this,
			&AJoA_GameMode::HandleTurnExpired,
			Limit,
			false);
	}

	State->SetTurnDeadline(
		State->GetServerWorldTimeSeconds()
		+ Timers.GetTimerRemaining(TurnTimer));
}

void AJoA_GameMode::RestartTurnTimer()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(TurnTimer);
	}

	ApplyTurnTimer();
}

void AJoA_GameMode::HandleTurnExpired()
{
	const AJoA_GameState* State =
		GetGameState<AJoA_GameState>();
	if (!State || !State->IsPlayable())
	{
		return;
	}

	UE_LOG(
		LogJoA_Net,
		Log,
		TEXT("Seat %d ran out of time. Turn passes."),
		State->GetActiveSeat());
	EndTurn();
}

void AJoA_GameMode::SpawnOpeningLineup()
{
	if (bOpeningLineupSpawned)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	if (!IsValid(GridManager))
	{
		GridManager = FindGridManager(World);
	}
	if (!GridManager)
	{
		return;
	}

	bOpeningLineupSpawned = true;

	/* BeginPlay used the grid default. The host
	 * menu count and seed replace it. Tests keep
	 * the layout they already asserted. */
	if (!GIsAutomationTesting)
	{
		if (const UJoA_GameInstance* Instance =
			GetGameInstance<UJoA_GameInstance>())
		{
			GridManager->ReseedOpeningObstacles(
				Instance->GetOpeningObstaclePairs(),
				Instance->GetOpeningObstacleSeed());
		}
	}

	const ESpawnActorCollisionHandlingMethod SpawnMode =
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	for (const FOpeningSquare& Square : OpeningLine)
	{
		FGridCell Cell;
		if (GridManager->GetCell(
				Square.X,
				Square.Y,
				Cell)
			&& Cell.ObstacleHeight > 0)
		{
			GridManager->SetObstacleHeight(
				Square.X,
				Square.Y,
				0);
		}

		AJoA_Figure* Figure =
			World->SpawnActorDeferred<AJoA_Figure>(
				AJoA_Figure::StaticClass(),
				FTransform::Identity,
				nullptr,
				nullptr,
				SpawnMode);
		if (!Figure)
		{
			continue;
		}

		Figure->UnitType = Square.Type;
		Figure->CellX = Square.X;
		Figure->CellY = Square.Y;
		Figure->Facing = Square.Facing;
		Figure->OwningPlayerIndex = Square.PlayerIndex;
		Figure->FinishSpawning(FTransform::Identity);
		MARK_PROPERTY_DIRTY_FROM_NAME(
			AJoA_Figure,
			UnitType,
			Figure);
		MARK_PROPERTY_DIRTY_FROM_NAME(
			AJoA_Figure,
			OwningPlayerIndex,
			Figure);
		Figure->SetBoardPlacement(
			Square.X,
			Square.Y,
			Square.Facing);

		if (Square.Type == EUnitType::Medic)
		{
			SpawnTeamFlag(
				Square.X,
				Square.Y,
				Square.PlayerIndex);
		}
	}

	AssignOpeningSeats();
}

void AJoA_GameMode::SpawnTeamFlag(
	int32 CellX,
	int32 CellY,
	int32 Seat)
{
	UWorld* World = GetWorld();
	if (!World || !IsValid(GridManager))
	{
		return;
	}

	/* The flag stands on the cell next to the Mage, at
	 * the end of the line, so the Mage does not hold it.
	 * The two flag cells are mirrors of each other, so
	 * clearing an obstacle there costs neither side. */
	const int32 FlagY =
		CellY + (Seat == FrenchSeat ? -1 : 1);
	FGridCell FlagCell;
	if (GridManager->GetCell(CellX, FlagY, FlagCell)
		&& FlagCell.ObstacleHeight > 0)
	{
		GridManager->SetObstacleHeight(CellX, FlagY, 0);
	}

	const FVector FlagLocation =
		GridManager->GetCellWorldLocation(CellX, FlagY);

	/* The blue banner points the wrong way on the board
	 * until it is turned by 180 degrees around Z. */
	const FRotator FlagRotation(
		0.f,
		Seat == FrenchSeat ? 180.f : 0.f,
		0.f);
	const FTransform FlagTransform(
		FlagRotation,
		FlagLocation,
		FVector(0.5f));

	TSubclassOf<AJoA_Flag> SpawnClass = FlagClass;
	if (!SpawnClass)
	{
		UE_LOG(
			LogJoA_Net,
			Warning,
			TEXT("FlagClass is not set on %s. The flag "
				"has no meshes. Set it in BP_GM."),
			*GetClass()->GetName());
		SpawnClass = AJoA_Flag::StaticClass();
	}

	AJoA_Flag* Flag = World->SpawnActorDeferred<AJoA_Flag>(
		SpawnClass,
		FlagTransform,
		nullptr,
		nullptr,
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (Flag)
	{
		Flag->SetReplicates(true);
		Flag->bAlwaysRelevant = true;
		Flag->SetOwningPlayerIndex(Seat);
		Flag->SetBoardCell(CellX, FlagY);
		Flag->FinishSpawning(FlagTransform);
		Flag->ForceNetUpdate();
		UE_LOG(
			LogJoA_Net,
			Log,
			TEXT("Flag of seat %d spawned at %s."),
			Seat,
			*Flag->GetActorLocation().ToString());
	}
}

void AJoA_GameMode::AssignOpeningSeats()
{
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
		AssignSeat(It->Get());
	}
}

void AJoA_GameMode::AssignSeat(
	APlayerController* Controller)
{
	if (!Controller || !GameState)
	{
		return;
	}

	AJoA_PlayerState* SeatState =
		Controller->GetPlayerState<AJoA_PlayerState>();
	if (!SeatState)
	{
		return;
	}

	/* The seat is the one the player took at login. Its
	 * place in PlayerArray moves when the other player
	 * leaves, so it cannot name the seat. */
	const int32 Seat = SeatState->GetSeatIndex();
	if (!IsValidSeat(Seat))
	{
		return;
	}

	SeatState->SetNation(GetNation(Seat));

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	for (TActorIterator<AJoA_Figure> It(World); It; ++It)
	{
		AJoA_Figure* Figure = *It;
		if (!Figure
			|| Figure->OwningPlayerIndex != Seat)
		{
			continue;
		}

		if (Figure->GetOwningPlayerState() == SeatState)
		{
			continue;
		}

		Figure->SetOwningPlayer(SeatState, Seat);
	}

	/* The match opens with the Heavy Knight of the seat on
	 * turn already selected. Not before the throw. */
	const AJoA_GameState* Turn =
		GetGameState<AJoA_GameState>();
	if (Turn
		&& Turn->HasMatchStarted()
		&& Turn->GetActiveSeat() == Seat)
	{
		RestoreSeatSelection(Seat);
	}
}

namespace
{
	struct FClientGridEntry
	{
		TWeakObjectPtr<UWorld> World;
		TWeakObjectPtr<AJoA_GridManager> Grid;
	};

}

AJoA_GridManager* AJoA_GameMode::GetGridManager(
	const UObject* WorldContext)
{
	UWorld* World = WorldContext
		? WorldContext->GetWorld()
		: nullptr;
	if (!World)
	{
		return nullptr;
	}

	if (AJoA_GameMode* Mode =
		World->GetAuthGameMode<AJoA_GameMode>())
	{
		if (!IsValid(Mode->GridManager))
		{
			Mode->GridManager = FindGridManager(World);
		}
		return Mode->GridManager;
	}

	/* The client has no auth GameMode. Each reach
	 * test walked every actor to find the board.
	 * One weak pointer per world. A dead board is
	 * looked up again. PIE keeps its own worlds. */
	static TArray<FClientGridEntry, TInlineAllocator<4>>
		Entries;

	for (int32 Index = Entries.Num() - 1;
		Index >= 0;
		--Index)
	{
		FClientGridEntry& Entry = Entries[Index];
		if (!Entry.World.IsValid())
		{
			Entries.RemoveAtSwap(Index);
			continue;
		}

		if (Entry.World.Get() != World)
		{
			continue;
		}

		if (AJoA_GridManager* Grid = Entry.Grid.Get())
		{
			return Grid;
		}

		Entry.Grid = FindGridManager(World);
		return Entry.Grid.Get();
	}

	FClientGridEntry Added;
	Added.World = World;
	Added.Grid = FindGridManager(World);
	Entries.Add(Added);
	return Added.Grid.Get();
}

AJoA_GridManager* AJoA_GameMode::FindGridManager(
	UWorld* World)
{
	if (!World)
	{
		return nullptr;
	}

	TActorIterator<AJoA_GridManager> It(World);
	if (It)
	{
		return *It;
	}

	return nullptr;
}

void AJoA_GameMode::NotifyFigureMoved(AJoA_Figure* Figure)
{
	AJoA_GameState* State = GetGameState<AJoA_GameState>();
	if (!State || !State->CanFigureAct(Figure))
	{
		return;
	}

	State->CommitActingFigure(Figure, true);

	/* A figure that stepped may have nothing to hit
	 * from its new square. */
	EvaluateAutoPass();
}

void AJoA_GameMode::NotifyFigureActed(AJoA_Figure* Figure)
{
	AJoA_GameState* State = GetGameState<AJoA_GameState>();
	if (!State || !IsValid(Figure))
	{
		return;
	}

	/* The acting figure may already be dead. Its seat
	 * still ends the turn, unless the match is over. */
	if (!State->IsPlayable()
		|| Figure->OwningPlayerIndex
			!= State->GetActiveSeat())
	{
		return;
	}

	EndTurn();
}

void AJoA_GameMode::RequestEndTurn(int32 Seat)
{
	AJoA_GameState* State = GetGameState<AJoA_GameState>();
	if (!State
		|| !State->IsPlayable()
		|| State->GetActiveSeat() != Seat)
	{
		return;
	}

	EndTurn();
}

void AJoA_GameMode::NotifyFigureDied(AJoA_Figure* Figure)
{
	AJoA_GameState* State = GetGameState<AJoA_GameState>();
	UWorld* World = GetWorld();
	if (!State || !World || !IsValid(Figure))
	{
		return;
	}

	const int32 Seat = Figure->OwningPlayerIndex;
	if (Seat == INDEX_NONE)
	{
		return;
	}

	int32 Alive = 0;
	for (TActorIterator<AJoA_Figure> It(World); It; ++It)
	{
		const AJoA_Figure* Other = *It;
		if (IsValid(Other)
			&& !Other->IsDead()
			&& Other->OwningPlayerIndex == Seat)
		{
			++Alive;
		}
	}

	if (Alive > 0)
	{
		EvaluateBoardDraw();
		return;
	}

	const int32 Winner = OtherSeat(Seat);
	UE_LOG(
		LogTemp,
		Log,
		TEXT("Seat %d lost every figure. Seat %d wins."),
		Seat,
		Winner);
	DeclareWinner(Winner, EMatchEndReason::Elimination);
}

void AJoA_GameMode::EvaluateBoardDraw()
{
	AJoA_GameState* State = GetGameState<AJoA_GameState>();
	if (!HasAuthority()
		|| !State
		|| !State->HasMatchStarted()
		|| State->IsMatchOver()
		|| !IsSplitDraw(this))
	{
		return;
	}

	UE_LOG(
		LogTemp,
		Log,
		TEXT("Neither side can cross. Draw."));
	DeclareWinner(INDEX_NONE, EMatchEndReason::Draw);
}

void AJoA_GameMode::NotifyFlagCarried(AJoA_Figure* Carrier)
{
	AJoA_GameState* State = GetGameState<AJoA_GameState>();
	if (!State || !IsValid(Carrier) || State->IsMatchOver())
	{
		return;
	}

	AJoA_Flag* Flag = Carrier->GetHeldFlag();
	const int32 Seat = Carrier->OwningPlayerIndex;
	if (!Flag || !IsValidSeat(Seat)
		|| Flag->GetOwningPlayerIndex() == Seat)
	{
		return;
	}

	const int32 HomeX = Seat == FrenchSeat
		? GridSize - 1
		: 0;
	if (Carrier->CellX != HomeX)
	{
		return;
	}

	UE_LOG(
		LogTemp,
		Log,
		TEXT("Seat %d captured the enemy flag."),
		Seat);
	DeclareWinner(Seat, EMatchEndReason::FlagCapture);
}

void AJoA_GameMode::EndTurn()
{
	AJoA_GameState* State = GetGameState<AJoA_GameState>();
	if (!State)
	{
		return;
	}

	const int32 EndedSeat = State->GetActiveSeat();
	const int32 NextSeat = OtherSeat(EndedSeat);
	NotifyFiguresTurnEnded(EndedSeat);

	/* The seat that played is no longer selected. Without
	 * this its last figure stays marked and possessed
	 * during the opponent's turn. */
	ClearSeatSelection(EndedSeat);
	State->SetActiveSeat(NextSeat);
	RestoreSeatSelection(NextSeat);
	BeginTurn(NextSeat);
	RestartTurnTimer();
}

void AJoA_GameMode::NotifyFiguresTurnEnded(int32 Seat)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	for (TActorIterator<AJoA_Figure> It(World); It; ++It)
	{
		if (AJoA_Figure* Figure = *It)
		{
			Figure->HandleTurnEnded(Seat);
		}
	}
}

void AJoA_GameMode::ClearSeatSelection(int32 Seat)
{
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
		AJoA_Figure* Selected = Controller
			? Controller->GetSelectedFigure()
			: nullptr;
		if (Selected && Selected->OwningPlayerIndex == Seat)
		{
			if (IsValidSeat(Seat))
			{
				LastSelected[Seat] = Selected;
			}
			Controller->UnPossess();
		}
	}
}

void AJoA_GameMode::RestoreSeatSelection(int32 Seat)
{
	UWorld* World = GetWorld();
	const AJoA_GameState* State =
		GetGameState<AJoA_GameState>();
	const UJoA_FigureRegistry* Registry =
		UJoA_FigureRegistry::Get(this);
	if (!World
		|| !State
		|| !Registry
		|| State->IsMatchOver()
		|| !IsValidSeat(Seat))
	{
		return;
	}

	AJoA_PlayerController* SeatController = nullptr;
	for (FConstPlayerControllerIterator It =
			World->GetPlayerControllerIterator();
		It;
		++It)
	{
		AJoA_PlayerController* Controller =
			Cast<AJoA_PlayerController>(It->Get());
		const AJoA_PlayerState* SeatState = Controller
			? Controller->GetPlayerState<AJoA_PlayerState>()
			: nullptr;
		if (SeatState && SeatState->GetSeatIndex() == Seat)
		{
			SeatController = Controller;
			break;
		}
	}

	if (!SeatController)
	{
		return;
	}

	/* UnPossess keeps the pick so QWE still
	 * has a figure. The pawn is gone. Possess
	 * again or the client cannot activate. */
	AJoA_Figure* Kept =
		SeatController->GetSelectedFigure();
	if (IsValid(Kept)
		&& SeatController->CanSelectFigure(Kept)
		&& SeatController->GetPawn() != Kept)
	{
		SeatController->Possess(Kept);
		return;
	}

	if (IsValid(Kept))
	{
		return;
	}

	/* A figure already committed this turn is the only one
	 * the seat may act with. A player who returns mid turn
	 * gets exactly that one, or could do nothing. */
	AJoA_Figure* Committed = State->GetActingFigure();
	if (IsValid(Committed)
		&& !Committed->IsDead()
		&& Committed->OwningPlayerIndex == Seat)
	{
		SeatController->Possess(Committed);
		return;
	}

	TArray<AJoA_Figure*> Own;
	Registry->CollectFiguresOfSeat(Seat, Own);
	if (Own.IsEmpty())
	{
		return;
	}

	AJoA_Figure* Pick = LastSelected[Seat].Get();
	if (!Pick || Pick->IsDead() || !Own.Contains(Pick))
	{
		Pick = Own[0];
		for (AJoA_Figure* Candidate : Own)
		{
			if (Candidate->UnitType == EUnitType::HeavyKnight)
			{
				Pick = Candidate;
				break;
			}
		}
	}

	if (SeatController->CanSelectFigure(Pick))
	{
		SeatController->Possess(Pick);
	}
}

bool AJoA_GameMode::SeatHasAnyAction(
	int32 Seat,
	FString* OutReason) const
{
	const UJoA_FigureRegistry* Registry =
		UJoA_FigureRegistry::Get(this);
	if (!Registry)
	{
		return true;
	}

	TArray<AJoA_Figure*> Own;
	Registry->CollectFiguresOfSeat(Seat, Own);
	for (const AJoA_Figure* Figure : Own)
	{
		if (Figure->HasAnyAction(OutReason))
		{
			return true;
		}
	}
	return false;
}

void AJoA_GameMode::EvaluateAutoPass()
{
	const AJoA_GameState* State =
		GetGameState<AJoA_GameState>();
	if (!State || State->IsMatchOver())
	{
		return;
	}

	const int32 Seat = State->GetActiveSeat();
	FString Reason;
	if (SeatHasAnyAction(Seat, &Reason))
	{
		UE_LOG(
			LogTemp,
			Log,
			TEXT("Seat %d keeps the turn: %s."),
			Seat,
			*Reason);
		return;
	}

	UE_LOG(
		LogTemp,
		Log,
		TEXT("Seat %d has no action. Turn passes."),
		Seat);
	EndTurn();
}

void AJoA_GameMode::BeginTurn(int32 Seat)
{
	AJoA_GameState* State = GetGameState<AJoA_GameState>();
	const UJoA_FigureRegistry* Registry =
		UJoA_FigureRegistry::Get(this);
	if (!State
		|| !Registry
		|| State->IsMatchOver()
		|| !IsValidSeat(Seat))
	{
		return;
	}

	TArray<AJoA_Figure*> Own;
	Registry->CollectFiguresOfSeat(Seat, Own);
	if (Own.IsEmpty())
	{
		return;
	}

	EvaluateBoardDraw();
	if (State->IsMatchOver())
	{
		return;
	}

	if (SeatHasAnyAction(Seat, nullptr))
	{
		bSeatWasBlocked[Seat] = false;
		return;
	}

	/* No option on a turn that just began. The seat skips
	 * on its own and reads why in the HUD. Blocked again on
	 * its next turn, nothing the opponent did freed it, so
	 * it loses the stalemate. */
	if (bSeatWasBlocked[Seat])
	{
		const int32 Winner = OtherSeat(Seat);
		UE_LOG(
			LogTemp,
			Log,
			TEXT("Seat %d is stuck again. Seat %d wins."),
			Seat,
			Winner);
		DeclareWinner(Winner, EMatchEndReason::Stalemate);
		return;
	}

	bSeatWasBlocked[Seat] = true;
	State->SetActiveSeatBlocked(true);
	UE_LOG(
		LogTemp,
		Log,
		TEXT("Seat %d has no option. It must skip."),
		Seat);
}
