// Copyright Epic Games, Inc. All Rights Reserved.

#include "Player/JoA_PlayerController.h"

// --- Engine ---
#include "Algo/Sort.h"
#include "Blueprint/UserWidget.h"
#include "Camera/CameraTypes.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LevelScriptActor.h"
#include "EngineUtils.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "EnhancedPlayerInput.h"
#include "Framework/Application/SlateApplication.h"
#include "InputAction.h"
#include "InputKeyEventArgs.h"
#include "InputMappingContext.h"
#include "TimerManager.h"
#include "Widgets/SViewport.h"

// --- GAME ---
#include "Match/JoA_GameMode.h"
#include "Match/JoA_CookedContent.h"
#include "Player/JoA_GameState.h"
#include "Player/JoA_PlayerState.h"
#include "Board/JoA_Flag.h"
#include "Board/JoA_GridManager.h"
#include "JoA_Arena.h"
#include "Match/JoA_GameInstance.h"
#include "Player/JoA_PlayerCameraManager.h"
#include "Player/JoA_ViewCamera.h"
#include "Player/Units/JoA_Figure.h"
#include "Player/Units/JoA_FigureMarker.h"
#include "Player/Units/JoA_FigureRegistry.h"
#include "UI/JoA_W_Game.h"
#include "UI/JoA_W_Play.h"

// --- GAS ---
#include "GameplayTagContainer.h"

// --- Net ---
#include "Net/Core/PushModel/PushModel.h"
#include "Net/UnrealNetwork.h"

#if UE_WITH_IRIS
#include "Iris/ReplicationSystem/ReplicationFragment.h"
#endif

AJoA_PlayerController::AJoA_PlayerController()
{
	/* The engine would otherwise snap the view back
	 * to the possessed pawn's camera. */
	bAutoManageActiveCameraTarget = false;

	/* Input is processed in TickActor. Without
	 * this tick, keys never reach Enhanced Input. */
	PrimaryActorTick.bCanEverTick = true;

	/* Selection changes rarely. ForceNetUpdate on
	 * that write still leaves the same frame. */
	SetNetUpdateFrequency(10.f);
	SetMinNetUpdateFrequency(2.f);

	PlayerCameraManagerClass =
		AJoA_PlayerCameraManager::StaticClass();
}

void AJoA_PlayerController::GetLifetimeReplicatedProps(
	TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	FDoRepLifetimeParams Params;
	Params.bIsPushBased = true;
	DOREPLIFETIME_WITH_PARAMS_FAST(
		AJoA_PlayerController,
		SelectedFigure,
		Params);
}

#if UE_WITH_IRIS
void AJoA_PlayerController::RegisterReplicationFragments(
	UE::Net::FFragmentRegistrationContext& Context,
	UE::Net::EFragmentRegistrationFlags RegistrationFlags)
{
	Super::RegisterReplicationFragments(
		Context,
		RegistrationFlags);
}
#endif

void AJoA_PlayerController::PostInitializeComponents()
{
	/* A Blueprint default replaces the constructor.
	 * Super spawns the camera manager. */
	PlayerCameraManagerClass =
		AJoA_PlayerCameraManager::StaticClass();
	bAutoManageActiveCameraTarget = false;
	Super::PostInitializeComponents();
}

void AJoA_PlayerController::ReceivedPlayer()
{
	Super::ReceivedPlayer();
	EnableInputTick();
	ApplyStaticArenaView();
	SetupTestInput();
	SetupGameInput();
	CreatePlayWidget();
	BindHudEvents();
	SetupMarkers();
	ApplyMenuAutoPickup();
}

void AJoA_PlayerController::BeginPlay()
{
	Super::BeginPlay();
	if (IsLocalController())
	{
		if (UJoA_GameInstance* Instance =
			GetGameInstance<UJoA_GameInstance>())
		{
			Instance->PullSavedSoundVolume();
		}
	}
	EnableInputTick();
	ApplyStaticArenaView();
	SetupTestInput();
	SetupGameInput();
	CreatePlayWidget();
	BindHudEvents();
	SetupMarkers();
	ApplyMenuAutoPickup();
}

void AJoA_PlayerController::ApplyMenuAutoPickup()
{
	if (bMenuAutoPickupApplied
		|| !IsLocalPlayerController())
	{
		return;
	}

	const UJoA_GameInstance* Instance =
		GetGameInstance<UJoA_GameInstance>();
	if (!Instance)
	{
		return;
	}

	const bool bEnabled = Instance->GetAutoPickupFlag();
	if (HasAuthority())
	{
		bAutoPickupFlag = bEnabled;
		bMenuAutoPickupApplied = true;
		return;
	}

	/* ReceivedPlayer is the first point a client
	 * RPC is accepted. BeginPlay can run earlier. */
	if (!GetNetConnection())
	{
		return;
	}

	ServerSetPickup(bEnabled);
	bMenuAutoPickupApplied = true;
}

void AJoA_PlayerController::ServerSetPickup_Implementation(
	bool bEnabled)
{
	bAutoPickupFlag = bEnabled;
}

void AJoA_PlayerController::EndPlay(
	const EEndPlayReason::Type EndPlayReason)
{
	if (UJoA_FigureRegistry* Registry =
		UJoA_FigureRegistry::Get(this))
	{
		Registry->OnBoardStateChanged.Remove(BoardHandle);
	}
	bHudEventsBound = false;
	RemoveOpeningThrowWidget();
	Super::EndPlay(EndPlayReason);
}

void AJoA_PlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	UEnhancedInputComponent* Component =
		Cast<UEnhancedInputComponent>(InputComponent);
	if (!Component)
	{
		return;
	}

	EnsureSelectionActions();
	const int32 SlotCount = SelectSlotActions.Num();
	for (int32 Slot = 0; Slot < SlotCount; ++Slot)
	{
		Component->BindAction(
			SelectSlotActions[Slot],
			ETriggerEvent::Started,
			this,
			&AJoA_PlayerController::HandleSelectSlot,
			Slot);
	}

	Component->BindAction(
		SelectNextAction,
		ETriggerEvent::Started,
		this,
		&AJoA_PlayerController::HandleSelectStep,
		1);
	Component->BindAction(
		SelectPreviousAction,
		ETriggerEvent::Started,
		this,
		&AJoA_PlayerController::HandleSelectStep,
		-1);

	/* Same path as Btn_TurnLeft, Btn_Go, and
	 * Btn_TurnRight. The selected figure asks. */
	Component->BindAction(
		TurnLeftAction,
		ETriggerEvent::Started,
		this,
		&AJoA_PlayerController::HandleTurnLeft);
	Component->BindAction(
		GoAction,
		ETriggerEvent::Started,
		this,
		&AJoA_PlayerController::HandleGo);
	Component->BindAction(
		TurnRightAction,
		ETriggerEvent::Started,
		this,
		&AJoA_PlayerController::HandleTurnRight);
}

void AJoA_PlayerController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);
	if (HasAuthority())
	{
		AJoA_Figure* Figure = Cast<AJoA_Figure>(InPawn);
		if (SelectedFigure != Figure)
		{
			SelectedFigure = Figure;
			MARK_PROPERTY_DIRTY_FROM_NAME(
				AJoA_PlayerController,
				SelectedFigure,
				this);
			ForceNetUpdate();
		}
	}
	ApplyStaticArenaView();
	BroadcastSelection();
	if (IsLocalController())
	{
		FocusGameViewport();
	}
}

void AJoA_PlayerController::OnUnPossess()
{
	/* Keep the pick. The view pawn is not a
	 * figure. Clearing here made QWE wait
	 * until the player clicked a portrait. */
	Super::OnUnPossess();
	BroadcastSelection();
}

void AJoA_PlayerController::OnRep_Pawn()
{
	Super::OnRep_Pawn();
	BroadcastSelection();
	if (IsLocalController() && GetPawn())
	{
		FocusGameViewport();
	}
}

void AJoA_PlayerController::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();

	/* OwnsFigure needs this state. Portraits built
	 * before it arrived stayed unclickable. */
	if (UJoA_FigureRegistry* Registry =
		UJoA_FigureRegistry::Get(this))
	{
		Registry->NotifyBoardChanged();
	}
	BroadcastSelection();
	SyncOpeningThrowWidget();
}

void AJoA_PlayerController::ClientRestart_Implementation(
	APawn* NewPawn)
{
	Super::ClientRestart_Implementation(NewPawn);
	/* Pawn restart runs after the first apply. */
	ApplyStaticArenaView();
	BroadcastSelection();
}

void AJoA_PlayerController::PawnLeavingGame()
{
	AJoA_Figure* Held = GetSelectedFigure();
	if (!Held)
	{
		Super::PawnLeavingGame();
		return;
	}

	UWorld* World = GetWorld();
	AJoA_GameMode* Mode = World
		? World->GetAuthGameMode<AJoA_GameMode>()
		: nullptr;
	if (Mode)
	{
		Mode->RememberSeatFigure(
			Held->OwningPlayerIndex,
			Held);
	}

	UnPossess();
}

void AJoA_PlayerController::
	ClientStoreReconnectTicket_Implementation(
		const FString& Token,
		int32 Seat)
{
	UJoA_GameInstance* Instance =
		GetGameInstance<UJoA_GameInstance>();
	UWorld* World = GetWorld();
	if (!Instance || !World)
	{
		return;
	}

	Instance->StoreReconnectTicket(
		World->GetAddressURL(),
		Token,
		Seat);
}

void AJoA_PlayerController::ClientWasKicked_Implementation(
	const FText& KickReason)
{
	Super::ClientWasKicked_Implementation(KickReason);

	/* The server put this player out, for example because a
	 * new connection took the seat. Trying to rejoin would
	 * kick that connection in turn. */
	if (UJoA_GameInstance* Instance =
		GetGameInstance<UJoA_GameInstance>())
	{
		Instance->ClearReconnectTicket();
	}

	UE_LOG(
		LogJoA_Net,
		Warning,
		TEXT("Removed from the match: %s"),
		*KickReason.ToString());
}

void AJoA_PlayerController::LeaveMatch()
{
	if (UJoA_GameInstance* Instance =
		GetGameInstance<UJoA_GameInstance>())
	{
		Instance->ClearReconnectTicket();
		Instance->LeaveOnlineSession();

		const AJoA_GameState* MatchState =
			AJoA_GameState::Get(this);
		if (MatchState && MatchState->IsMatchOver())
		{
			/* The result is already on screen.
			 * Leaving must not add a loss line. */
			Instance->HoldEmptyMenuNotice();
		}
		else
		{
			Instance->SetMenuNotice(
				TEXT("Play.DefeatForfeit"));
		}
	}

	if (HasAuthority())
	{
		ServerLeaveMatch_Implementation();
	}
	else
	{
		ServerLeaveMatch();
	}

	GetWorldTimerManager().SetTimer(
		LeaveTimer,
		this,
		&AJoA_PlayerController::CloseConnection,
		0.3f,
		false);
}

void AJoA_PlayerController::CloseConnection()
{
	ConsoleCommand(TEXT("disconnect"));
}

void AJoA_PlayerController::
	ServerLeaveMatch_Implementation()
{
	UWorld* World = GetWorld();
	AJoA_GameMode* Mode = World
		? World->GetAuthGameMode<AJoA_GameMode>()
		: nullptr;
	const AJoA_PlayerState* LeaveState =
		GetPlayerState<AJoA_PlayerState>();
	if (Mode && LeaveState)
	{
		Mode->RequestLeaveMatch(LeaveState->GetSeatIndex());
	}
}

AJoA_Figure* AJoA_PlayerController::GetSelectedFigure() const
{
	if (IsValid(SelectedFigure) && !SelectedFigure->IsDead())
	{
		return SelectedFigure;
	}

	return Cast<AJoA_Figure>(GetPawn());
}

void AJoA_PlayerController::OnRep_SelectedFigure()
{
	BroadcastSelection();
}

void AJoA_PlayerController::BroadcastSelection()
{
	OnSelectedFigureChanged.Broadcast(GetSelectedFigure());
	RefreshMarkers();
}

void AJoA_PlayerController::SetupMarkers()
{
	if (!IsLocalController() || bMarkersReady)
	{
		return;
	}

	UJoA_FigureRegistry* Registry =
		UJoA_FigureRegistry::Get(this);
	if (!Registry)
	{
		return;
	}

	/* Board, facing, and turn changes all notify the
	 * registry, so the markers follow without a tick. */
	Registry->OnBoardStateChanged.AddUObject(
		this,
		&AJoA_PlayerController::RefreshMarkers);
	bMarkersReady = true;
	RefreshMarkers();
}

AJoA_FigureMarker* AJoA_PlayerController::SpawnMarker()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride =
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	return World->SpawnActor<AJoA_FigureMarker>(
		AJoA_FigureMarker::StaticClass(),
		FTransform::Identity,
		Params);
}

void AJoA_PlayerController::RefreshMarkers()
{
	if (!IsLocalController())
	{
		return;
	}

	AJoA_Figure* Selected = GetSelectedFigure();
	const bool bShowSelected =
		IsValid(Selected) && !Selected->IsDead();
	if (bShowSelected && !SelectionMarker)
	{
		SelectionMarker = SpawnMarker();
	}
	if (SelectionMarker)
	{
		if (bShowSelected)
		{
			SelectionMarker->ShowOn(
				Selected,
				EFigureMarkerKind::Selected);
		}
		else
		{
			SelectionMarker->HideMarker();
		}
	}

	RefreshCellMarkers(bShowSelected ? Selected : nullptr);

	TSet<const AJoA_Figure*> Shown;
	const UJoA_FigureRegistry* Registry =
		UJoA_FigureRegistry::Get(this);
	if (bShowSelected && Registry)
	{
		TArray<AJoA_Figure*> All;
		Registry->CollectFigures(All);
		for (AJoA_Figure* Target : All)
		{
			if (Target == Selected)
			{
				continue;
			}

			EFigureMarkerKind Kind = EFigureMarkerKind::None;
			if (Selected->CanAttackTarget(Target))
			{
				Kind = EFigureMarkerKind::Attackable;
			}
			else if (Selected->CanHealTarget(Target))
			{
				Kind = EFigureMarkerKind::Healable;
			}

			if (Kind == EFigureMarkerKind::None)
			{
				continue;
			}

			TObjectPtr<AJoA_FigureMarker>& Marker =
				TargetMarkers.FindOrAdd(Target);
			if (!Marker)
			{
				Marker = SpawnMarker();
			}
			if (Marker)
			{
				Marker->ShowOn(Target, Kind);
				Shown.Add(Target);
			}
		}
	}

	for (auto It = TargetMarkers.CreateIterator(); It; ++It)
	{
		const AJoA_Figure* Key = It.Key();
		if (!IsValid(Key) || !IsValid(It.Value()))
		{
			if (AJoA_FigureMarker* Stale = It.Value())
			{
				Stale->Destroy();
			}
			It.RemoveCurrent();
		}
		else if (!Shown.Contains(Key))
		{
			It.Value()->HideMarker();
		}
	}
}

void AJoA_PlayerController::RefreshCellMarkers(
	const AJoA_Figure* Selected)
{
	const AJoA_GridManager* Grid =
		AJoA_GameMode::GetGridManager(this);

	/* Green on the free square the step button leads to.
	 * Brown under an obstacle, a square that can be
	 * raised, or a flag in front. A build replaces the
	 * green ring: the left click raises that square. */
	FVector StepLocation = FVector::ZeroVector;
	bool bShowStep = false;
	FVector ObstacleLocation = FVector::ZeroVector;
	bool bShowObstacle = false;
	FVector FlagLocation = FVector::ZeroVector;
	bool bShowFlag = false;
	if (Selected && Grid)
	{
		const AJoA_Flag* PickupFlag =
			Selected->FindPickupFlag();
		if (PickupFlag)
		{
			FlagLocation = Grid->GetCellWorldLocation(
				PickupFlag->GetCellX(),
				PickupFlag->GetCellY());
			bShowFlag = true;
		}

		FIntPoint BuildCell = FIntPoint::ZeroValue;
		const bool bCanBuild =
			Selected->FindBuildableCell(BuildCell);

		if (Selected->CanStep(EStepDirection::Forward))
		{
			const FIntPoint Cell = Selected->GetStepTarget(
				EStepDirection::Forward);
			/* The flag ring and the build ring are
			 * brown. Either one replaces the green
			 * step ring on that square. */
			const bool bFlagHere = PickupFlag
				&& PickupFlag->GetCellX() == Cell.X
				&& PickupFlag->GetCellY() == Cell.Y;
			const bool bBuildHere = bCanBuild
				&& BuildCell.X == Cell.X
				&& BuildCell.Y == Cell.Y;
			if (!bFlagHere && !bBuildHere)
			{
				StepLocation =
					Grid->GetCellWorldLocation(
						Cell.X,
						Cell.Y);
				bShowStep = true;
			}
		}

		FIntPoint ObstacleCell = BuildCell;
		const bool bCanDestroy = !bCanBuild
			&& Selected->FindDestroyableObstacle(
				ObstacleCell);
		if (bCanBuild || bCanDestroy)
		{
			ObstacleLocation =
				Grid->GetCellWorldLocation(
					ObstacleCell.X,
					ObstacleCell.Y);
			bShowObstacle = true;
		}
	}

	if (bShowStep && !StepMarker)
	{
		StepMarker = SpawnMarker();
	}
	if (StepMarker)
	{
		if (bShowStep)
		{
			StepMarker->ShowOnCell(
				StepLocation,
				EFigureMarkerKind::StepTarget);
		}
		else
		{
			StepMarker->HideMarker();
		}
	}

	if (bShowObstacle && !ObstacleMarker)
	{
		ObstacleMarker = SpawnMarker();
	}
	if (ObstacleMarker)
	{
		if (bShowObstacle)
		{
			ObstacleMarker->ShowOnCell(
				ObstacleLocation,
				EFigureMarkerKind::Obstacle);
		}
		else
		{
			ObstacleMarker->HideMarker();
		}
	}

	if (bShowFlag && !FlagMarker)
	{
		FlagMarker = SpawnMarker();
	}
	if (FlagMarker)
	{
		if (bShowFlag)
		{
			FlagMarker->ShowOnCell(
				FlagLocation,
				EFigureMarkerKind::FlagPickup);
		}
		else
		{
			FlagMarker->HideMarker();
		}
	}
}

AJoA_Figure* AJoA_PlayerController::FindFigureUnderCursor() const
{
	FVector RayOrigin = FVector::ZeroVector;
	FVector RayDirection = FVector::ForwardVector;
	if (!DeprojectMousePositionToWorld(
			RayOrigin,
			RayDirection))
	{
		UE_LOG(
			LogTemp,
			Log,
			TEXT("Click: no mouse position in the viewport."));
		return nullptr;
	}

	return FindFigureAlongRay(
		RayOrigin,
		RayOrigin + RayDirection * 100000.0);
}

AJoA_Figure* AJoA_PlayerController::FindFigureAlongRay(
	const FVector& RayOrigin,
	const FVector& RayEnd) const
{
	const UJoA_FigureRegistry* Registry =
		UJoA_FigureRegistry::Get(this);
	if (!Registry)
	{
		return nullptr;
	}

	/* Figures have no collision. Each one is tested as an
	 * upright capsule from its feet to above its head. */
	const float BodyHeight = 110.f;
	const float BodyRadius = 45.f;

	TArray<AJoA_Figure*> All;
	Registry->CollectFigures(All);

	/* The selected figure usually stands between the camera
	 * and its target, so the ray crosses both. A figure it
	 * can attack or heal wins over a nearer one, otherwise
	 * a click on the target would hit the attacker. */
	const AJoA_Figure* Selected = GetSelectedFigure();
	AJoA_Figure* Best = nullptr;
	bool bBestActionable = false;
	double BestDistance = TNumericLimits<double>::Max();
	for (AJoA_Figure* Figure : All)
	{
		if (Figure->IsDead())
		{
			continue;
		}

		const FVector Feet = Figure->GetActorLocation();
		FVector OnRay = FVector::ZeroVector;
		FVector OnBody = FVector::ZeroVector;
		FMath::SegmentDistToSegmentSafe(
			RayOrigin,
			RayEnd,
			Feet,
			Feet + FVector(0.0, 0.0, BodyHeight),
			OnRay,
			OnBody);
		if (FVector::Dist(OnRay, OnBody) > BodyRadius)
		{
			continue;
		}

		const bool bActionable = IsValid(Selected)
			&& Selected != Figure
			&& (Selected->CanAttackTarget(Figure)
				|| Selected->CanHealTarget(Figure));
		const double Along = FVector::Dist(RayOrigin, OnRay);
		if ((bActionable && !bBestActionable)
			|| (bActionable == bBestActionable
				&& Along < BestDistance))
		{
			BestDistance = Along;
			bBestActionable = bActionable;
			Best = Figure;
		}
	}

	return Best;
}

AJoA_Flag* AJoA_PlayerController::FindFlagAlongRay(
	const FVector& RayOrigin,
	const FVector& RayEnd) const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}

	const float PoleHeight = 90.f;
	const float PoleRadius = 30.f;
	AJoA_Flag* Best = nullptr;
	double BestDistance = TNumericLimits<double>::Max();
	for (TActorIterator<AJoA_Flag> It(World); It; ++It)
	{
		AJoA_Flag* Flag = *It;
		if (!IsValid(Flag) || Flag->IsCarried())
		{
			continue;
		}

		const FVector Base = Flag->GetActorLocation();
		FVector OnRay = FVector::ZeroVector;
		FVector OnPole = FVector::ZeroVector;
		FMath::SegmentDistToSegmentSafe(
			RayOrigin,
			RayEnd,
			Base,
			Base + FVector(0.0, 0.0, PoleHeight),
			OnRay,
			OnPole);
		if (FVector::Dist(OnRay, OnPole) > PoleRadius)
		{
			continue;
		}

		const double Along = FVector::Dist(RayOrigin, OnRay);
		if (Along < BestDistance)
		{
			BestDistance = Along;
			Best = Flag;
		}
	}

	return Best;
}

static bool RayHitsBuildCell(
	const FVector& RayOrigin,
	const FVector& RayEnd,
	const FVector& CellLocation)
{
	/* Aim is the ground of that square. A tall
	 * segment would also match a ray that only
	 * passes through the cell toward a figure
	 * behind it, and that click is a select. */
	const FVector GroundHit =
		FMath::LinePlaneIntersection(
			RayOrigin,
			RayEnd,
			CellLocation,
			FVector::UpVector);
	return FVector::Dist2D(GroundHit, CellLocation)
		<= 50.0;
}

void AJoA_PlayerController::HandleWorldClick()
{
	/* The opening throw sits on top of the board.
	 * A click there is a pick, not a world click. */
	if (GameWidget && GameWidget->IsInViewport())
	{
		return;
	}

	/* A portrait click still deprojects into the
	 * board. That must not step or build. */
	if (PlayWidget
		&& FSlateApplication::IsInitialized()
		&& PlayWidget->HitsHud(
			FVector2D(
				FSlateApplication::Get().GetCursorPos())))
	{
		return;
	}

	FVector RayOrigin = FVector::ZeroVector;
	FVector RayDirection = FVector::ForwardVector;
	if (!DeprojectMousePositionToWorld(
			RayOrigin,
			RayDirection))
	{
		return;
	}

	ClickAlongRay(
		RayOrigin,
		RayOrigin + RayDirection * 100000.0);
}

void AJoA_PlayerController::ClickAlongRay(
	const FVector& RayOrigin,
	const FVector& RayEnd)
{
	AJoA_Figure* Clicked =
		FindFigureAlongRay(RayOrigin, RayEnd);
	AJoA_Figure* Selected = GetSelectedFigure();
	const bool bFigureActionable = Clicked
		&& IsValid(Selected)
		&& Selected != Clicked
		&& (Selected->CanAttackTarget(Clicked)
			|| Selected->CanHealTarget(Clicked));

	/* Pickup needs a click on the flag itself, or
	 * on its brown ring. An attack or heal on
	 * another figure wins. */
	AJoA_Flag* PickupFlag = IsValid(Selected)
		? Selected->FindPickupFlag()
		: nullptr;
	AJoA_Flag* ClickedFlag =
		FindFlagAlongRay(RayOrigin, RayEnd);
	if (PickupFlag
		&& !bFigureActionable
		&& ClickedFlag == PickupFlag)
	{
		Selected->RequestPickupFlag();
		return;
	}

	if (PickupFlag
		&& !Clicked
		&& FlagMarker
		&& FlagMarker->GetKind()
			== EFigureMarkerKind::FlagPickup)
	{
		const FVector Ground =
			FlagMarker->GetActorLocation();
		const FVector Hit = FMath::LinePlaneIntersection(
			RayOrigin,
			RayEnd,
			Ground,
			FVector::UpVector);
		if (FVector::Dist2D(Hit, Ground) <= 50.0)
		{
			Selected->RequestPickupFlag();
			return;
		}
	}

	/* The builder stands between the camera and the
	 * square he faces. A click on that ground still
	 * counts when the ray first hits him. A click
	 * on another figure is a select or an attack. */
	FIntPoint BuildCell = FIntPoint::ZeroValue;
	const bool bOtherFigure =
		Clicked && Clicked != Selected;
	if (!bOtherFigure
		&& IsValid(Selected)
		&& ObstacleMarker
		&& ObstacleMarker->GetKind()
			== EFigureMarkerKind::Obstacle
		&& Selected->FindBuildableCell(BuildCell)
		&& RayHitsBuildCell(
			RayOrigin,
			RayEnd,
			ObstacleMarker->GetActorLocation()))
	{
		Selected->RequestBuildObstacle();
		return;
	}

	/* A click on the brown ring destroys the wall.
	 * A ray through any figure, including the
	 * knight, belongs to that figure. */
	FIntPoint ObstacleCell = FIntPoint::ZeroValue;
	if (!Clicked
		&& IsValid(Selected)
		&& ObstacleMarker
		&& ObstacleMarker->GetKind()
			== EFigureMarkerKind::Obstacle
		&& Selected->FindDestroyableObstacle(ObstacleCell))
	{
		const FVector Base =
			ObstacleMarker->GetActorLocation();
		FVector OnRay = FVector::ZeroVector;
		FVector OnWall = FVector::ZeroVector;
		FMath::SegmentDistToSegmentSafe(
			RayOrigin,
			RayEnd,
			Base,
			Base + FVector(0.0, 0.0, 120.0),
			OnRay,
			OnWall);
		if (FVector::Dist(OnRay, OnWall) <= 50.0)
		{
			RequestDestroyObstacle(
				Selected,
				ObstacleCell.X,
				ObstacleCell.Y);
			return;
		}
	}

	/* A click on the green ring steps onto that square.
	 * The ray is cut with the ground plane of the ring. */
	if (!Clicked
		&& IsValid(Selected)
		&& StepMarker
		&& StepMarker->GetKind()
			== EFigureMarkerKind::StepTarget
		&& Selected->CanStep(EStepDirection::Forward))
	{
		const FVector Ground = StepMarker->GetActorLocation();
		const FVector Hit = FMath::LinePlaneIntersection(
			RayOrigin,
			RayEnd,
			Ground,
			FVector::UpVector);
		if (FVector::Dist2D(Hit, Ground) <= 50.0)
		{
			Selected->RequestStep(EStepDirection::Forward);
			return;
		}
	}

	ApplyClickOn(Clicked);
}

void AJoA_PlayerController::HandleWorldRightClick()
{
	if (GameWidget && GameWidget->IsInViewport())
	{
		return;
	}

	FVector RayOrigin = FVector::ZeroVector;
	FVector RayDirection = FVector::ForwardVector;
	if (!DeprojectMousePositionToWorld(
			RayOrigin,
			RayDirection))
	{
		return;
	}

	AJoA_Figure* Clicked = FindFigureAlongRay(
		RayOrigin,
		RayOrigin + RayDirection * 100000.0);
	if (Clicked && Clicked->GetHeldFlag())
	{
		Clicked->RequestDropFlag();
		return;
	}

	if (AJoA_Figure* Figure = GetCommandFigure())
	{
		/* Rest still spends the turn. After a
		 * move only skip is legal, so that
		 * stays the fallback. */
		if (Figure->CanRest())
		{
			Figure->RequestRest();
		}
		else
		{
			Figure->RequestSkipTurn();
		}
	}
}

void AJoA_PlayerController::ApplyClickOn(
	AJoA_Figure* Clicked)
{
	UE_LOG(
		LogTemp,
		Log,
		TEXT("Click on figure unit %d."),
		Clicked ? static_cast<int32>(Clicked->UnitType) : -1);
	if (!Clicked)
	{
		return;
	}

	AJoA_Figure* Selected = GetSelectedFigure();
	if (IsValid(Selected) && Selected != Clicked)
	{
		if (Selected->CanAttackTarget(Clicked))
		{
			RequestAttack(Selected, Clicked);
			return;
		}

		if (Selected->CanHealTarget(Clicked))
		{
			RequestHeal(Selected, Clicked);
			return;
		}
	}

	if (OwnsFigure(Clicked))
	{
		RequestSelectFigure(Clicked);
	}
}

bool AJoA_PlayerController::CanSelectFigure(
	const AJoA_Figure* Figure) const
{
	if (!OwnsFigure(Figure) || Figure->IsDead())
	{
		return false;
	}

	const AJoA_GameState* State = AJoA_GameState::Get(this);
	if (!State)
	{
		return true;
	}

	if (!HasAuthority())
	{
		/* ActiveSeat and ActingFigure can be stale
		 * on a client. The server still checks
		 * CanSeatSelect. */
		return State->IsPlayable();
	}

	return State->CanSeatSelect(
		Figure->OwningPlayerIndex);
}

void AJoA_PlayerController::RequestSelectFigure(
	AJoA_Figure* Figure)
{
	if (!CanSelectFigure(Figure))
	{
		UE_LOG(
			LogTemp,
			Log,
			TEXT("Select refused: not own, dead, or not")
			TEXT(" the turn of this seat."));
		return;
	}

	Figure->NotifyPicked();
	if (HasAuthority())
	{
		if (GetSelectedFigure() != Figure)
		{
			Possess(Figure);
		}
		else
		{
			BroadcastSelection();
		}
		return;
	}

	/* A local pawn can already match this figure
	 * before the server has it. Skipping the call
	 * left the client with no marker and no keys. */
	ServerSelectFigure(Figure);
}

void AJoA_PlayerController::ServerSelectFigure_Implementation(
	AJoA_Figure* Figure)
{
	/* The server decides. A client cannot possess a
	 * figure that is not its own or not on its turn. */
	if (!CanSelectFigure(Figure))
	{
		return;
	}

	Figure->NotifyPicked();
	if (GetSelectedFigure() != Figure)
	{
		Possess(Figure);
		ForceNetUpdate();
	}

	/* Property replication can lag the click.
	 * The client paints the marker from this. */
	ClientSyncSelectedFigure(Figure);
}

void AJoA_PlayerController::
	ClientSyncSelectedFigure_Implementation(
		AJoA_Figure* Figure)
{
	SelectedFigure = Figure;
	BroadcastSelection();
}

void AJoA_PlayerController::CollectOwnFigures(
	TArray<AJoA_Figure*>& OutFigures) const
{
	const UJoA_FigureRegistry* Registry =
		UJoA_FigureRegistry::Get(this);
	if (!Registry)
	{
		return;
	}

	TArray<AJoA_Figure*> All;
	Registry->CollectFigures(All);
	for (AJoA_Figure* Figure : All)
	{
		if (OwnsFigure(Figure))
		{
			OutFigures.Add(Figure);
		}
	}

	/* Same order on both machines, and the order of the
	 * portraits in the HUD. */
	Algo::Sort(
		OutFigures,
		[](const AJoA_Figure* A, const AJoA_Figure* B)
		{
			return static_cast<uint8>(A->UnitType)
				< static_cast<uint8>(B->UnitType);
		});
}

void AJoA_PlayerController::HandleSelectSlot(int32 Slot)
{
	/* The slot is the unit type, so key 4 stays the
	 * Medic after a figure fell. A fallen figure is not
	 * in the list and the key does nothing. */
	TArray<AJoA_Figure*> Own;
	CollectOwnFigures(Own);
	for (AJoA_Figure* Figure : Own)
	{
		if (static_cast<int32>(Figure->UnitType) == Slot)
		{
			RequestSelectFigure(Figure);
			return;
		}
	}
}

void AJoA_PlayerController::HandleSelectStep(
	int32 Direction)
{
	TArray<AJoA_Figure*> Own;
	CollectOwnFigures(Own);
	if (Own.IsEmpty())
	{
		return;
	}

	const int32 Count = Own.Num();
	int32 Index = Own.IndexOfByKey(GetSelectedFigure());
	if (Index == INDEX_NONE)
	{
		Index = Direction > 0 ? Count - 1 : 0;
	}

	const int32 Next =
		(Index + Direction % Count + Count) % Count;
	RequestSelectFigure(Own[Next]);
}

AJoA_Figure* AJoA_PlayerController::GetCommandFigure() const
{
	if (AJoA_Figure* Figure = GetSelectedFigure())
	{
		return Figure;
	}

	/* Opening pick may not have replicated.
	 * QWE still drive the first own figure. */
	TArray<AJoA_Figure*> Own;
	CollectOwnFigures(Own);
	for (AJoA_Figure* Figure : Own)
	{
		if (Figure
			&& !Figure->IsDead()
			&& Figure->UnitType
				== EUnitType::HeavyKnight)
		{
			return Figure;
		}
	}

	for (AJoA_Figure* Figure : Own)
	{
		if (Figure && !Figure->IsDead())
		{
			return Figure;
		}
	}

	return nullptr;
}

void AJoA_PlayerController::HandleTurnLeft()
{
	if (AJoA_Figure* Figure = GetCommandFigure())
	{
		Figure->RequestTurn(ETurnDirection::Left);
	}
}

void AJoA_PlayerController::HandleGo()
{
	if (AJoA_Figure* Figure = GetCommandFigure())
	{
		Figure->RequestStep(EStepDirection::Forward);
	}
}

void AJoA_PlayerController::HandleTurnRight()
{
	if (AJoA_Figure* Figure = GetCommandFigure())
	{
		Figure->RequestTurn(ETurnDirection::Right);
	}
}

void AJoA_PlayerController::EnsureSelectionActions()
{
	if (SelectNextAction)
	{
		return;
	}

	const int32 SlotCount = 4;
	SelectSlotActions.Reset();
	for (int32 Slot = 0; Slot < SlotCount; ++Slot)
	{
		SelectSlotActions.Add(NewObject<UInputAction>(this));
	}

	SelectNextAction = NewObject<UInputAction>(this);
	SelectPreviousAction = NewObject<UInputAction>(this);
	TurnLeftAction = NewObject<UInputAction>(this);
	GoAction = NewObject<UInputAction>(this);
	TurnRightAction = NewObject<UInputAction>(this);
}

void AJoA_PlayerController::SetupGameInput()
{
	if (!IsLocalController() || bGameInputReady)
	{
		return;
	}

	ULocalPlayer* LocalPlayer = GetLocalPlayer();
	UEnhancedInputLocalPlayerSubsystem* InputSubsystem =
		LocalPlayer
		? LocalPlayer->GetSubsystem<
			UEnhancedInputLocalPlayerSubsystem>()
		: nullptr;
	if (!InputSubsystem)
	{
		if (InputSetupAttempts < 20)
		{
			++InputSetupAttempts;
			if (UWorld* World = GetWorld())
			{
				FTimerHandle RetryHandle;
				World->GetTimerManager().SetTimer(
					RetryHandle,
					this,
					&AJoA_PlayerController::SetupGameInput,
					0.1f,
					false);
			}
		}
		return;
	}

	EnsureSelectionActions();

	/* The number row, not the numpad. The numpad is
	 * already mapped to the test actions. */
	const FKey SlotKeys[] =
	{
		EKeys::One,
		EKeys::Two,
		EKeys::Three,
		EKeys::Four
	};

	SelectionContext = NewObject<UInputMappingContext>(this);
	const int32 SlotCount = SelectSlotActions.Num();
	for (int32 Slot = 0; Slot < SlotCount; ++Slot)
	{
		SelectionContext->MapKey(
			SelectSlotActions[Slot],
			SlotKeys[Slot]);
	}

	SelectionContext->MapKey(
		SelectNextAction,
		EKeys::MouseScrollDown);
	SelectionContext->MapKey(
		SelectPreviousAction,
		EKeys::MouseScrollUp);
	SelectionContext->MapKey(
		TurnLeftAction,
		EKeys::Q);
	SelectionContext->MapKey(
		GoAction,
		EKeys::W);
	SelectionContext->MapKey(
		TurnRightAction,
		EKeys::E);

	InputSubsystem->AddMappingContext(SelectionContext, 1);
	bGameInputReady = true;
}

void AJoA_PlayerController::CreatePlayWidget()
{
	if (!IsLocalController() || PlayWidget)
	{
		return;
	}

	TSubclassOf<UJoA_W_Play> WidgetClass = PlayWidgetClass;
	if (!WidgetClass)
	{
		WidgetClass = UJoA_CookedContent::LoadWidget(
			this,
			TEXT("/Game/WBP/WBP_Play.WBP_Play_C"));
	}

	if (!WidgetClass)
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("WBP_Play is missing or its parent is not")
			TEXT(" UJoA_W_Play. Reparent it."));
		return;
	}

	PlayWidget = CreateWidget<UJoA_W_Play>(
		this,
		WidgetClass);
	if (PlayWidget)
	{
		PlayWidget->AddToViewport();
	}

	/* The widget steals viewport focus. QWE
	 * then miss Enhanced Input until a click. */
	FocusGameViewport();
}

void AJoA_PlayerController::BindHudEvents()
{
	if (!IsLocalController() || bHudEventsBound)
	{
		return;
	}

	if (UJoA_FigureRegistry* Registry =
		UJoA_FigureRegistry::Get(this))
	{
		BoardHandle =
			Registry->OnBoardStateChanged.AddUObject(
				this,
				&AJoA_PlayerController::
					SyncOpeningThrowWidget);
		bHudEventsBound = true;
	}

	SyncOpeningThrowWidget();
}

void AJoA_PlayerController::SyncOpeningThrowWidget()
{
	if (!IsLocalController())
	{
		return;
	}

	const AJoA_GameState* State =
		AJoA_GameState::Get(this);
	const AJoA_PlayerState* OwnerState =
		GetPlayerState<AJoA_PlayerState>();
	const int32 Seat = OwnerState
		? OwnerState->GetSeatIndex()
		: INDEX_NONE;
	/* The host picks before the other seat
	 * exists. The match stays paused. */
	const bool bShow = State
		&& OwnerState
		&& !State->IsMatchOver()
		&& !State->HasMatchStarted()
		&& Seat >= 0
		&& Seat < 2
		&& !State->HasSeatThrown(Seat);

	if (bShow)
	{
		const bool bPickedAgain =
			LocalOpeningThrow != EOpeningThrow::None;
		LocalOpeningThrow = EOpeningThrow::None;
		CreateOpeningThrowWidget();
		if (bPickedAgain)
		{
			if (UJoA_FigureRegistry* Registry =
				UJoA_FigureRegistry::Get(this))
			{
				Registry->NotifyBoardChanged();
			}
		}
		return;
	}

	RemoveOpeningThrowWidget();
}

void AJoA_PlayerController::CreateOpeningThrowWidget()
{
	if (GameWidget)
	{
		return;
	}

	TSubclassOf<UJoA_W_Game> WidgetClass = GameWidgetClass;
	if (!WidgetClass)
	{
		WidgetClass = UJoA_CookedContent::LoadWidget(
			this,
			TEXT("/Game/WBP/WBP_Game.WBP_Game_C"));
	}

	if (!WidgetClass)
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("WBP_Game is missing or its parent is not")
			TEXT(" UJoA_W_Game. Reparent it."));
		return;
	}

	GameWidget = CreateWidget<UJoA_W_Game>(
		this,
		WidgetClass);
	if (GameWidget)
	{
		/* Above WBP_Play. Buttons must receive the
		 * click. The root must not stay after the
		 * pick, or the client loses board input. */
		GameWidget->AddToViewport(10);
	}
}

void AJoA_PlayerController::RemoveOpeningThrowWidget()
{
	if (!GameWidget)
	{
		return;
	}

	GameWidget->RemoveFromParent();
	GameWidget = nullptr;

	UWorld* World = GetWorld();
	if (!World || World->bIsTearingDown)
	{
		return;
	}

	/* The throw widget had the mouse. Viewport
	 * focus puts keys back on the client. */
	FocusGameViewport();
}

void AJoA_PlayerController::RequestOpeningThrow(
	EOpeningThrow Throw)
{
	if (Throw == EOpeningThrow::None)
	{
		return;
	}

	/* Gone before the server answers. A tie brings
	 * the widget back when the mask clears. */
	LocalOpeningThrow = Throw;
	RemoveOpeningThrowWidget();
	ServerSubmitOpeningThrow(Throw);
	if (UJoA_FigureRegistry* Registry =
		UJoA_FigureRegistry::Get(this))
	{
		Registry->NotifyBoardChanged();
	}
}

void AJoA_PlayerController::
	ServerSubmitOpeningThrow_Implementation(
		EOpeningThrow Throw)
{
	AJoA_GameMode* Mode =
		GetWorld()
			? GetWorld()->GetAuthGameMode<AJoA_GameMode>()
			: nullptr;
	const AJoA_PlayerState* OwnerState =
		GetPlayerState<AJoA_PlayerState>();
	if (!Mode || !OwnerState)
	{
		return;
	}

	Mode->SubmitOpeningThrow(
		OwnerState->GetSeatIndex(),
		Throw);
}

void AJoA_PlayerController::
	ClientSetViewTarget_Implementation(
		AActor* A,
		FViewTargetTransitionParams)
{
	/* The server sends its pawn as the target.
	 * Keep the camera assigned at startup. */
	if (A == ViewCamera)
	{
		return;
	}

	ApplyStaticArenaView();
}

void AJoA_PlayerController::EnableInputTick()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;
	PrimaryActorTick.Target = this;
	if (ULevel* Level = GetLevel())
	{
		PrimaryActorTick.RegisterTickFunction(Level);
	}
	SetActorTickEnabled(true);
}

void AJoA_PlayerController::PlayerTick(float DeltaTime)
{
	Super::PlayerTick(DeltaTime);

	if (!bLoggedInputTick)
	{
		bLoggedInputTick = true;
		const UEnhancedPlayerInput* Enhanced =
			Cast<UEnhancedPlayerInput>(PlayerInput);
		const int32 MappingCount = Enhanced
			? Enhanced->GetEnhancedActionMappingsView()
				.Num()
			: 0;
		UE_LOG(
			LogTemp,
			Log,
			TEXT("Player tick is running. Mappings %d."),
			MappingCount);
	}
}

void AJoA_PlayerController::CalcCamera(
	float DeltaTime,
	FMinimalViewInfo& OutResult)
{
	AJoA_ViewCamera::ApplyToViewInfo(OutResult, this);
}

void AJoA_PlayerController::GetPlayerViewPoint(
	FVector& OutLocation,
	FRotator& OutRotation) const
{
	/* The viewport reads this, not only the
	 * camera manager. Possession would otherwise
	 * show the figure's eyes. */
	const FTransform View =
		AJoA_ViewCamera::GetLocalViewTransform(this);
	OutLocation = View.GetLocation();
	OutRotation = View.Rotator();
}

bool AJoA_PlayerController::InputKey(
	const FInputKeyEventArgs& Params)
{
	if (Params.Event == IE_Pressed && IsLocalController())
	{
		if (Params.Key == EKeys::LeftMouseButton)
		{
			UE_LOG(
				LogTemp,
				Log,
				TEXT("Left mouse reached the controller."));
			HandleWorldClick();
		}
		else if (Params.Key == EKeys::RightMouseButton)
		{
			/* A HUD portrait handles its own right
			 * click. The world path must not steal
			 * that, or skip the turn under the HUD. */
			if (PlayWidget
				&& FSlateApplication::IsInitialized()
				&& PlayWidget->HitsHud(
					FVector2D(
						FSlateApplication::Get()
							.GetCursorPos())))
			{
				return false;
			}

			HandleWorldRightClick();
			return true;
		}
		else if (Params.Key == EKeys::SpaceBar)
		{
			/* Slate treats Space as a click on the
			 * focused HUD button. Jump or Fall must
			 * win, so this key is consumed here. */
			if (AJoA_Figure* Selected = GetSelectedFigure())
			{
				if (Selected->CanJump())
				{
					Selected->RequestJump();
				}
				else if (Selected->CanFall())
				{
					Selected->RequestFall();
				}
			}
			return true;
		}
		else
		{
			/* Plus, e with caron, s with caron, c with
			 * caron: the unshifted Czech number row. */
			const FName KeyName = Params.Key.GetFName();
			const FName RowKeys[] =
			{
				FName(TEXT("UnknownCharCode_43")),
				FName(TEXT("UnknownCharCode_283")),
				FName(TEXT("UnknownCharCode_353")),
				FName(TEXT("UnknownCharCode_269"))
			};
			for (int32 Slot = 0;
				Slot < UE_ARRAY_COUNT(RowKeys);
				++Slot)
			{
				if (RowKeys[Slot] == KeyName)
				{
					HandleSelectSlot(Slot);
					return true;
				}
			}
		}
	}

	return Super::InputKey(Params);
}

void AJoA_PlayerController::SetupTestInput()
{
	if (!IsLocalController() || bTestInputReady)
	{
		return;
	}

	if (!PlayerInput)
	{
		InitInputSystem();
	}

	if (!Cast<UEnhancedPlayerInput>(PlayerInput))
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("PlayerInput is not Enhanced."));
		return;
	}

	UInputMappingContext* Context = TestMappingContext;
	if (!Context)
	{
		Context = LoadObject<UInputMappingContext>(
			nullptr,
			TEXT("/Game/Input/IMC_ArenaTest.IMC_ArenaTest"));
	}

	if (!Context)
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("Missing IMC_ArenaTest."));
		/* The test map is optional. Without this
		 * the client never gets viewport focus,
		 * so portraits and keys 1-4 do nothing. */
		FocusGameViewport();
		bTestInputReady = true;
		return;
	}

	ULocalPlayer* LocalPlayer = GetLocalPlayer();
	if (!LocalPlayer)
	{
		return;
	}

	UEnhancedInputLocalPlayerSubsystem* InputSubsystem =
		LocalPlayer->GetSubsystem<
			UEnhancedInputLocalPlayerSubsystem>();
	if (!InputSubsystem
		|| !InputSubsystem->GetPlayerInput())
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("Enhanced input is not ready."));
		return;
	}

	const FGameplayTag ModeTag =
		FGameplayTag::RequestGameplayTag(
			TEXT("EnhancedInput.Modes.Default"),
			false);
	if (ModeTag.IsValid())
	{
		FGameplayTagContainer Mode;
		Mode.AddTag(ModeTag);
		InputSubsystem->SetInputMode(Mode);
	}

	FModifyContextOptions Options;
	Options.bForceImmediately = true;
	Options.bIgnoreAllPressedKeysUntilRelease = false;
	InputSubsystem->AddMappingContext(
		Context,
		0,
		Options);
	AddNumpadTestKeys(InputSubsystem, Options);

	EnsureLevelInput();
	FocusGameViewport();

	bTestInputReady = true;

	const UEnhancedPlayerInput* Enhanced =
		Cast<UEnhancedPlayerInput>(PlayerInput);
	const int32 AppliedCount = Enhanced
		? Enhanced->GetEnhancedActionMappingsView().Num()
		: 0;
	UE_LOG(
		LogTemp,
		Log,
		TEXT("Mappings asset %d applied %d."),
		Context->GetMappings().Num(),
		AppliedCount);
}

void AJoA_PlayerController::AddNumpadTestKeys(
	UEnhancedInputLocalPlayerSubsystem* InputSubsystem,
	const FModifyContextOptions& Options)
{
	UInputMappingContext* NumpadContext =
		NewObject<UInputMappingContext>(this);
	if (!NumpadContext || !InputSubsystem)
	{
		return;
	}

	const FKey NumpadKeys[] =
	{
		EKeys::NumPadOne,
		EKeys::NumPadTwo,
		EKeys::NumPadThree,
		EKeys::NumPadFour,
		EKeys::NumPadFive,
		EKeys::NumPadSix
	};

	for (int32 KeyIndex = 0; KeyIndex < 6; ++KeyIndex)
	{
		const FString ActionPath = FString::Printf(
			TEXT("/Game/Input/IA_Test%d.IA_Test%d"),
			KeyIndex + 1,
			KeyIndex + 1);
		UInputAction* Action = LoadObject<UInputAction>(
			nullptr,
			*ActionPath);
		if (!Action)
		{
			continue;
		}

		NumpadContext->MapKey(
			Action,
			NumpadKeys[KeyIndex]);
	}

	InputSubsystem->AddMappingContext(
		NumpadContext,
		1,
		Options);
}

void AJoA_PlayerController::FocusGameViewport()
{
	if (!FSlateApplication::IsInitialized())
	{
		return;
	}

	/* The throw widget is up. Pulling focus
	 * back here minimizes the window and
	 * fights the rock-paper-scissors buttons. */
	if (GameWidget && GameWidget->IsInViewport())
	{
		return;
	}

	/* SetInputMode during BeginPlay finds no
	 * viewport widget yet, so the focus is lost. */
	ULocalPlayer* LocalPlayer = GetLocalPlayer();
	UGameViewportClient* ViewportClient = LocalPlayer
		? LocalPlayer->ViewportClient
		: nullptr;
	TSharedPtr<SViewport> ViewportWidget = ViewportClient
		? ViewportClient->GetGameViewportWidget()
		: nullptr;
	if (!ViewportWidget.IsValid())
	{
		if (FocusAttempts < 20)
		{
			++FocusAttempts;
			FTimerHandle FocusHandle;
			GetWorldTimerManager().SetTimer(
				FocusHandle,
				this,
				&AJoA_PlayerController::FocusGameViewport,
				0.1f,
				false);
		}
		return;
	}

	FSlateApplication& Slate =
		FSlateApplication::Get();
	/* A replicate calls this on every turn.
	 * SetInputMode and ActivateGameViewport
	 * then fight the window. Skip when the
	 * viewport already has the keys. */
	if (bGameViewportFocused
		&& Slate.GetKeyboardFocusedWidget()
			== ViewportWidget)
	{
		return;
	}

	/* GameOnly captures the cursor on click.
	 * Shift+F1 is the editor release. */
	FInputModeGameAndUI InputMode;
	InputMode.SetLockMouseToViewportBehavior(
		EMouseLockMode::DoNotLock);
	InputMode.SetHideCursorDuringCapture(false);
	InputMode.SetWidgetToFocus(ViewportWidget);
	SetInputMode(InputMode);
	bShowMouseCursor = true;
	/* NoCapture drops the mouse down before it reaches
	 * InputKey in a game viewport, so no click on the board
	 * arrives. Capture during the mouse down lets it pass
	 * and releases the mouse on the button up. */
	ViewportClient->SetMouseCaptureMode(
		EMouseCaptureMode::CaptureDuringMouseDown);
	ViewportClient->SetHideCursorDuringCapture(false);
	ViewportClient->SetMouseLockMode(
		EMouseLockMode::DoNotLock);

	Slate.SetKeyboardFocus(
		ViewportWidget,
		EFocusCause::SetDirectly);
	Slate.ActivateGameViewport();

	if (!bGameViewportFocused)
	{
		bGameViewportFocused = true;
		UE_LOG(
			LogTemp,
			Log,
			TEXT("Game viewport focused."));
	}
}

void AJoA_PlayerController::EnsureLevelInput()
{
	UWorld* World = GetWorld();
	ALevelScriptActor* LevelScript = World
		? World->GetLevelScriptActor()
		: nullptr;
	if (!LevelScript || LevelScript->InputComponent)
	{
		return;
	}

	LevelScript->CreateInputComponent(
		UEnhancedInputComponent::StaticClass());
}

void AJoA_PlayerController::ApplyStaticArenaView()
{
	if (!IsLocalController())
	{
		return;
	}

	/* A Blueprint default can turn this on
	 * after the constructor and steal the view. */
	bAutoManageActiveCameraTarget = false;

	AJoA_ViewCamera* Camera = FindOrSpawnViewCamera();
	if (!Camera)
	{
		return;
	}

	ViewCamera = Camera;

	/* French is X 1000, Z 600. English is X -1000.
	 * Always applied, so a camera placed in the level
	 * or kept from an earlier call cannot keep a
	 * stale location. The client is English. */
	if (GetNetMode() == NM_Client)
	{
		Camera->ApplyEnglishView();
	}
	else
	{
		Camera->ApplyFrenchView();
	}

	if (!PlayerCameraManager)
	{
		return;
	}

	/* GetViewTarget retargets to the possessed
	 * figure. Assign the board camera every
	 * time. The POV write still has to ignore
	 * that steal. */
	PlayerCameraManager->SetViewTarget(Camera);
}

AJoA_ViewCamera*
	AJoA_PlayerController::FindOrSpawnViewCamera()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}

	if (IsValid(ViewCamera))
	{
		return ViewCamera;
	}

	for (TActorIterator<AJoA_ViewCamera> It(World); It; ++It)
	{
		if (IsValid(*It))
		{
			return *It;
		}
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride =
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	SpawnParams.Owner = this;
	const bool bClient = GetNetMode() == NM_Client;
	const FTransform SpawnTransform = bClient
		? AJoA_ViewCamera::GetEnglishViewTransform()
		: AJoA_ViewCamera::GetFrenchViewTransform();
	return World->SpawnActor<AJoA_ViewCamera>(
		AJoA_ViewCamera::StaticClass(),
		SpawnTransform,
		SpawnParams);
}

bool AJoA_PlayerController::OwnsFigure(
	const AJoA_Figure* Figure) const
{
	if (!Figure)
	{
		return false;
	}

	const AJoA_PlayerState* OwnerState =
		GetPlayerState<AJoA_PlayerState>();
	if (!OwnerState)
	{
		return false;
	}

	if (Figure->GetOwningPlayerState() == OwnerState)
	{
		return true;
	}

	/* The pointer can still be unmapped on a client
	 * while the seat int is already there. */
	const int32 Seat = OwnerState->GetSeatIndex();
	return Seat != INDEX_NONE
		&& Figure->OwningPlayerIndex == Seat;
}

void AJoA_PlayerController::RequestAttack(
	AJoA_Figure* Attacker,
	AJoA_Figure* Target)
{
	if (!OwnsFigure(Attacker)
		|| !Attacker->CanAttackTarget(Target))
	{
		return;
	}

	if (HasAuthority())
	{
		Attacker->ResolveAttack(Target);
		return;
	}

	ServerRequestAttack(Attacker, Target);
}

void AJoA_PlayerController::RequestHeal(
	AJoA_Figure* Healer,
	AJoA_Figure* Target)
{
	if (!OwnsFigure(Healer)
		|| !Healer->CanHealTarget(Target))
	{
		return;
	}

	if (HasAuthority())
	{
		Healer->ResolveHeal(Target);
		return;
	}

	ServerRequestHeal(Healer, Target);
}

void AJoA_PlayerController::RequestDestroyObstacle(
	AJoA_Figure* Attacker,
	int32 X,
	int32 Y)
{
	if (!OwnsFigure(Attacker)
		|| !Attacker->CanDestroyObstacleAt(X, Y))
	{
		return;
	}

	if (HasAuthority())
	{
		Attacker->ResolveDestroyObstacle(X, Y);
		return;
	}

	ServerRequestDestroyObstacle(Attacker, X, Y);
}

void AJoA_PlayerController::
	ServerRequestAttack_Implementation(
		AJoA_Figure* Attacker,
		AJoA_Figure* Target)
{
	if (!OwnsFigure(Attacker)
		|| !Attacker->CanAttackTarget(Target))
	{
		return;
	}

	Attacker->ResolveAttack(Target);
}

void AJoA_PlayerController::
	ServerRequestHeal_Implementation(
		AJoA_Figure* Healer,
		AJoA_Figure* Target)
{
	if (!OwnsFigure(Healer)
		|| !Healer->CanHealTarget(Target))
	{
		return;
	}

	Healer->ResolveHeal(Target);
}

void AJoA_PlayerController::
	ServerRequestDestroyObstacle_Implementation(
		AJoA_Figure* Attacker,
		int32 X,
		int32 Y)
{
	if (!OwnsFigure(Attacker)
		|| !Attacker->CanDestroyObstacleAt(X, Y))
	{
		return;
	}

	Attacker->ResolveDestroyObstacle(X, Y);
}
