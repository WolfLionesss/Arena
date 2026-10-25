// Copyright Epic Games, Inc. All Rights Reserved.

#include "Player/Units/JoA_Figure.h"

// --- Engine ---
#include "GameFramework/PlayerController.h"

// --- GAME ---
#include "Match/JoA_GameMode.h"
#include "Player/JoA_PlayerController.h"
#include "Player/JoA_PlayerState.h"
#include "Board/JoA_Flag.h"
#include "Board/JoA_GridManager.h"
#include "Player/JoA_Nation.h"
#include "Player/Units/JoA_FigureHelpers.h"
#include "Player/Units/JoA_FigureRegistry.h"

// --- GAS ---
#include "GAS/Abilities/JoA_GAS_FlagAbility.h"
#include "GAS/Effects/JoA_GAS_PromotionEffect.h"
#include "GAS/JoA_GAS_AbilitySystemComponent.h"
#include "GAS/JoA_GAS_GameplayTags.h"

// --- Components ---
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"

// --- Net ---
#include "Net/Core/PushModel/PushModel.h"

namespace
{
	const float MeshImportScale = 0.5f;
}

void AJoA_Figure::OnRep_HeldFlag()
{
	if (HeldFlag)
	{
		HeldFlag->AttachToHolder(this);
	}

	RefreshHeldItemPresentation();
	if (PlayingBodyAnim == EFigureAnim::Idle)
	{
		PlayBodyAnim(EFigureAnim::Idle, true);
	}

	if (UJoA_FigureRegistry* Registry =
		UJoA_FigureRegistry::Get(this))
	{
		Registry->NotifyBoardChanged();
	}
}

void AJoA_Figure::OnRep_bPromoted()
{
	ApplyPromotedVisual();
}

AJoA_Flag* AJoA_Figure::FindPickupFlag() const
{
	if (IsDead() || HeldFlag || !IsAllowedToAct())
	{
		return nullptr;
	}

	if (AJoA_Flag* OnCell = AJoA_Flag::FindGroundedAt(
			this,
			CellX,
			CellY))
	{
		return OnCell;
	}

	const FIntPoint Front =
		GetStepTarget(EStepDirection::Forward);
	return AJoA_Flag::FindGroundedAt(
		this,
		Front.X,
		Front.Y);
}

bool AJoA_Figure::CanPickupFlag() const
{
	return FindPickupFlag() != nullptr;
}

bool AJoA_Figure::CanDropFlag() const
{
	return !IsDead() && HeldFlag && IsAllowedToAct();
}

float AJoA_Figure::GetHeldFlagSpendPenalty() const
{
	return HeldFlag ? 1.f : 0.f;
}

bool AJoA_Figure::WantsAutoPickupFlag() const
{
	const AJoA_PlayerState* OwnerState =
		OwningPlayerState;
	const APlayerController* OwnerController =
		OwnerState
			? OwnerState->GetPlayerController()
			: nullptr;
	const AJoA_PlayerController* ArenaController =
		Cast<AJoA_PlayerController>(OwnerController);
	return ArenaController
		&& ArenaController->bAutoPickupFlag;
}

bool AJoA_Figure::ShouldDeferFlagArrival(
	const FIntPoint& Cell) const
{
	/* A step leaves the drop cell, so the
	 * suppress flag is about to clear. */
	const bool bSameCell =
		Cell.X == CellX && Cell.Y == CellY;
	if ((bSuppressFlagAutoPickup && bSameCell)
		|| HeldFlag
		|| IsDead()
		|| !IsAllowedToAct()
		|| !AbilitySystemComponent
		|| !WantsAutoPickupFlag())
	{
		return false;
	}

	return AJoA_Flag::FindGroundedAt(
		this,
		Cell.X,
		Cell.Y) != nullptr;
}

void AJoA_Figure::TryAutoPickupFlag()
{
	if (!HasAuthority()
		|| bSuppressFlagAutoPickup
		|| !WantsAutoPickupFlag()
		|| HeldFlag
		|| IsDead()
		|| !IsAllowedToAct()
		|| !AbilitySystemComponent)
	{
		return;
	}

	if (!AJoA_Flag::FindGroundedAt(this, CellX, CellY))
	{
		return;
	}

	AbilitySystemComponent->TryActivateAbilityByInputID(
		UJoA_GAS_FlagAbility::StaticClass(),
		static_cast<int32>(EFlagCommand::Pickup));
}

void AJoA_Figure::SetHeldFlag(AJoA_Flag* Flag)
{
	if (!HasAuthority() || HeldFlag == Flag)
	{
		return;
	}

	HeldFlag = Flag;
	MARK_PROPERTY_DIRTY_FROM_NAME(
		AJoA_Figure,
		HeldFlag,
		this);
	ApplyFlagCarryEffect();
	RefreshHeldItemPresentation();
	if (PlayingBodyAnim == EFigureAnim::Idle)
	{
		PlayBodyAnim(EFigureAnim::Idle, true);
	}
	ForceNetUpdate();

	if (UJoA_FigureRegistry* Registry =
		UJoA_FigureRegistry::Get(this))
	{
		Registry->NotifyBoardChanged();
	}
}

void AJoA_Figure::ApplyFlagCarryEffect()
{
}

void AJoA_Figure::DropHeldFlag()
{
	if (!HasAuthority() || !HeldFlag)
	{
		return;
	}

	AJoA_Flag* Flag = HeldFlag;
	Flag->SetBoardCell(CellX, CellY);
	Flag->DropOnBoard();
	SetHeldFlag(nullptr);
}

void AJoA_Figure::NotifyFlagCarried()
{
	if (AJoA_GameMode* Mode = GetArenaGameMode())
	{
		Mode->NotifyFlagCarried(this);
	}
}

void AJoA_Figure::TryPromoteOnEnemyHome()
{
	if (!HasAuthority()
		|| bPromoted
		|| IsDead()
		|| HeldFlag
		|| OwningPlayerIndex == INDEX_NONE)
	{
		return;
	}

	const int32 EnemyHome =
		OwningPlayerIndex == FrenchSeat
			? 0
			: GridSize - 1;
	if (CellX != EnemyHome)
	{
		return;
	}

	ApplyPromotion();
}

void AJoA_Figure::ApplyPromotion()
{
	if (!HasAuthority()
		|| !AbilitySystemComponent
		|| !AbilitySystemComponent->
			IsAbilitySystemDataInitialized())
	{
		return;
	}

	FGameplayEffectContextHandle Context =
		AbilitySystemComponent->MakeEffectContext();
	Context.AddSourceObject(this);
	const FGameplayEffectSpecHandle Spec =
		AbilitySystemComponent->MakeOutgoingSpec(
			UJoA_GAS_PromotionEffect::StaticClass(),
			1.f,
			Context);
	if (!Spec.IsValid() || !Spec.Data.IsValid())
	{
		return;
	}

	bPromoted = true;
	MARK_PROPERTY_DIRTY_FROM_NAME(
		AJoA_Figure,
		bPromoted,
		this);
	ForceNetUpdate();
	ApplyPromotedVisual();

	AbilitySystemComponent->ApplyGameplayEffectSpecToSelf(
		*Spec.Data.Get());

	const float Bonus = PromotionVitalBonus;
	if (UsesStamina())
	{
		ApplyVitalChange(
			0.f,
			Bonus,
			0.f,
			0.f,
			Bonus,
			0.f);
	}
	else
	{
		ApplyVitalChange(
			0.f,
			Bonus,
			0.f,
			0.f,
			0.f,
			Bonus);
	}

	UE_LOG(
		LogTemp,
		Log,
		TEXT("Seat %d figure promoted."),
		OwningPlayerIndex);
}

void AJoA_Figure::ApplyBodyScale()
{
	if (!Mesh)
	{
		return;
	}

	const float Scale = MeshImportScale
		* (bPromoted
			? PromotionMeshMultiplier
			: 1.f);
	Mesh->SetRelativeScale3D(FVector(Scale));
	JoAFigure::MatchHeldScale(LeftHandItem, Mesh);
	JoAFigure::MatchHeldScale(RightHandItem, Mesh);
}

void AJoA_Figure::ApplyPromotedVisual()
{
	ApplyBodyScale();
	if (!AbilitySystemComponent || !bPromoted)
	{
		return;
	}

	if (!AbilitySystemComponent->HasMatchingGameplayTag(
			TAG_JoA_GAS_State_Promoted))
	{
		AbilitySystemComponent->AddLooseGameplayTag(
			TAG_JoA_GAS_State_Promoted);
	}
}
