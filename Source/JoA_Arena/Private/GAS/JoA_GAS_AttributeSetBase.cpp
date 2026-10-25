// Copyright Epic Games, Inc. All Rights Reserved.

#include "GAS/JoA_GAS_AttributeSetBase.h"

// --- Engine ---
#include "GameFramework/Actor.h"

// --- Net ---
#include "Net/Core/PushModel/PushModel.h"

void UJoA_GAS_AttributeSetBase::PreAttributeChange(
	const FGameplayAttribute& Attribute,
	float& NewValue)
{
	Super::PreAttributeChange(Attribute, NewValue);
	ClampAttribute(Attribute, NewValue);
}

void UJoA_GAS_AttributeSetBase::PreAttributeBaseChange(
	const FGameplayAttribute& Attribute,
	float& NewValue) const
{
	Super::PreAttributeBaseChange(Attribute, NewValue);
	ClampAttribute(Attribute, NewValue);
}

void UJoA_GAS_AttributeSetBase::PostAttributeChange(
	const FGameplayAttribute& Attribute,
	float OldValue,
	float NewValue)
{
	Super::PostAttributeChange(
		Attribute,
		OldValue,
		NewValue);
	MarkReplicatedAttribute(Attribute);
}

void UJoA_GAS_AttributeSetBase::PostAttributeBaseChange(
	const FGameplayAttribute& Attribute,
	float OldValue,
	float NewValue) const
{
	Super::PostAttributeBaseChange(
		Attribute,
		OldValue,
		NewValue);

	/* SetNumericAttributeBase updates the base through
	 * this const hook. Push model still needs a dirty
	 * mark or the client keeps the constructor value. */
	UJoA_GAS_AttributeSetBase* MutableThis =
		const_cast<UJoA_GAS_AttributeSetBase*>(this);
	MutableThis->MarkReplicatedAttribute(Attribute);
}

void UJoA_GAS_AttributeSetBase::ClampAttribute(
	const FGameplayAttribute& Attribute,
	float& NewValue) const
{
	(void)Attribute;
	(void)NewValue;
}

bool UJoA_GAS_AttributeSetBase::IsReplicatedAttribute(
	const FGameplayAttribute& Attribute) const
{
	(void)Attribute;
	return false;
}

void UJoA_GAS_AttributeSetBase::MarkReplicatedAttribute(
	const FGameplayAttribute& Attribute)
{
	if (!IsReplicatedAttribute(Attribute))
	{
		return;
	}

	FProperty* Property = Attribute.GetUProperty();
	if (!Property)
	{
		return;
	}

	MARK_PROPERTY_DIRTY(this, Property);

	/* The set replicates with the figure. A low
	 * actor poll must not hold the bar back. */
	if (AActor* OwningActor = GetOwningActor())
	{
		OwningActor->ForceNetUpdate();
	}
}
