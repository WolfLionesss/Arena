// Copyright Epic Games, Inc. All Rights Reserved.

#include "Player/Units/JoA_Figure.h"

// --- Engine ---
#include "Engine/StaticMesh.h"

// --- GAME ---
#include "Board/JoA_Atlas.h"
#include "Match/JoA_CookedContent.h"
#include "Player/JoA_Nation.h"
#include "Player/Units/JoA_FigureHelpers.h"

// --- Components ---
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"

namespace
{
	UStaticMesh* LoadHeldMesh(
		const UObject* WorldContext,
		const TCHAR* AssetName)
	{
		return UJoA_CookedContent::LoadProp(
			WorldContext,
			AssetName);
	}

	void HideHeldProp(UStaticMeshComponent* Item)
	{
		if (!Item)
		{
			return;
		}

		Item->SetStaticMesh(nullptr);
		Item->SetVisibility(false, true);
		Item->SetHiddenInGame(true);
	}

	void SetHeldItem(
		UStaticMeshComponent* Item,
		USkeletalMeshComponent* Body,
		FName Socket,
		UStaticMesh* Prop,
		float TileIndex,
		float PaletteRow)
	{
		if (!Item || !Body)
		{
			return;
		}

		/* Grip lives on the socket. Relative
		 * stays identity so a code rotator cannot
		 * stack on it. */
		Item->AttachToComponent(
			Body,
			FAttachmentTransformRules::
				SnapToTargetNotIncludingScale,
			Socket);
		Item->SetStaticMesh(Prop);
		Item->SetRelativeRotation(
			FRotator::ZeroRotator);
		Item->SetRelativeLocation(
			FVector::ZeroVector);
		JoAFigure::MatchHeldScale(Item, Body);
		const bool bShow = Prop != nullptr;
		Item->SetVisibility(bShow, true);
		Item->SetHiddenInGame(!bShow);
		if (!bShow)
		{
			return;
		}

		JoAAtlas::ApplyPalette(
			Item,
			TileIndex,
			PaletteRow);
	}
}

bool AJoA_Figure::ShouldStowHeldItems() const
{
	return HeldFlag != nullptr
		|| PlayingBodyAnim == EFigureAnim::Sit
		|| PlayingBodyAnim == EFigureAnim::Lie;
}

void AJoA_Figure::RefreshHeldItemPresentation()
{
	if (ShouldStowHeldItems())
	{
		ApplyStowedItems();
		return;
	}

	ApplyHeldItems();
}

void AJoA_Figure::ApplyHeldItems()
{
	if (!Mesh || !LeftHandItem || !RightHandItem)
	{
		return;
	}

	/* Hand sockets need the body mesh. */
	if (!Mesh->GetSkeletalMeshAsset())
	{
		return;
	}

	HideHeldProp(BackWeaponItem);
	HideHeldProp(BackShieldItem);
	HideHeldProp(GroundBookItem);

	float TileIndex = JoAAtlas::WhiteTile;
	if (OwningPlayerIndex == FrenchSeat)
	{
		TileIndex = JoAAtlas::BlueTile;
	}
	else if (OwningPlayerIndex == EnglishSeat)
	{
		TileIndex = JoAAtlas::RedTile;
	}

	const TCHAR* LeftName = nullptr;
	const TCHAR* RightName = nullptr;
	FName LeftSocket = TEXT("handslot_l");
	FName RightSocket = TEXT("handslot_r");
	switch (UnitType)
	{
	case EUnitType::HeavyKnight:
		LeftName = TEXT("sword_1handed");
		RightName = TEXT("shield_badge");
		LeftSocket = TEXT("gripslot_sword");
		RightSocket = TEXT("gripslot_shield");
		break;
	case EUnitType::LightInfantry:
		LeftName = TEXT("axe_1handed");
		LeftSocket = TEXT("gripslot_axe");
		break;
	case EUnitType::Archer:
		LeftName = TEXT("bow_withString");
		LeftSocket = TEXT("gripslot_bow");
		break;
	case EUnitType::Medic:
		LeftName = TEXT("staff");
		RightName = TEXT("spellbook_open");
		LeftSocket = TEXT("gripslot_staff");
		RightSocket = TEXT("gripslot_book");
		break;
	default:
		break;
	}

	const float Row = JoAFigure::PaletteRowFor(UnitType);
	SetHeldItem(
		LeftHandItem,
		Mesh,
		LeftSocket,
		LoadHeldMesh(this, LeftName),
		TileIndex,
		Row);
	SetHeldItem(
		RightHandItem,
		Mesh,
		RightSocket,
		LoadHeldMesh(this, RightName),
		TileIndex,
		Row);

	/* Run swings the bow hand. Force roll 90
	 * for that clip only. */
	if (UnitType == EUnitType::Archer
		&& PlayingBodyAnim == EFigureAnim::Run)
	{
		LeftHandItem->SetRelativeRotation(
			FRotator(0.f, 0.f, 90.f));
	}
}

void AJoA_Figure::ApplyStowedItems()
{
	if (!Mesh
		|| !LeftHandItem
		|| !RightHandItem
		|| !BackWeaponItem
		|| !BackShieldItem
		|| !GroundBookItem)
	{
		return;
	}

	if (!Mesh->GetSkeletalMeshAsset())
	{
		return;
	}

	HideHeldProp(LeftHandItem);
	HideHeldProp(RightHandItem);
	HideHeldProp(BackWeaponItem);
	HideHeldProp(BackShieldItem);
	HideHeldProp(GroundBookItem);

	float TileIndex = JoAAtlas::WhiteTile;
	if (OwningPlayerIndex == FrenchSeat)
	{
		TileIndex = JoAAtlas::BlueTile;
	}
	else if (OwningPlayerIndex == EnglishSeat)
	{
		TileIndex = JoAAtlas::RedTile;
	}

	const float Row = JoAFigure::PaletteRowFor(UnitType);
	const FName BackWeaponSocket(
		TEXT("backslot_weapon"));
	const FName BackShieldSocket(
		TEXT("backslot_shield"));

	const TCHAR* WeaponName = nullptr;
	const TCHAR* ShieldName = nullptr;
	/* Closed book only for sit/lie. Flag
	 * carry hides the book completely. */
	bool bGroundBook = false;

	switch (UnitType)
	{
	case EUnitType::HeavyKnight:
		WeaponName = TEXT("sword_1handed");
		ShieldName = TEXT("shield_badge");
		break;
	case EUnitType::LightInfantry:
		WeaponName = TEXT("axe_1handed");
		break;
	case EUnitType::Archer:
		WeaponName = TEXT("bow_withString");
		break;
	case EUnitType::Medic:
		WeaponName = TEXT("staff");
		bGroundBook = (HeldFlag == nullptr);
		break;
	default:
		break;
	}

	SetHeldItem(
		BackWeaponItem,
		Mesh,
		BackWeaponSocket,
		LoadHeldMesh(this, WeaponName),
		TileIndex,
		Row);
	SetHeldItem(
		BackShieldItem,
		Mesh,
		BackShieldSocket,
		LoadHeldMesh(this, ShieldName),
		TileIndex,
		Row);

	if (!bGroundBook)
	{
		return;
	}

	/* PIE: GroundBookItem Relative + Local
	 * Space gizmo while mage sits/lies. */
	const FVector BookLoc(-32.f, 34.f, 5.f);
	const FRotator BookRot(88.f, 180.f, -30.f);

	GroundBookItem->AttachToComponent(
		SceneRoot,
		FAttachmentTransformRules::
			SnapToTargetNotIncludingScale);
	GroundBookItem->SetStaticMesh(
		LoadHeldMesh(this, TEXT("spellbook_closed")));
	GroundBookItem->SetRelativeLocation(BookLoc);
	GroundBookItem->SetRelativeRotation(BookRot);
	JoAFigure::MatchHeldScale(GroundBookItem, Mesh);
	GroundBookItem->SetVisibility(true, true);
	GroundBookItem->SetHiddenInGame(false);
	JoAAtlas::ApplyPalette(
		GroundBookItem,
		TileIndex,
		Row);
}
