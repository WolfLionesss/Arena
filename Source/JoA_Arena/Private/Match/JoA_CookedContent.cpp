// Copyright Epic Games, Inc. All Rights Reserved.

#include "Match/JoA_CookedContent.h"

// --- Engine ---
#include "Animation/AnimSequence.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"
#include "NiagaraSystem.h"
#include "Sound/SoundBase.h"

// --- GAME ---
#include "Player/Units/JoA_UnitTypes.h"

USkeletalMesh* UJoA_CookedContent::LoadBody(
	const UObject* /*WorldContext*/,
	EUnitType Type)
{
	const TCHAR* AssetName = TEXT("SM_Knight");
	switch (Type)
	{
	case EUnitType::LightInfantry:
		AssetName = TEXT("SM_Barbarian");
		break;
	case EUnitType::Medic:
		AssetName = TEXT("SM_Mage");
		break;
	case EUnitType::Archer:
		AssetName = TEXT("SM_Ranger");
		break;
	default:
		break;
	}

	return LoadObject<USkeletalMesh>(
		nullptr,
		*(FString(TEXT(
			"/Game/_Assets/KayKit/Characters/fbx/"))
			+ AssetName
			+ TEXT(".")
			+ AssetName));
}

UAnimSequence* UJoA_CookedContent::LoadAnim(
	const UObject* /*WorldContext*/,
	const TCHAR* AssetName)
{
	if (!AssetName)
	{
		return nullptr;
	}

	return LoadObject<UAnimSequence>(
		nullptr,
		*(FString(TEXT(
			"/Game/_Assets/KayKit/Animations/fbx/"
			"Rig_Medium/"))
			+ AssetName
			+ TEXT(".")
			+ AssetName));
}

USoundBase* UJoA_CookedContent::LoadSound(
	const UObject* /*WorldContext*/,
	const TCHAR* AssetPath)
{
	if (!AssetPath)
	{
		return nullptr;
	}

	return LoadObject<USoundBase>(nullptr, AssetPath);
}

UStaticMesh* UJoA_CookedContent::LoadProp(
	const UObject* /*WorldContext*/,
	const TCHAR* AssetName)
{
	if (!AssetName)
	{
		return nullptr;
	}

	return LoadObject<UStaticMesh>(
		nullptr,
		*(FString(TEXT(
			"/Game/_Assets/KayKit/Assets/fbx/"))
			+ AssetName
			+ TEXT(".")
			+ AssetName));
}

UNiagaraSystem* UJoA_CookedContent::LoadCrateChips(
	const UObject* /*WorldContext*/)
{
	return LoadObject<UNiagaraSystem>(
		nullptr,
		TEXT("/Game/VFX/NS_CrateChips.NS_CrateChips"),
		nullptr,
		LOAD_NoWarn);
}

UTexture2D* UJoA_CookedContent::LoadAttackIcon(
	const UObject* /*WorldContext*/)
{
	return LoadObject<UTexture2D>(
		nullptr,
		TEXT("/Game/Materials/Textures/Icons/"
			"T_attackIcon.T_attackIcon"));
}

UClass* UJoA_CookedContent::LoadWidget(
	const UObject* /*WorldContext*/,
	const TCHAR* ClassPath)
{
	if (!ClassPath)
	{
		return nullptr;
	}

	return LoadClass<UUserWidget>(nullptr, ClassPath);
}
