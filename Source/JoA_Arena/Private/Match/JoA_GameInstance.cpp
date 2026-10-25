// Copyright Epic Games, Inc. All Rights Reserved.

#include "Match/JoA_GameInstance.h"

// --- Engine ---
#include "AudioDevice.h"
#include "Engine/Engine.h"
#include "Engine/NetDriver.h"
#include "Engine/PendingNetGame.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "HAL/FileManager.h"
#include "Interfaces/OnlineExternalUIInterface.h"
#include "Interfaces/OnlineIdentityInterface.h"
#include "Interfaces/OnlineSessionInterface.h"
#include "Internationalization/StringTable.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/Paths.h"
#include "Online/OnlineSessionNames.h"
#include "OnlineSessionSettings.h"
#include "OnlineSubsystem.h"
#include "TimerManager.h"

// --- GAME ---
#include "JoA_Arena.h"
#include "Match/JoA_MatchSettings.h"
#include "UI/JoA_UIText.h"

namespace
{
	const int32 MinSoundVolume = 0;
	const int32 MaxSoundVolume = 100;
	const TCHAR* SettingsSection = TEXT("Settings");
}

namespace
{
	const TCHAR* HostArenaMap =
		TEXT("/Game/Map/Arena");

	IOnlineSessionPtr JoA_FindSteamSession()
	{
		IOnlineSubsystem* SteamOSS =
			IOnlineSubsystem::Get(STEAM_SUBSYSTEM);
		return SteamOSS
			? SteamOSS->GetSessionInterface()
			: IOnlineSessionPtr();
	}
}

struct FJoA_SteamSession
{
	FOnlineSessionSearchResult PendingInvite;

	static void OnCreate(
		FName SessionName,
		bool bWasSuccessful,
		UJoA_GameInstance* Instance);
	static void OnDestroy(
		FName SessionName,
		bool bWasSuccessful,
		UJoA_GameInstance* Instance);
	static void OnJoin(
		FName SessionName,
		EOnJoinSessionCompleteResult::Type Result,
		UJoA_GameInstance* Instance);
	static void OnInviteAccepted(
		bool bWasSuccessful,
		int32,
		FUniqueNetIdPtr,
		const FOnlineSessionSearchResult& InviteResult,
		UJoA_GameInstance* Instance);
};

UJoA_GameInstance::~UJoA_GameInstance() = default;

void UJoA_GameInstance::Init()
{
	Super::Init();

	/* A cooked build does not guarantee the String Table
	 * is loaded when the first widget asks for a key. The
	 * HUD would show missing-entry text. Rooted so GC does
	 * not unregister the table. */
	UStringTable* UITable = LoadObject<UStringTable>(
		nullptr,
		*JoA_UITextTableId().ToString());
	if (UITable)
	{
		UITable->AddToRoot();
	}

	LoadSavedTicket();
	LoadSavedSettings();
	BindSteamSessionDelegates();

	if (GEngine)
	{
		FailureHandle =
			GEngine->OnNetworkFailure().AddUObject(
				this,
				&UJoA_GameInstance::HandleNetworkFailure);
	}
}

void UJoA_GameInstance::Shutdown()
{
	if (GEngine)
	{
		GEngine->OnNetworkFailure().Remove(FailureHandle);
	}

	GetTimerManager().ClearTimer(RetryTimer);
	LeaveOnlineSession();
	UnbindSteamSessionDelegates();
	Super::Shutdown();
}

void UJoA_GameInstance::LoadComplete(
	const float LoadTime,
	const FString& MapName)
{
	Super::LoadComplete(LoadTime, MapName);
	ApplySoundVolume();
	if (!bShowInviteAfterTravel
		|| !MapName.Contains(TEXT("Arena")))
	{
		return;
	}

	bShowInviteAfterTravel = false;
	ShowSteamInviteUI();
}

void UJoA_GameInstance::OnWorldChanged(
	UWorld* OldWorld,
	UWorld* NewWorld)
{
	Super::OnWorldChanged(OldWorld, NewWorld);
	ApplySoundVolume();
}

void UJoA_GameInstance::StoreReconnectTicket(
	const FString& InConnectString,
	const FString& InToken,
	int32 InSeat)
{
	ConnectString = InConnectString;
	Token = InToken;
	TicketSeat = InSeat;
	bHasTicket = true;

	if (bReconnecting)
	{
		UE_LOG(
			LogJoA_Net,
			Log,
			TEXT("Reconnected to seat %d ")
			TEXT("after %d attempts."),
			TicketSeat,
			ReconnectAttempt);
	}

	bReconnecting = false;
	ReconnectAttempt = 0;
	MenuNotice = NAME_None;
	bHoldMenuNotice = false;
	GetTimerManager().ClearTimer(RetryTimer);
	SaveTicket();
}

void UJoA_GameInstance::ClearReconnectTicket()
{
	bHasTicket = false;
	bReconnecting = false;
	ReconnectAttempt = 0;
	TicketSeat = INDEX_NONE;
	ConnectString.Reset();
	Token.Reset();
	SavedConnectString.Reset();
	SavedToken.Reset();
	GetTimerManager().ClearTimer(RetryTimer);
	IFileManager::Get().Delete(
		*TicketFilePath(),
		false,
		true,
		true);
}

FString UJoA_GameInstance::TicketFilePath()
{
	return FPaths::ProjectSavedDir()
		/ TEXT("Config/JoA_Reconnect.ini");
}

void UJoA_GameInstance::LoadSavedTicket()
{
	/* The editor runs several game instances in one
	 * process and one Saved folder. */
	if (GIsEditor)
	{
		return;
	}

	FConfigFile File;
	File.Read(TicketFilePath());
	File.GetString(
		TEXT("Ticket"),
		TEXT("Address"),
		SavedConnectString);
	File.GetString(
		TEXT("Ticket"),
		TEXT("Token"),
		SavedToken);

	UE_LOG(
		LogJoA_Net,
		Log,
		TEXT("Loaded reconnect ticket for %s, token %s."),
		*SavedConnectString,
		SavedToken.IsEmpty()
			? TEXT("none")
			: TEXT("present"));
}

void UJoA_GameInstance::SaveTicket() const
{
	if (GIsEditor || !bHasTicket)
	{
		return;
	}

	/* A closed or crashed window loses the memory ticket.
	 * The file lets the next Join Game take the held seat
	 * back within the grace time. */
	FConfigFile File;
	File.SetString(
		TEXT("Ticket"),
		TEXT("Address"),
		*ConnectString);
	File.SetString(
		TEXT("Ticket"),
		TEXT("Token"),
		*Token);
	File.Write(TicketFilePath());
	UE_LOG(
		LogJoA_Net,
		Log,
		TEXT("Saved reconnect ticket for %s."),
		*ConnectString);
}

FString UJoA_GameInstance::SettingsFilePath()
{
	return FPaths::ProjectSavedDir()
		/ TEXT("Config/JoA_Settings.ini");
}

void UJoA_GameInstance::LoadSavedSettings()
{
	FConfigFile File;
	File.Read(SettingsFilePath());

	bool bLoadedPickup = bAutoPickupFlag;
	if (File.GetBool(
		SettingsSection,
		TEXT("AutoPickup"),
		bLoadedPickup))
	{
		bAutoPickupFlag = bLoadedPickup;
	}

	int64 LoadedPairs = OpeningObstaclePairs;
	if (File.GetInt64(
		SettingsSection,
		TEXT("ObstaclePairs"),
		LoadedPairs))
	{
		const int64 ClampedPairs = FMath::Clamp(
			LoadedPairs,
			static_cast<int64>(MinOpeningObstaclePairs),
			static_cast<int64>(MaxOpeningObstaclePairs));
		OpeningObstaclePairs =
			static_cast<int32>(ClampedPairs);
	}

	int64 LoadedSeed = OpeningObstacleSeed;
	if (File.GetInt64(
		SettingsSection,
		TEXT("ObstacleSeed"),
		LoadedSeed))
	{
		const int64 ClampedSeed = FMath::Clamp(
			LoadedSeed,
			static_cast<int64>(MinOpeningObstacleSeed),
			static_cast<int64>(MaxOpeningObstacleSeed));
		OpeningObstacleSeed =
			static_cast<int32>(ClampedSeed);
	}

	int64 LoadedVolume = SoundVolume;
	if (File.GetInt64(
		SettingsSection,
		TEXT("Sound"),
		LoadedVolume))
	{
		SoundVolume = FMath::Clamp(
			static_cast<int32>(LoadedVolume),
			MinSoundVolume,
			MaxSoundVolume);
	}

	bool bLoadedTutorial = bShowTutorial;
	if (File.GetBool(
		SettingsSection,
		TEXT("Tutorial"),
		bLoadedTutorial))
	{
		bShowTutorial = bLoadedTutorial;
	}

	ApplySoundVolume();
}

void UJoA_GameInstance::SaveSettings() const
{
	/* The menu and the match are different worlds.
	 * The file is what the next launch still has.
	 * Play In Editor shares one file between its
	 * players. The match still uses this instance. */
	FConfigFile File;
	File.Read(SettingsFilePath());
	File.SetString(
		SettingsSection,
		TEXT("AutoPickup"),
		bAutoPickupFlag ? TEXT("True") : TEXT("False"));
	File.SetString(
		SettingsSection,
		TEXT("ObstaclePairs"),
		*FString::FromInt(OpeningObstaclePairs));
	File.SetString(
		SettingsSection,
		TEXT("ObstacleSeed"),
		*FString::FromInt(OpeningObstacleSeed));
	File.SetString(
		SettingsSection,
		TEXT("Sound"),
		*FString::FromInt(SoundVolume));
	File.SetString(
		SettingsSection,
		TEXT("Tutorial"),
		bShowTutorial ? TEXT("True") : TEXT("False"));
	File.Write(SettingsFilePath());
}

void UJoA_GameInstance::SetSoundVolume(int32 Volume)
{
	const int32 Clamped = FMath::Clamp(
		Volume,
		MinSoundVolume,
		MaxSoundVolume);
	const bool bChanged = SoundVolume != Clamped;
	SoundVolume = Clamped;
	if (bChanged)
	{
		SaveSettings();
	}

	/* Same number still has to reach the device.
	 * A 0% mute otherwise stays after 100%. */
	ApplySoundVolume();
}

void UJoA_GameInstance::PullSavedSoundVolume()
{
	FConfigFile File;
	File.Read(SettingsFilePath());
	int64 Loaded = SoundVolume;
	if (File.GetInt64(
		SettingsSection,
		TEXT("Sound"),
		Loaded))
	{
		SoundVolume = FMath::Clamp(
			static_cast<int32>(Loaded),
			MinSoundVolume,
			MaxSoundVolume);
	}

	ApplySoundVolume();
}

void UJoA_GameInstance::ApplySoundVolume()
{
	if (!GEngine)
	{
		return;
	}

	const float Gain =
		static_cast<float>(SoundVolume) / 100.f;
	for (const FWorldContext& Context :
		GEngine->GetWorldContexts())
	{
		UWorld* World = Context.World();
		if (!World || World->GetGameInstance() != this)
		{
			continue;
		}

		FAudioDeviceHandle Device =
			World->GetAudioDevice();
		if (!Device.IsValid())
		{
			continue;
		}

		Device->SetTransientPrimaryVolume(Gain);
	}
}

void UJoA_GameInstance::SetAutoPickupFlag(bool bEnabled)
{
	if (bAutoPickupFlag == bEnabled)
	{
		return;
	}

	bAutoPickupFlag = bEnabled;
	SaveSettings();
}

void UJoA_GameInstance::SetShowTutorial(bool bEnabled)
{
	if (bShowTutorial == bEnabled)
	{
		return;
	}

	bShowTutorial = bEnabled;
	SaveSettings();
}

void UJoA_GameInstance::SetOpeningObstaclePairs(
	int32 PairCount)
{
	const int32 Clamped = FMath::Clamp(
		PairCount,
		MinOpeningObstaclePairs,
		MaxOpeningObstaclePairs);
	if (OpeningObstaclePairs == Clamped)
	{
		return;
	}

	OpeningObstaclePairs = Clamped;
	SaveSettings();
}

void UJoA_GameInstance::SetOpeningObstacleSeed(
	int32 Seed)
{
	const int32 Clamped = FMath::Clamp(
		Seed,
		MinOpeningObstacleSeed,
		MaxOpeningObstacleSeed);
	if (OpeningObstacleSeed == Clamped)
	{
		return;
	}

	OpeningObstacleSeed = Clamped;
	SaveSettings();
}

FString UJoA_GameInstance::BuildJoinUrl(
	const FString& Address) const
{
	/* The stored host has no port (engine default 7777).
	 * The menu always sends host:port. Matching strings
	 * would drop the token and the server would take the
	 * player for a third seat. */
	const FString UseToken =
		!Token.IsEmpty() ? Token : SavedToken;
	if (UseToken.IsEmpty())
	{
		return Address;
	}

	UE_LOG(
		LogJoA_Net,
		Log,
		TEXT("Join uses saved reconnect token."));
	const TCHAR* Sep = Address.Contains(TEXT("?"))
		? TEXT("&")
		: TEXT("?");
	return FString::Printf(
		TEXT("%s%sReconnectToken=%s"),
		*Address,
		Sep,
		*UseToken);
}

void UJoA_GameInstance::SetMenuNotice(const FName& InNoticeKey)
{
	MenuNotice = InNoticeKey;
	bHoldMenuNotice = false;
}

void UJoA_GameInstance::ClearMenuNotice()
{
	MenuNotice = NAME_None;
	bHoldMenuNotice = false;
}

void UJoA_GameInstance::HoldEmptyMenuNotice()
{
	MenuNotice = NAME_None;
	bHoldMenuNotice = true;
}

FString UJoA_GameInstance::ConsumeNetworkNotice()
{
	FString Notice = NetworkNotice;
	NetworkNotice.Reset();
	return Notice;
}

bool UJoA_GameInstance::OwnsDriver(
	const UWorld* FailedWorld,
	const UNetDriver* FailedDriver) const
{
	if (!FailedDriver)
	{
		return false;
	}

	if (FailedWorld)
	{
		return FailedWorld->GetGameInstance() == this;
	}

	// A driver still connecting has no world yet.
	const FWorldContext* Context = GetWorldContext();
	return Context
		&& Context->PendingNetGame
		&& Context->PendingNetGame->NetDriver
			== FailedDriver;
}

void UJoA_GameInstance::HandleNetworkFailure(
	UWorld* FailedWorld,
	UNetDriver* FailedDriver,
	ENetworkFailure::Type FailureType,
	const FString& ErrorString)
{
	if (!OwnsDriver(FailedWorld, FailedDriver)
		|| (FailedDriver && FailedDriver->IsServer()))
	{
		return;
	}

	const bool bMatchFull =
		ErrorString.Contains(TEXT("Match is full"));
	const bool bMatchOver =
		ErrorString.Contains(TEXT("Match is over"));
	const bool bJoinRefused =
		FailureType
			== ENetworkFailure::PendingConnectionFailure
		|| bMatchFull
		|| bMatchOver;
	/* Notices are String Table keys, so a designer
	 * can rewrite the text without breaking these
	 * checks. */
	const bool bLeftOnPurpose =
		MenuNotice == FName(TEXT("Play.DefeatForfeit"));
	const bool bKeepNotice =
		MenuNotice
			== FName(TEXT("Menu.Notice.MatchFull"))
		|| MenuNotice
			== FName(TEXT("Menu.Notice.MatchOver"))
		|| bLeftOnPurpose
		|| bHoldMenuNotice;

	if (bMatchFull)
	{
		MenuNotice = TEXT("Menu.Notice.MatchFull");
	}
	else if (bMatchOver)
	{
		MenuNotice = TEXT("Menu.Notice.MatchOver");
	}
	else if (!bJoinRefused && !bKeepNotice)
	{
		/* A refused join then tears the pending driver
		 * down. That second event is not a lost host.
		 * Only a player who was in the match is told the
		 * host went away. */
		const bool bFromArena = FailedWorld
			&& FailedWorld->GetMapName().Contains(
				TEXT("Arena"));
		if (bHasTicket || bFromArena)
		{
			MenuNotice =
				TEXT("Menu.Notice.HostClosed");
		}
	}

	/* Status is for other join errors. Full and over go to
	 * TB_Info, so they are not repeated here. */
	if (!bMatchFull && !bMatchOver)
	{
		NetworkNotice = ErrorString;
	}

	if (!bHasTicket)
	{
		/* The server answered a manual join with the file
		 * token and still said no. The seat is gone. */
		if (ErrorString.Contains(TEXT("Match is")))
		{
			ClearReconnectTicket();
		}
		return;
	}

	/* The server answered and said no, for example the
	 * seat was already given away or the player was
	 * kicked. Trying again changes nothing. */
	if (FailureType == ENetworkFailure::FailureReceived
		&& bJoinRefused)
	{
		UE_LOG(
			LogJoA_Net,
			Warning,
			TEXT("Server refused: %s. No more attempts."),
			*ErrorString);
		ClearReconnectTicket();
		return;
	}

	if (FailureType == ENetworkFailure::FailureReceived
		&& !bJoinRefused)
	{
		/* The host closed the process. The listen server
		 * is gone, so further attempts cannot restore it. */
		UE_LOG(
			LogJoA_Net,
			Warning,
			TEXT("Host closed the connection: %s."),
			*ErrorString);
		ClearReconnectTicket();
		return;
	}

	if (!bReconnecting)
	{
		bReconnecting = true;
		ReconnectAttempt = 0;
		LostAtSeconds = FPlatformTime::Seconds();
		UE_LOG(
			LogJoA_Net,
			Warning,
			TEXT("Connection lost (%s): %s. ")
			TEXT("Seat %d is held."),
			ENetworkFailure::ToString(FailureType),
			*ErrorString,
			TicketSeat);
	}

	/* A failed attempt reports here again, so each
	 * failure schedules the next one. */
	ScheduleReconnect();
}

void UJoA_GameInstance::ScheduleReconnect()
{
	const UJoA_MatchSettings* Rules =
		GetDefault<UJoA_MatchSettings>();
	GetTimerManager().SetTimer(
		RetryTimer,
		this,
		&UJoA_GameInstance::AttemptReconnect,
		Rules->ClientRetryIntervalSeconds,
		false);
}

void UJoA_GameInstance::ReconnectNow()
{
	if (bReconnecting)
	{
		AttemptReconnect();
	}
}

void UJoA_GameInstance::AttemptReconnect()
{
	const UJoA_MatchSettings* Rules =
		GetDefault<UJoA_MatchSettings>();
	const double Waited =
		FPlatformTime::Seconds() - LostAtSeconds;
	const double Window =
		Rules->ReconnectGraceSeconds
		+ Rules->ClientRetryMarginSeconds;
	if (!bHasTicket || Waited > Window)
	{
		UE_LOG(
			LogJoA_Net,
			Warning,
			TEXT("Giving up on seat %d after %.0f s."),
			TicketSeat,
			Waited);
		ClearReconnectTicket();
		return;
	}

	UWorld* CurrentWorld = GetWorld();
	if (!GEngine || !CurrentWorld)
	{
		ScheduleReconnect();
		return;
	}

	++ReconnectAttempt;
	UE_LOG(
		LogJoA_Net,
		Log,
		TEXT("Reconnect attempt %d to %s, %.0f s lost."),
		ReconnectAttempt,
		*ConnectString,
		Waited);

	const FString Url = FString::Printf(
		TEXT("%s?ReconnectToken=%s"),
		*ConnectString,
		*Token);
	GEngine->SetClientTravel(
		CurrentWorld,
		*Url,
		TRAVEL_Absolute);
}

void UJoA_GameInstance::BindSteamSessionDelegates()
{
	if (bSteamDelegatesBound)
	{
		return;
	}

	const IOnlineSessionPtr Session =
		JoA_FindSteamSession();
	if (!Session.IsValid())
	{
		UE_LOG(
			LogJoA_Net,
			Log,
			TEXT("Steam session is not available. ")
			TEXT("New Game hosts on this machine."));
		return;
	}

	const FOnCreateSessionCompleteDelegate OnCreate =
		FOnCreateSessionCompleteDelegate::CreateWeakLambda(
			this,
			[this](FName SessionName, bool bWasSuccessful)
			{
				FJoA_SteamSession::OnCreate(
					SessionName,
					bWasSuccessful,
					this);
			});
	CreateSessionHandle =
		Session->AddOnCreateSessionCompleteDelegate_Handle(
			OnCreate);

	const FOnDestroySessionCompleteDelegate OnDestroy =
		FOnDestroySessionCompleteDelegate::CreateWeakLambda(
			this,
			[this](FName SessionName, bool bWasSuccessful)
			{
				FJoA_SteamSession::OnDestroy(
					SessionName,
					bWasSuccessful,
					this);
			});
	DestroySessionHandle =
		Session->AddOnDestroySessionCompleteDelegate_Handle(
			OnDestroy);

	const FOnJoinSessionCompleteDelegate OnJoin =
		FOnJoinSessionCompleteDelegate::CreateWeakLambda(
			this,
			[this](
				FName SessionName,
				EOnJoinSessionCompleteResult::Type Result)
			{
				FJoA_SteamSession::OnJoin(
					SessionName,
					Result,
					this);
			});
	JoinSessionHandle =
		Session->AddOnJoinSessionCompleteDelegate_Handle(
			OnJoin);

	const FOnSessionUserInviteAcceptedDelegate
		OnInvite =
			FOnSessionUserInviteAcceptedDelegate::
			CreateWeakLambda(
				this,
				[this](
					bool bWasSuccessful,
					int32 ControllerId,
					FUniqueNetIdPtr UserId,
					const FOnlineSessionSearchResult&
						InviteResult)
				{
					FJoA_SteamSession::OnInviteAccepted(
						bWasSuccessful,
						ControllerId,
						UserId,
						InviteResult,
						this);
				});
	InviteAcceptedHandle =
		Session->
		AddOnSessionUserInviteAcceptedDelegate_Handle(
			OnInvite);
	bSteamDelegatesBound = true;
}

void UJoA_GameInstance::UnbindSteamSessionDelegates()
{
	if (!bSteamDelegatesBound)
	{
		return;
	}

	const IOnlineSessionPtr Session =
		JoA_FindSteamSession();
	if (!Session.IsValid())
	{
		bSteamDelegatesBound = false;
		return;
	}

	Session->ClearOnCreateSessionCompleteDelegate_Handle(
		CreateSessionHandle);
	Session->ClearOnDestroySessionCompleteDelegate_Handle(
		DestroySessionHandle);
	Session->ClearOnJoinSessionCompleteDelegate_Handle(
		JoinSessionHandle);
	Session->
		ClearOnSessionUserInviteAcceptedDelegate_Handle(
			InviteAcceptedHandle);

	bSteamDelegatesBound = false;
}

void UJoA_GameInstance::HostMatch()
{
	if (bHostInProgress)
	{
		return;
	}

	bHostInProgress = true;
	ClearMenuNotice();
	ClearReconnectTicket();
	bShowInviteAfterTravel = false;
	bJoinAfterDestroy = false;

	const IOnlineSessionPtr Session =
		JoA_FindSteamSession();
	if (!Session.IsValid())
	{
		TravelToArenaAsHost();
		return;
	}

	BindSteamSessionDelegates();
	if (Session->GetNamedSession(NAME_GameSession))
	{
		bHostAfterDestroy = true;
		if (!Session->DestroySession(NAME_GameSession))
		{
			bHostAfterDestroy = false;
			TravelToArenaAsHost();
		}
		return;
	}

	CreateHostSession();
}

void UJoA_GameInstance::CreateHostSession()
{
	const IOnlineSessionPtr Session =
		JoA_FindSteamSession();
	if (!Session.IsValid())
	{
		TravelToArenaAsHost();
		return;
	}

	FOnlineSessionSettings Settings;
	Settings.bIsLANMatch = false;
	Settings.NumPublicConnections = 2;
	Settings.bShouldAdvertise = true;
	Settings.bAllowJoinInProgress = true;
	Settings.bAllowInvites = true;
	Settings.bUsesPresence = true;
	Settings.bUseLobbiesIfAvailable = true;
	Settings.bAllowJoinViaPresence = true;
	Settings.bIsDedicated = false;
	Settings.Set(
		SETTING_MAPNAME,
		FString(HostArenaMap),
		EOnlineDataAdvertisementType::ViaOnlineService);

	if (!Session->CreateSession(
		0,
		NAME_GameSession,
		Settings))
	{
		UE_LOG(
			LogJoA_Net,
			Warning,
			TEXT("CreateSession failed at once. ")
			TEXT("Hosting on this machine."));
		TravelToArenaAsHost();
	}
}

void UJoA_GameInstance::TravelToArenaAsHost()
{
	bHostInProgress = false;
	UGameplayStatics::OpenLevel(
		this,
		FName(HostArenaMap),
		true,
		TEXT("listen"));
}

void UJoA_GameInstance::ShowSteamInviteUI()
{
	IOnlineSubsystem* SteamOSS =
		IOnlineSubsystem::Get(STEAM_SUBSYSTEM);
	if (!SteamOSS)
	{
		return;
	}

	const IOnlineExternalUIPtr ExternalUI =
		SteamOSS->GetExternalUIInterface();
	if (!ExternalUI.IsValid()
		|| !ExternalUI->ShowInviteUI(
			0,
			NAME_GameSession))
	{
		UE_LOG(
			LogJoA_Net,
			Warning,
			TEXT("Steam invite overlay did not open. ")
			TEXT("Invite from the Steam overlay."));
	}
}

bool UJoA_GameInstance::CanUseSteamSession() const
{
	return JoA_FindSteamSession().IsValid();
}

bool UJoA_GameInstance::HasOnlineSession() const
{
	const IOnlineSessionPtr Session =
		JoA_FindSteamSession();
	return Session.IsValid()
		&& Session->GetNamedSession(NAME_GameSession)
			!= nullptr;
}

FString UJoA_GameInstance::SteamPeerRefusal(
	const FString& SteamId) const
{
	if (SteamId.IsEmpty() || !HasOnlineSession())
	{
		return TEXT("Join through the Steam invite.");
	}

	IOnlineSubsystem* SteamOSS =
		IOnlineSubsystem::Get(STEAM_SUBSYSTEM);
	const IOnlineIdentityPtr Identity = SteamOSS
		? SteamOSS->GetIdentityInterface()
		: IOnlineIdentityPtr();
	const FUniqueNetIdPtr HostId = Identity.IsValid()
		? Identity->GetUniquePlayerId(0)
		: FUniqueNetIdPtr();
	const FString HostKey = HostId.IsValid()
		? HostId->ToString()
		: FString();
	if (!HostKey.IsEmpty() && HostKey == SteamId)
	{
		return TEXT("Match is full.");
	}

	const IOnlineSessionPtr Session =
		JoA_FindSteamSession();
	FNamedOnlineSession* Named = Session.IsValid()
		? Session->GetNamedSession(NAME_GameSession)
		: nullptr;
	if (!Named)
	{
		return TEXT("Join through the Steam invite.");
	}

	/* RegisterPlayer runs after a successful login,
	 * so the first Steam peer is not listed yet.
	 * A later peer is refused while one is listed. */
	bool bOtherPeer = false;
	for (const FUniqueNetIdRef& Player :
		Named->RegisteredPlayers)
	{
		const FString Listed = Player->ToString();
		if (Listed.IsEmpty()
			|| (!HostKey.IsEmpty() && Listed == HostKey))
		{
			continue;
		}
		if (Listed == SteamId)
		{
			return FString();
		}
		bOtherPeer = true;
	}

	return bOtherPeer
		? FString(TEXT("Match is full."))
		: FString();
}

void UJoA_GameInstance::LeaveOnlineSession()
{
	bHostAfterDestroy = false;
	bJoinAfterDestroy = false;
	bShowInviteAfterTravel = false;

	const IOnlineSessionPtr Session =
		JoA_FindSteamSession();
	if (!Session.IsValid()
		|| !Session->GetNamedSession(NAME_GameSession))
	{
		return;
	}

	Session->DestroySession(NAME_GameSession);
}

void FJoA_SteamSession::OnCreate(
	FName SessionName,
	bool bWasSuccessful,
	UJoA_GameInstance* Instance)
{
	if (!Instance || SessionName != NAME_GameSession)
	{
		return;
	}

	if (!bWasSuccessful)
	{
		UE_LOG(
			LogJoA_Net,
			Warning,
			TEXT("Steam lobby was not created. ")
			TEXT("Hosting on this machine."));
		Instance->TravelToArenaAsHost();
		return;
	}

	/* Invite after Arena is up. A friend who presses
	 * Play at once then finds a listening host. */
	Instance->bShowInviteAfterTravel = true;
	Instance->TravelToArenaAsHost();
}

void FJoA_SteamSession::OnDestroy(
	FName SessionName,
	bool bWasSuccessful,
	UJoA_GameInstance* Instance)
{
	if (!Instance || SessionName != NAME_GameSession)
	{
		return;
	}

	if (Instance->bHostAfterDestroy)
	{
		Instance->bHostAfterDestroy = false;
		if (bWasSuccessful)
		{
			Instance->CreateHostSession();
		}
		else
		{
			Instance->TravelToArenaAsHost();
		}
		return;
	}

	if (!Instance->bJoinAfterDestroy)
	{
		return;
	}

	Instance->bJoinAfterDestroy = false;
	if (!bWasSuccessful || !Instance->SteamSession)
	{
		Instance->SetMenuNotice(
			TEXT("Menu.Notice.InviteFailed"));
		return;
	}

	const IOnlineSessionPtr Session =
		JoA_FindSteamSession();
	if (!Session.IsValid()
		|| !Session->JoinSession(
			0,
			NAME_GameSession,
			Instance->SteamSession->PendingInvite))
	{
		Instance->SetMenuNotice(
			TEXT("Menu.Notice.InviteFailed"));
	}
}

void FJoA_SteamSession::OnJoin(
	FName SessionName,
	EOnJoinSessionCompleteResult::Type Result,
	UJoA_GameInstance* Instance)
{
	if (!Instance || SessionName != NAME_GameSession)
	{
		return;
	}

	if (Result != EOnJoinSessionCompleteResult::Success)
	{
		UE_LOG(
			LogJoA_Net,
			Warning,
			TEXT("JoinSession failed (%d)."),
			static_cast<int32>(Result));
		Instance->SetMenuNotice(
			TEXT("Menu.Notice.InviteFailed"));
		return;
	}

	Instance->TravelToSession();
}

void UJoA_GameInstance::TravelToSession()
{
	const IOnlineSessionPtr Session =
		JoA_FindSteamSession();
	UWorld* CurrentWorld = GetWorld();
	if (!Session.IsValid() || !GEngine || !CurrentWorld)
	{
		SetMenuNotice(
			TEXT("Menu.Notice.InviteFailed"));
		return;
	}

	FString Address;
	if (!Session->GetResolvedConnectString(
		NAME_GameSession,
		Address)
		|| Address.IsEmpty())
	{
		UE_LOG(
			LogJoA_Net,
			Warning,
			TEXT("No Steam connect string."));
		SetMenuNotice(
			TEXT("Menu.Notice.InviteFailed"));
		return;
	}

	ClearMenuNotice();
	const FString Url = BuildJoinUrl(Address);
	UE_LOG(
		LogJoA_Net,
		Log,
		TEXT("Invite travel to %s."),
		*Url);
	GEngine->SetClientTravel(
		CurrentWorld,
		*Url,
		TRAVEL_Absolute);
}

void FJoA_SteamSession::OnInviteAccepted(
	bool bWasSuccessful,
	int32,
	FUniqueNetIdPtr,
	const FOnlineSessionSearchResult& InviteResult,
	UJoA_GameInstance* Instance)
{
	if (!Instance
		|| !bWasSuccessful
		|| !InviteResult.IsValid())
	{
		UE_LOG(
			LogJoA_Net,
			Warning,
			TEXT("Steam invite was not accepted."));
		return;
	}

	Instance->ClearMenuNotice();
	Instance->ClearReconnectTicket();
	Instance->bHostAfterDestroy = false;
	Instance->bHostInProgress = false;
	Instance->bShowInviteAfterTravel = false;

	const IOnlineSessionPtr Session =
		JoA_FindSteamSession();
	if (!Session.IsValid())
	{
		Instance->SetMenuNotice(
			TEXT("Menu.Notice.InviteFailed"));
		return;
	}

	if (!Instance->SteamSession)
	{
		Instance->SteamSession =
			MakeUnique<FJoA_SteamSession>();
	}
	Instance->SteamSession->PendingInvite = InviteResult;

	if (Session->GetNamedSession(NAME_GameSession))
	{
		Instance->bJoinAfterDestroy = true;
		if (!Session->DestroySession(NAME_GameSession))
		{
			Instance->bJoinAfterDestroy = false;
			Instance->SetMenuNotice(
				TEXT("Menu.Notice.InviteFailed"));
		}
		return;
	}

	if (!Session->JoinSession(
		0,
		NAME_GameSession,
		InviteResult))
	{
		Instance->SetMenuNotice(
			TEXT("Menu.Notice.InviteFailed"));
	}
}
