// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

// --- GAS ---
#include "AttributeSet.h"
#include "GameplayTagContainer.h"

#include "JoA_GAS_InitializationData.generated.h"

class UGameplayAbility;
class UGameplayEffect;

/**
 * One base value written when a figure's ASC inits.
 * The figure fills it from FUnitStats.
 */
USTRUCT(BlueprintType)
struct JOA_ARENA_API FJoA_GAS_AttributeBaseValue
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FGameplayAttribute Attribute;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	float Value = 0.f;
};

/**
 * One ability granted at init. The InputID tells an
 * ability class which direction this spec stands for,
 * so one class serves all four steps.
 */
USTRUCT(BlueprintType)
struct JOA_ARENA_API FAbilityGrant
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TSubclassOf<UGameplayAbility> Ability;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	int32 InputID = INDEX_NONE;
};

/**
 * Everything the server grants to one figure's ASC.
 * Sets, base values, abilities, effects, and tags
 * travel together so startup has one entry point.
 */
USTRUCT(BlueprintType)
struct JOA_ARENA_API FJoA_GAS_InitializationData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TArray<TSubclassOf<UAttributeSet>> AttributeSets;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TArray<FJoA_GAS_AttributeBaseValue> AttributeBaseValues;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TArray<FAbilityGrant> AbilityGrants;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TArray<TSubclassOf<UGameplayEffect>> GameplayEffects;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FGameplayTagContainer GameplayTags;
};
