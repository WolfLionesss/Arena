// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

// --- GAS ---
#include "NativeGameplayTags.h"

/* Magnitudes for UJoA_GAS_VitalExecution.
 * The execution writes these onto meta attributes.
 * It does not write Health, Stamina, or Mana. */
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_JoA_GAS_Data_Damage);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_JoA_GAS_Data_Healing);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(
	TAG_JoA_GAS_Data_StaminaSpend);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_JoA_GAS_Data_ManaSpend);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(
	TAG_JoA_GAS_Data_StaminaGain);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_JoA_GAS_Data_ManaGain);

/* Set on a figure that reached the enemy home
 * file without a flag. */
UE_DECLARE_GAMEPLAY_TAG_EXTERN(
	TAG_JoA_GAS_State_Promoted);
