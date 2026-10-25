// Copyright Epic Games, Inc. All Rights Reserved.

#include "Player/Units/JoA_Figure.h"

// --- Engine ---
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "GameFramework/Controller.h"
#include "TimerManager.h"

// --- GAME ---
#include "Match/JoA_GameMode.h"
#include "Player/JoA_PlayerState.h"
#include "Board/JoA_Atlas.h"
#include "Board/JoA_Flag.h"
#include "Match/JoA_CookedContent.h"
#include "Player/JoA_Nation.h"
#include "Player/JoA_ViewCamera.h"
#include "Player/Units/JoA_FigureHelpers.h"
#include "Player/Units/JoA_FigureRegistry.h"

// --- GAS ---
#include "GAS/Abilities/JoA_GAS_GameplayAbility.h"
#include "GAS/JoA_GAS_AbilitySystemComponent.h"
#include "GAS/JoA_GAS_HealthSet.h"

// --- Components ---
#include "Components/SceneComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"

// --- Net ---
#include "Net/Core/PushModel/PushModel.h"
#include "Net/UnrealNetwork.h"

#if UE_WITH_IRIS
#include "Iris/ReplicationSystem/ReplicationFragment.h"
#endif

namespace
{
	void SetupHeldPropComp(
		UStaticMeshComponent* Item)
	{
		if (!Item)
		{
			return;
		}

		Item->SetMobility(
			EComponentMobility::Movable);
		Item->SetCollisionEnabled(
			ECollisionEnabled::NoCollision);
		Item->SetCastShadow(false);
		Item->SetVisibleInRayTracing(false);
	}
}

bool AJoA_Figure::bForceCombatHit = false;
bool AJoA_Figure::bForceCombatMiss = false;
int32 AJoA_Figure::ForcedBenchStart = -1;

AJoA_Figure::AJoA_Figure()
{
	/* Tick runs only while a remote mesh slides.
	 * A resting figure stays asleep. */
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;

	/* Both players see every figure. Movement is
	 * placement on the board, not character movement. */
	bReplicates = true;
	bAlwaysRelevant = true;
	SetReplicateMovement(false);

	/* Pawn default is 100 Hz. Idle figures do not
	 * need that poll. ForceNetUpdate on a change
	 * still sends the packet this frame. */
	SetNetUpdateFrequency(10.f);
	SetMinNetUpdateFrequency(2.f);

	/* A possessing controller would otherwise turn the
	 * actor with its control rotation, which hides the
	 * Facing yaw applied to the body. */
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	/* Default 64 sits in this mesh's head
	 * once the figure is the view target. */
	BaseEyeHeight = 0.f;
	bFindCameraComponentWhenViewTarget = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(
		TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	Mesh = CreateDefaultSubobject<USkeletalMeshComponent>(
		TEXT("Mesh"));
	Mesh->SetupAttachment(SceneRoot);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->SetCastShadow(false);
	Mesh->SetVisibleInRayTracing(false);
	Mesh->SetComponentTickEnabled(true);
	Mesh->bPauseAnims = false;
	Mesh->VisibilityBasedAnimTickOption =
		EVisibilityBasedAnimTickOption::
			AlwaysTickPoseAndRefreshBones;
	Mesh->SetAnimationMode(
		EAnimationMode::AnimationSingleNode);
	/* A post-process graph reads velocity and
	 * plays Run over the single-node clip. */
	Mesh->SetDisablePostProcessBlueprint(true);

	LeftHandItem =
		CreateDefaultSubobject<UStaticMeshComponent>(
			TEXT("LeftHandItem"));
	LeftHandItem->SetupAttachment(
		Mesh,
		TEXT("handslot_l"));
	SetupHeldPropComp(LeftHandItem);

	RightHandItem =
		CreateDefaultSubobject<UStaticMeshComponent>(
			TEXT("RightHandItem"));
	RightHandItem->SetupAttachment(
		Mesh,
		TEXT("handslot_r"));
	SetupHeldPropComp(RightHandItem);

	BackWeaponItem =
		CreateDefaultSubobject<UStaticMeshComponent>(
			TEXT("BackWeaponItem"));
	BackWeaponItem->SetupAttachment(
		Mesh,
		TEXT("backslot_weapon"));
	SetupHeldPropComp(BackWeaponItem);
	BackWeaponItem->SetVisibility(false, true);
	BackWeaponItem->SetHiddenInGame(true);

	BackShieldItem =
		CreateDefaultSubobject<UStaticMeshComponent>(
			TEXT("BackShieldItem"));
	BackShieldItem->SetupAttachment(
		Mesh,
		TEXT("backslot_shield"));
	SetupHeldPropComp(BackShieldItem);
	BackShieldItem->SetVisibility(false, true);
	BackShieldItem->SetHiddenInGame(true);

	GroundBookItem =
		CreateDefaultSubobject<UStaticMeshComponent>(
			TEXT("GroundBookItem"));
	GroundBookItem->SetupAttachment(SceneRoot);
	SetupHeldPropComp(GroundBookItem);
	GroundBookItem->SetVisibility(false, true);
	GroundBookItem->SetHiddenInGame(true);

	/* Import scale is about two cells tall.
	 * Half of that fits one cell. The origin
	 * is the feet, so Z stays on the board. */
	ApplyBodyScale();
	Mesh->SetRelativeLocation(FVector::ZeroVector);


	/* Mixed replication is set on the component.
	 * Attribute sets are granted when the owner is known. */
	AbilitySystemComponent = CreateDefaultSubobject<
		UJoA_GAS_AbilitySystemComponent>(
			TEXT("AbilitySystemComponent"));
}

void AJoA_Figure::GetLifetimeReplicatedProps(
	TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	// Push model
	FDoRepLifetimeParams Params;
	Params.bIsPushBased = true;
	DOREPLIFETIME_WITH_PARAMS_FAST(
		AJoA_Figure,
		UnitType,
		Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(
		AJoA_Figure,
		CellX,
		Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(
		AJoA_Figure,
		CellY,
		Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(
		AJoA_Figure,
		Facing,
		Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(
		AJoA_Figure,
		OwningPlayerIndex,
		Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(
		AJoA_Figure,
		OwningPlayerState,
		Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(
		AJoA_Figure,
		bIsDead,
		Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(
		AJoA_Figure,
		HeldFlag,
		Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(
		AJoA_Figure,
		bPromoted,
		Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(
		AJoA_Figure,
		NetBodyAnim,
		Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(
		AJoA_Figure,
		BodyCue,
		Params);
}

UAbilitySystemComponent*
	AJoA_Figure::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}

void AJoA_Figure::CalcCamera(
	float DeltaTime,
	FMinimalViewInfo& OutResult)
{
	AJoA_ViewCamera::ApplyToViewInfo(OutResult, this);
}

void AJoA_Figure::GetActorEyesViewPoint(
	FVector& OutLocation,
	FRotator& OutRotation) const
{
	const FTransform View =
		AJoA_ViewCamera::GetLocalViewTransform(this);
	OutLocation = View.GetLocation();
	OutRotation = View.Rotator();
}

FVector AJoA_Figure::GetPawnViewLocation() const
{
	return AJoA_ViewCamera::GetLocalViewTransform(this)
		.GetLocation();
}

#if UE_WITH_IRIS
void AJoA_Figure::RegisterReplicationFragments(
	UE::Net::FFragmentRegistrationContext& Context,
	UE::Net::EFragmentRegistrationFlags RegistrationFlags)
{
	Super::RegisterReplicationFragments(
		Context,
		RegistrationFlags);
}
#endif

void AJoA_Figure::OnConstruction(
	const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	ApplyBodyMesh();

	/* Game spawn still has default CellX/Y here.
	 * A snap to 0,0 then slides the English
	 * Heavy Knight onto 0,2. BeginPlay places. */
	const UWorld* World = GetWorld();
	if (!World || !World->IsGameWorld())
	{
		ApplyBoardTransform();
	}
}

void AJoA_Figure::BeginPlay()
{
	Super::BeginPlay();
	ApplyBodyMesh();

	/* Dedicated server has no view. The mesh
	 * would still refresh bones every frame. */
	if (Mesh && GetNetMode() == NM_DedicatedServer)
	{
		Mesh->SetComponentTickEnabled(false);
	}

	ApplyBoardTransform();

	/* Server and client both join here. On a client
	 * the replicated fields are applied before
	 * BeginPlay, so the seat and type are known. */
	if (UJoA_FigureRegistry* Registry =
		UJoA_FigureRegistry::Get(this))
	{
		Registry->RegisterFigure(this);
		BoardChangedHandle =
			Registry->OnBoardStateChanged.AddUObject(
				this,
				&AJoA_Figure::HandleBoardChanged);
	}

	if (bIsDead)
	{
		PlayBodyAnim(EFigureAnim::Death);
	}
	else
	{
		PlayBodyAnim(EFigureAnim::Idle);
	}
}

void AJoA_Figure::EndPlay(
	const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(
			VisualConfirmHandle);
		World->GetTimerManager().ClearTimer(
			BodyAnimHandle);
		World->GetTimerManager().ClearTimer(
			DeferredHandle);
	}

	if (UJoA_FigureRegistry* Registry =
		UJoA_FigureRegistry::Get(this))
	{
		Registry->OnBoardStateChanged.Remove(
			BoardChangedHandle);
		BoardChangedHandle.Reset();
		Registry->UnregisterFigure(this);
	}

	if (AbilitySystemComponent
		&& HealthChangedHandle.IsValid())
	{
		AbilitySystemComponent->
			GetGameplayAttributeValueChangeDelegate(
				UJoA_GAS_HealthSet::GetHealthAttribute())
			.Remove(HealthChangedHandle);
		HealthChangedHandle.Reset();
	}

	Super::EndPlay(EndPlayReason);
}

void AJoA_Figure::OnRep_UnitType()
{
	ApplyBodyMesh();
	if (HeldFlag)
	{
		HeldFlag->AttachToHolder(this);
	}
}

void AJoA_Figure::ApplyBodyMesh()
{
	if (!Mesh)
	{
		return;
	}

	USkeletalMesh* BodyMesh =
		UJoA_CookedContent::LoadBody(this, UnitType);
	if (BodyMesh)
	{
		Mesh->SetSkinnedAssetAndUpdate(BodyMesh);
	}

	ApplyTeamColor();
}

#if WITH_EDITOR
void AJoA_Figure::PostEditChangeProperty(
	FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	if (PropertyChangedEvent.GetPropertyName()
		== GET_MEMBER_NAME_CHECKED(
			AJoA_Figure,
			UnitType))
	{
		ApplyBodyMesh();
	}
}
#endif

void AJoA_Figure::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);

	/* Server only. Possession makes the Controller the
	 * owner, which Mixed replication expects. It is also
	 * the connection that may send ability requests. */
	if (!HasAuthority())
	{
		return;
	}

	if (!OwningPlayerState)
	{
		AJoA_PlayerState* OwnerState = nullptr;
		if (NewController)
		{
			OwnerState = NewController->
				GetPlayerState<AJoA_PlayerState>();
		}
		if (OwnerState)
		{
			SetOwningPlayer(OwnerState, OwningPlayerIndex);
		}
	}

	HandleControllerChanged();
}

void AJoA_Figure::UnPossessed()
{
	Super::UnPossessed();
	HandleControllerChanged();
}

void AJoA_Figure::OnRep_Controller()
{
	Super::OnRep_Controller();
	HandleControllerChanged();
}

void AJoA_Figure::HandleControllerChanged()
{
	if (AbilitySystemComponent
		&& AbilitySystemComponent->
			IsAbilitySystemDataInitialized())
	{
		/* The actor info caches the player controller.
		 * Without a refresh it keeps the old one. */
		AbilitySystemComponent->RefreshAbilityActorInfo();
	}

	RefreshAbilityInput();
}

void AJoA_Figure::RefreshAbilityInput()
{
	if (!AbilitySystemComponent)
	{
		return;
	}

	const FGameplayAbilityActorInfo* ActorInfo =
		AbilitySystemComponent->AbilityActorInfo.Get();
	if (!ActorInfo)
	{
		return;
	}

	for (const FGameplayAbilitySpec& Spec :
		AbilitySystemComponent->GetActivatableAbilities())
	{
		UJoA_GAS_GameplayAbility* Ability =
			Cast<UJoA_GAS_GameplayAbility>(
				Spec.GetPrimaryInstance());
		if (Ability)
		{
			Ability->RefreshInputBinding(ActorInfo, Spec);
		}
	}
}

void AJoA_Figure::OnRep_OwningPlayerState()
{
	/* Client actor info and loose tags. The server
	 * already granted the sets. Those replicate alone. */
	if (HasAuthority() || !OwningPlayerState)
	{
		return;
	}

	InitializeAbilitySystem();

	/* The HUD sorts figures into the two sides by this
	 * owner, which arrives after BeginPlay. */
	if (UJoA_FigureRegistry* Registry =
		UJoA_FigureRegistry::Get(this))
	{
		Registry->NotifyBoardChanged();
	}
}

void AJoA_Figure::OnHealthChanged(
	const FOnAttributeChangeData& Data)
{
	if (HasAuthority() && Data.NewValue <= 0.f)
	{
		HandleDeath();
	}
}

void AJoA_Figure::HandleDeath()
{
	if (!HasAuthority() || bIsDead)
	{
		return;
	}

	bIsDead = true;
	MARK_PROPERTY_DIRTY_FROM_NAME(
		AJoA_Figure,
		bIsDead,
		this);
	ForceNetUpdate();

	DropHeldFlag();

	/* The player has no selected figure until the
	 * next pick. The controller keeps no stale pawn. */
	if (AController* OldController = GetController())
	{
		OldController->UnPossess();
	}

	ApplyDeathState();

	if (AJoA_GameMode* Mode = GetArenaGameMode())
	{
		Mode->NotifyFigureDied(this);
	}
}

void AJoA_Figure::OnRep_IsDead()
{
	ApplyDeathState();
}

void AJoA_Figure::ApplyDeathState()
{
	bOnBench = false;
	bBenchIntro = true;
	bSkipNextBenchAdvance = true;
	BenchPhase = EBenchPhase::Wave;
	UnusedOwnerTurns = 0;
	PlayBodyAnim(EFigureAnim::Death);

	/* The HUD removes the stat widget when the figure
	 * leaves the registry. The body stays on the
	 * square until the death clip ends. */
	if (UJoA_FigureRegistry* Registry =
		UJoA_FigureRegistry::Get(this))
	{
		Registry->UnregisterFigure(this);
	}
}

void AJoA_Figure::PlaceOnBench()
{
	if (bOnBench)
	{
		return;
	}

	bOnBench = true;
	ApplyBoardTransform();
}

AJoA_GameMode* AJoA_Figure::GetArenaGameMode() const
{
	const UWorld* World = GetWorld();
	return World
		? World->GetAuthGameMode<AJoA_GameMode>()
		: nullptr;
}

void AJoA_Figure::OnRep_OwningPlayerIndex()
{
	ApplyTeamColor();

	/* The owner pointer can still be unmapped.
	 * SyncFigures matches this seat to the local
	 * SeatIndex. Without a rebuild the portrait
	 * stays in the opponent row and ignores the
	 * click. */
	if (UJoA_FigureRegistry* Registry =
		UJoA_FigureRegistry::Get(this))
	{
		Registry->NotifyBoardChanged();
	}
}

void AJoA_Figure::ApplyTeamColor()
{
	if (!Mesh)
	{
		return;
	}

	float TileIndex = JoAAtlas::WhiteTile;
	if (OwningPlayerIndex == FrenchSeat)
	{
		TileIndex = JoAAtlas::BlueTile;
	}
	else if (OwningPlayerIndex == EnglishSeat)
	{
		TileIndex = JoAAtlas::RedTile;
	}

	// Body reads its palette row.
	JoAAtlas::ApplyPalette(
		Mesh,
		TileIndex,
		JoAFigure::PaletteRowFor(UnitType));
	RefreshHeldItemPresentation();
}

void AJoA_Figure::SetOwningPlayer(
	AJoA_PlayerState* InPlayerState,
	int32 InPlayerIndex)
{
	if (!HasAuthority())
	{
		return;
	}

	OwningPlayerState = InPlayerState;
	OwningPlayerIndex = InPlayerIndex;
	MARK_PROPERTY_DIRTY_FROM_NAME(
		AJoA_Figure,
		OwningPlayerState,
		this);
	MARK_PROPERTY_DIRTY_FROM_NAME(
		AJoA_Figure,
		OwningPlayerIndex,
		this);
	ForceNetUpdate();
	ApplyTeamColor();

	/* Owner is known. Grant sets and base values once. */
	if (OwningPlayerState)
	{
		InitializeAbilitySystem();
	}

	/* The server HUD sorts figures by owner too. Without
	 * this it never learns the owner was assigned. */
	if (UJoA_FigureRegistry* Registry =
		UJoA_FigureRegistry::Get(this))
	{
		Registry->NotifyBoardChanged();
	}
}

bool AJoA_Figure::UsesStamina() const
{
	return FUnitStats::GetDefaults(UnitType).bUsesStamina;
}
