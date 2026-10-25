// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

// --- GAS ---
#include "AbilitySystemComponent.h"
#include "GAS/JoA_GAS_InitializationData.h"

#include "JoA_GAS_AbilitySystemComponent.generated.h"

/**
 * Figure ASC. One init grants sets, base values,
 * abilities, effects, and loose tags.
 * The figure is both owner and avatar.
 */
UCLASS()
class JOA_ARENA_API UJoA_GAS_AbilitySystemComponent
	: public UAbilitySystemComponent
{
	GENERATED_BODY()

public:
	UJoA_GAS_AbilitySystemComponent();

	/* Server grants the data. Client only binds actor
	 * info and loose tags. A second call does nothing. */
	void InitializeAbilitySystemData(
		const FJoA_GAS_InitializationData&
			InitializationData,
		AActor* InOwningActor,
		AActor* InAvatarActor);

	bool IsAbilitySystemDataInitialized() const
	{
		return bAbilitySystemDataInitialized;
	}

	/* One class is granted several times, once per
	 * direction. The InputID of the spec picks one.
	 * Used by the UI buttons, which share the path of
	 * the key press. */
	bool TryActivateAbilityByInputID(
		TSubclassOf<UGameplayAbility> AbilityClass,
		int32 InputID);

protected:
	const UAttributeSet* GetOrCreateAttributeSet(
		const TSubclassOf<UAttributeSet>& AttributeSetClass);

	bool bAbilitySystemDataInitialized = false;
};
