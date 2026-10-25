// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

// --- Engine ---
#include "InputCoreTypes.h"
#include "InputTriggers.h"

// --- GAS ---
#include "Abilities/GameplayAbility.h"

#include "JoA_GAS_GameplayAbility.generated.h"

class AJoA_Figure;
class UEnhancedInputComponent;
class UEnhancedInputLocalPlayerSubsystem;
class UInputAction;
class UInputMappingContext;

/**
 * Base of the figure commands: step, turn, and skip.
 * The ability holds the input action. It binds that
 * action on the Enhanced Input component of the
 * controller that possesses the figure, and drops the
 * binding when the figure loses that controller. Only
 * the possessed figure is bound, so one key press
 * reaches one figure.
 * A class is granted once per direction. The InputID
 * of the spec is that direction, so the direction is
 * read from the spec, never from the class.
 * The ability runs on the server. The client only asks.
 */
UCLASS(Abstract)
class JOA_ARENA_API UJoA_GAS_GameplayAbility
	: public UGameplayAbility
{
	GENERATED_BODY()

public:
	UJoA_GAS_GameplayAbility();

	virtual bool CanActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayTagContainer* SourceTags = nullptr,
		const FGameplayTagContainer* TargetTags = nullptr,
		FGameplayTagContainer* OptionalRelevantTags = nullptr
	) const override;

	/* Binds to the local controller of the figure, or
	 * clears the binding when there is none. The figure
	 * calls this whenever possession changes. */
	void RefreshInputBinding(
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilitySpec& Spec);

	/* Input action set in a Blueprint child. When it is
	 * empty, the ability builds its own action and maps
	 * GetDefaultKeys onto it. */
	UPROPERTY(
		EditDefaultsOnly,
		BlueprintReadOnly,
		Category = "Input")
	TObjectPtr<UInputAction> ActivationInputAction;

	UPROPERTY(
		EditDefaultsOnly,
		BlueprintReadOnly,
		Category = "Input")
	ETriggerEvent InputTriggerType = ETriggerEvent::Started;

protected:
	virtual void OnGiveAbility(
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilitySpec& Spec) override;

	virtual void OnRemoveAbility(
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilitySpec& Spec) override;

	/* Same rule for the key, the button, and the
	 * server. CommandID is the InputID of the spec. */
	virtual bool CanRunCommand(
		const AJoA_Figure* Figure,
		int32 CommandID) const;

	// Keys of the built-in action for this command.
	virtual TArray<FKey> GetDefaultKeys(
		int32 CommandID) const;

	static AJoA_Figure* GetFigure(
		const FGameplayAbilityActorInfo* ActorInfo);

	// InputID of the granted spec, or INDEX_NONE.
	static int32 GetCommandID(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo);

	void UnbindInput();

	UInputAction* ResolveInputAction(
		const FGameplayAbilitySpec& Spec);

	void HandleInputPressed();

	/* Built-in action and context. They exist only when
	 * ActivationInputAction is empty. */
	UPROPERTY(Transient)
	TObjectPtr<UInputAction> RuntimeInputAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputMappingContext> RuntimeContext;

	FGameplayAbilitySpecHandle BoundSpecHandle;
	TWeakObjectPtr<UAbilitySystemComponent> BoundASC;
	TWeakObjectPtr<UEnhancedInputComponent> BoundComponent;
	TWeakObjectPtr<UEnhancedInputLocalPlayerSubsystem>
		BoundSubsystem;
};
