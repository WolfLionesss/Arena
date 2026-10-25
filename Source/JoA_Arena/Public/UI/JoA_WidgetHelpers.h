// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

// --- Components ---
#include "Components/Button.h"

/* One definition for the module. Unity builds merge
 * .cpp files, so a copy in two anonymous namespaces
 * is C2084. */
inline void JoA_DisableButtonFocus(UButton* Button)
{
	if (!Button)
	{
		return;
	}
	/* Space activates a focused button.
	 * Jump and Fall bind Space. Set before
	 * RebuildWidget; UButton has no setter. */
PRAGMA_DISABLE_DEPRECATION_WARNINGS
	Button->IsFocusable = false;
PRAGMA_ENABLE_DEPRECATION_WARNINGS
}
