// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

// --- Engine ---
#include "Engine/TimerHandle.h"
#include "GameFramework/Pawn.h"

// --- GAS ---
#include "AbilitySystemInterface.h"

// --- GAME ---
#include "Player/Units/JoA_CombatLibrary.h"

#include "JoA_Figure.generated.h"

class AJoA_Arrow;
class AJoA_Flag;
class AJoA_PlayerState;
struct FOnAttributeChangeData;
class UJoA_GAS_AbilitySystemComponent;
class UJoA_GAS_CombatSet;
class UJoA_GAS_HealthSet;
class UJoA_GAS_ManaSet;
class UJoA_GAS_StaminaSet;
class UAnimSequence;
class USkeletalMeshComponent;
class UStaticMeshComponent;

/**
 * One board figure. Stats live here because one player
 * fields several figures, and one ASC holds one
 * attribute set of a given class.
 * The server resolves attacks. Turn order stays on the
 * GameMode. Combo memory lives on AJoA_PlayerState and
 * is shared by that player's figures.
 * Stats are granted once the owning state is known.
 * The figure the player picked is possessed by that
 * player's controller. Only that figure has the key
 * bindings of its abilities, and only that figure may
 * send ability requests to the server.
 * A dead figure leaves the registry and stands beside
 * the board.
 */
UCLASS(Blueprintable)
class JOA_ARENA_API AJoA_Figure
	: public APawn
	, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	AJoA_Figure();

	virtual void OnConstruction(
		const FTransform& Transform) override;

	virtual void Tick(float DeltaSeconds) override;

	virtual void GetLifetimeReplicatedProps(
		TArray<FLifetimeProperty>& OutLifetimeProps
	) const override;

	virtual UAbilitySystemComponent*
		GetAbilitySystemComponent() const override;

#if UE_WITH_IRIS
	virtual void RegisterReplicationFragments(
		UE::Net::FFragmentRegistrationContext& Context,
		UE::Net::EFragmentRegistrationFlags
			RegistrationFlags) override;
#endif

	/* Grants sets, base values, abilities, effects,
	 * and tags once. Server: SetOwningPlayer.
	 * Client: OnRep_OwningPlayerState. */
	void InitializeAbilitySystem();

	bool IsDead() const
	{
		return bIsDead;
	}

	/* True after the death clip, once the body
	 * stands on the board edge. */
	bool IsOnBench() const
	{
		return bOnBench;
	}

	/* True after this figure reached the enemy
	 * home file without a flag. */
	UFUNCTION(BlueprintPure, Category = "Unit")
	bool IsPromoted() const
	{
		return bPromoted;
	}

	/* Rebinds the keys of every granted ability to the
	 * controller that possesses this figure, or drops
	 * them when there is none. */
	void RefreshAbilityInput();

	/* True while it is this figure's turn to act. A
	 * world without AJoA_GameState has no turn rule. */
	UFUNCTION(BlueprintPure, Category = "Turn")
	bool IsAllowedToAct() const;

	/* One square, relative to Facing. Off the board,
	 * onto an obstacle, onto a living figure, a second
	 * move in the turn, or the wrong turn: false. The
	 * controls and the ability both ask this. */
	UFUNCTION(BlueprintPure, Category = "Turn")
	bool CanStep(EStepDirection Direction) const;

	// Turning is free and does not commit the figure.
	UFUNCTION(BlueprintPure, Category = "Turn")
	bool CanTurn() const;

	UFUNCTION(BlueprintPure, Category = "Turn")
	bool CanSkipTurn() const;

	/* Own turn, before a step, an attack, or a
	 * heal. Those actions already end the turn.
	 * Turning does not. The whole seat is paid. */
	UFUNCTION(BlueprintPure, Category = "Turn")
	bool CanRest() const;

	/* Climb onto the obstacle in front. Height
	 * may rise by one, or by two when promoted.
	 * HeavyKnight never can. */
	UFUNCTION(BlueprintPure, Category = "Turn")
	bool CanJump() const;

	/* Step down from height 1 or 2 onto the
	 * square in front. Height 2 onto empty
	 * costs 1 health, unless promoted.
	 * HeavyKnight never can. */
	UFUNCTION(BlueprintPure, Category = "Turn")
	bool CanFall() const;

	FIntPoint GetStepTarget(EStepDirection Direction) const;

	/* Local entry points of the controls. They send
	 * the ability request, like a key press does. */
	void RequestStep(EStepDirection Direction);
	void RequestTurn(ETurnDirection Direction);
	void RequestSkipTurn();
	void RequestRest();
	void RequestJump();
	void RequestFall();
	void RequestPickupFlag();
	void RequestDropFlag();
	void RequestBuildObstacle();

	/* Server only. Called by the abilities. */
	bool ResolveStep(EStepDirection Direction);
	bool ResolveTurn(ETurnDirection Direction);
	bool ResolveSkipTurn();
	bool ResolveRest();
	bool ResolveJump();
	bool ResolveFall();
	bool ResolvePickupFlag();
	bool ResolveDropFlag();

	UFUNCTION(BlueprintPure, Category = "Flag")
	bool CanPickupFlag() const;

	/* Grounded flag on this cell, or on the cell
	 * this figure faces. Null when pickup is not
	 * allowed, so the ability cannot run. */
	AJoA_Flag* FindPickupFlag() const;

	UFUNCTION(BlueprintPure, Category = "Flag")
	bool CanDropFlag() const;

	AJoA_Flag* GetHeldFlag() const
	{
		return HeldFlag;
	}

	USkeletalMeshComponent* GetBodyMesh() const
	{
		return Mesh;
	}

	/* Possession makes this pawn the view
	 * target. The board camera stays in
	 * use, not the default eye height. */
	virtual void CalcCamera(
		float DeltaTime,
		FMinimalViewInfo& OutResult) override;
	virtual void GetActorEyesViewPoint(
		FVector& OutLocation,
		FRotator& OutRotation) const override;
	virtual FVector GetPawnViewLocation() const override;

	/* Local range test on the replicated board. The
	 * client rejects a click before the controller
	 * sends the server call. */
	UFUNCTION(BlueprintPure, Category = "Combat")
	bool IsCellReachable(FIntPoint Cell) const;

	/* Local attack test, including this unit's
	 * obstacle rule and the attacker's stamina. */
	UFUNCTION(BlueprintPure, Category = "Combat")
	bool CanAttackTarget(const AJoA_Figure* Target) const;

	/* Allied heal, or the medic himself.
	 * Mana spent equals the health restored.
	 * Health already at 10 takes nothing. */
	UFUNCTION(BlueprintPure, Category = "Combat")
	bool CanHealTarget(const AJoA_Figure* Target) const;

	/* Local test. HeavyKnight only, and only the first
	 * obstacle within Range. */
	UFUNCTION(BlueprintPure, Category = "Combat")
	bool CanDestroyObstacleAt(int32 X, int32 Y) const;

	/* The obstacle this figure could hit now, on its
	 * current facing. False when it cannot hit one. */
	bool FindDestroyableObstacle(FIntPoint& OutCell) const;

	/* Local test. LightInfantry only. The one square
	 * in front, empty or height 1. A carried flag,
	 * a figure, or a grounded flag blocks it. */
	bool CanBuildObstacleAt(int32 X, int32 Y) const;
	bool FindBuildableCell(FIntPoint& OutCell) const;
	bool FindBuildableCellFacing(
		EFacing InFacing,
		FIntPoint& OutCell) const;

	/* Same tests as above, for a facing the figure could
	 * turn to. Turning is free, so the auto pass asks
	 * this for every facing. */
	bool IsCellReachableFacing(
		FIntPoint Cell,
		EFacing InFacing) const;
	bool CanAttackTargetFacing(
		const AJoA_Figure* Target,
		EFacing InFacing) const;
	bool CanHealTargetFacing(
		const AJoA_Figure* Target,
		EFacing InFacing) const;
	bool IsHealRowOpen(
		FIntPoint Cell,
		EFacing InFacing) const;

	/* True when this figure can still step, attack,
	 * heal, hit an obstacle, build, jump, or fall
	 * this turn, after any free turn. The server
	 * passes the turn when no figure can. OutReason
	 * names the first action found. */
	bool HasAnyAction(FString* OutReason = nullptr) const;

	bool CanJumpFacing(EFacing InFacing) const;
	bool CanFallFacing(EFacing InFacing) const;

	/* Server only. Every machine shows the combat text
	 * over this figure: Miss, Hit 2, or Heal 2. */
	UFUNCTION(NetMulticast, Reliable)
	void MulticastCombatText(
		ECombatTextKind Kind,
		int32 Amount);

	/* Clients play the clip. The dedicated
	 * server has no mesh pose to show. */
	UFUNCTION(NetMulticast, Reliable)
	void MulticastBodyAnim(EFigureAnim Anim);

	UFUNCTION(NetMulticast, Unreliable)
	void MulticastLaunchArrow(
		FVector From,
		FVector To,
		bool bHaltAtEnd);

	EFigureAnim GetPlayingBodyAnim() const
	{
		return PlayingBodyAnim;
	}

	/* A click or a portrait pick. Living figures
	 * stand back to idle. */
	void NotifyPicked();

	/* Server only. Living unused figures of the
	 * ended seat sit, then lie. Edge figures
	 * advance their rest cycle when nobody
	 * clapped or cried this turn. */
	void HandleTurnEnded(int32 EndedSeat);

	/* WinnerSeat claps. The other seat cries. */
	static void BroadcastGoodNews(
		const UObject* WorldContext,
		int32 WinnerSeat,
		AJoA_Figure* DelayOn = nullptr,
		AJoA_Figure* Skip = nullptr);

	/* VictimSeat cries. The other seat cheers. */
	static void BroadcastBadNews(
		const UObject* WorldContext,
		int32 VictimSeat,
		AJoA_Figure* DelayOn = nullptr,
		AJoA_Figure* Skip = nullptr);

	/* -1 random, 0 idle, 1 wave. Tests pin the
	 * 50% bench restart. */
	static void SetForcedBenchStart(int32 WaveIfOne);

	/* Tests force the next attack roll. */
	static bool bForceCombatHit;
	static bool bForceCombatMiss;

	/* Eight-way ring helpers. One step is 45 degrees,
	 * positive is to the right. */
	static EFacing RotateFacingBy(
		EFacing InFacing,
		int32 Steps);
	static int32 FacingRingIndex(EFacing InFacing);
	static bool IsDiagonalFacing(EFacing InFacing);

	/* Server only. Called on that player's turn. Out of
	 * range, a medic, a dead figure, or the same player
	 * spends nothing and leaves the combo unchanged. */
	UFUNCTION(
		BlueprintCallable,
		BlueprintAuthorityOnly,
		Category = "Combat")
	FAttackResult ResolveAttack(AJoA_Figure* Target);

	/* Server only. Heals an ally in range, or himself.
	 * Mana drops by the health actually restored.
	 * Health already at 10 spends nothing. */
	UFUNCTION(
		BlueprintCallable,
		BlueprintAuthorityOnly,
		Category = "Combat")
	FHealResult ResolveHeal(AJoA_Figure* Target);

	/* Server only. HeavyKnight lowers the obstacle by 1.
	 * Attack is ignored. Height 2 becomes 1. Height 1
	 * becomes 0. */
	UFUNCTION(
		BlueprintCallable,
		BlueprintAuthorityOnly,
		Category = "Combat")
	bool ResolveDestroyObstacle(int32 X, int32 Y);

	/* Server only. Raises the square in front by 1
	 * and ends the turn. */
	bool ResolveBuildObstacle();

	UFUNCTION(
		BlueprintCallable,
		BlueprintAuthorityOnly,
		Category = "Board")
	void SetBoardPlacement(
		int32 InX,
		int32 InY,
		EFacing InFacing);

	UFUNCTION(
		BlueprintCallable,
		BlueprintAuthorityOnly,
		Category = "Board")
	void SetOwningPlayer(
		AJoA_PlayerState* InPlayerState,
		int32 InPlayerIndex);

	UFUNCTION(BlueprintPure, Category = "Unit")
	bool UsesStamina() const;

	/* Extra stamina a carried flag adds to a
	 * step. The portrait prints it beside the
	 * boot. */
	float GetHeldFlagSpendPenalty() const;

	UFUNCTION(BlueprintPure, Category = "Attributes")
	UJoA_GAS_HealthSet* GetHealthSet() const;

	UFUNCTION(BlueprintPure, Category = "Attributes")
	UJoA_GAS_StaminaSet* GetStaminaSet() const;

	UFUNCTION(BlueprintPure, Category = "Attributes")
	UJoA_GAS_ManaSet* GetManaSet() const;

	UFUNCTION(BlueprintPure, Category = "Attributes")
	UJoA_GAS_CombatSet* GetCombatSet() const;

	AJoA_PlayerState* GetOwningPlayerState() const
	{
		return OwningPlayerState;
	}

	UPROPERTY(
		EditAnywhere,
		ReplicatedUsing = OnRep_UnitType,
		BlueprintReadOnly,
		Category = "Unit",
		meta = (ExposeOnSpawn = true))
	EUnitType UnitType = EUnitType::HeavyKnight;

	UPROPERTY(
		EditAnywhere,
		ReplicatedUsing = OnRep_BoardPlacement,
		BlueprintReadOnly,
		Category = "Board",
		meta = (
			ExposeOnSpawn = true,
			ClampMin = "0",
			ClampMax = "7"))
	int32 CellX = 0;

	UPROPERTY(
		EditAnywhere,
		ReplicatedUsing = OnRep_BoardPlacement,
		BlueprintReadOnly,
		Category = "Board",
		meta = (
			ExposeOnSpawn = true,
			ClampMin = "0",
			ClampMax = "7"))
	int32 CellY = 0;

	UPROPERTY(
		EditAnywhere,
		ReplicatedUsing = OnRep_BoardPlacement,
		BlueprintReadOnly,
		Category = "Board",
		meta = (ExposeOnSpawn = true))
	EFacing Facing = EFacing::PositiveX;

	/* INDEX_NONE until SetOwningPlayer. Seat 0 is
	 * French. Seat 1 is English. One seat cannot
	 * attack its own figures. */
	UPROPERTY(
		EditAnywhere,
		ReplicatedUsing = OnRep_OwningPlayerIndex,
		BlueprintReadOnly,
		Category = "Board",
		meta = (ExposeOnSpawn = true))
	int32 OwningPlayerIndex = INDEX_NONE;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(
		const EEndPlayReason::Type EndPlayReason
	) override;
#if WITH_EDITOR
	virtual void PostEditChangeProperty(
		FPropertyChangedEvent& PropertyChangedEvent
	) override;
#endif
	virtual void PossessedBy(
		AController* NewController) override;
	virtual void UnPossessed() override;
	virtual void OnRep_Controller() override;

	UFUNCTION()
	void OnRep_OwningPlayerState();

	UFUNCTION()
	void OnRep_IsDead();

	/* Actor info holds the controller. It must be
	 * refreshed whenever possession changes. */
	void HandleControllerChanged();

	// Server only. Health at 0 removes the figure.
	void OnHealthChanged(const FOnAttributeChangeData& Data);
	void HandleDeath();
	void ApplyDeathState();
	void PlaceOnBench();

	// Null on a client. Only the server has a GameMode.
	class AJoA_GameMode* GetArenaGameMode() const;

	UFUNCTION()
	void OnRep_OwningPlayerIndex();

	UFUNCTION()
	void OnRep_UnitType();

	UFUNCTION()
	void OnRep_BoardPlacement();

	UFUNCTION()
	void OnRep_HeldFlag();

	UFUNCTION()
	void OnRep_bPromoted();

	UFUNCTION()
	void OnRep_BodyCue();

	/* Obstacle height can change without a move.
	 * The body must sit on the new top. */
	void HandleBoardChanged();

	void ApplyBodyMesh();
	void ApplyBoardTransform();
	void ApplyTeamColor();
	void ApplyHeldItems();
	void ApplyStowedItems();
	void RefreshHeldItemPresentation();
	bool ShouldStowHeldItems() const;

	/* Game worlds slide. A dedicated server snaps.
	 * A board refresh must not play Run. That clip
	 * is only for a real step. */
	void StartSlide(
		const FVector& Target,
		bool bPlayMoveClip);
	void SnapToCell(const FVector& Target);
	bool CanSlideTo(const FVector& Target) const;

	/* Local client moves the mesh before the server
	 * answers. A reject has no replication, so the
	 * timer puts the mesh back on the server cell. */
	void PredictMoveTo(const FIntPoint& Cell);
	void ArmVisualConfirm();
	void ConfirmVisual();

	/* Server only. Positive amounts. Zero skips
	 * that meta. Gain is the rest reward. The
	 * execution commits the change. */
	void ApplyVitalChange(
		float Damage,
		float Healing,
		float StaminaSpend,
		float ManaSpend,
		float StaminaGain = 0.f,
		float ManaGain = 0.f);

	void TryAutoPickupFlag();
	bool WantsAutoPickupFlag() const;

	/* Grounded flag on that cell, and this
	 * player picks flags up by stepping. */
	bool ShouldDeferFlagArrival(
		const FIntPoint& Cell) const;

	/* Pickup finished. Walk onto the flag cell. */
	void ReleaseDeferredArrival();
	void BeginArrivalWalk();
	void RewindArrivalSlide();
	void SetHeldFlag(AJoA_Flag* Flag);
	void ApplyFlagCarryEffect();
	void DropHeldFlag();
	void NotifyFlagCarried();

	/* Server only. Grows this figure once if it
	 * stands on the enemy home file and holds no
	 * flag. A second visit must not stack. */
	void TryPromoteOnEnemyHome();
	void ApplyPromotion();
	void ApplyBodyScale();
	void ApplyPromotedVisual();

	void PlayBodyAnim(EFigureAnim Anim);
	void PlayBodyAnim(
		EFigureAnim Anim,
		bool bLoop);

	/* The server multicasts, so every machine plays the
	 * clip. A client has no authority and plays it only
	 * on its own machine. */
	void PlayOrMulticastBodyAnim(EFigureAnim Anim);
	void PlayActionCue(EFigureAnim Anim);
	void ResumeIdleAnim();
	void PlayRestFromUnused();
	void ApplySeatRestPose();
	void MarkActiveThisTurn();
	void StandFromIncomingAttack();
	void AdvanceBenchRest();
	void PlayBenchStart();
	void PlayTeamNews(EFigureAnim Anim);
	void QueueFollowUp(EFigureAnim Anim);
	void KeepNetBodyAnim();
	UAnimSequence* FindBodyAnim(
		EFigureAnim Anim,
		bool bLoopPose = false) const;
	float BodyAnimSeconds(EFigureAnim Anim) const;
	void ArmDeferred(float Seconds);
	void CommitDeferred();
	void ClearDeferred();
	void LaunchMatchArrow(
		const FVector& From,
		const FVector& To,
		bool bHaltAtEnd);
	static void BroadcastTeamNews(
		const UObject* WorldContext,
		int32 FirstSeat,
		EFigureAnim FirstAnim,
		EFigureAnim OtherAnim,
		AJoA_Figure* DelayOn,
		AJoA_Figure* Skip = nullptr);
	static void BroadcastSeatNews(
		const UObject* WorldContext,
		int32 Seat,
		EFigureAnim Anim,
		AJoA_Figure* Skip);

	UPROPERTY(
		VisibleAnywhere,
		BlueprintReadOnly,
		Category = "Abilities")
	TObjectPtr<UJoA_GAS_AbilitySystemComponent>
		AbilitySystemComponent;

	UPROPERTY(
		ReplicatedUsing = OnRep_OwningPlayerState,
		BlueprintReadOnly,
		Category = "Board")
	TObjectPtr<AJoA_PlayerState> OwningPlayerState;

	/* Set once when health reaches 0. The death
	 * clip plays on the square. The body moves to
	 * the edge after that clip. */
	bool bOnBench = false;

	/* Set once when health reaches 0. The client
	 * plays the death clip from OnRep. */
	UPROPERTY(
		ReplicatedUsing = OnRep_IsDead,
		BlueprintReadOnly,
		Category = "Board")
	bool bIsDead = false;

	UPROPERTY(
		ReplicatedUsing = OnRep_bPromoted,
		BlueprintReadOnly,
		Category = "Board")
	bool bPromoted = false;

	UPROPERTY(
		ReplicatedUsing = OnRep_HeldFlag,
		BlueprintReadOnly,
		Category = "Flag")
	TObjectPtr<AJoA_Flag> HeldFlag;

	/* After a drop, auto pickup waits until this
	 * figure leaves the cell. I still picks up. */
	bool bSuppressFlagAutoPickup = false;

	/* Auto pickup plays on the square just left.
	 * The mesh walks to the flag after that clip. */
	bool bDeferStepArrival = false;

	FDelegateHandle HealthChangedHandle;
	FDelegateHandle BoardChangedHandle;

	UPROPERTY(VisibleAnywhere, Category = "Figure")
	TObjectPtr<USceneComponent> SceneRoot;

	/* KayKit body. The mesh origin is the feet.
	 * Mesh yaw is the facing. */
	UPROPERTY(VisibleAnywhere, Category = "Figure")
	TObjectPtr<USkeletalMeshComponent> Mesh;

	/* Prop on handslot_l. Hidden when this
	 * unit holds nothing in the left hand. */
	UPROPERTY(VisibleAnywhere, Category = "Figure")
	TObjectPtr<UStaticMeshComponent> LeftHandItem;

	/* Prop on handslot_r. Hidden when this
	 * unit holds nothing in the right hand. */
	UPROPERTY(VisibleAnywhere, Category = "Figure")
	TObjectPtr<UStaticMeshComponent> RightHandItem;

	/* Left-hand weapon parked on the back
	 * while sitting, lying, or carrying a flag. */
	UPROPERTY(VisibleAnywhere, Category = "Figure")
	TObjectPtr<UStaticMeshComponent> BackWeaponItem;

	/* Knight shield on the back, after the
	 * sword. Hidden for other unit types. */
	UPROPERTY(VisibleAnywhere, Category = "Figure")
	TObjectPtr<UStaticMeshComponent> BackShieldItem;

	/* Mage closed book on the cell in front
	 * while stowed. Hidden for other types. */
	UPROPERTY(VisibleAnywhere, Category = "Figure")
	TObjectPtr<UStaticMeshComponent> GroundBookItem;

	FVector SlideFrom;
	FVector SlideTo;
	float SlideElapsed = 0.f;
	bool bSliding = false;

	/* The first placement is a snap. A spawn at the
	 * origin must not slide onto its opening square. */
	bool bPlacedOnBoard = false;
	int32 AppliedCellX = INDEX_NONE;
	int32 AppliedCellY = INDEX_NONE;

	/* True after a local predict, until the server
	 * cell arrives or the confirm timer gives up. */
	bool bAwaitingConfirm = false;

	/* The server reuses this for the walk that
	 * follows an automatic flag pickup. */
	FTimerHandle VisualConfirmHandle;
	FTimerHandle BodyAnimHandle;
	FTimerHandle DeferredHandle;
	bool bResolvePending = false;

	/**
	 * What an action applies once its clip reaches the
	 * impact frame, or the arrow lands. The server rolls
	 * the result at once and holds it here until then.
	 */
	struct FDeferredResolveData
	{
		EDeferredResolve Kind = EDeferredResolve::None;
		TWeakObjectPtr<AJoA_Figure> Target;
		TWeakObjectPtr<AJoA_Flag> Flag;
		float Damage = 0.f;
		float Healing = 0.f;
		float ManaSpend = 0.f;
		float TargetStamina = 0.f;
		int32 ObstacleX = 0;
		int32 ObstacleY = 0;
		uint8 ObstacleHeight = 0;
		bool bHit = false;
		bool bEndTurn = false;
		bool bGoodNews = false;
		bool bBadNews = false;
		int32 NewsSeat = INDEX_NONE;
		ECombatTextKind TextKind = ECombatTextKind::Miss;
		int32 TextAmount = 0;
	};

	FDeferredResolveData Deferred;
	float SlideDuration = 0.4f;

	/* Server body clip. A board refresh on the
	 * client must not replace Hit, Cry, or Cheer
	 * with Run. BodyCue repeats the same clip. */
	UPROPERTY(Replicated)
	EFigureAnim NetBodyAnim = EFigureAnim::Idle;

	UPROPERTY(ReplicatedUsing = OnRep_BodyCue)
	int32 BodyCue = 0;

	EFigureAnim PlayingBodyAnim = EFigureAnim::Idle;
	EFigureAnim FollowUpAnim = EFigureAnim::Idle;
	bool bHasFollowUp = false;
	/* True only while a square step asks
	 * for Run. A hit must not start it. */
	bool bArmedStepRun = false;
	/* The client already played this from
	 * prediction. The host still plays.
	 * A new slide clears it. */
	bool bMoveCuePlayed = false;
	/* Client keeps this clip until it ends.
	 * A board refresh must not swap in Run. */
	bool bHoldBodyAnim = false;
	bool bTouchedThisTurn = false;
	bool bRestedThisTurn = false;
	bool bReactedThisTurn = false;
	bool bSkipNextBenchAdvance = false;
	bool bBenchIntro = false;
	int32 UnusedOwnerTurns = 0;
	EBenchPhase BenchPhase = EBenchPhase::None;
	static int32 ForcedBenchStart;
};
