// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

// --- GAS ---
#include "GAS/JoA_GAS_AttributeSetBase.h"

#include "JoA_GAS_CombatSet.generated.h"

/**
 * Attack, Defense, and Heal for one figure.
 * Health, Stamina, and Mana live on their own sets.
 * These three replicate to both players.
 */
UCLASS()
class JOA_ARENA_API UJoA_GAS_CombatSet
	: public UJoA_GAS_AttributeSetBase
{
	GENERATED_BODY()

public:
	UJoA_GAS_CombatSet();

	virtual void GetLifetimeReplicatedProps(
		TArray<FLifetimeProperty>& OutLifetimeProps
	) const override;

	virtual void PostGameplayEffectExecute(
		const FGameplayEffectModCallbackData& Data
	) override;

	UPROPERTY(
		BlueprintReadOnly,
		Category = "Combat",
		ReplicatedUsing = OnRep_Attack)
	FGameplayAttributeData Attack;
	ATTRIBUTE_ACCESSORS(UJoA_GAS_CombatSet, Attack)

	UPROPERTY(
		BlueprintReadOnly,
		Category = "Combat",
		ReplicatedUsing = OnRep_Defense)
	FGameplayAttributeData Defense;
	ATTRIBUTE_ACCESSORS(UJoA_GAS_CombatSet, Defense)

	UPROPERTY(
		BlueprintReadOnly,
		Category = "Combat",
		ReplicatedUsing = OnRep_Heal)
	FGameplayAttributeData Heal;
	ATTRIBUTE_ACCESSORS(UJoA_GAS_CombatSet, Heal)

protected:
	virtual void ClampAttribute(
		const FGameplayAttribute& Attribute,
		float& NewValue) const override;

	virtual bool IsReplicatedAttribute(
		const FGameplayAttribute& Attribute) const override;

	UFUNCTION()
	void OnRep_Attack(
		const FGameplayAttributeData& OldAttack);

	UFUNCTION()
	void OnRep_Defense(
		const FGameplayAttributeData& OldDefense);

	UFUNCTION()
	void OnRep_Heal(const FGameplayAttributeData& OldHeal);
};
