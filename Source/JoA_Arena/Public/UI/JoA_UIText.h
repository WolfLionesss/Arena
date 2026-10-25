// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/* Every player-facing UI text lives in one String Table
 * asset, so a designer can rewrite it in the editor
 * without a compile. Unity builds merge .cpp files, so
 * the lookup is defined once here as inline. */
inline FName JoA_UITextTableId()
{
	return FName(TEXT("/Game/UI/Text/ST_UI.ST_UI"));
}

/* A missing key shows the engine's missing-entry text
 * in the HUD, so a wrong key is visible in PIE. */
inline FText JoA_UIText(const TCHAR* Key)
{
	return FText::FromStringTable(
		JoA_UITextTableId(),
		FString(Key));
}

inline FText JoA_UIText(const FName& Key)
{
	return FText::FromStringTable(
		JoA_UITextTableId(),
		Key.ToString());
}

/* Texts with {0}, {1} placeholders. The designer may
 * move a placeholder but must not delete it. */
inline FText JoA_UIText(
	const TCHAR* Key,
	const FFormatOrderedArguments& Arguments)
{
	return FText::Format(
		JoA_UIText(Key),
		Arguments);
}
