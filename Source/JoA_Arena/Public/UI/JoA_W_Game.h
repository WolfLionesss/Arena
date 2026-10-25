// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

// --- Engine ---
#include "Blueprint/UserWidget.h"

// --- GAME ---
#include "Player/JoA_SeatTypes.h"

#include "JoA_W_Game.generated.h"

class UButton;

/**
 * WBP_Game. Rock, paper, or scissors before the
 * first turn. The winner of this throw moves
 * first. The widget is removed after the local
 * pick so board input still reaches the client.
 */
UCLASS()
class JOA_ARENA_API UJoA_W_Game : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;

	UFUNCTION()
	void OnRockClicked();

	UFUNCTION()
	void OnPaperClicked();

	UFUNCTION()
	void OnScissorsClicked();

	void Choose(EOpeningThrow Throw);

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> Btn_Rock;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> Btn_Paper;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> Btn_Scissors;
};
