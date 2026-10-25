// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

// --- Engine ---
#include "Engine/EngineBaseTypes.h"
#include "Engine/GameInstance.h"
#include "Engine/TimerHandle.h"

#include "JoA_GameInstance.generated.h"

class UNetDriver;
struct FJoA_SteamSession;

/**
 * Client memory of the match and the Steam lobby.
 * After a join the server hands over a ticket: where
 * to connect and the secret of the seat. The instance
 * outlives every world, so when the connection breaks
 * it keeps retrying with that ticket until the server
 * gives the seat away, or the reconnect window has
 * run out.
 * New Game creates a Steam lobby, then a listen
 * server, then the Steam invite overlay. A friend
 * who presses Play on that invite joins the lobby
 * and travels to the host. Without Steam the same
 * New Game still hosts on this machine.
 */
UCLASS()
class JOA_ARENA_API UJoA_GameInstance
	: public UGameInstance
{
	GENERATED_BODY()

public:
	virtual void Init() override;
	virtual void Shutdown() override;
	~UJoA_GameInstance();
	virtual void LoadComplete(
		const float LoadTime,
		const FString& MapName) override;
	virtual void OnWorldChanged(
		UWorld* OldWorld,
		UWorld* NewWorld) override;

	/* Steam lobby, then Arena as listen server, then
	 * the Steam invite overlay. No Steam: listen only. */
	void HostMatch();

	/* Drops the Steam lobby. A host who left or quit
	 * must not stay listed as in a match. */
	void LeaveOnlineSession();

	/* Steam OSS is up and has a session interface. */
	bool CanUseSteamSession() const;

	/* A named GameSession lobby exists on Steam. */
	bool HasOnlineSession() const;

	/* Empty when this SteamID may join the listen
	 * server. The host ID is refused. Once another
	 * peer is registered, only that peer may join. */
	FString SteamPeerRefusal(
		const FString& SteamId) const;

	/* Called on the client after every join, and so also
	 * after a successful reconnect. */
	void StoreReconnectTicket(
		const FString& InConnectString,
		const FString& InToken,
		int32 InSeat);

	/* The match is over, the player left on purpose, or
	 * the server refused the seat. No more attempts. */
	void ClearReconnectTicket();

	bool HasReconnectTicket() const
	{
		return bHasTicket;
	}

	// True from a lost connection until it is restored.
	bool IsReconnecting() const
	{
		return bReconnecting;
	}

	int32 GetReconnectAttempt() const
	{
		return ReconnectAttempt;
	}

	/* Skips the wait before the next attempt. A watched
	 * test uses it to hold the match paused first. */
	void ReconnectNow();

	/* Adds the seat token when a ticket for this address
	 * exists. A window that was closed or crashed has no
	 * live ticket, so the token comes from the file. */
	FString BuildJoinUrl(const FString& Address) const;

	/* The last reason a join failed, for the menu. Read
	 * once, then it is gone. */
	FString ConsumeNetworkNotice();

	/* String Table key of the menu line after the match
	 * was left. It stays through the reconnect retries,
	 * which reload the menu, and is cleared by the next
	 * join or host. */
	void SetMenuNotice(const FName& InNoticeKey);
	void ClearMenuNotice();

	/* Winner already saw the win in the match.
	 * The menu stays empty. A later disconnect
	 * must not write the host-closed line. */
	void HoldEmptyMenuNotice();

	// Next join or host may show its own line.
	void ReleaseMenuNoticeHold()
	{
		bHoldMenuNotice = false;
	}

	const FName& GetMenuNotice() const
	{
		return MenuNotice;
	}

	/* Menu choice. Stepping onto a flag picks it up.
	 * Each player keeps their own. The match controller
	 * copies it, and a client sends it to the server. */
	bool GetAutoPickupFlag() const
	{
		return bAutoPickupFlag;
	}
	void SetAutoPickupFlag(bool bEnabled);

	static constexpr int32 MinOpeningObstaclePairs = 0;
	static constexpr int32 MaxOpeningObstaclePairs = 10;
	static constexpr int32 MinOpeningObstacleSeed = 0;
	static constexpr int32 MaxOpeningObstacleSeed =
		2147483647;

	/* Mirrored obstacle pairs placed when this player
	 * hosts. 0 is an empty board. 6 is the usual count. */
	int32 GetOpeningObstaclePairs() const
	{
		return OpeningObstaclePairs;
	}
	void SetOpeningObstaclePairs(int32 PairCount);

	/* Same value every match keeps the same layout.
	 * FRandomStream only accepts this int32 range. */
	int32 GetOpeningObstacleSeed() const
	{
		return OpeningObstacleSeed;
	}
	void SetOpeningObstacleSeed(int32 Seed);

	/* 0 is silent. 100 is full. Each player
	 * keeps their own. It is not replicated. */
	int32 GetSoundVolume() const
	{
		return SoundVolume;
	}
	void SetSoundVolume(int32 Volume);

	/* Key hints beside the board. Each player
	 * keeps their own. It is not replicated. */
	bool GetShowTutorial() const
	{
		return bShowTutorial;
	}
	void SetShowTutorial(bool bEnabled);

	/* 0 silences every sound on this instance.
	 * A new map starts at full volume, so this
	 * has to run again after travel. */
	void ApplySoundVolume();

	/* The other player keeps its own instance.
	 * It reads the saved slider, then pushes it. */
	void PullSavedSoundVolume();

private:
	void LoadSavedTicket();
	void SaveTicket() const;
	static FString TicketFilePath();

	void LoadSavedSettings();
	void SaveSettings() const;
	static FString SettingsFilePath();

	void HandleNetworkFailure(
		UWorld* FailedWorld,
		UNetDriver* FailedDriver,
		ENetworkFailure::Type FailureType,
		const FString& ErrorString);

	/* The engine event reaches every game instance of the
	 * process. Play In Editor runs several. */
	bool OwnsDriver(
		const UWorld* FailedWorld,
		const UNetDriver* FailedDriver) const;

	void ScheduleReconnect();
	void AttemptReconnect();

	void BindSteamSessionDelegates();
	void UnbindSteamSessionDelegates();
	void CreateHostSession();
	void TravelToArenaAsHost();
	void ShowSteamInviteUI();
	void TravelToSession();

	FDelegateHandle FailureHandle;
	FTimerHandle RetryTimer;

	FDelegateHandle CreateSessionHandle;
	FDelegateHandle DestroySessionHandle;
	FDelegateHandle JoinSessionHandle;
	FDelegateHandle InviteAcceptedHandle;

	/* Online types stay in the cpp. This header is
	 * included by widgets that only read a volume. */
	friend struct FJoA_SteamSession;
	TUniquePtr<FJoA_SteamSession> SteamSession;

	FString ConnectString;
	FString Token;
	int32 TicketSeat = INDEX_NONE;
	bool bHasTicket = false;

	// Ticket read from disk. Used only by a manual join.
	FString SavedConnectString;
	FString SavedToken;

	FString NetworkNotice;
	FName MenuNotice;

	/* HoldEmptyMenuNotice. Cleared by a real
	 * notice, so the next join can speak. */
	bool bHoldMenuNotice = false;

	bool bAutoPickupFlag = true;
	bool bShowTutorial = true;
	int32 OpeningObstaclePairs = 6;
	int32 OpeningObstacleSeed = 1337;
	int32 SoundVolume = 100;

	bool bReconnecting = false;
	int32 ReconnectAttempt = 0;
	double LostAtSeconds = 0.0;

	bool bSteamDelegatesBound = false;
	bool bHostAfterDestroy = false;
	bool bJoinAfterDestroy = false;
	bool bShowInviteAfterTravel = false;
	bool bHostInProgress = false;
};
