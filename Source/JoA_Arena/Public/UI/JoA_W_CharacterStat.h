// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

// --- Engine ---
#include "Blueprint/UserWidget.h"
#include "Engine/TimerHandle.h"

// --- GAS ---
#include "AttributeSet.h"

#include "JoA_W_CharacterStat.generated.h"

class AJoA_Figure;
class UAbilitySystemComponent;
class UImage;
class UProgressBar;
class USpacer;
class UTexture2D;
class UTextBlock;
struct FOnAttributeChangeData;

/* Click on the portrait or anywhere on the widget. */
DECLARE_MULTICAST_DELEGATE_OneParam(
	FOnFigureStatClicked,
	AJoA_Figure*);

/**
 * One number that moves to its new value over time.
 * The first real amount snaps. A 0 read before the
 * set replicates is ignored, so spawn does not
 * count up from empty.
 */
struct FStatTween
{
	float Shown = 0.f;
	float From = 0.f;
	float To = 0.f;
	float Elapsed = 0.f;
	// 0 when a change starts, 1 when it has arrived.
	float Progress = 1.f;
	bool bInitialized = false;
	bool bActive = false;

	void SetTarget(float Value)
	{
		if (!bInitialized)
		{
			if (Value <= 0.f)
			{
				return;
			}

			bInitialized = true;
			Shown = From = To = Value;
			Progress = 1.f;
			bActive = false;
			return;
		}

		From = Shown;
		To = Value;
		Elapsed = 0.f;
		bActive = !FMath::IsNearlyEqual(From, To);
		Progress = bActive ? 0.f : 1.f;
	}

	void Step(float DeltaTime, float Duration)
	{
		if (!bActive)
		{
			return;
		}

		Elapsed += DeltaTime;
		const float Alpha =
			FMath::Clamp(Elapsed / Duration, 0.f, 1.f);
		Shown = FMath::Lerp(From, To, Alpha);
		Progress = Alpha;
		bActive = Alpha < 1.f;
	}
};

/**
 * WBP_CharacterStat. Stats of one figure: health,
 * stamina or mana, attack or heal, defense, and
 * range. Beside the portrait, a flag or a boot
 * shows whether the figure carries a flag, and
 * the stamina one step costs in that state.
 * Values are not pushed by a gameplay effect. The
 * widget listens to the attribute change delegates of
 * the figure ASC, like the Narxim ability system
 * widget. Those delegates fire on the server and, after
 * the replication notify, on every client, so both
 * players see the same numbers.
 * The Medic shows mana and heal. The others show
 * stamina and attack.
 */
UCLASS()
class JOA_ARENA_API UJoA_W_CharacterStat
	: public UUserWidget
{
	GENERATED_BODY()

public:
	UJoA_W_CharacterStat(
		const FObjectInitializer& ObjectInitializer);

	/* Binds to the figure ASC and reads the current
	 * values. A previous figure is released first. */
	void SetFigure(AJoA_Figure* InFigure);

	void ClearFigure();

	AJoA_Figure* GetFigure() const
	{
		return Figure.Get();
	}

	// Highlights the selected figure of this player.
	void SetSelected(bool bInSelected);

	/* The local player picked this figure, or picked
	 * another one. A figure that cannot pay for one
	 * step pulses HB_StaminaMana for three seconds.
	 * Picking it again starts those three seconds
	 * over. A mage has no stamina, so it never pulses. */
	void NotifyPicked(bool bPicked);

	bool IsStaminaRowPulsing() const;

	float GetStaminaRowScale() const;

	/* Only the portraits of the local player's figures
	 * react to a click. */
	void SetClickable(bool bInClickable)
	{
		bClickable = bInClickable;

		/* A user widget that is not hit-testable never
		 * receives the click. */
		SetVisibility(
			bInClickable
				? ESlateVisibility::Visible
				: ESlateVisibility::SelfHitTestInvisible);
	}

	FOnFigureStatClicked OnFigureClicked;

	/* One idle picture per unit and nation.
	 * An empty slot leaves the Blueprint picture. */
	UPROPERTY(EditAnywhere, Category = "Portrait")
	TObjectPtr<UTexture2D> KnightFrench;

	UPROPERTY(EditAnywhere, Category = "Portrait")
	TObjectPtr<UTexture2D> KnightEnglish;

	UPROPERTY(EditAnywhere, Category = "Portrait")
	TObjectPtr<UTexture2D> BarbarianFrench;

	UPROPERTY(EditAnywhere, Category = "Portrait")
	TObjectPtr<UTexture2D> BarbarianEnglish;

	UPROPERTY(EditAnywhere, Category = "Portrait")
	TObjectPtr<UTexture2D> MageFrench;

	UPROPERTY(EditAnywhere, Category = "Portrait")
	TObjectPtr<UTexture2D> MageEnglish;

	UPROPERTY(EditAnywhere, Category = "Portrait")
	TObjectPtr<UTexture2D> RangerFrench;

	UPROPERTY(EditAnywhere, Category = "Portrait")
	TObjectPtr<UTexture2D> RangerEnglish;

	UPROPERTY(EditAnywhere, Category = "Icons")
	TObjectPtr<UTexture2D> RangeIcon;

	UPROPERTY(EditAnywhere, Category = "Icons")
	TObjectPtr<UTexture2D> FlagBlueIcon;

	UPROPERTY(EditAnywhere, Category = "Icons")
	TObjectPtr<UTexture2D> FlagRedIcon;

	UPROPERTY(EditAnywhere, Category = "Icons")
	TObjectPtr<UTexture2D> BootIcon;

	UPROPERTY(EditAnywhere, Category = "Portrait")
	float PortraitSize = 128.f;

protected:
	/* Moving the widget between panels destructs and
	 * constructs it again. The figure stays bound.
	 * Only the pulse timer is cleared, then armed
	 * again if the three seconds are still running. */
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	/* Moves the bars and the resource number toward the
	 * new value. Runs only while a change is animating. */
	virtual void NativeTick(
		const FGeometry& MyGeometry,
		float InDeltaTime) override;

	virtual FReply NativeOnMouseButtonDown(
		const FGeometry& InGeometry,
		const FPointerEvent& InMouseEvent) override;

	void BindAttribute(const FGameplayAttribute& Attribute);
	void UnbindAll();
	void HandleAttributeChanged(
		const FOnAttributeChangeData& Data);

	// One pass over every number and bar.
	void Refresh();

	void ApplyPortrait();

	// Writes the bars and the numbers.
	void ApplyBars();

	/* Mage shows a green plus where the other
	 * figures show the lightning icon. */
	void ApplyResourceIcon();

	// Sword, shield, and the reach beside them.
	void ApplyCombatRow();

	/* Flag or boot beside the portrait, and the
	 * stamina a step costs in that state. A figure
	 * with no stamina shows only the flag or boot. */
	void ApplyCarry();

	void BindBoard();
	void UnbindBoard();
	void HandleBoardChanged();

	bool LacksStaminaToStep() const;
	UWidget* FindStaminaRow() const;
	void StartStaminaPulse();
	void StopStaminaPulse();
	void ArmStaminaPulse();
	void UpdateStaminaPulse();

	/* Current number and "/max" text of one stat. The
	 * current number starts at double size and shrinks to
	 * normal while the change plays. */
	void WriteValue(
		UTextBlock* Current,
		UTextBlock* Max,
		const FStatTween& Tween,
		float MaxValue) const;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> Img_CharacterPortrait;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UProgressBar> PB_Health;

	/* The current health and the maximum
	 * as "/10". The current number counts down with the
	 * bar. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> TB_HealthCurrent;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> TB_HealthMax;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UProgressBar> PB_StaminaMana;

	// Same layout as health: current number, then "/max".
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> TB_StaminaManaCurrent;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> TB_StaminaManaMax;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> TB_AttackHealVal;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> TB_DefVal;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> Img_Range;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> TB_RangeVal;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> Img_Carry;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<USpacer> Spacer_CarryEnergy;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> Img_CarryEnergy;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<USpacer> Spacer_CarryNumber;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> TB_CarryCost;

	TWeakObjectPtr<AJoA_Figure> Figure;
	TWeakObjectPtr<UAbilitySystemComponent> BoundASC;
	TArray<TPair<FGameplayAttribute, FDelegateHandle>>
		Bindings;

	bool bClickable = false;

	FDelegateHandle BoardHandle;

	// A change takes this long to show.
	static constexpr float TweenSeconds = 1.5f;

	FStatTween HealthTween;
	FStatTween ResourceTween;
	float LastMaxHealth = 0.f;
	float LastMaxResource = 0.f;

	/* Same grow-and-shrink as TB_Info at the end of
	 * a turn: 150 % down to the normal size, once
	 * a second. It stops after three seconds. */
	FTimerHandle StaminaPulseHandle;
	double StaminaPulseDeadline = 0.0;
	static constexpr float StaminaPulseSeconds = 3.f;
};
