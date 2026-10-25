// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/JoA_W_Figures.h"

// --- Engine ---
#include "Blueprint/WidgetTree.h"

// --- GAME ---
#include "UI/JoA_UIText.h"
#include "UI/JoA_WidgetHelpers.h"

// --- Components ---
#include "Components/Button.h"
#include "Components/GridPanel.h"
#include "Components/GridSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/PanelWidget.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Components/Spacer.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"

namespace
{
	void HideImage(UImage* Image)
	{
		if (Image)
		{
			Image->SetVisibility(
				ESlateVisibility::Collapsed);
		}
	}

	bool Holds(const UWidget* Panel, const UWidget* Target)
	{
		for (const UWidget* Walk = Target; Walk;
			Walk = Walk->GetParent())
		{
			if (Walk == Panel)
			{
				return true;
			}
		}
		return false;
	}

	float PadTop(const UWidget* Child)
	{
		if (const UVerticalBoxSlot* VSlot =
			Cast<UVerticalBoxSlot>(Child->Slot))
		{
			return VSlot->GetPadding().Top;
		}
		if (const UHorizontalBoxSlot* HSlot =
			Cast<UHorizontalBoxSlot>(Child->Slot))
		{
			return HSlot->GetPadding().Top;
		}
		return 0.f;
	}

	float PadHeight(const UWidget* Child)
	{
		if (const UVerticalBoxSlot* VSlot =
			Cast<UVerticalBoxSlot>(Child->Slot))
		{
			const FMargin Pad = VSlot->GetPadding();
			return Pad.Top + Pad.Bottom;
		}
		if (const UHorizontalBoxSlot* HSlot =
			Cast<UHorizontalBoxSlot>(Child->Slot))
		{
			const FMargin Pad = HSlot->GetPadding();
			return Pad.Top + Pad.Bottom;
		}
		return 0.f;
	}

	float BottomOf(UWidget* Panel, const UWidget* Target)
	{
		if (!Panel || Panel == Target)
		{
			if (Panel)
			{
				Panel->ForceLayoutPrepass();
				return Panel->GetDesiredSize().Y;
			}
			return 0.f;
		}

		UPanelWidget* Children = Cast<UPanelWidget>(Panel);
		if (!Children)
		{
			Panel->ForceLayoutPrepass();
			return Panel->GetDesiredSize().Y;
		}

		const bool bSide =
			Cast<UHorizontalBox>(Panel) != nullptr;
		float Y = 0.f;
		for (int32 Index = 0;
			Index < Children->GetChildrenCount();
			++Index)
		{
			UWidget* Child = Children->GetChildAt(Index);
			if (!Child)
			{
				continue;
			}
			if (Child == Target || Holds(Child, Target))
			{
				return Y + PadTop(Child)
					+ BottomOf(Child, Target);
			}
			if (bSide)
			{
				continue;
			}
			Child->ForceLayoutPrepass();
			Y += PadHeight(Child)
				+ Child->GetDesiredSize().Y;
		}
		return Y;
	}

	/* Text beside the flag pictures. */
	const float FlagTextWidth = 480.f;
	const float AbilityScrollHeight = 300.f;

	/* Pictures plus that text. The row stays
	 * this wide when the pictures hide. */
	float AbilityTextWidth = 960.f;

	/* Full pickup row, out to the orange line. */
	float FigureTextWidth = 1200.f;
}

void UJoA_W_Figures::NativeConstruct()
{
	Super::NativeConstruct();

	TB_FigureDescription = Cast<UTextBlock>(
		GetWidgetFromName(
			TEXT("TB_FigureDescription")));
	TB_AbilitiesDescription = Cast<UTextBlock>(
		GetWidgetFromName(
			TEXT("TB_AbilitiesDescription")));
	Img_Show01 = Cast<UImage>(
		GetWidgetFromName(TEXT("Img_Show01")));
	Img_Show02 = Cast<UImage>(
		GetWidgetFromName(TEXT("Img_Show02")));
	AbilitySpacer = Cast<USpacer>(
		GetWidgetFromName(TEXT("Spacer_126")));

	LockRowWidth();
	LockFigureDescription();
	LockAbilityColumn();
	PinPicture(Img_Show01);
	PinPicture(Img_Show02);

	/* Designer left both pictures visible.
	 * Only Pick up flag may show them. */
	HideImage(Img_Show01);
	HideImage(Img_Show02);
	if (AbilitySpacer)
	{
		AbilitySpacer->SetVisibility(
			ESlateVisibility::Collapsed);
	}

	BindFigureButtons();
	if (UImage* Info = Cast<UImage>(
		GetWidgetFromName(TEXT("Img_Info"))))
	{
		Info->SetVisibility(ESlateVisibility::Collapsed);
	}
	if (TB_AbilitiesDescription)
	{
		TB_AbilitiesDescription->SetAutoWrapText(true);
	}

	OnPickupFlagClicked();
}

void UJoA_W_Figures::BindFigureButtons()
{
	auto Find = [this](const TCHAR* Name) -> UButton*
	{
		UButton* Button = Cast<UButton>(
			GetWidgetFromName(Name));
		if (Button)
		{
			JoA_DisableButtonFocus(Button);
		}
		return Button;
	};

	if (UButton* Button = Find(TEXT("Btn_Knight")))
	{
		Button->OnClicked.AddUniqueDynamic(
			this,
			&UJoA_W_Figures::OnKnightClicked);
	}
	if (UButton* Button = Find(TEXT("Btn_Infantry")))
	{
		Button->OnClicked.AddUniqueDynamic(
			this,
			&UJoA_W_Figures::OnInfantryClicked);
	}
	if (UButton* Button = Find(TEXT("Btn_Archer")))
	{
		Button->OnClicked.AddUniqueDynamic(
			this,
			&UJoA_W_Figures::OnArcherClicked);
	}
	if (UButton* Button = Find(TEXT("Btn_Medic")))
	{
		Button->OnClicked.AddUniqueDynamic(
			this,
			&UJoA_W_Figures::OnMedicClicked);
	}
	if (UButton* Button = Find(TEXT("Btn_Attack")))
	{
		Button->OnClicked.AddUniqueDynamic(
			this,
			&UJoA_W_Figures::OnAttackClicked);
	}
	if (UButton* Button = Find(TEXT("Btn_Defense")))
	{
		Button->OnClicked.AddUniqueDynamic(
			this,
			&UJoA_W_Figures::OnDefenseClicked);
	}
	if (UButton* Button = Find(TEXT("Btn_Reach")))
	{
		Button->OnClicked.AddUniqueDynamic(
			this,
			&UJoA_W_Figures::OnReachClicked);
	}
	if (UButton* Button = Find(TEXT("Btn_Heal")))
	{
		Button->OnClicked.AddUniqueDynamic(
			this,
			&UJoA_W_Figures::OnHealClicked);
	}
	if (UButton* Button =
		Find(TEXT("Btn_PickUpFlag")))
	{
		Button->OnClicked.AddUniqueDynamic(
			this,
			&UJoA_W_Figures::OnPickupFlagClicked);
	}
	if (UButton* Button = Find(TEXT("Btn_Upgrade")))
	{
		Button->OnClicked.AddUniqueDynamic(
			this,
			&UJoA_W_Figures::OnUpgradeClicked);
	}
}

void UJoA_W_Figures::ShowFigure(const FText& Text)
{
	if (TB_FigureDescription)
	{
		TB_FigureDescription->SetText(Text);
	}
}

void UJoA_W_Figures::ShowAbility(
	const FText& Text,
	bool bFlagImages,
	bool bShowInfo)
{
	/* Pictures use their own width. Without
	 * them the copy keeps the same right edge. */
	const float TextWidth = bFlagImages
		? FlagTextWidth
		: AbilityTextWidth;
	if (AbilityFrame)
	{
		AbilityFrame->SetWidthOverride(TextWidth);
		AbilityFrame->InvalidateLayoutAndVolatility();
	}
	if (TB_AbilitiesDescription)
	{
		TB_AbilitiesDescription->SetWrapTextAt(
			TextWidth - 16.f);
		TB_AbilitiesDescription->SetText(Text);
	}

	const ESlateVisibility Pictures = bFlagImages
		? ESlateVisibility::SelfHitTestInvisible
		: ESlateVisibility::Collapsed;
	if (Img_Show01)
	{
		Img_Show01->SetVisibility(Pictures);
	}
	if (Img_Show02)
	{
		Img_Show02->SetVisibility(Pictures);
	}
	if (AbilitySpacer)
	{
		AbilitySpacer->SetVisibility(Pictures);
	}
	if (AbilityScroll)
	{
		AbilityScroll->ScrollToStart();
	}
	if (UImage* Info = Cast<UImage>(
		GetWidgetFromName(TEXT("Img_Info"))))
	{
		Info->SetVisibility(
			bShowInfo
				? ESlateVisibility::SelfHitTestInvisible
				: ESlateVisibility::Collapsed);
	}
}

void UJoA_W_Figures::LockFigureDescription()
{
	if (!WidgetTree || !TB_FigureDescription)
	{
		return;
	}

	UVerticalBox* Root = Cast<UVerticalBox>(
		TB_FigureDescription->GetParent());
	if (!Root)
	{
		return;
	}

	/* InsertChildAt only reorders the list.
	 * The live box appends, so this line sat
	 * under the buttons. The rows are added
	 * again with the description first. */
	struct FKeptChild
	{
		UWidget* Widget = nullptr;
		FMargin Padding;
		EHorizontalAlignment HAlign = HAlign_Fill;
		EVerticalAlignment VAlign = VAlign_Fill;
		FSlateChildSize Size;
	};

	TArray<FKeptChild> Kept;
	const int32 Count = Root->GetChildrenCount();
	for (int32 Index = 0; Index < Count; ++Index)
	{
		FKeptChild Item;
		Item.Widget = Root->GetChildAt(Index);
		if (!Item.Widget)
		{
			continue;
		}

		if (UVerticalBoxSlot* ChildSlot =
			Cast<UVerticalBoxSlot>(Item.Widget->Slot))
		{
			Item.Padding = ChildSlot->GetPadding();
			Item.HAlign =
				ChildSlot->GetHorizontalAlignment();
			Item.VAlign =
				ChildSlot->GetVerticalAlignment();
			Item.Size = ChildSlot->GetSize();
		}
		Kept.Add(Item);
	}

	for (const FKeptChild& Item : Kept)
	{
		Root->RemoveChild(Item.Widget);
	}

	USizeBox* Box =
		WidgetTree->ConstructWidget<USizeBox>(
			USizeBox::StaticClass(),
			TEXT("SZ_FigureDescription"));
	Box->SetWidthOverride(FigureTextWidth);
	Box->AddChild(TB_FigureDescription);

	auto Place = [Root](
		UWidget* Child,
		const FKeptChild& Item)
	{
		UVerticalBoxSlot* TextSlot =
			Root->AddChildToVerticalBox(Child);
		if (!TextSlot)
		{
			return;
		}

		TextSlot->SetPadding(Item.Padding);
		TextSlot->SetHorizontalAlignment(Item.HAlign);
		TextSlot->SetVerticalAlignment(Item.VAlign);
		TextSlot->SetSize(Item.Size);
	};

	for (const FKeptChild& Item : Kept)
	{
		if (Item.Widget == TB_FigureDescription)
		{
			Place(Box, Item);
			break;
		}
	}
	for (const FKeptChild& Item : Kept)
	{
		if (Item.Widget != TB_FigureDescription)
		{
			Place(Item.Widget, Item);
		}
	}

	/* Portraits stay put. The block is as tall
	 * as the Archer paragraph, then Spacer_157. */
	TB_FigureDescription->SetAutoWrapText(true);
	TB_FigureDescription->SetWrapTextAt(
		FigureTextWidth - 8.f);
	TB_FigureDescription->SetText(
		JoA_UIText(TEXT("Figures.Archer")));
	TB_FigureDescription->ForceLayoutPrepass();
	float Height =
		TB_FigureDescription->GetDesiredSize().Y;
	if (Height < 1.f)
	{
		Height = 96.f;
	}
	Box->SetHeightOverride(Height);
	TB_FigureDescription->SetText(
		JoA_UIText(TEXT("Figures.Knight")));
}

void UJoA_W_Figures::LockRowWidth()
{
	float Pictures = 0.f;
	if (Img_Show01)
	{
		Pictures +=
			Img_Show01->GetBrush().ImageSize.X;
	}
	if (Img_Show02)
	{
		Pictures +=
			Img_Show02->GetBrush().ImageSize.X;
	}
	if (AbilitySpacer)
	{
		Pictures += AbilitySpacer->GetSize().X;
	}
	AbilityTextWidth = Pictures + FlagTextWidth;

	float Buttons = 0.f;
	if (UWidget* Column = GetWidgetFromName(
		TEXT("VerticalBox_120")))
	{
		Column->ForceLayoutPrepass();
		Buttons = Column->GetDesiredSize().X;
	}
	float Mid = 0.f;
	if (USpacer* Gap = Cast<USpacer>(
		GetWidgetFromName(TEXT("Spacer_564"))))
	{
		Mid = Gap->GetSize().X;
	}

	FigureTextWidth = Buttons + Mid + AbilityTextWidth;
	if (FigureTextWidth < FlagTextWidth * 2.f)
	{
		FigureTextWidth = FlagTextWidth * 2.f;
	}
}

void UJoA_W_Figures::PinPicture(UWidget* Picture)
{
	UHorizontalBoxSlot* PictureSlot =
		Picture
			? Cast<UHorizontalBoxSlot>(Picture->Slot)
			: nullptr;
	if (!PictureSlot)
	{
		return;
	}

	/* The text beside them is taller. Fill
	 * would stretch the brush to that height. */
	PictureSlot->SetHorizontalAlignment(HAlign_Left);
	PictureSlot->SetVerticalAlignment(VAlign_Top);
	FSlateChildSize Auto;
	Auto.SizeRule = ESlateSizeRule::Automatic;
	PictureSlot->SetSize(Auto);
}

void UJoA_W_Figures::LockAbilityColumn()
{
	if (!WidgetTree || !TB_AbilitiesDescription
		|| AbilityFrame)
	{
		return;
	}

	UWidget* Column = TB_AbilitiesDescription->GetParent();
	UHorizontalBox* Row = Column
		? Cast<UHorizontalBox>(Column->GetParent())
		: nullptr;
	if (!Row)
	{
		return;
	}

	Column->RemoveFromParent();
	AbilityFrame =
		WidgetTree->ConstructWidget<USizeBox>(
			USizeBox::StaticClass(),
			TEXT("SZ_AbilityColumn"));
	AbilityFrame->SetWidthOverride(AbilityTextWidth);

	/* One vertical scroll for text and table. The
	 * fixed height keeps the row from changing
	 * between abilities. Same height as picture. */
	float Height = Img_Show01
		? Img_Show01->GetBrush().ImageSize.Y
		: 0.f;
	if (Height < 1.f)
	{
		Height = AbilityScrollHeight;
	}
	AbilityFrame->SetHeightOverride(Height);
	AbilityScroll =
		WidgetTree->ConstructWidget<UScrollBox>(
			UScrollBox::StaticClass(),
			TEXT("SB_AbilityText"));
	AbilityScroll->SetOrientation(Orient_Vertical);
	AbilityScroll->AddChild(Column);
	AbilityFrame->AddChild(AbilityScroll);
	if (UHorizontalBoxSlot* FrameSlot =
		Row->AddChildToHorizontalBox(AbilityFrame))
	{
		FrameSlot->SetVerticalAlignment(VAlign_Top);
	}
}

float UJoA_W_Figures::GetUpgradeBottom()
{
	UWidget* Upgrade = GetWidgetFromName(
		TEXT("Btn_Upgrade"));
	UWidget* Root = GetRootWidget();
	if (!Upgrade || !Root)
	{
		return 0.f;
	}

	return BottomOf(Root, Upgrade);
}

void UJoA_W_Figures::OnKnightClicked()
{
	ShowFigure(JoA_UIText(TEXT("Figures.Knight")));
}

void UJoA_W_Figures::OnInfantryClicked()
{
	ShowFigure(JoA_UIText(TEXT("Figures.Infantry")));
}

void UJoA_W_Figures::OnArcherClicked()
{
	ShowFigure(JoA_UIText(TEXT("Figures.Archer")));
}

void UJoA_W_Figures::OnMedicClicked()
{
	ShowFigure(JoA_UIText(TEXT("Figures.Medic")));
}

void UJoA_W_Figures::OnAttackClicked()
{
	ShowAbility(
		JoA_UIText(TEXT("Figures.Attack")),
		false,
		true);
}

void UJoA_W_Figures::OnDefenseClicked()
{
	ShowAbility(
		JoA_UIText(TEXT("Figures.DefenseSeeAttack")),
		false,
		false);
}

void UJoA_W_Figures::OnReachClicked()
{
	ShowAbility(
		JoA_UIText(TEXT("Figures.Reach")),
		false,
		false);
}

void UJoA_W_Figures::OnHealClicked()
{
	ShowAbility(
		JoA_UIText(TEXT("Figures.Heal")),
		false,
		false);
}

void UJoA_W_Figures::OnPickupFlagClicked()
{
	ShowAbility(
		JoA_UIText(TEXT("Figures.Pickup")),
		true,
		false);
}

void UJoA_W_Figures::OnUpgradeClicked()
{
	ShowAbility(
		JoA_UIText(TEXT("Figures.Upgrade")),
		false,
		false);
}
