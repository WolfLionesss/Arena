// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "JoA_CookedContent.generated.h"

class UAnimSequence;
class UNiagaraSystem;
class USkeletalMesh;
class USoundBase;
class UStaticMesh;
class UTexture2D;
enum class EUnitType : uint8;

/**
 * Loads match assets by path at play time.
 * The constructor must not load them. Blueprint
 * compile flushes the async loader, and a load
 * from a class default deadlocks that flush.
 * Shipping includes the files through the
 * packaging list in DefaultGame.ini.
 */
UCLASS()
class JOA_ARENA_API UJoA_CookedContent : public UObject
{
	GENERATED_BODY()

public:
	static USkeletalMesh* LoadBody(
		const UObject* WorldContext,
		EUnitType Type);

	static UAnimSequence* LoadAnim(
		const UObject* WorldContext,
		const TCHAR* AssetName);

	static USoundBase* LoadSound(
		const UObject* WorldContext,
		const TCHAR* AssetPath);

	static UStaticMesh* LoadProp(
		const UObject* WorldContext,
		const TCHAR* AssetName);

	static UNiagaraSystem* LoadCrateChips(
		const UObject* WorldContext);

	static UTexture2D* LoadAttackIcon(
		const UObject* WorldContext);

	static UClass* LoadWidget(
		const UObject* WorldContext,
		const TCHAR* ClassPath);
};
