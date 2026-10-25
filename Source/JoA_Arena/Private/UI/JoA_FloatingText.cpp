// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/JoA_FloatingText.h"

// --- GAME ---
#include "Player/Units/JoA_Figure.h"
#include "UI/JoA_W_FloatingText.h"

// --- Components ---
#include "Components/SceneComponent.h"
#include "Components/WidgetComponent.h"

AJoA_FloatingText::AJoA_FloatingText()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = false;

	USceneComponent* Root =
		CreateDefaultSubobject<USceneComponent>(
			TEXT("Root"));
	SetRootComponent(Root);

	TextWidget = CreateDefaultSubobject<UWidgetComponent>(
		TEXT("TextWidget"));
	TextWidget->SetupAttachment(Root);
	TextWidget->SetWidgetSpace(EWidgetSpace::Screen);
	TextWidget->SetWidgetClass(
		UJoA_W_FloatingText::StaticClass());
	TextWidget->SetDrawSize(FVector2D(420.0, 140.0));
	TextWidget->SetPivot(FVector2D(0.5, 0.5));
	TextWidget->SetCollisionEnabled(
		ECollisionEnabled::NoCollision);
}

void AJoA_FloatingText::Show(
	AJoA_Figure* Figure,
	const FText& Text,
	const FLinearColor& Color)
{
	if (IsValid(Figure))
	{
		AttachToActor(
			Figure,
			FAttachmentTransformRules::
				SnapToTargetNotIncludingScale);
	}

	/* Above the head and above the selection cone. */
	SetActorRelativeLocation(FVector(0.0, 0.0, 190.0));

	TextWidget->InitWidget();
	if (UJoA_W_FloatingText* Widget =
		Cast<UJoA_W_FloatingText>(TextWidget->GetWidget()))
	{
		Widget->Play(Text, Color);
	}

	SetLifeSpan(UJoA_W_FloatingText::Duration);
}
