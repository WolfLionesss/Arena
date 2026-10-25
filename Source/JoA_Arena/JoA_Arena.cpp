// Copyright Epic Games, Inc. All Rights Reserved.

#include "JoA_Arena.h"

// --- Engine ---
#include "GenericPlatform/GenericPlatformFile.h"
#include "HAL/PlatformProcess.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"

DEFINE_LOG_CATEGORY(LogJoA_Net);

/* Shipping builds of the engine skip this file.
 * SteamAPI_Init then has no app id, so the
 * overlay never hooks and Shift+Tab draws
 * nothing. Non-shipping writes it inside Init.
 * This writes the same file before that call. */
static void JoA_WriteSteamAppIdFile()
{
	int32 SteamDevAppId = 0;
	if (!GConfig
		|| !GConfig->GetInt(
			TEXT("OnlineSubsystemSteam"),
			TEXT("SteamDevAppId"),
			SteamDevAppId,
			GEngineIni)
		|| SteamDevAppId <= 0)
	{
		return;
	}

	const FString FilePath = FPaths::Combine(
		FPlatformProcess::BaseDir(),
		TEXT("steam_appid.txt"));
	IPlatformFile& Physical =
		IPlatformFile::GetPlatformPhysical();
	TUniquePtr<IFileHandle> Handle(
		Physical.OpenWrite(*FilePath, false, false));
	if (!Handle)
	{
		return;
	}

	const FTCHARToUTF8 Digits(
		*FString::FromInt(SteamDevAppId));
	Handle->Write(
		(const uint8*)Digits.Get(),
		Digits.Length());
}

class FJoA_ArenaModule : public FDefaultGameModuleImpl
{
public:
	virtual void StartupModule() override
	{
		JoA_WriteSteamAppIdFile();
	}
};

IMPLEMENT_PRIMARY_GAME_MODULE(
	FJoA_ArenaModule,
	JoA_Arena,
	"JoA_Arena");
