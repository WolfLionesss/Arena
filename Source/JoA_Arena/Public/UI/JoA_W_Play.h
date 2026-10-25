// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

// --- Engine ---
#include "Blueprint/UserWidget.h"
#include "Engine/TimerHandle.h"

#include "JoA_W_Play.generated.h"

class AJoA_Figure;
class UButton;
enum class EActionKind : uint8;
class UJoA_W_CharacterStat;
class UPanelWidget;
class UTexture2D;
class UTextBlock;
class UVerticalBox;

/**
 * WBP_Play. Stat widgets of both players and the move
 * controls for the selected figure.
 * Figures replicate one by one, so a loop at startup
 * would miss late ones. The widget listens to the
 * figure registry and the turn state instead, and
 * builds or removes one UJoA_W_CharacterStat per
 * living figure. The local player's figures go to
 * HB_LocalFigures, the opponent's to HB_OpponentFigures.
 * A fallen figure leaves the registry and its stat
 * widget is removed.
 */
UCLASS()
class JOA_ARENA_API UJoA_W_Play : public UUserWidget
{
	GENERATED_BODY()

public:
	// Text of TB_Info, for example "Your turn".
	FText GetInfoText() const;

	/* True when the desktop cursor is over a
	 * portrait or a control. A world click must
	 * not run for that press. */
	bool HitsHud(const FVector2D& DesktopPos) const;

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(
		const FGeometry& MyGeometry,
		float InDeltaTime) override;

	// Rebuilds the stat rows and the controls.
	void Rebuild();
	void SyncFigures();
	void RefreshControls();
	void RefreshKeyHints();
	void EnsureKeyHints();
	bool PlaceKeyHints();
	void RefreshActions();
	void PinActionButtons();
	bool PlaceActionButtonsNextToArrows();
	void EnsureJumpFallButtons();
	bool PlaceJumpFallButtons();
	bool PlaceInfoBesidePortraits();
	void KeepLayoutTick();
	void BindJumpFallClicks();
	void RefreshJumpFall();
	void UpdateGoArrow(const AJoA_Figure* Figure);
	void RefreshSelection();
	void RefreshTurnInfo();
	void HideOpeningThrowInfo();

	void HandleFigureClicked(AJoA_Figure* Figure);
	void HandleSelectedFigureChanged(AJoA_Figure* Figure);
	void HandleActionClicked(
		AJoA_Figure* Target,
		EActionKind Kind);

	UFUNCTION()
	void OnGoClicked();

	UFUNCTION()
	void OnTurnLeftClicked();

	UFUNCTION()
	void OnTurnRightClicked();

	UFUNCTION()
	void OnSkipTurnClicked();

	UFUNCTION()
	void OnRestClicked();

	UFUNCTION()
	void OnJumpClicked();

	UFUNCTION()
	void OnFallClicked();

	// Child of HB_LocalFigures and HB_OpponentFigures.
	UPROPERTY(EditAnywhere, Category = "UI")
	TSubclassOf<UJoA_W_CharacterStat> StatWidgetClass;

	// Local player's figures.
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UPanelWidget> HB_LocalFigures;

	// Opponent's figures.
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UPanelWidget> HB_OpponentFigures;

	/* One step button. Its arrow turns to the facing of
	 * the figure, seen from the camera of this player, and
	 * it is enabled when that square is free. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> Btn_Go;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> Btn_TurnLeft;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> Btn_TurnRight;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> Btn_SkipTurn;

	/* Ends the turn before a step, an attack, or
	 * a heal. Every figure of this seat gains
	 * one stamina and one mana. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> Btn_Rest;

	/* Shown only when the selected figure can
	 * climb the obstacle in front. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> Btn_Jump;

	/* Shown only when the selected figure stands
	 * on height 1 or 2 and can step down. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> Btn_Fall;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UPanelWidget> HB_JumpFall;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> TB_Info;

	/* Attack and heal buttons of the selected figure.
	 * Without this panel in the Blueprint, a box is added
	 * to the root canvas next to the arrows. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UPanelWidget> HB_ActionButtons;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UJoA_W_CharacterStat>> StatWidgets;

	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> AttackIcon;

	/* Runs only while TB_Info shows a count of seconds, the
	 * turn clock or the wait for a dropped opponent. */
	FTimerHandle CountdownHandle;
	FTimerHandle OpeningThrowInfoHandle;
	bool bOpeningThrowInfoHidden = false;

	// Colour of TB_Info before a warning turned it red.
	FSlateColor InfoDefaultColor;
	bool bInfoColorSaved = false;

	// Interval of CountdownHandle now, to switch it for pulse.
	float CountdownInterval = 0.f;

	/* Pulse ticks at 25 Hz. SetText only when the
	 * sentence or the shown second changes. */
	FText ShownInfoText;
	FSlateColor ShownInfoColor;
	bool bShownInfoReady = false;
	float ShownInfoScale = -1.f;

	// Own turn below half of the limit.
	FLinearColor InfoCautionColor =
		FLinearColor(1.f, 0.55f, 0.f);

	FLinearColor InfoWarningColor =
		FLinearColor(1.f, 0.05f, 0.05f);
	FLinearColor InfoWonColor =
		FLinearColor(1.f, 0.55f, 0.f);
	FLinearColor InfoYourTurnColor =
		FLinearColor(0.55f, 1.f, 0.55f);
	FLinearColor InfoWhiteColor =
		FLinearColor(1.f, 1.f, 1.f);


	// One entry per shown action button.
	TArray<uint32> ActionSignature;
	bool bActionButtonsPinned = false;
	bool bJumpFallPinned = false;
	bool bInfoBesidePortraits = false;
	bool bKeyHintsPlaced = false;

	/* Bottom of this box is pinned above the
	 * local portraits. More lines grow upward. */
	UPROPERTY(Transient)
	TObjectPtr<UVerticalBox> VB_KeyHints;

	TArray<FName> ShownKeyHints;
	bool bJumpClickedBound = false;
	bool bFallClickedBound = false;

	FDelegateHandle SelectionHandle;
	FDelegateHandle BoardHandle;
};
