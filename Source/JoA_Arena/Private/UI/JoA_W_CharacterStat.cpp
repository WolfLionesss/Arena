// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/JoA_W_CharacterStat.h"

// --- Engine ---
#include "Engine/Texture2D.h"
#include "Input/Events.h"

// --- GAME ---
#include "Board/JoA_Flag.h"
#include "Board/JoA_GridManager.h"
#include "Player/JoA_Nation.h"
#include "Player/Units/JoA_Figure.h"
#include "Player/Units/JoA_FigureHelpers.h"
#include "Player/Units/JoA_FigureRegistry.h"
#include "Player/Units/JoA_UnitTypes.h"

// --- GAS ---
#include "AbilitySystemComponent.h"
#include "GAS/JoA_GAS_CombatSet.h"
#include "GAS/JoA_GAS_HealthSet.h"
#include "GAS/JoA_GAS_ManaSet.h"
#include "GAS/JoA_GAS_StaminaSet.h"

// --- Components ---
#include "Blueprint/WidgetTree.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/PanelWidget.h"
#include "Components/SizeBox.h"
#include "Components/SizeBoxSlot.h"
#include "Components/ProgressBar.h"
#include "Components/Spacer.h"
#include "Components/TextBlock.h"

// --- Helpers ---
#include "UObject/ConstructorHelpers.h"

namespace
{
	FText WholeNumber(float Value)
	{
		return FText::AsNumber(FMath::RoundToInt(Value));
	}

	const FLinearColor BarHealthColor(0.85f, 0.05f, 0.05f);
	const FLinearColor BarStaminaColor(0.1f, 0.35f, 1.f);
	const FLinearColor BarManaColor(0.55f, 1.f, 0.55f);

	float Ratio(float Current, float Maximum)
	{
		return Maximum > 0.f
			? FMath::Clamp(Current / Maximum, 0.f, 1.f)
			: 0.f;
	}

	void UseTexture(UImage* Image, UTexture2D* Texture)
	{
		if (!Image || !Texture)
		{
			return;
		}

		// Keep the 16px size from the widget.
		Image->SetBrushFromTexture(Texture, false);
	}

	void ShowQuiet(UWidget* Widget, bool bShow)
	{
		if (!Widget)
		{
			return;
		}

		Widget->SetVisibility(
			bShow
				? ESlateVisibility::HitTestInvisible
				: ESlateVisibility::Collapsed);
	}

	/* Archer stores 0. A shot uses the rest of
	 * the line, so the card shows that length. */
	int32 ReachSquares(EUnitType Type)
	{
		const FUnitStats Defaults =
			FUnitStats::GetDefaults(Type);
		if (Defaults.ReachPolicy == EReachPolicy::Archer)
		{
			return GridSize - 1;
		}

		return Defaults.Range;
	}
}

UJoA_W_CharacterStat::UJoA_W_CharacterStat(
	const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	static ConstructorHelpers::FObjectFinder<UTexture2D>
		KnightFrenchTex(TEXT(
			"/Game/Materials/Textures/Icons/"
			"T_Portrait_Knight_FR"
			".T_Portrait_Knight_FR"));
	static ConstructorHelpers::FObjectFinder<UTexture2D>
		KnightEnglishTex(TEXT(
			"/Game/Materials/Textures/Icons/"
			"T_Portrait_Knight_EN"
			".T_Portrait_Knight_EN"));
	static ConstructorHelpers::FObjectFinder<UTexture2D>
		BarbarianFrenchTex(TEXT(
			"/Game/Materials/Textures/Icons/"
			"T_Portrait_Barbarian_FR"
			".T_Portrait_Barbarian_FR"));
	static ConstructorHelpers::FObjectFinder<UTexture2D>
		BarbarianEnglishTex(TEXT(
			"/Game/Materials/Textures/Icons/"
			"T_Portrait_Barbarian_EN"
			".T_Portrait_Barbarian_EN"));
	static ConstructorHelpers::FObjectFinder<UTexture2D>
		MageFrenchTex(TEXT(
			"/Game/Materials/Textures/Icons/"
			"T_Portrait_Mage_FR"
			".T_Portrait_Mage_FR"));
	static ConstructorHelpers::FObjectFinder<UTexture2D>
		MageEnglishTex(TEXT(
			"/Game/Materials/Textures/Icons/"
			"T_Portrait_Mage_EN"
			".T_Portrait_Mage_EN"));
	static ConstructorHelpers::FObjectFinder<UTexture2D>
		RangerFrenchTex(TEXT(
			"/Game/Materials/Textures/Icons/"
			"T_Portrait_Ranger_FR"
			".T_Portrait_Ranger_FR"));
	static ConstructorHelpers::FObjectFinder<UTexture2D>
		RangerEnglishTex(TEXT(
			"/Game/Materials/Textures/Icons/"
			"T_Portrait_Ranger_EN"
			".T_Portrait_Ranger_EN"));
	static ConstructorHelpers::FObjectFinder<UTexture2D>
		RangeTex(TEXT(
			"/Game/Materials/Textures/Icons/"
			"T_rangeIcon.T_rangeIcon"));
	static ConstructorHelpers::FObjectFinder<UTexture2D>
		FlagBlueTex(TEXT(
			"/Game/Materials/Textures/Icons/"
			"T_flagBlueIcon.T_flagBlueIcon"));
	static ConstructorHelpers::FObjectFinder<UTexture2D>
		FlagRedTex(TEXT(
			"/Game/Materials/Textures/Icons/"
			"T_flagRedIcon.T_flagRedIcon"));
	static ConstructorHelpers::FObjectFinder<UTexture2D>
		BootTex(TEXT(
			"/Game/Materials/Textures/Icons/"
			"T_bootIcon.T_bootIcon"));

	if (KnightFrenchTex.Succeeded())
	{
		KnightFrench = KnightFrenchTex.Object;
	}
	if (KnightEnglishTex.Succeeded())
	{
		KnightEnglish = KnightEnglishTex.Object;
	}
	if (BarbarianFrenchTex.Succeeded())
	{
		BarbarianFrench = BarbarianFrenchTex.Object;
	}
	if (BarbarianEnglishTex.Succeeded())
	{
		BarbarianEnglish = BarbarianEnglishTex.Object;
	}
	if (MageFrenchTex.Succeeded())
	{
		MageFrench = MageFrenchTex.Object;
	}
	if (MageEnglishTex.Succeeded())
	{
		MageEnglish = MageEnglishTex.Object;
	}
	if (RangerFrenchTex.Succeeded())
	{
		RangerFrench = RangerFrenchTex.Object;
	}
	if (RangerEnglishTex.Succeeded())
	{
		RangerEnglish = RangerEnglishTex.Object;
	}
	if (RangeTex.Succeeded())
	{
		RangeIcon = RangeTex.Object;
	}
	if (FlagBlueTex.Succeeded())
	{
		FlagBlueIcon = FlagBlueTex.Object;
	}
	if (FlagRedTex.Succeeded())
	{
		FlagRedIcon = FlagRedTex.Object;
	}
	if (BootTex.Succeeded())
	{
		BootIcon = BootTex.Object;
	}
}

void UJoA_W_CharacterStat::NativeConstruct()
{
	Super::NativeConstruct();
	ApplyPortrait();
	if (StaminaPulseDeadline > 0.0)
	{
		ArmStaminaPulse();
	}
}

void UJoA_W_CharacterStat::NativeDestruct()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(
			StaminaPulseHandle);
	}

	Super::NativeDestruct();
}

void UJoA_W_CharacterStat::SetFigure(AJoA_Figure* InFigure)
{
	ClearFigure();

	if (!IsValid(InFigure))
	{
		return;
	}

	UAbilitySystemComponent* ASC =
		InFigure->GetAbilitySystemComponent();
	if (!ASC)
	{
		return;
	}

	Figure = InFigure;
	BoundASC = ASC;
	BindBoard();

	/* The delegate is stored by attribute, so it can be
	 * bound before the set reaches this client. The
	 * replication notify then fires it. */
	BindAttribute(UJoA_GAS_HealthSet::GetHealthAttribute());
	BindAttribute(
		UJoA_GAS_HealthSet::GetMaxHealthAttribute());
	BindAttribute(
		UJoA_GAS_StaminaSet::GetStaminaAttribute());
	BindAttribute(
		UJoA_GAS_StaminaSet::GetMaxStaminaAttribute());
	BindAttribute(UJoA_GAS_ManaSet::GetManaAttribute());
	BindAttribute(UJoA_GAS_ManaSet::GetMaxManaAttribute());
	BindAttribute(UJoA_GAS_CombatSet::GetAttackAttribute());
	BindAttribute(UJoA_GAS_CombatSet::GetDefenseAttribute());
	BindAttribute(UJoA_GAS_CombatSet::GetHealAttribute());

	ApplyPortrait();
	Refresh();
}

void UJoA_W_CharacterStat::ClearFigure()
{
	StopStaminaPulse();
	UnbindBoard();
	UnbindAll();
	Figure.Reset();
	BoundASC.Reset();
}

void UJoA_W_CharacterStat::BindAttribute(
	const FGameplayAttribute& Attribute)
{
	UAbilitySystemComponent* ASC = BoundASC.Get();
	if (!ASC)
	{
		return;
	}

	const FDelegateHandle Handle = ASC->
		GetGameplayAttributeValueChangeDelegate(Attribute)
		.AddUObject(
			this,
			&UJoA_W_CharacterStat::HandleAttributeChanged);
	Bindings.Emplace(Attribute, Handle);
}

void UJoA_W_CharacterStat::UnbindAll()
{
	if (UAbilitySystemComponent* ASC = BoundASC.Get())
	{
		for (const TPair<FGameplayAttribute, FDelegateHandle>&
			Binding : Bindings)
		{
			ASC->GetGameplayAttributeValueChangeDelegate(
				Binding.Key).Remove(Binding.Value);
		}
	}

	Bindings.Reset();
}

void UJoA_W_CharacterStat::HandleAttributeChanged(
	const FOnAttributeChangeData& Data)
{
	(void)Data;
	Refresh();
}

void UJoA_W_CharacterStat::Refresh()
{
	const AJoA_Figure* Owner = Figure.Get();
	const UAbilitySystemComponent* ASC = BoundASC.Get();
	if (!Owner || !ASC)
	{
		return;
	}

	const float Health = ASC->GetNumericAttribute(
		UJoA_GAS_HealthSet::GetHealthAttribute());
	const float MaxHealth = ASC->GetNumericAttribute(
		UJoA_GAS_HealthSet::GetMaxHealthAttribute());
	if (MaxHealth > 0.f)
	{
		LastMaxHealth = MaxHealth;
	}
	HealthTween.SetTarget(Health);

	/* Medic has mana and no stamina. The other figures
	 * have stamina and no mana. */
	const bool bUsesStamina = Owner->UsesStamina();
	const float Resource = ASC->GetNumericAttribute(
		bUsesStamina
			? UJoA_GAS_StaminaSet::GetStaminaAttribute()
			: UJoA_GAS_ManaSet::GetManaAttribute());
	const float MaxResource = ASC->GetNumericAttribute(
		bUsesStamina
			? UJoA_GAS_StaminaSet::GetMaxStaminaAttribute()
			: UJoA_GAS_ManaSet::GetMaxManaAttribute());
	if (MaxResource > 0.f)
	{
		LastMaxResource = MaxResource;
	}
	ResourceTween.SetTarget(Resource);

	/* Bars stay empty until both amounts exist, so
	 * the HUD does not flash 0 at spawn. */
	if (HealthTween.bInitialized
		&& ResourceTween.bInitialized)
	{
		ApplyBars();
	}

	ApplyCombatRow();
	ApplyCarry();
	ApplyResourceIcon();

	if (HealthTween.bActive || ResourceTween.bActive)
	{
		if (!bHasScriptImplementedTick)
		{
			bHasScriptImplementedTick = true;
			UpdateCanTick();
		}
	}
}

void UJoA_W_CharacterStat::ApplyCombatRow()
{
	const AJoA_Figure* Owner = Figure.Get();
	const UAbilitySystemComponent* ASC = BoundASC.Get();
	if (!Owner || !ASC)
	{
		return;
	}

	// The sword slot reads Heal for a healer.
	const bool bHeals =
		FUnitStats::GetDefaults(Owner->UnitType).bCanHeal;
	if (TB_AttackHealVal)
	{
		TB_AttackHealVal->SetText(WholeNumber(
			ASC->GetNumericAttribute(
				bHeals
					? UJoA_GAS_CombatSet::GetHealAttribute()
					: UJoA_GAS_CombatSet::GetAttackAttribute())));
	}
	if (TB_DefVal)
	{
		TB_DefVal->SetText(WholeNumber(
			ASC->GetNumericAttribute(
				UJoA_GAS_CombatSet::GetDefenseAttribute())));
	}

	UseTexture(Img_Range, RangeIcon.Get());
	if (TB_RangeVal)
	{
		TB_RangeVal->SetText(WholeNumber(
			static_cast<float>(
				ReachSquares(Owner->UnitType))));
	}
}

void UJoA_W_CharacterStat::ApplyCarry()
{
	const AJoA_Figure* Owner = Figure.Get();
	if (!Owner)
	{
		return;
	}

	/* Icons are a quarter of the portrait, the
	 * same share on width and on height. */
	const float IconSide = PortraitSize * 0.25f;
	const AJoA_Flag* Held = Owner->GetHeldFlag();
	UTexture2D* CarryIcon = BootIcon.Get();
	if (Held)
	{
		/* FrenchSeat paints the banner blue.
		 * EnglishSeat paints it red. */
		CarryIcon = Held->GetOwningPlayerIndex()
				== FrenchSeat
			? FlagBlueIcon.Get()
			: FlagRedIcon.Get();
	}
	UseTexture(Img_Carry, CarryIcon);
	if (Img_Carry)
	{
		Img_Carry->SetDesiredSizeOverride(
			FVector2D(IconSide, IconSide));
	}
	if (Img_CarryEnergy)
	{
		Img_CarryEnergy->SetDesiredSizeOverride(
			FVector2D(IconSide, IconSide));
	}

	/* Attack text is 12 next to a 16px sword.
	 * Keep that ratio beside the quarter icons. */
	if (TB_CarryCost)
	{
		FSlateFontInfo Font = TB_CarryCost->GetFont();
		Font.Size = FMath::RoundToInt(IconSide * 0.75f);
		TB_CarryCost->SetFont(Font);
	}

	/* A step costs stamina. Carrying the flag
	 * adds the penalty. Mana figures pay neither,
	 * so only the flag or the boot stays. */
	const bool bSpends = Owner->UsesStamina();
	ShowQuiet(Spacer_CarryEnergy, bSpends);
	ShowQuiet(Img_CarryEnergy, bSpends);
	ShowQuiet(Spacer_CarryNumber, bSpends);
	ShowQuiet(TB_CarryCost, bSpends);
	if (!bSpends || !TB_CarryCost)
	{
		return;
	}

	const int32 Cost = FMath::RoundToInt(
		JoAFigure::StepStaminaCost
		+ Owner->GetHeldFlagSpendPenalty());
	TB_CarryCost->SetText(FText::FromString(
		FString::Printf(TEXT("-%d"), Cost)));
}

void UJoA_W_CharacterStat::BindBoard()
{
	if (BoardHandle.IsValid())
	{
		return;
	}

	UJoA_FigureRegistry* Registry =
		UJoA_FigureRegistry::Get(this);
	if (!Registry)
	{
		return;
	}

	BoardHandle =
		Registry->OnBoardStateChanged.AddUObject(
			this,
			&UJoA_W_CharacterStat::HandleBoardChanged);
}

void UJoA_W_CharacterStat::UnbindBoard()
{
	if (!BoardHandle.IsValid())
	{
		return;
	}

	if (UJoA_FigureRegistry* Registry =
		UJoA_FigureRegistry::Get(this))
	{
		Registry->OnBoardStateChanged.Remove(BoardHandle);
	}

	BoardHandle.Reset();
}

void UJoA_W_CharacterStat::HandleBoardChanged()
{
	ApplyCarry();
}

void UJoA_W_CharacterStat::NativeTick(
	const FGeometry& MyGeometry,
	float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (!HealthTween.bActive && !ResourceTween.bActive)
	{
		bHasScriptImplementedTick = false;
		UpdateCanTick();
		return;
	}

	HealthTween.Step(InDeltaTime, TweenSeconds);
	ResourceTween.Step(InDeltaTime, TweenSeconds);
	ApplyBars();
	ApplyResourceIcon();

	if (!HealthTween.bActive && !ResourceTween.bActive)
	{
		bHasScriptImplementedTick = false;
		UpdateCanTick();
	}
}

void UJoA_W_CharacterStat::ApplyBars()
{
	const AJoA_Figure* Owner = Figure.Get();
	if (!Owner)
	{
		return;
	}

	/* While a change animates, the bar is translucent and
	 * shortens to the new value. */
	const float FadedAlpha = 0.45f;
	FLinearColor HealthColor = BarHealthColor;
	if (HealthTween.bActive)
	{
		HealthColor.A = FadedAlpha;
	}
	if (PB_Health)
	{
		PB_Health->SetFillColorAndOpacity(HealthColor);
		PB_Health->SetPercent(
			Ratio(HealthTween.Shown, LastMaxHealth));
	}

	FLinearColor ResourceColor = Owner->UsesStamina()
		? BarStaminaColor
		: BarManaColor;
	if (ResourceTween.bActive)
	{
		ResourceColor.A = FadedAlpha;
	}
	if (PB_StaminaMana)
	{
		PB_StaminaMana->SetFillColorAndOpacity(ResourceColor);
		PB_StaminaMana->SetPercent(
			Ratio(ResourceTween.Shown, LastMaxResource));
	}

	WriteValue(
		TB_HealthCurrent,
		TB_HealthMax,
		HealthTween,
		LastMaxHealth);
	WriteValue(
		TB_StaminaManaCurrent,
		TB_StaminaManaMax,
		ResourceTween,
		LastMaxResource);
}

void UJoA_W_CharacterStat::ApplyResourceIcon()
{
	if (!WidgetTree)
	{
		return;
	}

	UImage* Icon = Cast<UImage>(
		WidgetTree->FindWidget(
			TEXT("Img_StaminaMana")));
	if (!Icon)
	{
		return;
	}

	const AJoA_Figure* Owner = Figure.Get();
	const bool bMage =
		Owner && Owner->UnitType == EUnitType::Medic;

	UTextBlock* Plus = Cast<UTextBlock>(
		WidgetTree->FindWidget(TEXT("TB_ManaPlus")));
	USizeBox* Holder = Cast<USizeBox>(
		WidgetTree->FindWidget(TEXT("SB_ManaPlus")));
	if (bMage && !Plus)
	{
		Plus = WidgetTree->ConstructWidget<UTextBlock>(
			UTextBlock::StaticClass(),
			TEXT("TB_ManaPlus"));
		Plus->SetText(FText::FromString(TEXT("+")));
		/* Same green as the heal number. */
		Plus->SetColorAndOpacity(
			FSlateColor(BarManaColor));
		Plus->SetJustification(ETextJustify::Center);
		FSlateFontInfo Font = Plus->GetFont();
		Font.Size = 16;
		Plus->SetFont(Font);
		Plus->SetRenderTransformPivot(
			FVector2D(0.5, 0.5));
		Plus->SetRenderScale(FVector2D(1.55, 1.55));
		/* The glyph sits low in the line box.
		 * This puts its bar on the mana bar. */
		Plus->SetRenderTranslation(
			FVector2D(0.f, -4.f));
		Plus->SetVisibility(
			ESlateVisibility::HitTestInvisible);
	}

	if (bMage && Plus && !Holder)
	{
		UPanelWidget* Row = Icon->GetParent();
		if (!Row)
		{
			return;
		}

		/* The plus glyph is taller than the heart.
		 * The box keeps the mana row at the heart
		 * height, so the bar matches health. */
		Holder = WidgetTree->ConstructWidget<USizeBox>(
			USizeBox::StaticClass(),
			TEXT("SB_ManaPlus"));
		Holder->SetVisibility(
			ESlateVisibility::HitTestInvisible);
		if (Plus->GetParent())
		{
			Plus->RemoveFromParent();
		}
		Holder->AddChild(Plus);
		if (USizeBoxSlot* PlusSlot =
			Cast<USizeBoxSlot>(Plus->Slot))
		{
			PlusSlot->SetHorizontalAlignment(
				HAlign_Center);
			PlusSlot->SetVerticalAlignment(
				VAlign_Center);
		}

		const int32 Index = Row->GetChildIndex(Icon);
		Row->InsertChildAt(Index, Holder);
	}

	if (Holder)
	{
		/* Same slot as the heart, so the mana bar
		 * starts where the health bar starts and
		 * the plus sits on the bar. */
		UImage* Heart = Cast<UImage>(
			WidgetTree->FindWidget(
				TEXT("Img_HeartIcon")));
		float IconW = 16.f;
		float IconH = 16.f;
		if (Heart)
		{
			FVector2D HeartSize =
				Heart->GetCachedGeometry()
					.GetLocalSize();
			if (HeartSize.X < 1.f
				|| HeartSize.Y < 1.f)
			{
				HeartSize = Heart->GetDesiredSize();
			}
			if (HeartSize.X > 1.f
				&& HeartSize.Y > 1.f)
			{
				IconW = HeartSize.X;
				IconH = HeartSize.Y;
			}

			if (UHorizontalBoxSlot* HeartSlot =
				Cast<UHorizontalBoxSlot>(
					Heart->Slot))
			{
				if (UHorizontalBoxSlot* HolderSlot =
					Cast<UHorizontalBoxSlot>(
						Holder->Slot))
				{
					HolderSlot->SetPadding(
						HeartSlot->GetPadding());
					HolderSlot->SetVerticalAlignment(
						HeartSlot
							->GetVerticalAlignment());
					HolderSlot->SetHorizontalAlignment(
						HeartSlot
							->GetHorizontalAlignment());
				}
			}
		}

		Holder->SetWidthOverride(IconW);
		Holder->SetHeightOverride(IconH);

		if (PB_Health && PB_StaminaMana)
		{
			UHorizontalBoxSlot* HealthSlot =
				Cast<UHorizontalBoxSlot>(
					PB_Health->Slot);
			UHorizontalBoxSlot* ManaSlot =
				Cast<UHorizontalBoxSlot>(
					PB_StaminaMana->Slot);
			if (HealthSlot && ManaSlot)
			{
				ManaSlot->SetPadding(
					HealthSlot->GetPadding());
				ManaSlot->SetVerticalAlignment(
					HealthSlot->GetVerticalAlignment());
				ManaSlot->SetSize(HealthSlot->GetSize());
			}
		}

		if (Plus)
		{
			Plus->SetRenderTranslation(
				FVector2D(0.f, -4.f));
			if (USizeBoxSlot* PlusSlot =
				Cast<USizeBoxSlot>(Plus->Slot))
			{
				PlusSlot->SetHorizontalAlignment(
					HAlign_Center);
				PlusSlot->SetVerticalAlignment(
					VAlign_Center);
			}
		}
	}

	ShowQuiet(Icon, !bMage);
	ShowQuiet(Holder, bMage);
	ShowQuiet(Plus, bMage);
}

void UJoA_W_CharacterStat::WriteValue(
	UTextBlock* Current,
	UTextBlock* Max,
	const FStatTween& Tween,
	float MaxValue) const
{
	if (Current)
	{
		Current->SetText(WholeNumber(Tween.Shown));

		// The right edge stays next to the "/max" text.
		Current->SetRenderTransformPivot(
			FVector2D(1.0, 0.5));
		const float Scale =
			FMath::Lerp(2.f, 1.f, Tween.Progress);
		Current->SetRenderScale(FVector2D(Scale, Scale));
	}
	if (Max)
	{
		Max->SetText(FText::FromString(FString::Printf(
			TEXT("/%d"),
			FMath::RoundToInt(MaxValue))));
	}
}
void UJoA_W_CharacterStat::ApplyPortrait()
{
	if (!Img_CharacterPortrait)
	{
		return;
	}

	const AJoA_Figure* Owner = Figure.Get();
	if (Owner)
	{
		/* Knight is HeavyKnight, barbarian is
		 * LightInfantry, mage is Medic and ranger
		 * is Archer. French is seat 0. */
		const bool bFrench =
			Owner->OwningPlayerIndex == FrenchSeat;
		UTexture2D* Portrait = nullptr;
		switch (Owner->UnitType)
		{
		case EUnitType::HeavyKnight:
			Portrait = bFrench
				? KnightFrench.Get()
				: KnightEnglish.Get();
			break;
		case EUnitType::LightInfantry:
			Portrait = bFrench
				? BarbarianFrench.Get()
				: BarbarianEnglish.Get();
			break;
		case EUnitType::Medic:
			Portrait = bFrench
				? MageFrench.Get()
				: MageEnglish.Get();
			break;
		case EUnitType::Archer:
			Portrait = bFrench
				? RangerFrench.Get()
				: RangerEnglish.Get();
			break;
		default:
			break;
		}
		if (Portrait)
		{
			Img_CharacterPortrait->SetBrushFromTexture(
				Portrait);
		}
	}

	/* The imported portrait is far larger than the
	 * row. It is shown at a fixed square size. */
	Img_CharacterPortrait->SetDesiredSizeOverride(
		FVector2D(PortraitSize, PortraitSize));
}

void UJoA_W_CharacterStat::SetSelected(bool bInSelected)
{
	// The selected figure is full colour, the rest dim.
	const FLinearColor Selected(1.f, 1.f, 1.f, 1.f);
	const FLinearColor Dimmed(0.6f, 0.6f, 0.6f, 1.f);
	SetColorAndOpacity(bInSelected ? Selected : Dimmed);
}

void UJoA_W_CharacterStat::NotifyPicked(bool bPicked)
{
	if (bPicked && LacksStaminaToStep())
	{
		StartStaminaPulse();
		return;
	}

	StopStaminaPulse();
}

bool UJoA_W_CharacterStat::IsStaminaRowPulsing() const
{
	const UWorld* World = GetWorld();
	return World
		&& StaminaPulseDeadline > World->GetTimeSeconds();
}

float UJoA_W_CharacterStat::GetStaminaRowScale() const
{
	const UWidget* Row = FindStaminaRow();
	return Row
		? Row->GetRenderTransform().Scale.X
		: 1.f;
}

bool UJoA_W_CharacterStat::LacksStaminaToStep() const
{
	const AJoA_Figure* Owner = Figure.Get();
	if (!Owner || !Owner->UsesStamina())
	{
		return false;
	}

	const UJoA_GAS_StaminaSet* StaminaSet =
		Owner->GetStaminaSet();
	if (!StaminaSet)
	{
		return false;
	}

	const float Cost = JoAFigure::StepStaminaCost
		+ Owner->GetHeldFlagSpendPenalty();
	return StaminaSet->GetStamina() < Cost;
}

UWidget* UJoA_W_CharacterStat::FindStaminaRow() const
{
	return WidgetTree
		? WidgetTree->FindWidget(TEXT("HB_StaminaMana"))
		: nullptr;
}

void UJoA_W_CharacterStat::StartStaminaPulse()
{
	UWorld* World = GetWorld();
	UWidget* Row = FindStaminaRow();
	if (!World || !Row)
	{
		return;
	}

	/* Grows from the middle, same as TB_Info. */
	Row->SetRenderTransformPivot(FVector2D(0.5, 0.5));
	StaminaPulseDeadline = World->GetTimeSeconds()
		+ StaminaPulseSeconds;
	ArmStaminaPulse();
	UpdateStaminaPulse();
}

void UJoA_W_CharacterStat::StopStaminaPulse()
{
	StaminaPulseDeadline = 0.0;
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(
			StaminaPulseHandle);
	}

	if (UWidget* Row = FindStaminaRow())
	{
		Row->SetRenderScale(FVector2D(1.0, 1.0));
	}
}

void UJoA_W_CharacterStat::ArmStaminaPulse()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	if (StaminaPulseDeadline <= World->GetTimeSeconds())
	{
		StopStaminaPulse();
		return;
	}

	/* 25 Hz, the same step TB_Info uses while it
	 * pulses, so the shrink looks smooth. */
	World->GetTimerManager().SetTimer(
		StaminaPulseHandle,
		this,
		&UJoA_W_CharacterStat::UpdateStaminaPulse,
		0.04f,
		true);
}

void UJoA_W_CharacterStat::UpdateStaminaPulse()
{
	UWorld* World = GetWorld();
	UWidget* Row = FindStaminaRow();
	if (!World || !Row)
	{
		StopStaminaPulse();
		return;
	}

	const double Left = StaminaPulseDeadline
		- World->GetTimeSeconds();
	if (Left <= 0.0)
	{
		StopStaminaPulse();
		return;
	}

	/* Jumps to 150 % each second, then shrinks
	 * back to the normal size. Frac(Left) is the
	 * same term TB_Info uses at the end of a turn. */
	const float Scale =
		1.f + 0.5f * FMath::Frac(Left);
	Row->SetRenderScale(FVector2D(Scale, Scale));
}

FReply UJoA_W_CharacterStat::NativeOnMouseButtonDown(
	const FGeometry& InGeometry,
	const FPointerEvent& InMouseEvent)
{
	if (bClickable
		&& InMouseEvent.GetEffectingButton()
			== EKeys::LeftMouseButton)
	{
		if (AJoA_Figure* Clicked = Figure.Get())
		{
			OnFigureClicked.Broadcast(Clicked);
			return FReply::Handled();
		}
	}

	if (bClickable
		&& InMouseEvent.GetEffectingButton()
			== EKeys::RightMouseButton)
	{
		if (AJoA_Figure* Clicked = Figure.Get())
		{
			if (Clicked->GetHeldFlag())
			{
				Clicked->RequestDropFlag();
				return FReply::Handled();
			}
		}
	}

	return Super::NativeOnMouseButtonDown(
		InGeometry,
		InMouseEvent);
}
