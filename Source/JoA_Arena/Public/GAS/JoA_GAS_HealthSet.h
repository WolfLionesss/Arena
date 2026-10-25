// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

// --- GAS ---
#include "GAS/JoA_GAS_AttributeSetBase.h"

#include "JoA_GAS_HealthSet.generated.h"

/**
 * Health for one figure. Damage and Healing are meta
 * attributes: an execution writes them, then this set
 * commits the result onto Health and clears them.
 * Health replicates. The meta values stay on the server.
 */
UCLASS()
class JOA_ARENA_API UJoA_GAS_HealthSet
	: public UJoA_GAS_AttributeSetBase
{
	GENERATED_BODY()

public:
	UJoA_GAS_HealthSet();

	virtual void GetLifetimeReplicatedProps(
		TArray<FLifetimeProperty>& OutLifetimeProps
	) const override;

	virtual void PostGameplayEffectExecute(
		const FGameplayEffectModCallbackData& Data
	) override;

	UPROPERTY(
		BlueprintReadOnly,
		Category = "Health",
		meta = (HideInDetailsView))
	FGameplayAttributeData Damage;
	ATTRIBUTE_ACCESSORS(UJoA_GAS_HealthSet, Damage)

	UPROPERTY(
		BlueprintReadOnly,
		Category = "Health",
		meta = (HideInDetailsView))
	FGameplayAttributeData Healing;
	ATTRIBUTE_ACCESSORS(UJoA_GAS_HealthSet, Healing)

	UPROPERTY(
		BlueprintReadOnly,
		Category = "Health",
		ReplicatedUsing = OnRep_Health)
	FGameplayAttributeData Health;
	ATTRIBUTE_ACCESSORS(UJoA_GAS_HealthSet, Health)

	/* Full health. The widget shows Health over this. */
	UPROPERTY(
		BlueprintReadOnly,
		Category = "Health",
		ReplicatedUsing = OnRep_MaxHealth)
	FGameplayAttributeData MaxHealth;
	ATTRIBUTE_ACCESSORS(UJoA_GAS_HealthSet, MaxHealth)

protected:
	virtual void ClampAttribute(
		const FGameplayAttribute& Attribute,
		float& NewValue) const override;

	virtual bool IsReplicatedAttribute(
		const FGameplayAttribute& Attribute) const override;

	UFUNCTION()
	void OnRep_Health(
		const FGameplayAttributeData& OldHealth);

	UFUNCTION()
	void OnRep_MaxHealth(
		const FGameplayAttributeData& OldMaxHealth);
};
