// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/JoA_W_FloatingText.h"

// --- Components ---
#include "Blueprint/WidgetTree.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/TextBlock.h"

void UJoA_W_FloatingText::Play(
	const FText& InText,
	const FLinearColor& InColor)
{
	Message = InText;
	Color = InColor;
	Elapsed = 0.f;
	bStyleReady = false;
	bHasScriptImplementedTick = true;
	UpdateCanTick();
	ApplyLook();
}

TSharedRef<SWidget> UJoA_W_FloatingText::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		UOverlay* Root =
			WidgetTree->ConstructWidget<UOverlay>();
		WidgetTree->RootWidget = Root;

		Label = WidgetTree->ConstructWidget<UTextBlock>();
		UOverlaySlot* LabelSlot =
			Root->AddChildToOverlay(Label);
		LabelSlot->SetHorizontalAlignment(HAlign_Center);
		LabelSlot->SetVerticalAlignment(VAlign_Center);
	}

	TSharedRef<SWidget> Built = Super::RebuildWidget();
	ApplyLook();
	return Built;
}

void UJoA_W_FloatingText::NativeTick(
	const FGeometry& MyGeometry,
	float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	Elapsed += InDeltaTime;
	ApplyLook();
	if (Elapsed < Duration)
	{
		return;
	}

	bHasScriptImplementedTick = false;
	UpdateCanTick();
}

void UJoA_W_FloatingText::ApplyLook()
{
	if (!Label)
	{
		return;
	}

	/* The font stays at the final size and the label
	 * is scaled. A font change every frame re-lays
	 * the glyphs and the text jumps in steps. */
	if (!bStyleReady)
	{
		FSlateFontInfo Font = Label->GetFont();
		Font.Size = FMath::RoundToInt(EndSize);
		Font.OutlineSettings.OutlineSize = 2;
		Font.OutlineSettings.OutlineColor =
			FLinearColor::Black;
		Label->SetFont(Font);
		Label->SetText(Message);
		Label->SetColorAndOpacity(FSlateColor(Color));
		Label->SetRenderTransformPivot(
			FVector2D(0.5, 0.5));
		bStyleReady = true;
	}

	const float Alpha = FMath::Clamp(
		Elapsed / Duration,
		0.f,
		1.f);
	const float Scale =
		FMath::Lerp(StartSize, EndSize, Alpha) / EndSize;
	Label->SetRenderScale(FVector2D(Scale, Scale));
}
