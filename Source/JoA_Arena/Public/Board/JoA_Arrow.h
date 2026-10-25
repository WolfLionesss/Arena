// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

// --- Engine ---
#include "GameFramework/Actor.h"

#include "JoA_Arrow.generated.h"

class USceneComponent;
class UStaticMeshComponent;

/**
 * One reused shot. Hide the mesh instead of
 * destroying the actor.
 */
UCLASS(NotBlueprintable)
class JOA_ARENA_API AJoA_Arrow : public AActor
{
	GENERATED_BODY()

public:
	AJoA_Arrow();

	static AJoA_Arrow* Get(UWorld* World);

	void Launch(
		const FVector& From,
		const FVector& To,
		bool bHaltAtEnd);

	void HideShot();

	static constexpr float FlightSpeed = 900.f;

	virtual void Tick(float DeltaSeconds) override;

private:
	bool IsOffBoard() const;

	UPROPERTY()
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> Mesh;

	FVector FlightFrom = FVector::ZeroVector;
	FVector FlightTo = FVector::ZeroVector;
	float FlightSeconds = 0.2f;
	float FlightElapsed = 0.f;
	bool bFlying = false;
	bool bStopAtEnd = true;
};
