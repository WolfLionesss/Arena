// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/JoA_W_Game.h"

// --- GAME ---
#include "Player/JoA_PlayerController.h"
#include "UI/JoA_WidgetHelpers.h"

// --- Components ---
#include "Components/Button.h"

void UJoA_W_Game::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	JoA_DisableButtonFocus(Btn_Rock);
	JoA_DisableButtonFocus(Btn_Paper);
	JoA_DisableButtonFocus(Btn_Scissors);
}

void UJoA_W_Game::NativeConstruct()
{
	Super::NativeConstruct();

	/* The canvas must not hide TB_Info on WBP_Play
	 * or swallow the viewport after a bad layout. */
	SetVisibility(
		ESlateVisibility::SelfHitTestInvisible);

	if (Btn_Rock)
	{
		Btn_Rock->OnClicked.AddUniqueDynamic(
			this,
			&UJoA_W_Game::OnRockClicked);
	}
	if (Btn_Paper)
	{
		Btn_Paper->OnClicked.AddUniqueDynamic(
			this,
			&UJoA_W_Game::OnPaperClicked);
	}
	if (Btn_Scissors)
	{
		Btn_Scissors->OnClicked.AddUniqueDynamic(
			this,
			&UJoA_W_Game::OnScissorsClicked);
	}
}

void UJoA_W_Game::OnRockClicked()
{
	Choose(EOpeningThrow::Rock);
}

void UJoA_W_Game::OnPaperClicked()
{
	Choose(EOpeningThrow::Paper);
}

void UJoA_W_Game::OnScissorsClicked()
{
	Choose(EOpeningThrow::Scissors);
}

void UJoA_W_Game::Choose(EOpeningThrow Throw)
{
	AJoA_PlayerController* Controller =
		Cast<AJoA_PlayerController>(GetOwningPlayer());
	if (Controller)
	{
		Controller->RequestOpeningThrow(Throw);
	}
}
