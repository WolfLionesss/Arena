// Copyright Epic Games, Inc. All Rights Reserved.

#include "Player/Units/JoA_Figure.h"

// --- Engine ---
#include "Engine/World.h"

// --- GAME ---
#include "Match/JoA_GameMode.h"
#include "Player/JoA_PlayerState.h"
#include "Board/JoA_Arrow.h"
#include "Board/JoA_Flag.h"
#include "Board/JoA_GridManager.h"
#include "Player/Units/JoA_FigureHelpers.h"
#include "Player/Units/JoA_FigureRegistry.h"
#include "UI/JoA_FloatingText.h"

// --- GAS ---
#include "GAS/JoA_GAS_CombatSet.h"
#include "GAS/JoA_GAS_HealthSet.h"
#include "GAS/JoA_GAS_ManaSet.h"
#include "GAS/JoA_GAS_StaminaSet.h"

namespace
{
	float HealApplied(
		float HealAmount,
		float TargetHealth,
		float TargetMaxHealth)
	{
		const float Room = TargetMaxHealth - TargetHealth;
		return FMath::Min(
			HealAmount,
			FMath::Max(Room, 0.f));
	}
}

bool AJoA_Figure::IsCellReachable(FIntPoint Cell) const
{
	return IsCellReachableFacing(Cell, Facing);
}

bool AJoA_Figure::IsCellReachableFacing(
	FIntPoint Cell,
	EFacing InFacing) const
{
	const FUnitStats Defaults =
		FUnitStats::GetDefaults(UnitType);
	const int32 MaxSteps =
		Defaults.ReachPolicy == EReachPolicy::Archer
			? (GridSize - 1)
			: Defaults.Range;
	return UJoA_CombatLibrary::IsCellInFront(
		this,
		FIntPoint(CellX, CellY),
		Cell,
		InFacing,
		MaxSteps,
		Defaults.ReachPolicy);
}

bool AJoA_Figure::CanAttackTarget(
	const AJoA_Figure* Target) const
{
	return CanAttackTargetFacing(Target, Facing);
}

bool AJoA_Figure::CanAttackTargetFacing(
	const AJoA_Figure* Target,
	EFacing InFacing) const
{
	const UJoA_GAS_HealthSet* HealthSet = GetHealthSet();
	const UJoA_GAS_StaminaSet* StaminaSet = GetStaminaSet();
	const UJoA_GAS_CombatSet* CombatSet = GetCombatSet();
	const UJoA_GAS_HealthSet* TargetHealth = Target
		? Target->GetHealthSet()
		: nullptr;
	const UJoA_GAS_CombatSet* TargetCombat = Target
		? Target->GetCombatSet()
		: nullptr;
	const bool bCanAttack =
		FUnitStats::GetDefaults(UnitType).bCanAttack;
	if (!Target
		|| Target == this
		|| HeldFlag
		|| !bCanAttack)
	{
		return false;
	}

	if (OwningPlayerIndex == INDEX_NONE
		|| Target->OwningPlayerIndex == INDEX_NONE
		|| OwningPlayerIndex == Target->OwningPlayerIndex
		|| !IsAllowedToAct())
	{
		return false;
	}

	/* A heal can leave the client health read
	 * at 0 while the figure is still alive.
	 * Death is the replicated flag. */
	if (HasAuthority())
	{
		if (!HealthSet
			|| !StaminaSet
			|| !CombatSet
			|| !TargetHealth
			|| !TargetCombat
			|| HealthSet->GetHealth() <= 0.f
			|| TargetHealth->GetHealth() <= 0.f)
		{
			return false;
		}
	}
	else if (IsDead() || Target->IsDead())
	{
		return false;
	}

	if (UsesStamina()
		&& StaminaSet
		&& CombatSet
		&& TargetCombat)
	{
		float AttackCost = 0.f;
		float DefenseCost = 0.f;
		UJoA_CombatLibrary::GetStaminaCosts(
			CombatSet->GetAttack(),
			TargetCombat->GetDefense(),
			AttackCost,
			DefenseCost);
		if (StaminaSet->GetStamina() < AttackCost)
		{
			return false;
		}
	}

	return IsCellReachableFacing(
		FIntPoint(Target->CellX, Target->CellY),
		InFacing);
}

bool AJoA_Figure::CanHealTarget(
	const AJoA_Figure* Target) const
{
	return CanHealTargetFacing(Target, Facing);
}

bool AJoA_Figure::CanHealTargetFacing(
	const AJoA_Figure* Target,
	EFacing InFacing) const
{
	const UJoA_GAS_HealthSet* HealthSet = GetHealthSet();
	const UJoA_GAS_ManaSet* ManaSet = GetManaSet();
	const UJoA_GAS_CombatSet* CombatSet = GetCombatSet();
	const UJoA_GAS_HealthSet* TargetHealth = Target
		? Target->GetHealthSet()
		: nullptr;
	const bool bCanHeal =
		FUnitStats::GetDefaults(UnitType).bCanHeal;
	if (!Target
		|| HeldFlag
		|| !HealthSet
		|| !ManaSet
		|| !CombatSet
		|| !TargetHealth
		|| !bCanHeal)
	{
		return false;
	}

	if (OwningPlayerIndex == INDEX_NONE
		|| Target->OwningPlayerIndex != OwningPlayerIndex
		|| HealthSet->GetHealth() <= 0.f
		|| !IsAllowedToAct())
	{
		return false;
	}

	const float HealAmount = CombatSet->GetHeal();
	const float Applied = HealApplied(
		HealAmount,
		TargetHealth->GetHealth(),
		TargetHealth->GetMaxHealth());
	if (HealAmount <= 0.f
		|| Applied <= 0.f
		|| ManaSet->GetMana() < Applied)
	{
		return false;
	}

	// His own square is not in front of him.
	if (Target == this)
	{
		return true;
	}

	const FIntPoint Cell(
		Target->CellX,
		Target->CellY);
	if (IsCellReachableFacing(Cell, InFacing))
	{
		return true;
	}

	/* Rank, file, or the diagonal he faces.
	 * Height 1 on the target is the last legal
	 * square. Height 2 cannot be healed. */
	return IsHealRowOpen(Cell, InFacing);
}

bool AJoA_Figure::IsHealRowOpen(
	FIntPoint Cell,
	EFacing InFacing) const
{
	const int32 DistX = Cell.X - CellX;
	const int32 DistY = Cell.Y - CellY;
	const bool bAxis =
		(DistX == 0) != (DistY == 0);
	const bool bDiagonal =
		DistX != 0
		&& FMath::Abs(DistX) == FMath::Abs(DistY);
	if (!bAxis && !bDiagonal)
	{
		return false;
	}

	/* The row used to be open to the board edge,
	 * so a medic healed from any distance. The
	 * reach stays at the unit range. */
	const int32 Steps = FMath::Max(
		FMath::Abs(DistX),
		FMath::Abs(DistY));
	if (Steps > FUnitStats::GetDefaults(UnitType).Range)
	{
		return false;
	}

	/* A diagonal heal needs the medic to face
	 * that line. Rank and file stay open. */
	if (bDiagonal)
	{
		const FIntPoint Wanted(
			FMath::Sign(DistX),
			FMath::Sign(DistY));
		if (Wanted != JoAFigure::ForwardDelta(InFacing))
		{
			return false;
		}
	}

	const AJoA_GridManager* Grid =
		AJoA_GameMode::GetGridManager(this);
	if (!Grid)
	{
		return false;
	}

	const int32 StepX = FMath::Sign(Cell.X - CellX);
	const int32 StepY = FMath::Sign(Cell.Y - CellY);
	int32 X = CellX + StepX;
	int32 Y = CellY + StepY;
	while (X != Cell.X || Y != Cell.Y)
	{
		FGridCell Between;
		if (!Grid->GetCell(X, Y, Between)
			|| Between.ObstacleHeight >= 1)
		{
			return false;
		}

		X += StepX;
		Y += StepY;
	}

	FGridCell Dest;
	if (!Grid->GetCell(Cell.X, Cell.Y, Dest)
		|| Dest.ObstacleHeight >= 2)
	{
		return false;
	}

	return true;
}

bool AJoA_Figure::CanDestroyObstacleAt(
	int32 X,
	int32 Y) const
{
	FIntPoint ObstacleCell = FIntPoint::ZeroValue;
	return FindDestroyableObstacle(ObstacleCell)
		&& ObstacleCell.X == X
		&& ObstacleCell.Y == Y;
}

bool AJoA_Figure::FindDestroyableObstacle(
	FIntPoint& OutCell) const
{
	const FUnitStats Defaults =
		FUnitStats::GetDefaults(UnitType);
	const UJoA_GAS_HealthSet* HealthSet = GetHealthSet();
	const bool bDead =
		!HealthSet || HealthSet->GetHealth() <= 0.f;
	if (!Defaults.bCanDestroyObstacles
		|| bDead
		|| HeldFlag
		|| !IsAllowedToAct())
	{
		return false;
	}

	return UJoA_CombatLibrary::FindFirstObstacleInFront(
		this,
		FIntPoint(CellX, CellY),
		Facing,
		Defaults.Range,
		OutCell);
}

bool AJoA_Figure::CanBuildObstacleAt(
	int32 X,
	int32 Y) const
{
	FIntPoint BuildCell = FIntPoint::ZeroValue;
	return FindBuildableCell(BuildCell)
		&& BuildCell.X == X
		&& BuildCell.Y == Y;
}

bool AJoA_Figure::FindBuildableCell(
	FIntPoint& OutCell) const
{
	return FindBuildableCellFacing(Facing, OutCell);
}

bool AJoA_Figure::FindBuildableCellFacing(
	EFacing InFacing,
	FIntPoint& OutCell) const
{
	const FUnitStats Defaults =
		FUnitStats::GetDefaults(UnitType);
	const UJoA_GAS_HealthSet* HealthSet = GetHealthSet();
	const bool bDead =
		IsDead()
		|| !HealthSet
		|| HealthSet->GetHealth() <= 0.f;
	if (!Defaults.bCanBuildObstacles
		|| bDead
		|| HeldFlag
		|| !IsAllowedToAct())
	{
		return false;
	}

	const AJoA_GridManager* Grid =
		AJoA_GameMode::GetGridManager(this);
	const UJoA_FigureRegistry* Registry =
		UJoA_FigureRegistry::Get(this);
	if (!Grid || !Registry)
	{
		return false;
	}

	const FIntPoint Cell = FIntPoint(CellX, CellY)
		+ JoAFigure::StepDelta(InFacing, EStepDirection::Forward);
	FGridCell GridCell;
	if (!Grid->GetCell(Cell.X, Cell.Y, GridCell)
		|| GridCell.ObstacleHeight >= 2)
	{
		return false;
	}

	/* A unit or a grounded flag already owns the
	 * square. Height 0 and 1 are both free to raise. */
	if (Registry->FindFigureAt(Cell.X, Cell.Y)
		|| AJoA_Flag::FindGroundedAt(
			this,
			Cell.X,
			Cell.Y))
	{
		return false;
	}

	OutCell = Cell;
	return true;
}

FAttackResult AJoA_Figure::ResolveAttack(
	AJoA_Figure* Target)
{
	const UJoA_GAS_CombatSet* CombatSet = GetCombatSet();
	const UJoA_GAS_CombatSet* TargetCombat = Target
		? Target->GetCombatSet()
		: nullptr;
	FAttackResult Result;
	if (!HasAuthority()
		|| !Target
		|| Target == this
		|| !CombatSet
		|| !TargetCombat)
	{
		return Result;
	}

	if (!CanAttackTarget(Target))
	{
		return Result;
	}

	Result.bInRange = true;

	const float AttackValue = CombatSet->GetAttack();
	const float DefenseValue = TargetCombat->GetDefense();
	Result.PreviousComboAttacks = 0;
	if (OwningPlayerState)
	{
		Result.PreviousComboAttacks =
			OwningPlayerState->GetComboAttacksAgainst(Target);
	}
	Result.HitChance = UJoA_CombatLibrary::CalculateHitChance(
		AttackValue,
		DefenseValue,
		Result.PreviousComboAttacks);
	UJoA_CombatLibrary::GetStaminaCosts(
		AttackValue,
		DefenseValue,
		Result.AttackerStaminaCost,
		Result.DefenderStaminaCost);
	if (!Target->UsesStamina())
	{
		Result.DefenderStaminaCost = 0.f;
	}

	/* [0, 100). A 0% chance never hits.
	 * A 100% chance always hits. */
	if (bForceCombatMiss)
	{
		Result.bHit = false;
	}
	else if (bForceCombatHit)
	{
		Result.bHit = true;
	}
	else
	{
		const float Roll =
			FMath::FRandRange(0.f, 100.f);
		Result.bHit = Roll < Result.HitChance;
	}
	Result.bResolved = true;
	MarkActiveThisTurn();
	if (Result.bHit)
	{
		Result.HealthDamage = AttackValue;
	}

	ApplyVitalChange(
		0.f,
		0.f,
		Result.AttackerStaminaCost,
		0.f);
	if (OwningPlayerState)
	{
		OwningPlayerState->CommitComboAttack(Target);
	}

	ClearDeferred();
	Deferred.Kind = EDeferredResolve::Attack;
	Deferred.Target = Target;
	Deferred.Damage = Result.HealthDamage;
	Deferred.TargetStamina = Result.DefenderStaminaCost;
	Deferred.bHit = Result.bHit;
	Deferred.bEndTurn = true;
	Deferred.bGoodNews = Result.bHit;
	Deferred.NewsSeat = OwningPlayerIndex;
	Deferred.TextKind = Result.bHit
		? ECombatTextKind::Hit
		: ECombatTextKind::Miss;
	Deferred.TextAmount = FMath::RoundToInt(
		Result.HealthDamage);
	bResolvePending = true;

	const bool bBow = UnitType == EUnitType::Archer;
	MulticastBodyAnim(
		bBow
			? EFigureAnim::Shoot
			: EFigureAnim::Attack);

	float Delay = BodyAnimSeconds(
		bBow
			? EFigureAnim::Shoot
			: EFigureAnim::Attack) * 0.55f;
	if (bBow)
	{
		const FVector From =
			GetActorLocation() + FVector(0.f, 0.f, 40.f);
		const FVector Aim =
			Target->GetActorLocation()
			+ FVector(0.f, 0.f, 40.f);
		const FVector Dir =
			(Aim - From).GetSafeNormal();
		const FVector To = Result.bHit
			? Aim
			: From + Dir * 4000.f;
		MulticastLaunchArrow(From, To, Result.bHit);
		/* Miss flies past the board. The result
		 * still applies when the shot reaches
		 * the target, not the far end. */
		Delay = FVector::Dist(From, Aim)
			/ AJoA_Arrow::FlightSpeed;
	}

	ArmDeferred(Delay);
	return Result;
}

FHealResult AJoA_Figure::ResolveHeal(AJoA_Figure* Target)
{
	const UJoA_GAS_HealthSet* HealthSet = GetHealthSet();
	const UJoA_GAS_ManaSet* ManaSet = GetManaSet();
	const UJoA_GAS_CombatSet* CombatSet = GetCombatSet();
	const UJoA_GAS_HealthSet* TargetHealth = Target
		? Target->GetHealthSet()
		: nullptr;
	FHealResult Result;
	if (!HasAuthority()
		|| !Target
		|| !HealthSet
		|| !ManaSet
		|| !CombatSet
		|| !TargetHealth)
	{
		return Result;
	}

	const bool bCanHeal =
		FUnitStats::GetDefaults(UnitType).bCanHeal;
	const bool bSamePlayer =
		OwningPlayerIndex != INDEX_NONE
		&& Target->OwningPlayerIndex == OwningPlayerIndex;
	const FIntPoint HealCell(
		Target->CellX,
		Target->CellY);
	Result.bInRange = Target == this
		|| IsCellReachable(HealCell)
		|| IsHealRowOpen(HealCell, Facing);
	if (!bCanHeal
		|| HeldFlag
		|| !bSamePlayer
		|| HealthSet->GetHealth() <= 0.f
		|| !Result.bInRange
		|| !IsAllowedToAct())
	{
		return Result;
	}

	const float HealAmount = CombatSet->GetHeal();
	const float Applied = HealApplied(
		HealAmount,
		TargetHealth->GetHealth(),
		TargetHealth->GetMaxHealth());
	if (HealAmount <= 0.f || Applied <= 0.f)
	{
		return Result;
	}

	if (ManaSet->GetMana() < Applied)
	{
		Result.bInsufficientMana = true;
		return Result;
	}

	Result.HealthRestored = Applied;
	Result.ManaSpent = Applied;
	Result.bResolved = true;
	MarkActiveThisTurn();

	ClearDeferred();
	Deferred.Kind = EDeferredResolve::Heal;
	Deferred.Target = Target;
	Deferred.Healing = Applied;
	Deferred.ManaSpend = Applied;
	Deferred.bEndTurn = true;
	Deferred.TextKind = ECombatTextKind::Heal;
	Deferred.TextAmount = FMath::RoundToInt(Applied);
	bResolvePending = true;
	MulticastBodyAnim(EFigureAnim::Heal);
	ArmDeferred(
		BodyAnimSeconds(EFigureAnim::Heal) * 0.85f);
	return Result;
}

bool AJoA_Figure::ResolveDestroyObstacle(int32 X, int32 Y)
{
	AJoA_GridManager* Grid =
		AJoA_GameMode::GetGridManager(this);
	if (!HasAuthority()
		|| !Grid
		|| !CanDestroyObstacleAt(X, Y))
	{
		return false;
	}

	FGridCell Cell;
	if (!Grid->GetCell(X, Y, Cell) || Cell.ObstacleHeight == 0)
	{
		return false;
	}

	MarkActiveThisTurn();
	ClearDeferred();
	Deferred.Kind = EDeferredResolve::Destroy;
	Deferred.ObstacleX = X;
	Deferred.ObstacleY = Y;
	Deferred.ObstacleHeight = static_cast<uint8>(
		Cell.ObstacleHeight - 1);
	Deferred.bEndTurn = true;
	bResolvePending = true;
	MulticastBodyAnim(EFigureAnim::Attack);
	ArmDeferred(
		BodyAnimSeconds(EFigureAnim::Attack) * 0.55f);
	return true;
}

bool AJoA_Figure::ResolveBuildObstacle()
{
	FIntPoint Cell = FIntPoint::ZeroValue;
	AJoA_GridManager* Grid =
		AJoA_GameMode::GetGridManager(this);
	if (!HasAuthority()
		|| !Grid
		|| !FindBuildableCell(Cell))
	{
		return false;
	}

	FGridCell GridCell;
	if (!Grid->GetCell(Cell.X, Cell.Y, GridCell)
		|| GridCell.ObstacleHeight >= 2)
	{
		return false;
	}

	MarkActiveThisTurn();
	ClearDeferred();
	Deferred.Kind = EDeferredResolve::Build;
	Deferred.ObstacleX = Cell.X;
	Deferred.ObstacleY = Cell.Y;
	Deferred.ObstacleHeight = static_cast<uint8>(
		GridCell.ObstacleHeight + 1);
	Deferred.bEndTurn = true;
	bResolvePending = true;
	MulticastBodyAnim(EFigureAnim::Interact);
	ArmDeferred(
		BodyAnimSeconds(EFigureAnim::Interact) * 0.85f);
	return true;
}

void AJoA_Figure::MulticastCombatText_Implementation(
	ECombatTextKind Kind,
	int32 Amount)
{
	UWorld* World = GetWorld();
	if (!World || GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	FText Message;
	FLinearColor Color = FLinearColor::White;
	switch (Kind)
	{
	case ECombatTextKind::Hit:
		Message = FText::FromString(
			FString::Printf(TEXT("Hit %d"), Amount));
		Color = FLinearColor(1.f, 0.1f, 0.1f);
		break;
	case ECombatTextKind::Heal:
		Message = FText::FromString(
			FString::Printf(TEXT("Heal %d"), Amount));
		Color = FLinearColor(0.55f, 1.f, 0.55f);
		break;
	default:
		Message = FText::FromString(TEXT("Miss"));
		break;
	}

	AJoA_FloatingText* Text =
		World->SpawnActor<AJoA_FloatingText>(
			AJoA_FloatingText::StaticClass(),
			GetActorTransform());
	if (Text)
	{
		Text->Show(this, Message, Color);
	}
}

void AJoA_Figure::MulticastLaunchArrow_Implementation(
	FVector From,
	FVector To,
	bool bHaltAtEnd)
{
	LaunchMatchArrow(From, To, bHaltAtEnd);
}

void AJoA_Figure::LaunchMatchArrow(
	const FVector& From,
	const FVector& To,
	bool bHaltAtEnd)
{
	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	if (AJoA_Arrow* Shot = AJoA_Arrow::Get(GetWorld()))
	{
		Shot->Launch(From, To, bHaltAtEnd);
	}
}
