// Copyright Epic Games, Inc. All Rights Reserved.

#include "Board/JoA_Atlas.h"

// --- Components ---
#include "Components/PrimitiveComponent.h"

// --- Materials ---
#include "Materials/MaterialInterface.h"

namespace JoAAtlas
{
	void AssignMaterial(UPrimitiveComponent* Primitive)
	{
		UMaterialInterface* Material = GetMaterial();
		if (Material)
		{
			Primitive->SetMaterial(0, Material);
		}
	}

	UMaterialInterface* GetMaterial()
	{
		static TWeakObjectPtr<UMaterialInterface> Cached;
		if (Cached.IsValid())
		{
			return Cached.Get();
		}

		UMaterialInterface* Material =
			LoadObject<UMaterialInterface>(
				nullptr,
				TEXT("/Game/Materials/M_Atlas.M_Atlas"),
				nullptr,
				LOAD_NoWarn);
		if (Material)
		{
			Cached = Material;
			return Material;
		}

		static bool bLogged = false;
		if (!bLogged)
		{
			bLogged = true;
			UE_LOG(
				LogTemp,
				Warning,
				TEXT("Missing /Game/Materials/M_Atlas."));
		}
		return nullptr;
	}

	void ApplyTile(
		UPrimitiveComponent* Primitive,
		float TileIndex)
	{
		if (!Primitive)
		{
			return;
		}

		AssignMaterial(Primitive);
		Primitive->SetCustomPrimitiveDataFloat(
			TileDataIndex,
			TileIndex);
		// Index 1 stays 0 so this mesh reads tiles.
		Primitive->SetCustomPrimitiveDataFloat(
			UsePaletteDataIndex,
			0.f);
		Primitive->SetCustomPrimitiveDataFloat(
			PaletteRowDataIndex,
			0.f);
	}

	void ApplyPalette(
		UPrimitiveComponent* Primitive,
		float TileIndex,
		float PaletteRow)
	{
		if (!Primitive)
		{
			return;
		}

		AssignMaterial(Primitive);
		Primitive->SetCustomPrimitiveDataFloat(
			TileDataIndex,
			TileIndex);
		Primitive->SetCustomPrimitiveDataFloat(
			UsePaletteDataIndex,
			1.f);
		Primitive->SetCustomPrimitiveDataFloat(
			PaletteRowDataIndex,
			PaletteRow);
	}
}
