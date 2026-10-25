// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/JoA_W_About.h"

// --- Engine ---
#include "Blueprint/WidgetTree.h"

// --- GAME ---
#include "UI/JoA_UIText.h"
#include "UI/JoA_W_Figures.h"
#include "UI/JoA_WidgetHelpers.h"

// --- Components ---
#include "Components/Button.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "Components/Spacer.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"

namespace
{
	const TCHAR* FiguresWidgetPath =
		TEXT("/Game/WBP/WBP_Figures.WBP_Figures_C");
}

void UJoA_W_About::NativeConstruct()
{
	Super::NativeConstruct();

	Btn_Goal = Cast<UButton>(
		GetWidgetFromName(TEXT("Btn_Goal")));
	if (Btn_Goal)
	{
		JoA_DisableButtonFocus(Btn_Goal);
		Btn_Goal->OnClicked.AddUniqueDynamic(
			this,
			&UJoA_W_About::OnGoalClicked);
	}

	Btn_Figures = Cast<UButton>(
		GetWidgetFromName(TEXT("Btn_Figures")));
	if (Btn_Figures)
	{
		JoA_DisableButtonFocus(Btn_Figures);
		Btn_Figures->OnClicked.AddUniqueDynamic(
			this,
			&UJoA_W_About::OnFiguresClicked);
	}

	TB_GoalInfo = Cast<UTextBlock>(
		GetWidgetFromName(TEXT("TB_GoalInfo")));

	/* Without Back the menu column is gone
	 * and the player cannot leave. */
	UVerticalBox* Column = Btn_Figures
		? Cast<UVerticalBox>(Btn_Figures->GetParent())
		: nullptr;
	if (!Column || !WidgetTree
		|| GetWidgetFromName(TEXT("Btn_AboutBack")))
	{
		return;
	}

	UButton* Back =
		WidgetTree->ConstructWidget<UButton>(
			UButton::StaticClass(),
			TEXT("Btn_AboutBack"));
	JoA_DisableButtonFocus(Back);
	Back->SetStyle(Btn_Figures->GetStyle());
	UTextBlock* Label =
		WidgetTree->ConstructWidget<UTextBlock>(
			UTextBlock::StaticClass(),
			TEXT("TB_AboutBack"));
	Label->SetText(JoA_UIText(TEXT("About.Back")));
	if (UTextBlock* FiguresLabel =
		Cast<UTextBlock>(
			Btn_Figures->GetChildAt(0)))
	{
		Label->SetFont(FiguresLabel->GetFont());
		Label->SetColorAndOpacity(
			FiguresLabel->GetColorAndOpacity());
	}
	Back->AddChild(Label);
	Back->OnClicked.AddUniqueDynamic(
		this,
		&UJoA_W_About::OnBackClicked);

	USpacer* Push =
		WidgetTree->ConstructWidget<USpacer>(
			USpacer::StaticClass(),
			TEXT("SP_AboutBack"));
	Push->SetSize(FVector2D(1.f, 0.f));
	Column->AddChildToVerticalBox(Push);
	if (UVerticalBoxSlot* BackSlot =
		Column->AddChildToVerticalBox(Back))
	{
		BackSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 0.f));
		BackSlot->SetHorizontalAlignment(HAlign_Fill);
		BackSlot->SetVerticalAlignment(VAlign_Top);
	}

	if (UHorizontalBoxSlot* ColumnSlot =
		Cast<UHorizontalBoxSlot>(Column->Slot))
	{
		ColumnSlot->SetVerticalAlignment(VAlign_Top);
	}

	EnsureFigures();
	PlaceBack();
	LockGoalText();
	ShowGoal();
}

void UJoA_W_About::EnsureFigures()
{
	if (FiguresPage)
	{
		return;
	}

	UClass* FiguresClass = LoadClass<UJoA_W_Figures>(
		nullptr,
		FiguresWidgetPath);
	UHorizontalBox* Row =
		Cast<UHorizontalBox>(GetRootWidget());
	if (!FiguresClass || !Row || !WidgetTree)
	{
		return;
	}

	FiguresPage = CreateWidget<UJoA_W_Figures>(
		this,
		FiguresClass);
	if (!FiguresPage)
	{
		return;
	}

	UHorizontalBoxSlot* FiguresSlot =
		Row->AddChildToHorizontalBox(FiguresPage);
	if (FiguresSlot)
	{
		FiguresSlot->SetPadding(
			FMargin(40.f, 0.f, 0.f, 0.f));
		FiguresSlot->SetHorizontalAlignment(
			HAlign_Left);
		FiguresSlot->SetVerticalAlignment(VAlign_Top);
	}
}

void UJoA_W_About::LockGoalText()
{
	if (!WidgetTree || !TB_GoalInfo || !FiguresPage)
	{
		return;
	}

	UHorizontalBox* Parent = Cast<UHorizontalBox>(
		TB_GoalInfo->GetParent());
	if (!Parent)
	{
		return;
	}

	FiguresPage->ForceLayoutPrepass();
	float Width = FiguresPage->GetDesiredSize().X;
	if (Width < 1.f)
	{
		return;
	}

	/* Both pages share one overlay cell. The idle
	 * page is Hidden, not Collapsed, so the cell
	 * keeps the wider width and the buttons never
	 * shift when the page changes. */
	TB_GoalInfo->RemoveFromParent();
	FiguresPage->RemoveFromParent();
	USizeBox* Box =
		WidgetTree->ConstructWidget<USizeBox>(
			USizeBox::StaticClass(),
			TEXT("SZ_GoalInfo"));
	Box->SetWidthOverride(Width);
	Box->AddChild(TB_GoalInfo);
	UOverlay* Pages =
		WidgetTree->ConstructWidget<UOverlay>(
			UOverlay::StaticClass(),
			TEXT("OV_AboutPages"));
	if (UOverlaySlot* GoalSlot =
		Pages->AddChildToOverlay(Box))
	{
		GoalSlot->SetHorizontalAlignment(HAlign_Left);
		GoalSlot->SetVerticalAlignment(VAlign_Top);
	}
	if (UOverlaySlot* PageSlot =
		Pages->AddChildToOverlay(FiguresPage))
	{
		PageSlot->SetHorizontalAlignment(HAlign_Left);
		PageSlot->SetVerticalAlignment(VAlign_Top);
	}
	if (UHorizontalBoxSlot* CellSlot =
		Parent->AddChildToHorizontalBox(Pages))
	{
		CellSlot->SetPadding(
			FMargin(40.f, 0.f, 0.f, 0.f));
		CellSlot->SetHorizontalAlignment(HAlign_Left);
		CellSlot->SetVerticalAlignment(VAlign_Top);
	}

	TB_GoalInfo->SetAutoWrapText(true);
	TB_GoalInfo->SetWrapTextAt(Width - 8.f);
}

void UJoA_W_About::ShowGoal()
{
	if (TB_GoalInfo)
	{
		TB_GoalInfo->SetVisibility(
			ESlateVisibility::SelfHitTestInvisible);
	}
	if (FiguresPage)
	{
		FiguresPage->SetVisibility(
			ESlateVisibility::Hidden);
	}
}

void UJoA_W_About::OnGoalClicked()
{
	ShowGoal();
}

void UJoA_W_About::OnFiguresClicked()
{
	EnsureFigures();
	if (TB_GoalInfo)
	{
		TB_GoalInfo->SetVisibility(
			ESlateVisibility::Hidden);
	}
	if (FiguresPage)
	{
		FiguresPage->SetVisibility(
			ESlateVisibility::SelfHitTestInvisible);
	}
}

void UJoA_W_About::PlaceBack()
{
	USpacer* Gap = Cast<USpacer>(
		GetWidgetFromName(TEXT("SP_AboutBack")));
	UWidget* Goal =
		GetWidgetFromName(TEXT("Btn_Goal"));
	if (!Gap || !Goal || !Btn_Figures || !FiguresPage)
	{
		return;
	}

	/* 30px under Upgrade. Measured before the
	 * figures page is shown, so Back does not
	 * drop when that page opens. */
	const float Target =
		FiguresPage->GetUpgradeBottom() + 30.f;
	Goal->ForceLayoutPrepass();
	Btn_Figures->ForceLayoutPrepass();
	float Used = Goal->GetDesiredSize().Y;
	Used += Btn_Figures->GetDesiredSize().Y;
	float Space = Target - Used;
	if (Space < 0.f)
	{
		Space = 0.f;
	}
	Gap->SetSize(FVector2D(1.f, Space));
}

void UJoA_W_About::OnBackClicked()
{
	OnBackToMenu.ExecuteIfBound();
}
