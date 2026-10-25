// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

// --- Engine ---
#include "Engine/TimerHandle.h"
#include "GameFramework/PlayerController.h"

#include "JoA_MenuPlayerController.generated.h"

class UJoA_W_Menu;

/**
 * Player controller of the Menu map. Shows WBP_Menu and
 * the mouse cursor.
 */
UCLASS()
class JOA_ARENA_API AJoA_MenuPlayerController
	: public APlayerController
{
	GENERATED_BODY()

public:
	AJoA_MenuPlayerController();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(
		const EEndPlayReason::Type Reason) override;

	/* The input mode is set again after the viewport
	 * exists, otherwise the first click only focuses the
	 * window and the text box gets no keys. */
	void ApplyMenuInput();

	/* No pawn on this map, so the view would stay
	 * at the origin, inside the tavern floor. */
	void ApplyMenuCamera();

	/* WBP_Menu, reparented to UJoA_W_Menu. When empty,
	 * /Game/WBP/WBP_Menu is loaded. */
	UPROPERTY(EditDefaultsOnly, Category = "Menu")
	TSubclassOf<UJoA_W_Menu> MenuWidgetClass;

private:
	UPROPERTY()
	TObjectPtr<UJoA_W_Menu> MenuWidget;

	FTimerHandle InputTimer;
};
