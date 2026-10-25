// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class UMaterialInterface;
class UPrimitiveComponent;

/**
 * Atlas tiles and character palette rows.
 * Tile 0 white, 1 black, 2 blue, 3 red.
 * CheckerTile alternates white and black.
 * Data 0 is the tile. Data 1 is 1 when the
 * mesh reads a palette row. Data 2 is that row.
 * Row 0 knight, 1 barbarian, 2 mage, 3 ranger.
 * Row 4 is the brown wood cell.
 * Row 5 is the crate face.
 */
namespace JoAAtlas
{
	inline constexpr float WhiteTile = 0.f;
	inline constexpr float BlackTile = 1.f;
	inline constexpr float BlueTile = 2.f;
	inline constexpr float RedTile = 3.f;
	inline constexpr float CheckerTile = -1.f;
	inline constexpr float KnightRow = 0.f;
	inline constexpr float BarbarianRow = 1.f;
	inline constexpr float MageRow = 2.f;
	inline constexpr float RangerRow = 3.f;
	inline constexpr float WoodRow = 4.f;
	inline constexpr float CrateRow = 5.f;
	inline constexpr int32 TileDataIndex = 0;
	inline constexpr int32 UsePaletteDataIndex = 1;
	inline constexpr int32 PaletteRowDataIndex = 2;

	JOA_ARENA_API UMaterialInterface* GetMaterial();

	JOA_ARENA_API void ApplyTile(
		UPrimitiveComponent* Primitive,
		float TileIndex);

	/* Tile stays stored for the later team tint. */
	JOA_ARENA_API void ApplyPalette(
		UPrimitiveComponent* Primitive,
		float TileIndex,
		float PaletteRow);
}
