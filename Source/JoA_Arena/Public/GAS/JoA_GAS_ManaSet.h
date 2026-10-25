// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

// --- GAS ---
#include "GAS/JoA_GAS_AttributeSetBase.h"

#include "JoA_GAS_ManaSet.generated.h"

/**
 * Mana for one figure. ManaSpend and ManaGain are
 * meta attributes. The execution writes the cost or
 * the rest, then this set applies it to Mana and
 * clears the meta. Mana replicates. The metas stay
 * on the server.
 */
UCLASS()
class JOA_ARENA_API UJoA_GAS_ManaSet
	: public UJoA_GAS_AttributeSetBase
{
	GENERATED_BODY()

public:
	UJoA_GAS_ManaSet();

	virtual void GetLifetimeReplicatedProps(
		TArray<FLifetimeProperty>& OutLifetimeProps
	) const override;

	virtual void PostGameplayEffectExecute(
		const FGameplayEffectModCallbackData& Data
	) override;

	UPROPERTY(
		BlueprintReadOnly,
		Category = "Mana",
		meta = (HideInDetailsView))
	FGameplayAttributeData ManaSpend;
	ATTRIBUTE_ACCESSORS(UJoA_GAS_ManaSet, ManaSpend)

	/* Meta. One rest adds this, then it is cleared. */
	UPROPERTY(
		BlueprintReadOnly,
		Category = "Mana",
		meta = (HideInDetailsView))
	FGameplayAttributeData ManaGain;
	ATTRIBUTE_ACCESSORS(UJoA_GAS_ManaSet, ManaGain)

	UPROPERTY(
		BlueprintReadOnly,
		Category = "Mana",
		ReplicatedUsing = OnRep_Mana)
	FGameplayAttributeData Mana;
	ATTRIBUTE_ACCESSORS(UJoA_GAS_ManaSet, Mana)

	/* Full mana. Zero for a figure without mana. */
	UPROPERTY(
		BlueprintReadOnly,
		Category = "Mana",
		ReplicatedUsing = OnRep_MaxMana)
	FGameplayAttributeData MaxMana;
	ATTRIBUTE_ACCESSORS(UJoA_GAS_ManaSet, MaxMana)

protected:
	virtual void ClampAttribute(
		const FGameplayAttribute& Attribute,
		float& NewValue) const override;

	virtual bool IsReplicatedAttribute(
		const FGameplayAttribute& Attribute) const override;

	UFUNCTION()
	void OnRep_Mana(const FGameplayAttributeData& OldMana);

	UFUNCTION()
	void OnRep_MaxMana(
		const FGameplayAttributeData& OldMaxMana);
};
