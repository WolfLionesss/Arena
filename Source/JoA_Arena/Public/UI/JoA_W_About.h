// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

// --- Engine ---
#include "Blueprint/UserWidget.h"

#include "JoA_W_About.generated.h"

class UJoA_W_Figures;
class UButton;
class UTextBlock;

DECLARE_DELEGATE(FOnAboutBack);

/**
 * WBP_About. Replaces the menu column. Goal
 * shows TB_GoalInfo. Figures shows WBP_Figures
 * in that same width. Back returns to the menu.
 */
UCLASS()
class JOA_ARENA_API UJoA_W_About : public UUserWidget
{
	GENERATED_BODY()

public:
	FOnAboutBack OnBackToMenu;

protected:
	virtual void NativeConstruct() override;

	UFUNCTION()
	void OnGoalClicked();

	UFUNCTION()
	void OnFiguresClicked();

	UFUNCTION()
	void OnBackClicked();

	void EnsureFigures();
	void PlaceBack();
	void LockGoalText();
	void ShowGoal();

	UPROPERTY()
	TObjectPtr<UButton> Btn_Goal;

	UPROPERTY()
	TObjectPtr<UButton> Btn_Figures;

	UPROPERTY()
	TObjectPtr<UTextBlock> TB_GoalInfo;

	UPROPERTY()
	TObjectPtr<UJoA_W_Figures> FiguresPage;
};
