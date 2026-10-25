// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

// --- GAS ---
#include "GAS/JoA_GAS_AttributeSetBase.h"

#include "JoA_GAS_StaminaSet.generated.h"

/**
 * Stamina for one figure. StaminaSpend and
 * StaminaGain are meta attributes. The execution
 * writes the cost or the rest, then this set applies
 * it to Stamina and clears the meta. Stamina
 * replicates. The metas stay on the server.
 */
UCLASS()
class JOA_ARENA_API UJoA_GAS_StaminaSet
	: public UJoA_GAS_AttributeSetBase
{
	GENERATED_BODY()

public:
	UJoA_GAS_StaminaSet();

	virtual void GetLifetimeReplicatedProps(
		TArray<FLifetimeProperty>& OutLifetimeProps
	) const override;

	virtual void PostGameplayEffectExecute(
		const FGameplayEffectModCallbackData& Data
	) override;

	UPROPERTY(
		BlueprintReadOnly,
		Category = "Stamina",
		meta = (HideInDetailsView))
	FGameplayAttributeData StaminaSpend;
	ATTRIBUTE_ACCESSORS(UJoA_GAS_StaminaSet, StaminaSpend)

	/* Meta. One rest adds this, then it is cleared. */
	UPROPERTY(
		BlueprintReadOnly,
		Category = "Stamina",
		meta = (HideInDetailsView))
	FGameplayAttributeData StaminaGain;
	ATTRIBUTE_ACCESSORS(UJoA_GAS_StaminaSet, StaminaGain)

	UPROPERTY(
		BlueprintReadOnly,
		Category = "Stamina",
		ReplicatedUsing = OnRep_Stamina)
	FGameplayAttributeData Stamina;
	ATTRIBUTE_ACCESSORS(UJoA_GAS_StaminaSet, Stamina)

	/* Full stamina. Zero for a figure without stamina. */
	UPROPERTY(
		BlueprintReadOnly,
		Category = "Stamina",
		ReplicatedUsing = OnRep_MaxStamina)
	FGameplayAttributeData MaxStamina;
	ATTRIBUTE_ACCESSORS(UJoA_GAS_StaminaSet, MaxStamina)

protected:
	virtual void ClampAttribute(
		const FGameplayAttribute& Attribute,
		float& NewValue) const override;

	virtual bool IsReplicatedAttribute(
		const FGameplayAttribute& Attribute) const override;

	UFUNCTION()
	void OnRep_Stamina(
		const FGameplayAttributeData& OldStamina);

	UFUNCTION()
	void OnRep_MaxStamina(
		const FGameplayAttributeData& OldMaxStamina);
};
