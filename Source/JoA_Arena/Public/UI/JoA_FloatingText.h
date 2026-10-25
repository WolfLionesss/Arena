// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

// --- Engine ---
#include "GameFramework/Actor.h"

#include "JoA_FloatingText.generated.h"

class AJoA_Figure;
class UWidgetComponent;

/**
 * Local-only combat text above a figure. Each machine
 * spawns its own from the multicast of the figure, so it
 * never replicates. It removes itself after the text
 * animation ends.
 */
UCLASS(NotBlueprintable)
class JOA_ARENA_API AJoA_FloatingText : public AActor
{
	GENERATED_BODY()

public:
	AJoA_FloatingText();

	void Show(
		AJoA_Figure* Figure,
		const FText& Text,
		const FLinearColor& Color);

private:
	UPROPERTY()
	TObjectPtr<UWidgetComponent> TextWidget;
};
