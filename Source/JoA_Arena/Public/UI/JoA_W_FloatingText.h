// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

// --- Engine ---
#include "Blueprint/UserWidget.h"

#include "JoA_W_FloatingText.generated.h"

class UTextBlock;

/**
 * Combat text over a figure: Miss, Hit 2, Heal 2. The
 * text grows from StartSize to EndSize during Duration.
 * The tree is built in code, so no Blueprint is needed.
 */
UCLASS()
class JOA_ARENA_API UJoA_W_FloatingText : public UUserWidget
{
	GENERATED_BODY()

public:
	void Play(
		const FText& InText,
		const FLinearColor& InColor);

	static constexpr float StartSize = 32.f;
	static constexpr float EndSize = 64.f;
	static constexpr float Duration = 2.f;

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;

	virtual void NativeTick(
		const FGeometry& MyGeometry,
		float InDeltaTime) override;

private:
	void ApplyLook();

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> Label;

	bool bStyleReady = false;

	FText Message;
	FLinearColor Color = FLinearColor::White;
	float Elapsed = 0.f;
};
