// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

// --- Engine ---
#include "Blueprint/UserWidget.h"
#include "Engine/TimerHandle.h"
#include "Types/SlateEnums.h"

#include "JoA_W_Menu.generated.h"

class UButton;
class UCheckBox;
class UEditableTextBox;
class UJoA_W_About;
class USlider;
class UTextBlock;
class UWidget;

/**
 * WBP_Menu. Main menu on the Menu map. New Game makes
 * a Steam lobby, hosts a listen server on the Arena map
 * and opens the Steam invite overlay. Without Steam it
 * only hosts. Join Game connects to the host at the
 * address in ET_Address. Quit Game closes the
 * application.
 * Settings hides that column and shows what the match
 * buttons do, plus the flag and obstacle choices.
 * About hides it and shows WBP_About.
 */
UCLASS()
class JOA_ARENA_API UJoA_W_Menu : public UUserWidget
{
	GENERATED_BODY()

public:
	UJoA_W_Menu(const FObjectInitializer& ObjectInitializer);

protected:
	virtual void NativeConstruct() override;
	virtual void NativeTick(
		const FGeometry& MyGeometry,
		float InDeltaTime) override;
	virtual void NativeDestruct() override;

	void ApplyMenuNotice();

	/* Puts the line on the block. A non-empty line
	 * is cleared five seconds later. */
	void ShowTimedLine(
		UTextBlock* Block,
		const FText& Text,
		FTimerHandle& Handle,
		void (UJoA_W_Menu::*Hide)(),
		bool bCollapse);

	void HideInfoLine();
	void HideStatusLine();

	UFUNCTION()
	void OnNewGameClicked();

	UFUNCTION()
	void OnJoinGameClicked();

	UFUNCTION()
	void OnQuitGameClicked();

	UFUNCTION()
	void OnSettingsClicked();

	UFUNCTION()
	void OnAboutClicked();

	UFUNCTION()
	void OnBackClicked();

	UFUNCTION()
	void OnSaveClicked();

	/* Puts the saved values back on the controls. */
	void LoadControlsFromSaved();

	UFUNCTION()
	void OnObstaclePairsChanged(float Value);

	UFUNCTION()
	void OnSeedCommitted(
		const FText& Text,
		ETextCommit::Type CommitMethod);

	UFUNCTION()
	void OnSoundVolumeChanged(float Value);

	UFUNCTION()
	void OnTutorialChanged(bool bIsChecked);

	bool TryReadSeed(
		const FString& Raw,
		int32& OutSeed) const;
	void ShowSeedNotice(bool bInvalid);
	void ApplySeedText(int32 Seed);

	void SetSettingsOpen(bool bOpen);
	void ShowPage(uint8 Page);
	void EnsureAbout();
	void ApplyTutorialText(bool bOn);
	void BuildSoundRow();
	void ApplySoundVolumeText(int32 Volume);
	void ApplyObstacleCountText(int32 Pairs);

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> Btn_NewGame;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> Btn_JoinGame;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> Btn_QuitGame;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> Btn_Settings;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> Btn_About;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> Btn_Back;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> Btn_Save;

	/* The menu column. Not marked as a variable in
	 * the Blueprint, so it is found by name. */
	UPROPERTY(Transient)
	TObjectPtr<UWidget> MenuColumn;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UWidget> VB_Settings;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> TB_Help;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UCheckBox> CB_AutoPickup;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UCheckBox> CB_SetTutorial;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> TB_SetTutorial;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<USlider> Slider_Obstacles;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> TB_ObstacleCount;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UEditableTextBox> ET_Seed;

	UPROPERTY()
	TObjectPtr<USlider> Slider_Sound;

	UPROPERTY()
	TObjectPtr<UTextBlock> TB_SoundVolume;

	UPROPERTY()
	TObjectPtr<UJoA_W_About> AboutPage;

	/* Address of the host, with or without a port. Empty
	 * means this machine. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UEditableTextBox> ET_Address;

	// Join or host progress. Gone after five seconds.
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> TB_Status;

	/* Why the player is back in the menu, or a bad
	 * seed. Shown for five seconds, then hidden. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> TB_Info;

	/* TB_Info is also the leave-match notice.
	 * A bad seed must not be wiped by that path. */
	bool bShowingSeedNotice = false;

	/* The leave-match line already had its five
	 * seconds. Settings must not bring it back. */
	bool bInfoDismissed = false;

	FTimerHandle InfoHideHandle;
	FTimerHandle StatusHideHandle;
};
