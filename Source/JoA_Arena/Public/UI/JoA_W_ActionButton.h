// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

// --- Components ---
#include "Components/Button.h"

#include "JoA_W_ActionButton.generated.h"

class AJoA_Figure;

/**
 * Attack, heal, obstacle, build, pickup, or drop.
 * WBP_Play builds one per action in reach of the
 * selected figure. A null target is the obstacle,
 * the build, or the flag.
 */
UENUM()
enum class EActionKind : uint8
{
	Attack,
	Heal,
	Obstacle,
	Build,
	Pickup,
	Drop
};

DECLARE_MULTICAST_DELEGATE_TwoParams(
	FOnActionButtonClicked,
	AJoA_Figure*,
	EActionKind);

UCLASS()
class JOA_ARENA_API UJoA_W_ActionButton : public UButton
{
	GENERATED_BODY()

public:
	UJoA_W_ActionButton(
		const FObjectInitializer& ObjectInitializer);

	void Setup(
		AJoA_Figure* InTarget,
		EActionKind InKind);

	FOnActionButtonClicked OnActionClicked;

private:
	UFUNCTION()
	void HandleSelfClicked();

	TWeakObjectPtr<AJoA_Figure> Target;
	EActionKind Kind = EActionKind::Attack;
};
