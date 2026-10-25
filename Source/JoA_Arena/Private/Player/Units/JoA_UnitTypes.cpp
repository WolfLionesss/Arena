// Copyright Epic Games, Inc. All Rights Reserved.

#include "Player/Units/JoA_UnitTypes.h"

FUnitStats FUnitStats::GetDefaults(EUnitType Type)
{
	FUnitStats Stats;
	Stats.UnitType = Type;
	Stats.Health = 10.f;
	Stats.Stamina = 10.f;
	Stats.Mana = 0.f;
	Stats.Attack = 0.f;
	Stats.Defense = 0.f;
	Stats.Heal = 0.f;
	Stats.Range = 1;
	Stats.ReachPolicy = EReachPolicy::StopAtObstacle;
	Stats.bCanDestroyObstacles = false;
	Stats.bCanBuildObstacles = false;
	Stats.bCanJump = false;
	Stats.bCanAttack = true;
	Stats.bCanHeal = false;
	Stats.bUsesStamina = true;

	switch (Type)
	{
	case EUnitType::HeavyKnight:
		Stats.Attack = 2.f;
		Stats.Defense = 2.f;
		Stats.Range = 1;
		Stats.bCanDestroyObstacles = true;
		break;

	case EUnitType::LightInfantry:
		Stats.Attack = 1.f;
		Stats.Defense = 1.f;
		Stats.Range = 2;
		Stats.ReachPolicy = EReachPolicy::IncludeHeightOne;
		Stats.bCanBuildObstacles = true;
		Stats.bCanJump = true;
		break;

	case EUnitType::Archer:
		Stats.Attack = 2.f;
		Stats.Defense = 1.f;
		Stats.ReachPolicy = EReachPolicy::Archer;
		Stats.Range = 0;
		Stats.bCanJump = true;
		break;

	case EUnitType::Medic:
		Stats.Attack = 0.f;
		Stats.Defense = 0.f;
		Stats.Heal = 2.f;
		Stats.Stamina = 0.f;
		Stats.Mana = 10.f;
		Stats.Range = 2;
		Stats.ReachPolicy = EReachPolicy::IncludeHeightOne;
		Stats.bCanAttack = false;
		Stats.bCanHeal = true;
		Stats.bUsesStamina = false;
		Stats.bCanJump = true;
		break;

	default:
		break;
	}

	// A figure starts full.
	Stats.MaxHealth = Stats.Health;
	Stats.MaxStamina = Stats.Stamina;
	Stats.MaxMana = Stats.Mana;

	return Stats;
}
