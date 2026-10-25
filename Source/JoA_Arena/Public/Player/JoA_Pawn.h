// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

// --- Engine ---
#include "GameFramework/Pawn.h"

#include "JoA_Pawn.generated.h"

/**
 * Possession pawn with no movement.
 * While this pawn is the view target, CalcCamera
 * still shows the French or English board view.
 */
UCLASS(Blueprintable)
class JOA_ARENA_API AJoA_Pawn : public APawn
{
	GENERATED_BODY()

public:
	AJoA_Pawn();

	virtual void CalcCamera(
		float DeltaTime,
		FMinimalViewInfo& OutResult) override;
	virtual void GetActorEyesViewPoint(
		FVector& OutLocation,
		FRotator& OutRotation) const override;
	virtual FVector GetPawnViewLocation() const override;

protected:
	UPROPERTY(VisibleAnywhere, Category = "Arena")
	TObjectPtr<USceneComponent> SceneRoot;
};
