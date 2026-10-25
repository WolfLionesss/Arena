// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

// --- Engine ---
#include "GameFramework/GameModeBase.h"

#include "JoA_MenuGameMode.generated.h"

/**
 * Game mode of the Menu map. Set it as GameMode Override
 * in the World Settings of that map. The project default
 * game mode runs the match and would spawn the arena.
 */
UCLASS()
class JOA_ARENA_API AJoA_MenuGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AJoA_MenuGameMode();
};
