// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/JoA_W_ActionButton.h"

// --- GAME ---
#include "Player/Units/JoA_Figure.h"

UJoA_W_ActionButton::UJoA_W_ActionButton(
	const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	/* Space activates a focused button. Jump
	 * uses Space, so this never takes focus. */
	InitIsFocusable(false);
}

void UJoA_W_ActionButton::Setup(
	AJoA_Figure* InTarget,
	EActionKind InKind)
{
	Target = InTarget;
	Kind = InKind;
	OnClicked.AddUniqueDynamic(
		this,
		&UJoA_W_ActionButton::HandleSelfClicked);
}

void UJoA_W_ActionButton::HandleSelfClicked()
{
	OnActionClicked.Broadcast(Target.Get(), Kind);
}
