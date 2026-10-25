// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

// --- Engine ---
#include "Blueprint/UserWidget.h"

#include "JoA_W_Figures.generated.h"

class UGridPanel;
class UImage;
class UScrollBox;
class USizeBox;
class USpacer;
class UTextBlock;

/**
 * WBP_Figures. Figure buttons fill
 * TB_FigureDescription. Ability buttons fill
 * TB_AbilitiesDescription. The two pictures
 * show only for Pick up flag.
 */
UCLASS()
class JOA_ARENA_API UJoA_W_Figures : public UUserWidget
{
	GENERATED_BODY()

public:
	float GetUpgradeBottom();

protected:
	virtual void NativeConstruct() override;

	UFUNCTION()
	void OnKnightClicked();

	UFUNCTION()
	void OnInfantryClicked();

	UFUNCTION()
	void OnArcherClicked();

	UFUNCTION()
	void OnMedicClicked();

	UFUNCTION()
	void OnAttackClicked();

	UFUNCTION()
	void OnDefenseClicked();

	UFUNCTION()
	void OnReachClicked();

	UFUNCTION()
	void OnHealClicked();

	UFUNCTION()
	void OnPickupFlagClicked();

	UFUNCTION()
	void OnUpgradeClicked();

	void ShowFigure(const FText& Text);
	void ShowAbility(
		const FText& Text,
		bool bFlagImages,
		bool bShowInfo);
	void LockFigureDescription();
	void LockRowWidth();
	void LockAbilityColumn();
	void PinPicture(UWidget* Picture);
	void BindFigureButtons();

	UPROPERTY()
	TObjectPtr<UTextBlock> TB_FigureDescription;

	UPROPERTY()
	TObjectPtr<UTextBlock> TB_AbilitiesDescription;

	UPROPERTY()
	TObjectPtr<UImage> Img_Show01;

	UPROPERTY()
	TObjectPtr<UImage> Img_Show02;

	/* Sits between the flag pictures and the
	 * ability text. Hidden with the pictures. */
	UPROPERTY()
	TObjectPtr<USpacer> AbilitySpacer;

	/* Grows into the empty picture space. */
	UPROPERTY()
	TObjectPtr<USizeBox> AbilityFrame;

	UPROPERTY()
	TObjectPtr<UScrollBox> AbilityScroll;

	UPROPERTY()
	TObjectPtr<UGridPanel> AttackTable;
};
