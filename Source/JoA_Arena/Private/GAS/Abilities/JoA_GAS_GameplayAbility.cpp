// Copyright Epic Games, Inc. All Rights Reserved.

#include "GAS/Abilities/JoA_GAS_GameplayAbility.h"

// --- Engine ---
#include "Engine/LocalPlayer.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "GameFramework/PlayerController.h"
#include "InputAction.h"
#include "InputMappingContext.h"

// --- GAME ---
#include "Player/Units/JoA_Figure.h"

// --- GAS ---
#include "AbilitySystemComponent.h"

namespace
{
	// Above the selection keys of the controller.
	constexpr int32 RuntimeContextPriority = 2;
}

UJoA_GAS_GameplayAbility::UJoA_GAS_GameplayAbility()
{
	/* One instance per figure, so the binding belongs to
	 * that figure and clears with it. */
	InstancingPolicy =
		EGameplayAbilityInstancingPolicy::InstancedPerActor;

	/* The board is server state. The client only asks. */
	NetExecutionPolicy =
		EGameplayAbilityNetExecutionPolicy::ServerOnly;
}

bool UJoA_GAS_GameplayAbility::CanActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayTagContainer* SourceTags,
	const FGameplayTagContainer* TargetTags,
	FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!Super::CanActivateAbility(
			Handle,
			ActorInfo,
			SourceTags,
			TargetTags,
			OptionalRelevantTags))
	{
		return false;
	}

	const AJoA_Figure* Figure = GetFigure(ActorInfo);
	const int32 CommandID = GetCommandID(Handle, ActorInfo);
	if (!Figure || CommandID == INDEX_NONE)
	{
		return false;
	}

	return CanRunCommand(Figure, CommandID);
}

int32 UJoA_GAS_GameplayAbility::GetCommandID(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo)
{
	UAbilitySystemComponent* ASC = ActorInfo
		? ActorInfo->AbilitySystemComponent.Get()
		: nullptr;
	const FGameplayAbilitySpec* Spec = ASC
		? ASC->FindAbilitySpecFromHandle(Handle)
		: nullptr;
	return Spec ? Spec->InputID : INDEX_NONE;
}

bool UJoA_GAS_GameplayAbility::CanRunCommand(
	const AJoA_Figure* Figure,
	int32 CommandID) const
{
	(void)CommandID;
	return Figure && Figure->IsAllowedToAct();
}

TArray<FKey> UJoA_GAS_GameplayAbility::GetDefaultKeys(
	int32 CommandID) const
{
	(void)CommandID;
	return TArray<FKey>();
}

AJoA_Figure* UJoA_GAS_GameplayAbility::GetFigure(
	const FGameplayAbilityActorInfo* ActorInfo)
{
	return ActorInfo
		? Cast<AJoA_Figure>(ActorInfo->AvatarActor.Get())
		: nullptr;
}

void UJoA_GAS_GameplayAbility::OnGiveAbility(
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilitySpec& Spec)
{
	Super::OnGiveAbility(ActorInfo, Spec);

	/* The CDO is shared by every figure. Only the
	 * per-figure instance may hold a binding. */
	if (IsInstantiated())
	{
		RefreshInputBinding(ActorInfo, Spec);
	}
}

void UJoA_GAS_GameplayAbility::OnRemoveAbility(
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilitySpec& Spec)
{
	UnbindInput();
	Super::OnRemoveAbility(ActorInfo, Spec);
}

void UJoA_GAS_GameplayAbility::RefreshInputBinding(
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilitySpec& Spec)
{
	const AJoA_Figure* Figure = GetFigure(ActorInfo);
	APlayerController* Controller = Figure
		? Cast<APlayerController>(Figure->GetController())
		: nullptr;
	if (!Controller || !Controller->IsLocalController())
	{
		UnbindInput();
		return;
	}

	UEnhancedInputComponent* Component =
		Cast<UEnhancedInputComponent>(
			Controller->InputComponent);
	ULocalPlayer* LocalPlayer = Controller->GetLocalPlayer();
	UEnhancedInputLocalPlayerSubsystem* Subsystem =
		LocalPlayer
		? LocalPlayer->GetSubsystem<
			UEnhancedInputLocalPlayerSubsystem>()
		: nullptr;
	if (!Component || !Subsystem)
	{
		return;
	}

	// Already bound to this controller.
	if (BoundComponent.Get() == Component)
	{
		return;
	}

	UnbindInput();

	UInputAction* Action = ResolveInputAction(Spec);
	if (!Action)
	{
		return;
	}

	BoundSpecHandle = Spec.Handle;
	BoundASC = ActorInfo->AbilitySystemComponent;
	Component->BindAction(
		Action,
		InputTriggerType,
		this,
		&UJoA_GAS_GameplayAbility::HandleInputPressed);
	BoundComponent = Component;

	if (RuntimeContext)
	{
		Subsystem->AddMappingContext(
			RuntimeContext,
			RuntimeContextPriority);
	}
	BoundSubsystem = Subsystem;
}

void UJoA_GAS_GameplayAbility::UnbindInput()
{
	if (UEnhancedInputComponent* Component =
		BoundComponent.Get())
	{
		Component->ClearBindingsForObject(this);
	}

	if (UEnhancedInputLocalPlayerSubsystem* Subsystem =
		BoundSubsystem.Get())
	{
		if (RuntimeContext)
		{
			Subsystem->RemoveMappingContext(RuntimeContext);
		}
	}

	BoundComponent.Reset();
	BoundSubsystem.Reset();
	BoundASC.Reset();
}

UInputAction* UJoA_GAS_GameplayAbility::ResolveInputAction(
	const FGameplayAbilitySpec& Spec)
{
	if (ActivationInputAction)
	{
		return ActivationInputAction;
	}

	if (RuntimeInputAction)
	{
		return RuntimeInputAction;
	}

	const TArray<FKey> Keys = GetDefaultKeys(Spec.InputID);
	if (Keys.IsEmpty())
	{
		return nullptr;
	}

	RuntimeInputAction = NewObject<UInputAction>(this);
	RuntimeContext = NewObject<UInputMappingContext>(this);
	for (const FKey& Key : Keys)
	{
		RuntimeContext->MapKey(RuntimeInputAction, Key);
	}

	return RuntimeInputAction;
}

void UJoA_GAS_GameplayAbility::HandleInputPressed()
{
	if (UAbilitySystemComponent* ASC = BoundASC.Get())
	{
		ASC->TryActivateAbility(BoundSpecHandle);
	}
}
