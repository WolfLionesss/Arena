// Copyright Epic Games, Inc. All Rights Reserved.

#include "Match/JoA_MenuGameMode.h"

// --- GAME ---
#include "Player/JoA_MenuPlayerController.h"

AJoA_MenuGameMode::AJoA_MenuGameMode()
{
	PlayerControllerClass =
		AJoA_MenuPlayerController::StaticClass();
	DefaultPawnClass = nullptr;
}
