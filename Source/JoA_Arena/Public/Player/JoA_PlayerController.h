// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

// --- Engine ---
#include "Engine/TimerHandle.h"
#include "GameFramework/PlayerController.h"

// --- GAME ---
#include "Player/JoA_SeatTypes.h"
#include "Player/Units/JoA_Figure.h"

#include "JoA_PlayerController.generated.h"

class AJoA_Flag;
class AJoA_FigureMarker;
class AJoA_ViewCamera;
class UEnhancedInputLocalPlayerSubsystem;
class UInputAction;
class UInputMappingContext;
class UJoA_W_Game;
class UJoA_W_Play;
struct FModifyContextOptions;

/* The selected figure is the possessed one. Fired on the
 * server when it possesses, and on the owning client when
 * the pawn replicates. Null when nothing is selected. */
DECLARE_MULTICAST_DELEGATE_OneParam(
	FOnSelectedFigureChanged,
	AJoA_Figure*);

/**
 * Local picture is the nation board view.
 * The listen server sits behind French.
 * A client sits behind English.
 * That camera is assigned once at startup.
 * The player selects one figure at a time by
 * possessing it. Keys 1 to 4, the mouse wheel, or a
 * click on a portrait of the HUD pick it. Q and E
 * turn it. W is the same as Btn_Go. The server
 * confirms the pick, because the figure that is
 * possessed is the only one that may send commands.
 */
UCLASS(Blueprintable)
class JOA_ARENA_API AJoA_PlayerController
	: public APlayerController
{
	GENERATED_BODY()

public:
	AJoA_PlayerController();

	virtual void GetLifetimeReplicatedProps(
		TArray<FLifetimeProperty>& OutLifetimeProps
	) const override;

#if UE_WITH_IRIS
	virtual void RegisterReplicationFragments(
		UE::Net::FFragmentRegistrationContext& Context,
		UE::Net::EFragmentRegistrationFlags
			RegistrationFlags) override;
#endif

	/* Local entry points. The obstacle and range test
	 * runs here. A failed test does not send the
	 * server call. */
	UFUNCTION(BlueprintCallable, Category = "Combat")
	void RequestAttack(
		AJoA_Figure* Attacker,
		AJoA_Figure* Target);

	UFUNCTION(BlueprintCallable, Category = "Combat")
	void RequestHeal(
		AJoA_Figure* Healer,
		AJoA_Figure* Target);

	UFUNCTION(BlueprintCallable, Category = "Combat")
	void RequestDestroyObstacle(
		AJoA_Figure* Attacker,
		int32 X,
		int32 Y);

	/* The possessed figure, or null. */
	AJoA_Figure* GetSelectedFigure() const;

	/* HUD rebuild after a turn drop steals keys.
	 * GameState asks so QWE still reach the seat. */
	void FocusGameViewport();

	bool OwnsFigure(const AJoA_Figure* Figure) const;

	/* Local test, before the server call. Own figure,
	 * alive, own turn, and nothing committed yet. */
	bool CanSelectFigure(const AJoA_Figure* Figure) const;

	/* Local entry for a click on a portrait. */
	UFUNCTION(BlueprintCallable, Category = "Selection")
	void RequestSelectFigure(AJoA_Figure* Figure);

	FOnSelectedFigureChanged OnSelectedFigureChanged;

	/* When true, stepping onto a flag cell picks
	 * it up. After a drop the figure must leave
	 * that cell before auto pickup runs again.
	 * I still picks up without leaving. */
	UPROPERTY(
		EditAnywhere,
		BlueprintReadWrite,
		Category = "Flag")
	bool bAutoPickupFlag = true;

	/* The picking behind a click in the field. The ray is
	 * the cursor ray of this player. The automation test
	 * passes its own ray. */
	AJoA_Figure* FindFigureAlongRay(
		const FVector& RayOrigin,
		const FVector& RayEnd) const;
	AJoA_Flag* FindFlagAlongRay(
		const FVector& RayOrigin,
		const FVector& RayEnd) const;
	void ClickAlongRay(
		const FVector& RayOrigin,
		const FVector& RayEnd);

	/* Local entry of a Leave button. The match is lost at
	 * once and nothing tries to rejoin it. */
	UFUNCTION(Exec, BlueprintCallable, Category = "Match")
	void LeaveMatch();

	/* Local pick in the throw that names who moves
	 * first. The Game widget is removed at once so
	 * the client keeps board input. */
	UFUNCTION(BlueprintCallable, Category = "Match")
	void RequestOpeningThrow(EOpeningThrow Throw);

	/* Server to this client, after every join. Where to
	 * connect and the secret of the seat, kept by the game
	 * instance for a reconnect. */
	UFUNCTION(Client, Reliable)
	void ClientStoreReconnectTicket(
		const FString& Token,
		int32 Seat);

	/* A figure belongs to the board. The engine would
	 * destroy the pawn of a controller that leaves, which
	 * here is the figure the player held. */
	virtual void PawnLeavingGame() override;

	AJoA_FigureMarker* GetStepMarker() const
	{
		return StepMarker;
	}

	AJoA_FigureMarker* GetObstacleMarker() const
	{
		return ObstacleMarker;
	}

	AJoA_FigureMarker* GetFlagMarker() const
	{
		return FlagMarker;
	}

	AJoA_FigureMarker* GetSelectionMarker() const
	{
		return SelectionMarker;
	}

	bool HasPlayWidget() const
	{
		return PlayWidget != nullptr;
	}

	UJoA_W_Play* GetPlayWidget() const
	{
		return PlayWidget;
	}

	bool HasOpeningThrowWidget() const
	{
		return GameWidget != nullptr;
	}

	EOpeningThrow GetLocalOpeningThrow() const
	{
		return LocalOpeningThrow;
	}

protected:
	virtual void PostInitializeComponents() override;
	virtual void ReceivedPlayer() override;
	virtual void BeginPlay() override;
	virtual void EndPlay(
		const EEndPlayReason::Type EndPlayReason
	) override;
	virtual void SetupInputComponent() override;
	virtual void OnPossess(APawn* InPawn) override;
	virtual void OnUnPossess() override;
	virtual void OnRep_Pawn() override;
	virtual void OnRep_PlayerState() override;

	UFUNCTION()
	void OnRep_SelectedFigure();
	virtual void ClientRestart_Implementation(
		APawn* NewPawn) override;
	virtual void ClientSetViewTarget_Implementation(
		AActor* A,
		FViewTargetTransitionParams TransitionParams
	) override;
	virtual void ClientWasKicked_Implementation(
		const FText& KickReason) override;
	virtual void PlayerTick(float DeltaTime) override;
	virtual void CalcCamera(
		float DeltaTime,
		FMinimalViewInfo& OutResult) override;
	virtual void GetPlayerViewPoint(
		FVector& OutLocation,
		FRotator& OutRotation) const override;
	/* The Czech number row sends character keys, not
	 * One to Four, so they are mapped here. The left
	 * mouse button on the field selects, attacks, or
	 * heals. */
	virtual bool InputKey(
		const FInputKeyEventArgs& Params) override;

	void ApplyStaticArenaView();
	/* TickActor reads keys. A Blueprint default can
	 * turn this tick off after the constructor. */
	void EnableInputTick();
	/* Adds the test mapping context for this
	 * local controller. */
	void SetupTestInput();
	void AddNumpadTestKeys(
		UEnhancedInputLocalPlayerSubsystem* InputSubsystem,
		const FModifyContextOptions& Options);
	void EnsureLevelInput();
	AJoA_ViewCamera* FindOrSpawnViewCamera();

	/* Selection keys. The actions are built in code and
	 * are not assets, so no mapping can be missing. */
	void EnsureSelectionActions();
	void SetupGameInput();
	void HandleSelectSlot(int32 Slot);
	void HandleSelectStep(int32 Direction);
	void HandleTurnLeft();
	void HandleGo();
	void HandleTurnRight();
	AJoA_Figure* GetCommandFigure() const;
	void CollectOwnFigures(
		TArray<AJoA_Figure*>& OutFigures) const;
	void BroadcastSelection();

	/* Local picture of the selection and of the targets
	 * in reach. Markers are never replicated. */
	void SetupMarkers();
	void RefreshMarkers();
	AJoA_FigureMarker* SpawnMarker();
	void RefreshCellMarkers(const AJoA_Figure* Selected);

	/* Click in the field. A target in reach is attacked
	 * or healed, an own figure is selected. */
	void HandleWorldClick();
	void HandleWorldRightClick();
	void ApplyClickOn(AJoA_Figure* Clicked);
	AJoA_Figure* FindFigureUnderCursor() const;

	/* Once, on the local controller. The Blueprint
	 * class is set in PlayWidgetClass. */
	void CreatePlayWidget();

	void BindHudEvents();
	void SyncOpeningThrowWidget();
	void CreateOpeningThrowWidget();
	void RemoveOpeningThrowWidget();

	UFUNCTION(Server, Reliable)
	void ServerSubmitOpeningThrow(EOpeningThrow Throw);

	UFUNCTION(Server, Reliable)
	void ServerLeaveMatch();

	/* Closes the link a moment after ServerLeaveMatch, so
	 * the call is on the wire before the link goes. */
	void CloseConnection();

	UFUNCTION(Server, Reliable)
	void ServerSelectFigure(AJoA_Figure* Figure);

	UFUNCTION(Client, Reliable)
	void ClientSyncSelectedFigure(AJoA_Figure* Figure);

	UFUNCTION(Server, Reliable)
	void ServerRequestAttack(
		AJoA_Figure* Attacker,
		AJoA_Figure* Target);

	UFUNCTION(Server, Reliable)
	void ServerRequestHeal(
		AJoA_Figure* Healer,
		AJoA_Figure* Target);

	UFUNCTION(Server, Reliable)
	void ServerRequestDestroyObstacle(
		AJoA_Figure* Attacker,
		int32 X,
		int32 Y);

	UPROPERTY()
	TObjectPtr<AJoA_ViewCamera> ViewCamera;

	/* Maps the test keys. Loaded from
	 * /Game/Input/IMC_ArenaTest when unset. */
	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputMappingContext> TestMappingContext;

	/* WBP_Play, reparented to UJoA_W_Play. When empty,
	 * /Game/WBP/WBP_Play is loaded. */
	UPROPERTY(EditDefaultsOnly, Category = "UI")
	TSubclassOf<UJoA_W_Play> PlayWidgetClass;

	/* WBP_Game, reparented to UJoA_W_Game. When empty,
	 * /Game/WBP/WBP_Game is loaded. */
	UPROPERTY(EditDefaultsOnly, Category = "UI")
	TSubclassOf<UJoA_W_Game> GameWidgetClass;

	UPROPERTY(Transient)
	TObjectPtr<UJoA_W_Play> PlayWidget;

	UPROPERTY(Transient)
	TObjectPtr<UJoA_W_Game> GameWidget;

	/* Pawn may stay the view pawn on a client.
	 * HUD and wheel read this instead. */
	UPROPERTY(ReplicatedUsing = OnRep_SelectedFigure)
	TObjectPtr<AJoA_Figure> SelectedFigure;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UInputAction>> SelectSlotActions;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> SelectNextAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> SelectPreviousAction;

	/* Same commands as the HUD turn and Go
	 * buttons. Bound to Q, W, and E. */
	UPROPERTY(Transient)
	TObjectPtr<UInputAction> TurnLeftAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> GoAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> TurnRightAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputMappingContext> SelectionContext;

	UPROPERTY(Transient)
	TObjectPtr<AJoA_FigureMarker> SelectionMarker;

	// Rings on squares: the free step and a breakable wall.
	UPROPERTY(Transient)
	TObjectPtr<AJoA_FigureMarker> StepMarker;

	UPROPERTY(Transient)
	TObjectPtr<AJoA_FigureMarker> ObstacleMarker;

	UPROPERTY(Transient)
	TObjectPtr<AJoA_FigureMarker> FlagMarker;

	UPROPERTY(Transient)
	TMap<
		TObjectPtr<AJoA_Figure>,
		TObjectPtr<AJoA_FigureMarker>> TargetMarkers;

	FTimerHandle LeaveTimer;

	FDelegateHandle BoardHandle;
	bool bHudEventsBound = false;
	EOpeningThrow LocalOpeningThrow =
		EOpeningThrow::None;

	bool bMarkersReady = false;
	bool bGameInputReady = false;

	bool bTestInputReady = false;
	bool bLoggedInputTick = false;
	bool bGameViewportFocused = false;
	int32 FocusAttempts = 0;
	int32 InputSetupAttempts = 0;

	/* Copies the menu checkbox onto this controller.
	 * The figure reads the server copy, so a client
	 * sends the choice once the connection exists. */
	void ApplyMenuAutoPickup();

	UFUNCTION(Server, Reliable)
	void ServerSetPickup(bool bEnabled);

	bool bMenuAutoPickupApplied = false;
};
