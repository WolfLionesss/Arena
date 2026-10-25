// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "CoreMinimal.h"

// --- Engine ---
#include "Blueprint/UserWidget.h"
#include "Editor.h"
#include "Engine/Engine.h"
#include "Engine/NetConnection.h"
#include "EngineUtils.h"
#include "FileHelpers.h"
#include "Misc/App.h"
#include "Misc/AutomationTest.h"
#include "Framework/Application/IInputProcessor.h"
#include "Framework/Application/SlateApplication.h"
#include "Layout/WidgetPath.h"
#include "Settings/LevelEditorPlaySettings.h"
#include "Tests/AutomationCommon.h"
#include "Widgets/SWindow.h"

// --- GAME ---
#include "Match/JoA_GameMode.h"
#include "Player/JoA_GameState.h"
#include "Player/JoA_PlayerController.h"
#include "Player/JoA_PlayerState.h"
#include "Board/JoA_Flag.h"
#include "Board/JoA_GridManager.h"
#include "Match/JoA_GameInstance.h"
#include "Match/JoA_MatchSettings.h"
#include "Player/JoA_Nation.h"
#include "Player/Units/JoA_Figure.h"
#include "Player/Units/JoA_FigureMarker.h"
#include "Player/Units/JoA_FigureRegistry.h"
#include "Player/Units/JoA_UnitTypes.h"
#include "UI/JoA_FloatingText.h"
#include "UI/JoA_W_CharacterStat.h"
#include "UI/JoA_W_Play.h"

// --- GAS ---
#include "AbilitySystemComponent.h"
#include "GAS/JoA_GAS_CombatSet.h"
#include "GAS/JoA_GAS_HealthSet.h"
#include "GAS/JoA_GAS_ManaSet.h"
#include "GAS/JoA_GAS_StaminaSet.h"

// --- Components ---
#include "Components/PanelWidget.h"
#include "Components/SkeletalMeshComponent.h"

namespace JoATest
{
/* Length of every pause in a watched run. Change it in
 * the console: JoA.Test.HoldSeconds 30. */
static TAutoConsoleVariable<float> CVarHoldSeconds(
	TEXT("JoA.Test.HoldSeconds"),
	60.f,
	TEXT("Seconds each JoA test pause lasts."));

/* Watched: started from the editor UI, or with -JoAWatch
 * on the command line. Pauses are kept and PIE stays open
 * at the end. FApp::IsUnattended() cannot tell, it is true
 * during any automation test. Only the command line flag
 * says whether somebody is looking. */
static bool IsWatched()
{
	return !FParse::Param(
			FCommandLine::Get(),
			TEXT("Unattended"))
		|| FParse::Param(
			FCommandLine::Get(),
			TEXT("JoAWatch"));
}

	UWorld* FindPieWorld(bool bServer)
	{
		for (const FWorldContext& Context :
			GEditor->GetWorldContexts())
		{
			UWorld* World = Context.World();
			if (Context.WorldType != EWorldType::PIE
				|| !World
				|| !World->HasBegunPlay())
			{
				continue;
			}

			const ENetMode Mode = World->GetNetMode();
			if (bServer ? Mode == NM_ListenServer
				: Mode == NM_Client)
			{
				return World;
			}
		}
		return nullptr;
	}

	UWorld* FindPieAny()
	{
		for (const FWorldContext& Context :
			GEditor->GetWorldContexts())
		{
			UWorld* World = Context.World();
			if (Context.WorldType == EWorldType::PIE
				&& World
				&& World->HasBegunPlay())
			{
				return World;
			}
		}
		return nullptr;
	}

	AJoA_PlayerController* LocalPC(UWorld* World)
	{
		return World
			? Cast<AJoA_PlayerController>(
				World->GetFirstPlayerController())
			: nullptr;
	}

	// The server copy of the controller of the client.
	AJoA_PlayerController* RemotePC(UWorld* World)
	{
		if (!World)
		{
			return nullptr;
		}

		for (FConstPlayerControllerIterator It =
				World->GetPlayerControllerIterator();
			It;
			++It)
		{
			AJoA_PlayerController* PC =
				Cast<AJoA_PlayerController>(It->Get());
			if (PC && !PC->IsLocalController())
			{
				return PC;
			}
		}
		return nullptr;
	}

	AJoA_Figure* FindFigure(
		UWorld* World,
		EUnitType Type,
		int32 Seat)
	{
		if (!World)
		{
			return nullptr;
		}

		for (TActorIterator<AJoA_Figure> It(World); It; ++It)
		{
			if (It->UnitType == Type
				&& It->OwningPlayerIndex == Seat)
			{
				return *It;
			}
		}
		return nullptr;
	}

	AJoA_Flag* FindFlag(UWorld* World, int32 Seat)
	{
		if (!World)
		{
			return nullptr;
		}

		for (TActorIterator<AJoA_Flag> It(World); It; ++It)
		{
			if (It->GetOwningPlayerIndex() == Seat)
			{
				return *It;
			}
		}
		return nullptr;
	}

	int32 CountLiving(UWorld* World)
	{
		TArray<AJoA_Figure*> Living;
		if (const UJoA_FigureRegistry* Registry =
			UJoA_FigureRegistry::Get(World))
		{
			Registry->CollectFigures(Living);
		}
		return Living.Num();
	}

	/* Stat widgets that sit in a panel. A removed widget
	 * still exists until garbage collection but has no
	 * parent. */
	int32 CountStatWidgets(UWorld* World)
	{
		int32 Count = 0;
		for (TObjectIterator<UJoA_W_CharacterStat> It;
			It;
			++It)
		{
			const AJoA_Figure* Shown = It->GetFigure();
			if (Shown
				&& It->GetParent()
				&& Shown->GetWorld() == World)
			{
				++Count;
			}
		}
		return Count;
	}

	/* The stat card of this figure on this player's
	 * HUD. The other player's copy is a different
	 * widget and must not be read by mistake. */
	UJoA_W_CharacterStat* FindStat(
		const APlayerController* PC,
		const AJoA_Figure* Figure)
	{
		if (!PC || !Figure)
		{
			return nullptr;
		}

		for (TObjectIterator<UJoA_W_CharacterStat> It;
			It;
			++It)
		{
			if (It->GetOwningPlayer() == PC
				&& It->GetFigure() == Figure
				&& It->GetParent())
			{
				return *It;
			}
		}
		return nullptr;
	}

	int32 CountFloatingTexts(UWorld* World)
	{
		int32 Count = 0;
		for (TActorIterator<AJoA_FloatingText> It(World);
			It;
			++It)
		{
			++Count;
		}
		return Count;
	}

	/* A ray from the camera of that player through the
	 * figure, like the cursor ray of a click on it. */
	void RayThrough(
		const AJoA_PlayerController* PC,
		const AJoA_Figure* Figure,
		FVector& OutOrigin,
		FVector& OutEnd)
	{
		OutOrigin = PC->PlayerCameraManager->GetCameraLocation();
		const FVector Aim =
			Figure->GetActorLocation() + FVector(0, 0, 60);
		OutEnd = OutOrigin + (Aim - OutOrigin) * 2.0;
	}

	void ClickFigure(
		AJoA_PlayerController* PC,
		const AJoA_Figure* Figure)
	{
		FVector Origin;
		FVector End;
		RayThrough(PC, Figure, Origin, End);
		PC->ClickAlongRay(Origin, End);
	}

	/* True when the portrait receives a click at its
	 * centre. A widget laid over it would be in the path
	 * instead. Returns false and names the blocker. */
	bool PortraitReceivesClick(
		UJoA_W_CharacterStat* Widget,
		FString& OutBlocker)
	{
		const TSharedPtr<SWidget> Slate =
			Widget->GetCachedWidget();
		const FGeometry Geometry = Widget->GetCachedGeometry();
		if (!Slate.IsValid()
			|| Geometry.GetAbsoluteSize().X < 1.f
			|| !FSlateApplication::IsInitialized())
		{
			return true;
		}

		const TSharedPtr<SWindow> Window =
			FSlateApplication::Get().FindWidgetWindow(
				Slate.ToSharedRef());
		if (!Window.IsValid())
		{
			return true;
		}

		const FVector2f Center = Geometry.GetAbsolutePosition()
			+ Geometry.GetAbsoluteSize() * 0.5f;
		/* The cached geometry is in desktop space. */
		for (const FVector2f& Spot : { Center })
		{
			const FWidgetPath Path = FSlateApplication::Get()
				.LocateWindowUnderMouse(
					Spot,
					{ Window.ToSharedRef() },
					false,
					0);
			if (Path.IsValid()
				&& Path.ContainsWidget(Slate.Get()))
			{
				return true;
			}

			if (Path.IsValid() && Path.Widgets.Num() > 0)
			{
				OutBlocker.Empty();
				for (int32 I = 0; I < Path.Widgets.Num(); ++I)
				{
					OutBlocker += Path.Widgets[I].Widget
						->GetTypeAsString() + TEXT("(")
						+ Path.Widgets[I].Widget->GetVisibility()
							.ToString()
						+ TEXT(")>");
				}
			}
		}
		return false;
	}

	int32 CountDead(UWorld* World)
	{
		int32 Dead = 0;
		for (TActorIterator<AJoA_Figure> It(World); It; ++It)
		{
			Dead += It->IsDead() ? 1 : 0;
		}
		return Dead;
	}
	void DumpWidgets()
	{
		for (TObjectIterator<UJoA_W_CharacterStat> It;
			It;
			++It)
		{
			const AJoA_Figure* Shown = It->GetFigure();
			if (!Shown || !It->GetParent())
			{
				continue;
			}

			UE_LOG(
				LogTemp,
				Display,
				TEXT("JoATest widget %s net %d seat %d")
				TEXT(" type %d parent %s dead %d"),
				*It->GetName(),
				static_cast<int32>(
					Shown->GetWorld()->GetNetMode()),
				Shown->OwningPlayerIndex,
				static_cast<int32>(Shown->UnitType),
				*It->GetParent()->GetName(),
				Shown->IsDead() ? 1 : 0);
		}
	}
	void DumpWorlds()
	{
		for (const FWorldContext& Context :
			GEditor->GetWorldContexts())
		{
			UWorld* World = Context.World();
			if (!World)
			{
				continue;
			}

			int32 PCs = 0;
			for (FConstPlayerControllerIterator It =
					World->GetPlayerControllerIterator();
				It;
				++It)
			{
				++PCs;
			}

			UE_LOG(
				LogTemp,
				Display,
				TEXT("JoATest world %s type %d net %d")
				TEXT(" PCs %d widgets %d living %d dead %d"),
				*World->GetName(),
				static_cast<int32>(Context.WorldType),
				static_cast<int32>(World->GetNetMode()),
				PCs,
				CountStatWidgets(World),
				CountLiving(World),
				CountDead(World));
		}
	}
	float Attr(
		const AJoA_Figure* Figure,
		const FGameplayAttribute& Attribute)
	{
		const UAbilitySystemComponent* ASC = Figure
			? Figure->GetAbilitySystemComponent()
			: nullptr;
		return ASC ? ASC->GetNumericAttribute(Attribute) : -1.f;
	}

	TArray<FGameplayAttribute> AllAttributes()
	{
		return {
			UJoA_GAS_HealthSet::GetHealthAttribute(),
			UJoA_GAS_HealthSet::GetMaxHealthAttribute(),
			UJoA_GAS_StaminaSet::GetStaminaAttribute(),
			UJoA_GAS_StaminaSet::GetMaxStaminaAttribute(),
			UJoA_GAS_ManaSet::GetManaAttribute(),
			UJoA_GAS_ManaSet::GetMaxManaAttribute(),
			UJoA_GAS_CombatSet::GetAttackAttribute(),
			UJoA_GAS_CombatSet::GetDefenseAttribute(),
			UJoA_GAS_CombatSet::GetHealAttribute()
		};
	}

	bool SameAttributes(
		const AJoA_Figure* A,
		const AJoA_Figure* B)
	{
		if (!A || !B)
		{
			return false;
		}

		for (const FGameplayAttribute& Attribute :
			AllAttributes())
		{
			if (!FMath::IsNearlyEqual(
					Attr(A, Attribute),
					Attr(B, Attribute)))
			{
				return false;
			}
		}
		return true;
	}

	bool IsReady(UWorld* World)
	{
		const AJoA_PlayerController* PC = LocalPC(World);
		if (!PC
			|| !PC->PlayerState
			|| !AJoA_GameState::Get(World)
			|| CountLiving(World) != 8)
		{
			return false;
		}

		for (TActorIterator<AJoA_Figure> It(World); It; ++It)
		{
			if (!It->GetOwningPlayerState())
			{
				return false;
			}
		}
		return true;
	}

	int32 ActiveSeat(UWorld* World)
	{
		const AJoA_GameState* State =
			AJoA_GameState::Get(World);
		return State ? State->GetActiveSeat() : INDEX_NONE;
	}

	bool MatchStarted(UWorld* World)
	{
		const AJoA_GameState* State =
			AJoA_GameState::Get(World);
		return State && State->HasMatchStarted();
	}

	bool IsAt(const AJoA_Figure* Figure, FIntPoint Cell)
	{
		return Figure
			&& Figure->CellX == Cell.X
			&& Figure->CellY == Cell.Y;
	}

	bool Plays(
		const AJoA_Figure* Figure,
		EFigureAnim Anim)
	{
		return Figure
			&& Figure->GetPlayingBodyAnim() == Anim;
	}

	bool SeatPlays(
		UWorld* World,
		int32 Seat,
		EFigureAnim Anim)
	{
		if (!World)
		{
			return false;
		}

		bool bAny = false;
		for (TActorIterator<AJoA_Figure> It(World); It; ++It)
		{
			if (It->OwningPlayerIndex != Seat)
			{
				continue;
			}

			bAny = true;
			if (It->GetPlayingBodyAnim() != Anim)
			{
				return false;
			}
		}
		return bAny;
	}

	void SkipActive(UWorld* Server)
	{
		const int32 Seat = ActiveSeat(Server);
		if (Seat == INDEX_NONE || !Server)
		{
			return;
		}

		for (TActorIterator<AJoA_Figure> It(Server); It; ++It)
		{
			if (!It->IsDead()
				&& It->OwningPlayerIndex == Seat)
			{
				It->ResolveSkipTurn();
				return;
			}
		}
	}

	void Wound(AJoA_Figure* Figure, float Health)
	{
		UAbilitySystemComponent* ASC = Figure
			? Figure->GetAbilitySystemComponent()
			: nullptr;
		if (ASC)
		{
			ASC->SetNumericAttributeBase(
				UJoA_GAS_HealthSet::GetHealthAttribute(),
				Health);
		}
	}

	void ClearObstacles(UWorld* Server)
	{
		AJoA_GridManager* Grid =
			AJoA_GameMode::GetGridManager(Server);
		if (!Grid)
		{
			return;
		}

		for (int32 Y = 0; Y < GridSize; ++Y)
		{
			for (int32 X = 0; X < GridSize; ++X)
			{
				Grid->SetObstacleHeight(X, Y, 0);
			}
		}
	}

	void ParkSeat(
		UWorld* Server,
		int32 Seat,
		int32 FileX)
	{
		if (!Server)
		{
			return;
		}

		const int32 Slots[] = { 0, 1, 6, 7 };
		int32 Slot = 0;
		for (TActorIterator<AJoA_Figure> It(Server); It; ++It)
		{
			if (It->OwningPlayerIndex != Seat
				|| It->IsDead()
				|| Slot >= 4)
			{
				continue;
			}

			It->SetBoardPlacement(
				FileX,
				Slots[Slot],
				Seat == FrenchSeat
					? EFacing::NegativeX
					: EFacing::PositiveX);
			++Slot;
		}
	}

	bool LivingNewsDone(UWorld* World)
	{
		if (!World)
		{
			return false;
		}

		bool bAny = false;
		for (TActorIterator<AJoA_Figure> It(World); It; ++It)
		{
			if (It->IsDead())
			{
				continue;
			}

			bAny = true;
			const EFigureAnim Anim =
				It->GetPlayingBodyAnim();
			if (Anim == EFigureAnim::Clap
				|| Anim == EFigureAnim::Cheer
				|| Anim == EFigureAnim::Cry
				|| Anim == EFigureAnim::Hit
				|| Anim == EFigureAnim::Dodge)
			{
				return false;
			}
		}
		return bAny;
	}

	void KillFigure(AJoA_Figure* Figure)
	{
		UAbilitySystemComponent* ASC = Figure
			? Figure->GetAbilitySystemComponent()
			: nullptr;
		if (ASC)
		{
			ASC->SetNumericAttributeBase(
				UJoA_GAS_HealthSet::GetHealthAttribute(),
				0.f);
		}
	}

	bool CellHeightIs(
		UWorld* World,
		int32 X,
		int32 Y,
		uint8 Height)
	{
		const AJoA_GridManager* Grid =
			AJoA_GameMode::GetGridManager(World);
		FGridCell Cell;
		return Grid
			&& Grid->GetCell(X, Y, Cell)
			&& Cell.ObstacleHeight == Height;
	}

	/* Living figures only. A dead body does not
	 * count, and an empty board is not a pass. */
	bool NonePromoted(UWorld* World)
	{
		if (!World)
		{
			return false;
		}

		bool bAny = false;
		for (TActorIterator<AJoA_Figure> It(World); It; ++It)
		{
			if (It->IsDead())
			{
				continue;
			}

			bAny = true;
			if (It->IsPromoted())
			{
				return false;
			}
		}
		return bAny;
	}
}

enum class EScenario : int32
{
	Full,
	Stalemate,
	BlockedRecovers,
	MovePassesTurn,
	MoveKeepsTurn,
	TurnExpires,
	SeatDrop,
	SeatRejoin,
	ThirdPlayer,
	FlagCapture,
	FlagStamina,
	FlagCaptureEngland,
	Rest,
	BuildObstacle,
	JumpFall,
	PromotedJumpFall,
	Promotion,
	PromotionEngland,
	FigureAnims,
	MeleeReach,
	MedicHeal,
	ArcherShot,
	ObstacleBox,
	SplitBoard,
	StaminaPulse
};

enum class EStep : int32
{
	Start,
	WaitReady,
	VerifyAttributes,
	VerifyWidgets,
	CheckPortraitHits,
	RejectSelection,
	SelectFrench,
	StepFrench,
	WaitStepFrench,
	RejectSecondFigure,
	TurnRight,
	TurnLeft,
	PlaceTarget,
	Attack,
	SelectEnglish,
	StepEnglish,
	WaitStepEnglish,
	AutoPassEnglish,
	SkipFrench,
	HealEnglish,
	KillArcher,
	PossessLight,
	KillPossessed,
	KillRest,
	EndPlay,
	Done,

	// Stalemate scenario. Entered from WaitReady.
	StalemateSetup,
	StalemateFrenchSkips,
	StalemateEnglishSkips,
	StalemateFrenchSkipsAgain,
	RecoverFreeMedic,

	// Turn rule scenarios. Entered from WaitReady.
	MovePassesTurn,
	MoveKeepsTurn,

	// Network scenarios. Entered from WaitReady.
	TurnExpires,
	DropSeat,
	WaitForfeit,
	WaitRejoin,
	ThirdPlayer,

	FlagCaptureSetup,
	FlagCaptureStep,
	FlagStaminaSetup,
	FlagStaminaStep,
	FlagCaptureEnglandSkip,
	FlagCaptureEnglandSetup,
	FlagCaptureEnglandStep,

	// Rest. Entered from WaitReady.
	RestGrant,
	RestBlocked,

	// Light infantry builds the square in front.
	BuildMark,
	BuildRaise,

	// Jump onto height 1, then fall from 2.
	JumpRules,
	JumpClimb,
	JumpSkipEnglish,
	JumpFall,

	/* Promoted infantry jumps empty onto 2,
	 * then falls without losing health. */
	PromotedJumpPromote,
	PromotedJumpRules,
	PromotedJumpClimb,
	PromotedJumpSkipEnglish,
	PromotedJumpFall,

	/* French knight one file before English
	 * home, so one forward step promotes. */
	PromotionSetup,
	PromotionStep,

	/* English mage one file before French
	 * home, after the French skip. */
	PromotionEnglandSkip,
	PromotionEnglandSetup,
	PromotionEnglandStep,

	FigureAnimIdle,
	FigureAnimSit,
	FigureAnimSitEnglish,
	FigureAnimLie,
	FigureAnimLieEnglish,
	FigureAnimPick,
	FigureAnimClipRun,
	FigureAnimClipAttack,
	FigureAnimClipHeal,
	FigureAnimClipShoot,
	FigureAnimClipDodge,
	FigureAnimClipJump,
	FigureAnimClipFall,
	FigureAnimWound,
	FigureAnimNewsIdle,
	FigureAnimMiss,
	FigureAnimKill,
	FigureAnimKillBench,
	FigureAnimBenchSit,
	FigureAnimBenchLie,
	FigureAnimBenchNews,
	FigureAnimFlag,
	FigureAnimFlagBlock,
	FigureAnimFall,

	MeleeRules,
	MeleeClientHit,

	MedicHealRules,
	MedicHealFlag,

	ArcherShotRules,

	/* Last living figure is not a knight. Height 2
	 * fills every neighbour, so the seat must skip
	 * and loses when that happens again. */
	ObstacleBoxSetup,
	ObstacleBoxFrenchSkips,
	ObstacleBoxEnglishSkips,
	ObstacleBoxFrenchSkipsAgain,

	/* Files 3 and 4 are height 2, then both
	 * knights die. Nobody is promoted, so the
	 * match ends as a draw. */
	SplitBoardSetup,

	/* Knight with no stamina pulses. A full bar
	 * and the mage do not. Picking the knight
	 * again starts the three seconds over. */
	StaminaPulseQuiet,
	StaminaPulseSpent,
	StaminaPulseRun,
	StaminaPulseMage,
	StaminaPulseAgain
};

/* Takes the Enter key before any widget sees it. Enter
 * would press the focused button of the HUD, for example
 * Btn_Go, and play a move into the test. */
class FJoAEnterTrap : public IInputProcessor
{
public:
	virtual void Tick(
		const float DeltaTime,
		FSlateApplication& SlateApp,
		TSharedRef<ICursor> Cursor) override
	{
	}

	virtual bool HandleKeyDownEvent(
		FSlateApplication& SlateApp,
		const FKeyEvent& InKeyEvent) override
	{
		if (InKeyEvent.GetKey() != EKeys::Enter)
		{
			return false;
		}

		if (!InKeyEvent.IsRepeat())
		{
			bPressed = true;
		}
		return true;
	}

	virtual bool HandleKeyUpEvent(
		FSlateApplication& SlateApp,
		const FKeyEvent& InKeyEvent) override
	{
		return InKeyEvent.GetKey() == EKeys::Enter;
	}

	virtual const TCHAR* GetDebugName() const override
	{
		return TEXT("JoAEnterTrap");
	}

	bool bPressed = false;
};

/* One scripted match on a listen server and one client
 * in the same editor process. Every step waits for the
 * state on both worlds, so a value that never replicates
 * fails as a timeout with a message. */
class FJoAScript
{
public:
	explicit FJoAScript(
		FAutomationTestBase* InTest,
		EScenario InScenario = EScenario::Full)
		: Test(InTest)
		, Scenario(InScenario)
	{
		UJoA_MatchSettings* Match =
			GetMutableDefault<UJoA_MatchSettings>();
		SavedTurnSeconds = Match->TurnSeconds;
		SavedGraceSeconds = Match->ReconnectGraceSeconds;
		SavedRetrySeconds = Match->ClientRetryIntervalSeconds;

		/* The turn limit would pass the turn in the middle of
		 * a watched pause. Only the clock scenario keeps a
		 * limit, and a short one. */
		Match->TurnSeconds =
			InScenario == EScenario::TurnExpires ? 2.f : 0.f;
	}

	~FJoAScript()
	{
		UJoA_MatchSettings* Match =
			GetMutableDefault<UJoA_MatchSettings>();
		Match->TurnSeconds = SavedTurnSeconds;
		Match->ReconnectGraceSeconds = SavedGraceSeconds;
		Match->ClientRetryIntervalSeconds = SavedRetrySeconds;

		if (EnterTrap.IsValid()
			&& FSlateApplication::IsInitialized())
		{
			FSlateApplication::Get()
				.UnregisterInputPreProcessor(EnterTrap);
		}

		AJoA_Figure::bForceCombatHit = false;
		AJoA_Figure::bForceCombatMiss = false;
		AJoA_Figure::SetForcedBenchStart(-1);
	}

	bool Tick();

private:
	void Next()
	{
		Step = static_cast<EStep>(
			static_cast<int32>(Step) + 1);
		StepStart = FPlatformTime::Seconds();
		bStepActed = false;
		bActionSent = false;
		bLatched = false;
		bSettling = false;
	}

	void GoTo(EStep NewStep)
	{
		Step = NewStep;
		StepStart = FPlatformTime::Seconds();
		bStepActed = false;
		bActionSent = false;
		bLatched = false;
		bSettling = false;
	}

	/* A corner whose cell and two orthogonal neighbours
	 * and the diagonal are passable and free. Returns the
	 * corner and the three cells that wall it in. */
	bool FindWalledCorner(
		UWorld* Server,
		FIntPoint& OutCorner,
		TArray<FIntPoint>& OutWall) const
	{
		const AJoA_GridManager* Grid =
			AJoA_GameMode::GetGridManager(Server);
		const UJoA_FigureRegistry* Registry =
			UJoA_FigureRegistry::Get(Server);
		if (!Grid || !Registry)
		{
			return false;
		}

		const int32 Last = GridSize - 1;
		const FIntPoint Corners[] = {
			FIntPoint(0, 0),
			FIntPoint(Last, 0),
			FIntPoint(0, Last),
			FIntPoint(Last, Last)
		};
		for (const FIntPoint& Corner : Corners)
		{
			const int32 DX = Corner.X == 0 ? 1 : -1;
			const int32 DY = Corner.Y == 0 ? 1 : -1;
			const FIntPoint Cells[] = {
				Corner,
				FIntPoint(Corner.X + DX, Corner.Y),
				FIntPoint(Corner.X, Corner.Y + DY),
				FIntPoint(Corner.X + DX, Corner.Y + DY)
			};
			bool bClear = true;
			for (const FIntPoint& Cell : Cells)
			{
				FGridCell Info;
				bClear = bClear
					&& Grid->GetCell(Cell.X, Cell.Y, Info)
					&& Info.ObstacleHeight == 0
					&& !Registry->FindFigureAt(
						Cell.X,
						Cell.Y);
			}

			if (bClear)
			{
				OutCorner = Corner;
				OutWall = { Cells[1], Cells[2], Cells[3] };
				return true;
			}
		}
		return false;
	}

	// True once per step. For actions that must not repeat.
	bool Once()
	{
		if (bStepActed)
		{
			return false;
		}
		bStepActed = true;
		return true;
	}

	void Fail(const FString& Message)
	{
		Test->AddError(Message);
		JoATest::DumpWorlds();
		JoATest::DumpWidgets();
		bFailed = true;
		Step = bPlayStarted ? EStep::EndPlay : EStep::Done;
		StepStart = FPlatformTime::Seconds();
	}

	double Elapsed() const
	{
		return FPlatformTime::Seconds() - StepStart;
	}

	/* A run from the editor, not from the command line, is
	 * watched. A met step is held for a few seconds so the
	 * result can be seen, and the game stays open at the
	 * end. An unattended run goes straight through. */
	bool Settled(double Seconds)
	{
		if (!JoATest::IsWatched())
		{
			return true;
		}

		if (!EnterTrap.IsValid())
		{
			EnterTrap = MakeShared<FJoAEnterTrap>();
			FSlateApplication::Get().RegisterInputPreProcessor(
				EnterTrap);
		}

		if (!bSettling)
		{
			bSettling = true;
			EnterTrap->bPressed = false;
			SettleStart = FPlatformTime::Seconds();
			GEngine->AddOnScreenDebugMessage(
				9002,
				3600.f,
				FColor::Yellow,
				TEXT("Press Enter for the next test step."));
		}

		/* Enter on either window moves on at once. Without
		 * a key the pause ends after HoldSeconds. */
		bool bEnter = false;
		if (EnterTrap.IsValid())
		{
			bEnter = EnterTrap->bPressed;
			EnterTrap->bPressed = false;
		}

		const bool bElapsed =
			FPlatformTime::Seconds() - SettleStart
				>= static_cast<double>(
					JoATest::CVarHoldSeconds.GetValueOnGameThread());
		if (bEnter || bElapsed)
		{
			GEngine->RemoveOnScreenDebugMessage(9002);
			return true;
		}
		return false;
	}

	// Ends the scenario once the condition has held.
	void FinishWhen(
		bool bCondition,
		const TCHAR* FailText,
		double Timeout = 10.0)
	{
		if (bCondition)
		{
			bLatched = true;
		}

		if (bLatched)
		{
			if (Settled(5.0))
			{
				GoTo(EStep::EndPlay);
			}
		}
		else if (Elapsed() > Timeout)
		{
			Fail(FailText);
		}
	}

	void Wait(
		bool bCondition,
		double Timeout,
		const FString& What,
		double Hold = 2.0)
	{
		if (bCondition)
		{
			bLatched = true;
		}

		if (bLatched)
		{
			if (Settled(Hold))
			{
				Next();
			}
		}
		else if (Elapsed() > Timeout)
		{
			Fail(FString::Printf(
				TEXT("Timeout: %s"),
				*What));
		}
	}

	bool PickStep(
		AJoA_Figure* Figure,
		EStepDirection& OutDirection) const
	{
		const EStepDirection All[] = {
			EStepDirection::Forward,
			EStepDirection::Left,
			EStepDirection::Right,
			EStepDirection::Back
		};
		for (const EStepDirection Direction : All)
		{
			if (Figure->CanStep(Direction))
			{
				OutDirection = Direction;
				return true;
			}
		}
		return false;
	}

	void Kill(UWorld* Server, EUnitType Type) const
	{
		AJoA_Figure* Figure = JoATest::FindFigure(
			Server,
			Type,
			EnglishSeat);
		if (Figure && Figure->GetAbilitySystemComponent())
		{
			Figure->GetAbilitySystemComponent()
				->SetNumericAttributeBase(
					UJoA_GAS_HealthSet::GetHealthAttribute(),
					0.f);
		}
	}

	FAutomationTestBase* Test = nullptr;
	EScenario Scenario = EScenario::Full;
	float SavedTurnSeconds = 0.f;
	float SavedGraceSeconds = 0.f;
	float SavedRetrySeconds = 0.f;
	int32 SeenSeat = INDEX_NONE;
	int32 SeatChanges = 0;
	float DropGrace = 3.f;
	FIntPoint StalemateCorner = FIntPoint::ZeroValue;
	TArray<FIntPoint> StalemateWall;
	EStep Step = EStep::Start;
	double StepStart = 0.0;
	bool bFailed = false;
	bool bStepActed = false;
	bool bActionSent = false;
	bool bFreed = false;
	bool bLatched = false;
	TSharedPtr<FJoAEnterTrap> EnterTrap;
	bool bSettling = false;
	double SettleStart = 0.0;
	bool bPlayStarted = false;
	ULevelEditorPlaySettings* Settings = nullptr;

	FIntPoint TargetCell = FIntPoint::ZeroValue;
	FIntPoint EnemyCell = FIntPoint::ZeroValue;
	EFacing FacingBefore = EFacing::NegativeX;
	float SavedStamina = 0.f;
	float SavedHealth = 0.f;
};

bool FJoAScript::Tick()
{
	using namespace JoATest;

	UWorld* Server = FindPieWorld(true);
	UWorld* Client = FindPieWorld(false);
	AJoA_PlayerController* ServerPC = LocalPC(Server);
	AJoA_PlayerController* ClientPC = LocalPC(Client);
	AJoA_PlayerController* RemoteForClient = RemotePC(Server);

	AJoA_Figure* FrenchKnight =
		FindFigure(Server, EUnitType::HeavyKnight, FrenchSeat);
	AJoA_Figure* FrenchKnightC =
		FindFigure(Client, EUnitType::HeavyKnight, FrenchSeat);
	AJoA_Figure* FrenchLight =
		FindFigure(Server, EUnitType::LightInfantry, FrenchSeat);
	AJoA_Figure* EnglishKnight =
		FindFigure(Server, EUnitType::HeavyKnight, EnglishSeat);
	AJoA_Figure* EnglishKnightC =
		FindFigure(Client, EUnitType::HeavyKnight, EnglishSeat);
	AJoA_Figure* EnglishLight =
		FindFigure(Server, EUnitType::LightInfantry, EnglishSeat);
	AJoA_Figure* EnglishLightC =
		FindFigure(Client, EUnitType::LightInfantry, EnglishSeat);
	AJoA_Figure* FrenchLightC =
		FindFigure(Client, EUnitType::LightInfantry, FrenchSeat);
	AJoA_Figure* FrenchArcher =
		FindFigure(Server, EUnitType::Archer, FrenchSeat);
	AJoA_Figure* FrenchArcherC =
		FindFigure(Client, EUnitType::Archer, FrenchSeat);
	AJoA_Figure* EnglishArcher =
		FindFigure(Server, EUnitType::Archer, EnglishSeat);
	AJoA_Figure* EnglishArcherC =
		FindFigure(Client, EUnitType::Archer, EnglishSeat);
	AJoA_Figure* EnglishMedic =
		FindFigure(Server, EUnitType::Medic, EnglishSeat);
	AJoA_Figure* EnglishMedicC =
		FindFigure(Client, EUnitType::Medic, EnglishSeat);

	/* The seat scenarios cut the client off, so its world
	 * comes and goes. They only need the server. */
	const bool bNeedsClient =
		Scenario != EScenario::SeatDrop
		&& Scenario != EScenario::SeatRejoin
		&& Scenario != EScenario::ThirdPlayer;

	/* A watched run can be closed with Esc in the middle of
	 * a step. The steps then read controllers of worlds that
	 * are gone, so the script goes straight to the end. */
	if (Step != EStep::Start
		&& Step != EStep::WaitReady
		&& Step != EStep::EndPlay
		&& Step != EStep::Done
		&& (!ServerPC || (bNeedsClient && !ClientPC)))
	{
		Test->AddWarning(
			TEXT("PIE was closed before the test ended."));
		GoTo(EStep::EndPlay);
		return false;
	}

	switch (Step)
	{
	case EStep::Start:
	{
		if (!GEditor)
		{
			Fail(TEXT("No editor."));
			break;
		}

		Settings = NewObject<ULevelEditorPlaySettings>(
			GetTransientPackage());
		Settings->AddToRoot();
		Settings->SetPlayNetMode(PIE_ListenServer);
		/* The host and one client play. The third scenario
		 * adds a second client for a player too many. */
		Settings->SetPlayNumberOfClients(
			Scenario == EScenario::ThirdPlayer ? 3 : 2);
		Settings->SetRunUnderOneProcess(true);
		Settings->bLaunchSeparateServer = false;

		FRequestPlaySessionParams Params;
		Params.WorldType = EPlaySessionWorldType::PlayInEditor;
		Params.EditorPlaySettings = Settings;
		GEditor->RequestPlaySession(Params);
		bPlayStarted = true;
		Next();
		break;
	}

	case EStep::WaitReady:
	{
		const bool bReady =
			IsReady(Server) && IsReady(Client);
		if (bReady
			&& ServerPC
			&& ClientPC
			&& !MatchStarted(Server)
			&& Once())
		{
			/* French rock beats English scissors, so
			 * the rest of the flow still opens on
			 * the French seat. */
			ServerPC->RequestOpeningThrow(
				EOpeningThrow::Rock);
			ClientPC->RequestOpeningThrow(
				EOpeningThrow::Scissors);
		}

		const bool bMatchOn = bReady
			&& MatchStarted(Server)
			&& MatchStarted(Client);
		if (Scenario != EScenario::Full && bMatchOn)
		{
			switch (Scenario)
			{
			case EScenario::MovePassesTurn:
				GoTo(EStep::MovePassesTurn);
				break;
			case EScenario::MoveKeepsTurn:
				GoTo(EStep::MoveKeepsTurn);
				break;
			case EScenario::TurnExpires:
				GoTo(EStep::TurnExpires);
				break;
			case EScenario::SeatDrop:
			case EScenario::SeatRejoin:
				GoTo(EStep::DropSeat);
				break;
			case EScenario::ThirdPlayer:
				GoTo(EStep::ThirdPlayer);
				break;
			case EScenario::FlagCapture:
				GoTo(EStep::FlagCaptureSetup);
				break;
			case EScenario::FlagStamina:
				GoTo(EStep::FlagStaminaSetup);
				break;
			case EScenario::FlagCaptureEngland:
				GoTo(EStep::FlagCaptureEnglandSkip);
				break;
			case EScenario::Rest:
				GoTo(EStep::RestGrant);
				break;
			case EScenario::BuildObstacle:
				GoTo(EStep::BuildMark);
				break;
			case EScenario::JumpFall:
				GoTo(EStep::JumpRules);
				break;
			case EScenario::PromotedJumpFall:
				GoTo(EStep::PromotedJumpPromote);
				break;
			case EScenario::Promotion:
				GoTo(EStep::PromotionSetup);
				break;
			case EScenario::PromotionEngland:
				GoTo(EStep::PromotionEnglandSkip);
				break;
			case EScenario::FigureAnims:
				GoTo(EStep::FigureAnimIdle);
				break;
			case EScenario::MeleeReach:
				GoTo(EStep::MeleeRules);
				break;
			case EScenario::MedicHeal:
				GoTo(EStep::MedicHealRules);
				break;
			case EScenario::ArcherShot:
				GoTo(EStep::ArcherShotRules);
				break;
			case EScenario::ObstacleBox:
				GoTo(EStep::ObstacleBoxSetup);
				break;
			case EScenario::SplitBoard:
				GoTo(EStep::SplitBoardSetup);
				break;
			case EScenario::StaminaPulse:
				GoTo(EStep::StaminaPulseQuiet);
				break;
			default:
				GoTo(EStep::StalemateSetup);
				break;
			}
			break;
		}
		Wait(
			bMatchOn,
			90.0,
			TEXT("both worlds have 8 owned figures."));
		break;
	}

	case EStep::VerifyAttributes:
	{
		bool bEqual = true;
		for (const EUnitType Type : {
				EUnitType::HeavyKnight,
				EUnitType::LightInfantry,
				EUnitType::Archer,
				EUnitType::Medic })
		{
			for (const int32 Seat :
				{ FrenchSeat, EnglishSeat })
			{
				const AJoA_Figure* A =
					FindFigure(Server, Type, Seat);
				const AJoA_Figure* B =
					FindFigure(Client, Type, Seat);
				bEqual = bEqual && SameAttributes(A, B)
					&& Attr(
						A,
						UJoA_GAS_HealthSet::
							GetMaxHealthAttribute()) > 0.f;
			}
		}

		if (bEqual)
		{
			Test->TestEqual(
				TEXT("Server nation"),
				static_cast<int32>(
					ServerPC->GetPlayerState<
						AJoA_PlayerState>()->GetNation()),
				static_cast<int32>(ENation::French));
			Test->TestEqual(
				TEXT("Client nation"),
				static_cast<int32>(
					ClientPC->GetPlayerState<
						AJoA_PlayerState>()->GetNation()),
				static_cast<int32>(ENation::English));
			Test->TestEqual(
				TEXT("Server first seat"),
				ActiveSeat(Server),
				FrenchSeat);
			Test->TestEqual(
				TEXT("Client first seat"),
				ActiveSeat(Client),
				FrenchSeat);
			const UJoA_W_Play* ServerHud =
				ServerPC->GetPlayWidget();
			const UJoA_W_Play* ClientHud =
				ClientPC->GetPlayWidget();
			const FString ServerInfo = ServerHud
				? ServerHud->GetInfoText().ToString()
				: FString();
			const FString ClientInfo = ClientHud
				? ClientHud->GetInfoText().ToString()
				: FString();
			const FString LostThrow =
				FString(TEXT("The game has been won by "))
				+ TEXT("French, played with a rock.");
			/* The throw result hides after two seconds
			 * and the turn line takes its place. */
			Test->TestTrue(
				TEXT("French HUD first move"),
				ServerInfo
					== TEXT("You have first move!")
				|| ServerInfo == TEXT("Your turn"));
			Test->TestTrue(
				TEXT("English HUD lost the throw"),
				ClientInfo == LostThrow
				|| ClientInfo
					== TEXT("Opponent's turn"));
			Test->TestTrue(
				TEXT("Opening throw widget is gone"),
				!ServerPC->HasOpeningThrowWidget()
				&& !ClientPC->HasOpeningThrowWidget());
		}
		Wait(bEqual, 20.0, TEXT("attributes equal on both."));
		break;
	}

	case EStep::VerifyWidgets:
		Wait(
			CountStatWidgets(Server) == 8
				&& CountStatWidgets(Client) == 8,
			15.0,
			FString::Printf(
				TEXT("8 stat widgets, server %d, client %d,")
				TEXT(" PlayWidget %d."),
				CountStatWidgets(Server),
				CountStatWidgets(Client),
				static_cast<int32>(ServerPC && ServerPC->HasPlayWidget())));
		break;

	case EStep::CheckPortraitHits:
	{
		FString Problems;
		for (TObjectIterator<UJoA_W_CharacterStat> It;
			It;
			++It)
		{
			const AJoA_Figure* Shown = It->GetFigure();
			if (!Shown || !It->GetParent())
			{
				continue;
			}

			UWorld* World = Shown->GetWorld();
			AJoA_PlayerController* PC = LocalPC(World);
			if (!PC || !PC->OwnsFigure(Shown))
			{
				continue;
			}

			FString Blocker;
			const bool bOwnRow =
				It->GetParent()->GetFName()
					== TEXT("HB_LocalFigures");
			if (!bOwnRow)
			{
				Problems += FString::Printf(
					TEXT(" [net %d unit %d wrong row]"),
					static_cast<int32>(
						World->GetNetMode()),
					static_cast<int32>(Shown->UnitType));
			}
			else if (It->GetVisibility()
				!= ESlateVisibility::Visible)
			{
				Problems += FString::Printf(
					TEXT(" [net %d unit %d not clickable]"),
					static_cast<int32>(
						World->GetNetMode()),
					static_cast<int32>(Shown->UnitType));
			}
			else if (!PortraitReceivesClick(*It, Blocker))
			{
				/* The test window is small, so the layout
				 * overlaps. Reported, not failed. */
				Test->AddWarning(FString::Printf(
					TEXT("Portrait unit %d covered by %s"),
					static_cast<int32>(Shown->UnitType),
					*Blocker));
			}
		}

		Test->TestTrue(
			*FString::Printf(
				TEXT("Portraits receive clicks.%s"),
				*Problems),
			Problems.IsEmpty());

		auto BoardCentreClear = [&](UWorld* World)
		{
			AJoA_PlayerController* PC = LocalPC(World);
			UJoA_W_Play* Play = PC
				? PC->GetPlayWidget()
				: nullptr;
			if (!Play)
			{
				return true;
			}

			const FGeometry Geometry =
				Play->GetCachedGeometry();
			const FVector2f Size =
				Geometry.GetAbsoluteSize();
			if (Size.X < 1.f || Size.Y < 1.f)
			{
				return true;
			}

			const FVector2f Centre =
				Geometry.GetAbsolutePosition()
				+ Size * 0.5f;
			return !Play->HitsHud(FVector2D(Centre));
		};

		Test->TestTrue(
			TEXT("Server HUD leaves the board clickable"),
			BoardCentreClear(Server));
		Test->TestTrue(
			TEXT("Client HUD leaves the board clickable"),
			BoardCentreClear(Client));
		Next();
		break;
	}

	case EStep::RejectSelection:
	{
		if (Once())
		{
			ServerPC->RequestSelectFigure(EnglishKnight);
			ClientPC->RequestSelectFigure(FrenchKnightC);
			// Not the English turn yet.
			ClientPC->RequestSelectFigure(EnglishKnightC);
			break;
		}

		if (Elapsed() < 1.0)
		{
			break;
		}

		// The match opens with the French Heavy Knight held.
		Test->TestTrue(
			TEXT("Server holds the Heavy Knight by default"),
			ServerPC->GetSelectedFigure() == FrenchKnight);
		Test->TestNull(
			TEXT("Client pawn after bad picks"),
			ClientPC->GetSelectedFigure());
		Test->TestNull(
			TEXT("Remote pawn after bad picks"),
			RemoteForClient
				? RemoteForClient->GetSelectedFigure()
				: nullptr);
		Next();
		break;
	}

	case EStep::SelectFrench:
	{
		if (Once())
		{
			// A click on the figure in the field.
			ClickFigure(ServerPC, FrenchKnight);
		}

		const AJoA_FigureMarker* Marker =
			ServerPC->GetSelectionMarker();
		const AJoA_FigureMarker* StepMarker =
			ServerPC->GetStepMarker();
		Wait(
			ServerPC->GetSelectedFigure() == FrenchKnight
				&& Marker
				&& Marker->GetKind()
					== EFigureMarkerKind::Selected
				&& Marker->GetAttachParentActor()
					== FrenchKnight
				&& (!FrenchKnight->CanStep(
						EStepDirection::Forward)
					|| (StepMarker
						&& StepMarker->GetKind()
							== EFigureMarkerKind::StepTarget)),
			10.0,
			TEXT("French knight clicked, possessed, marked,")
			TEXT(" green ring on the free step."));
		break;
	}

	case EStep::StepFrench:
	{
		EStepDirection Direction = EStepDirection::Forward;
		if (!PickStep(FrenchKnight, Direction))
		{
			Fail(TEXT("French knight has no free step."));
			break;
		}

		TargetCell = FrenchKnight->GetStepTarget(Direction);

		/* Test shortcut: an enemy goes in front of the
		 * square the knight steps to. Without a target in
		 * reach the step would pass the turn at once. */
		EnemyCell = TargetCell + FIntPoint(-1, 0);
		if (AJoA_GridManager* Grid =
			AJoA_GameMode::GetGridManager(Server))
		{
			Grid->SetObstacleHeight(
				EnemyCell.X,
				EnemyCell.Y,
				0);
		}
		EnglishKnight->SetBoardPlacement(
			EnemyCell.X,
			EnemyCell.Y,
			EFacing::PositiveX);

		FrenchKnight->RequestStep(Direction);
		Next();
		break;
	}

	case EStep::WaitStepFrench:
	{
		const AJoA_GameState* State =
			AJoA_GameState::Get(Client);
		const bool bDone = IsAt(FrenchKnight, TargetCell)
			&& IsAt(FrenchKnightC, TargetCell)
			&& State
			&& State->HasActingFigureMoved()
			&& State->GetActingFigure() == FrenchKnightC;
		Wait(
			bDone,
			10.0,
			TEXT("step replicated, figure committed."));
		break;
	}

	case EStep::RejectSecondFigure:
		if (Once())
		{
			Test->TestFalse(
				TEXT("Second figure selectable"),
				ServerPC->CanSelectFigure(FrenchLight));
			ServerPC->RequestSelectFigure(FrenchLight);
			break;
		}

		if (Elapsed() > 1.0)
		{
			Test->TestTrue(
				TEXT("Still the committed figure"),
				ServerPC->GetSelectedFigure()
					== FrenchKnight);
			Next();
		}
		break;

	case EStep::TurnRight:
		if (Once())
		{
			FacingBefore = FrenchKnight->Facing;
			FrenchKnight->RequestTurn(ETurnDirection::Right);
		}
		Wait(
			FrenchKnight->Facing != FacingBefore
				&& FrenchKnightC->Facing
					== FrenchKnight->Facing,
			10.0,
			TEXT("turn replicated."));
		break;

	case EStep::TurnLeft:
		if (Once())
		{
			FrenchKnight->RequestTurn(ETurnDirection::Left);
		}
		Wait(
			FrenchKnight->Facing == FacingBefore
				&& FrenchKnightC->Facing == FacingBefore,
			10.0,
			TEXT("turn back replicated."));
		break;

	case EStep::PlaceTarget:
		TargetCell = EnemyCell;
		Next();
		break;

	case EStep::Attack:
		if (!bStepActed)
		{
			if (IsAt(EnglishKnightC, TargetCell)
				&& FrenchKnight->CanAttackTarget(
					EnglishKnight))
			{
				bStepActed = true;
				// A click on the target in the field.
				ClickFigure(ServerPC, EnglishKnight);
			}
			else if (Elapsed() > 10.0)
			{
				Fail(TEXT("Knight cannot attack target."));
			}
			break;
		}

		Wait(
			Attr(
				FrenchKnight,
				UJoA_GAS_StaminaSet::GetStaminaAttribute())
					< 10.f
				&& SameAttributes(
					FrenchKnight,
					FrenchKnightC)
				&& SameAttributes(
					EnglishKnight,
					EnglishKnightC)
				&& ActiveSeat(Server) == EnglishSeat
				&& ActiveSeat(Client) == EnglishSeat
				&& ServerPC->GetSelectedFigure() == nullptr
				&& CountFloatingTexts(Server) > 0
				&& CountFloatingTexts(Client) > 0,
			10.0,
			TEXT("attack spent stamina, turn passed,")
			TEXT(" selection cleared, combat text shown."));
		break;

	case EStep::SelectEnglish:
		if (Once())
		{
			ClickFigure(ClientPC, EnglishLightC);
		}
		Wait(
			ClientPC->GetSelectedFigure() == EnglishLightC
				&& RemoteForClient
				&& RemoteForClient->GetSelectedFigure()
					== EnglishLight,
			10.0,
			TEXT("client possessed English light infantry."));
		break;

	case EStep::StepEnglish:
	{
		EStepDirection Direction = EStepDirection::Forward;
		if (!PickStep(EnglishLightC, Direction))
		{
			Fail(TEXT("English light has no free step."));
			break;
		}

		TargetCell = EnglishLightC->GetStepTarget(Direction);
		EnglishLightC->RequestStep(Direction);
		Next();
		break;
	}

	case EStep::WaitStepEnglish:
		Wait(
			IsAt(EnglishLight, TargetCell)
				&& IsAt(EnglishLightC, TargetCell),
			10.0,
			TEXT("client step reached the server."));
		break;

	case EStep::AutoPassEnglish:
		Wait(
			ActiveSeat(Server) == FrenchSeat
				&& ActiveSeat(Client) == FrenchSeat,
			10.0,
			TEXT("step with no target passed the turn."));
		break;

	case EStep::SkipFrench:
		if (Once())
		{
			// The figure of the last turn is held again.
			Test->TestTrue(
				TEXT("French knight restored on return"),
				ServerPC->GetSelectedFigure() == FrenchKnight);
			ServerPC->RequestSelectFigure(FrenchKnight);
			break;
		}

		if (!bActionSent
			&& ServerPC->GetSelectedFigure() == FrenchKnight)
		{
			bActionSent = true;
			FrenchKnight->RequestSkipTurn();
		}
		Wait(
			ActiveSeat(Server) == EnglishSeat
				&& ActiveSeat(Client) == EnglishSeat,
			10.0,
			TEXT("skip passed the turn on both."));
		break;

	case EStep::HealEnglish:
	{
		const FGameplayAttribute MedicHealth =
			UJoA_GAS_HealthSet::GetHealthAttribute();
		if (Once())
		{
			// Test shortcut: the medic is hurt.
			EnglishMedic->GetAbilitySystemComponent()
				->SetNumericAttributeBase(MedicHealth, 6.f);
			ClientPC->RequestSelectFigure(EnglishMedicC);
			break;
		}

		if (!bActionSent)
		{
			if (ClientPC->GetSelectedFigure() == EnglishMedicC
				&& FMath::IsNearlyEqual(
					Attr(EnglishMedicC, MedicHealth),
					6.f))
			{
				bActionSent = true;
				ClientPC->RequestHeal(
					EnglishMedicC,
					EnglishMedicC);
			}
			else if (Elapsed() > 10.0)
			{
				Fail(TEXT("Medic cannot heal himself."));
			}
			break;
		}

		if (Elapsed() > 9.0)
		{
			UE_LOG(
				LogTemp,
				Display,
				TEXT("JoATest heal hp %f/%f mana %f sel %d")
				TEXT(" seat %d/%d texts %d"),
				Attr(EnglishMedic, MedicHealth),
				Attr(EnglishMedicC, MedicHealth),
				Attr(
					EnglishMedic,
					UJoA_GAS_ManaSet::GetManaAttribute()),
				ClientPC->GetSelectedFigure() ? 1 : 0,
				ActiveSeat(Server),
				ActiveSeat(Client),
				CountFloatingTexts(Client));
		}

		const FGameplayAttribute Mana =
			UJoA_GAS_ManaSet::GetManaAttribute();
		const FGameplayAttribute MaxMana =
			UJoA_GAS_ManaSet::GetMaxManaAttribute();
		Wait(
			Attr(EnglishMedic, MedicHealth) > 6.f
				&& SameAttributes(EnglishMedic, EnglishMedicC)
				&& Attr(EnglishMedic, Mana)
					< Attr(EnglishMedic, MaxMana)
				&& ActiveSeat(Server) == FrenchSeat
				&& ActiveSeat(Client) == FrenchSeat
				&& ClientPC->GetSelectedFigure() == nullptr
				&& CountFloatingTexts(Client) > 0,
			10.0,
			TEXT("heal restored health, spent mana, passed")
			TEXT(" the turn, deselected."));
		break;
	}

	case EStep::KillArcher:
		if (Once())
		{
			Kill(Server, EUnitType::Archer);
		}
		Wait(
			CountLiving(Server) == 7
				&& CountLiving(Client) == 7
				&& CountStatWidgets(Server) == 7
				&& CountStatWidgets(Client) == 7,
			10.0,
			TEXT("dead figure removed from registry, HUD."));
		break;

	case EStep::PossessLight:
		if (Once())
		{
			// Test shortcut: the client holds the light.
			RemoteForClient->Possess(EnglishLight);
		}
		Wait(
			ClientPC->GetSelectedFigure() == EnglishLightC,
			10.0,
			TEXT("client holds English light infantry."));
		break;

	case EStep::KillPossessed:
		if (Once())
		{
			Kill(Server, EUnitType::LightInfantry);
		}
		Wait(
			ClientPC->GetSelectedFigure() == nullptr
				&& RemoteForClient
				&& RemoteForClient->GetSelectedFigure()
					== nullptr
				&& CountLiving(Client) == 6,
			10.0,
			TEXT("possessed figure died, selection cleared."));
		break;

	case EStep::KillRest:
	{
		if (Once())
		{
			Kill(Server, EUnitType::HeavyKnight);
			Kill(Server, EUnitType::Medic);
		}

		const AJoA_GameState* ServerState =
			AJoA_GameState::Get(Server);
		const AJoA_GameState* ClientState =
			AJoA_GameState::Get(Client);
		Wait(
			ServerState && ClientState
				&& ServerState->IsMatchOver()
				&& ClientState->IsMatchOver()
				&& ServerState->GetWinnerSeat() == FrenchSeat
				&& ClientState->GetWinnerSeat() == FrenchSeat
				&& CountStatWidgets(Server) == 4
				&& CountStatWidgets(Client) == 4
				&& ServerPC->GetPlayWidget()
				&& ClientPC->GetPlayWidget()
				&& ServerPC->GetPlayWidget()
					->GetInfoText().ToString() == TEXT("You have won!")
				&& ClientPC->GetPlayWidget()
					->GetInfoText().ToString() == TEXT("You have lost!"),
			10.0,
			TEXT("French wins on both, 4 widgets left,")
			TEXT(" won and lost shown."));
		break;
	}

	case EStep::StalemateSetup:
	{
		if (Once())
		{
			// English keeps only the medic, in a corner.
			Kill(Server, EUnitType::Archer);
			Kill(Server, EUnitType::LightInfantry);
			Kill(Server, EUnitType::HeavyKnight);
			if (!FindWalledCorner(
					Server,
					StalemateCorner,
					StalemateWall))
			{
				Fail(TEXT("No free corner for the medic."));
				break;
			}

			EnglishMedic->SetBoardPlacement(
				StalemateCorner.X,
				StalemateCorner.Y,
				EnglishMedic->Facing);
			AJoA_Figure* const Walls[] = {
				FrenchKnight,
				FrenchLight,
				FrenchArcher
			};
			for (int32 I = 0; I < 3; ++I)
			{
				Walls[I]->SetBoardPlacement(
					StalemateWall[I].X,
					StalemateWall[I].Y,
					Walls[I]->Facing);
			}
			break;
		}

		bool bWalled = CountLiving(Server) == 5
			&& CountLiving(Client) == 5
			&& IsAt(EnglishMedicC, StalemateCorner)
			&& IsAt(FrenchKnightC, StalemateWall[0])
			&& IsAt(
				FindFigure(
					Client,
					EUnitType::LightInfantry,
					FrenchSeat),
				StalemateWall[1])
			&& IsAt(
				FindFigure(
					Client,
					EUnitType::Archer,
					FrenchSeat),
				StalemateWall[2]);
		Wait(
			bWalled,
			10.0,
			TEXT("medic walled in a corner on both."));
		break;
	}

	case EStep::StalemateFrenchSkips:
	{
		if (Once())
		{
			ServerPC->RequestSelectFigure(FrenchKnight);
			break;
		}

		if (!bActionSent
			&& ServerPC->GetSelectedFigure() == FrenchKnight)
		{
			bActionSent = true;
			FrenchKnight->RequestSkipTurn();
		}

		const AJoA_GameState* ServerState =
			AJoA_GameState::Get(Server);
		const AJoA_GameState* ClientState =
			AJoA_GameState::Get(Client);
		const UJoA_W_Play* ClientHud = ClientPC->GetPlayWidget();
		Wait(
			ServerState && ClientState
				&& ServerState->GetActiveSeat() == EnglishSeat
				&& ClientState->GetActiveSeat() == EnglishSeat
				&& ServerState->IsActiveSeatBlocked()
				&& ClientState->IsActiveSeatBlocked()
				&& !ServerState->IsMatchOver()
				&& ClientHud
				&& ClientHud->GetInfoText().ToString()
					.StartsWith(TEXT("You can't move")),
			10.0,
			TEXT("blocked English seat keeps the turn and")
			TEXT(" reads the skip message."),
			6.0);
		break;
	}

	case EStep::StalemateEnglishSkips:
	{
		if (Once())
		{
			ClientPC->RequestSelectFigure(EnglishMedicC);
			break;
		}

		if (!bActionSent
			&& ClientPC->GetSelectedFigure() == EnglishMedicC)
		{
			bActionSent = true;
			EnglishMedicC->RequestSkipTurn();
		}

		const AJoA_GameState* ServerState =
			AJoA_GameState::Get(Server);
		if (ServerState
			&& ServerState->GetActiveSeat() == FrenchSeat
			&& !ServerState->IsActiveSeatBlocked()
			&& !ServerState->IsMatchOver())
		{
			GoTo(
				Scenario == EScenario::BlockedRecovers
					? EStep::RecoverFreeMedic
					: EStep::StalemateFrenchSkipsAgain);
		}
		else if (Elapsed() > 10.0)
		{
			Fail(TEXT("Timeout: English skip passed the turn."));
		}
		break;
	}

	case EStep::RecoverFreeMedic:
	{
		if (Once())
		{
			ServerPC->RequestSelectFigure(FrenchKnight);
			break;
		}

		if (!bActionSent
			&& ServerPC->GetSelectedFigure() == FrenchKnight)
		{
			bActionSent = true;

			// Test shortcut: a wall figure leaves the corner.
			const AJoA_GridManager* Grid =
				AJoA_GameMode::GetGridManager(Server);
			const UJoA_FigureRegistry* Registry =
				UJoA_FigureRegistry::Get(Server);
			for (int32 Y = 0; Y < GridSize; ++Y)
			{
				for (int32 X = 0; X < GridSize; ++X)
				{
					FGridCell Info;
					if (!bFreed
						&& FMath::Abs(X - StalemateCorner.X) > 3
						&& FMath::Abs(Y - StalemateCorner.Y) > 3
						&& Grid->GetCell(X, Y, Info)
						&& Info.ObstacleHeight == 0
						&& !Registry->FindFigureAt(X, Y))
					{
						bFreed = true;
						FrenchKnight->SetBoardPlacement(
							X,
							Y,
							FrenchKnight->Facing);
					}
				}
			}
			FrenchKnight->RequestSkipTurn();
		}

		const AJoA_GameState* ServerState =
			AJoA_GameState::Get(Server);
		const AJoA_GameState* ClientState =
			AJoA_GameState::Get(Client);
		const UJoA_W_Play* ClientHud = ClientPC->GetPlayWidget();
		bool bRecovered = ServerState && ClientState
			&& ServerState->GetActiveSeat() == EnglishSeat
			&& ClientState->GetActiveSeat() == EnglishSeat
			&& !ServerState->IsActiveSeatBlocked()
			&& !ClientState->IsActiveSeatBlocked()
			&& !ServerState->IsMatchOver()
			&& ClientHud
			&& ClientHud->GetInfoText().ToString()
				== TEXT("Your turn");
		FinishWhen(
			bRecovered,
			TEXT("Timeout: freed English seat plays on."));
		break;
	}

	case EStep::MovePassesTurn:
	case EStep::MoveKeepsTurn:
	{
		const bool bKeeps = Step == EStep::MoveKeepsTurn;

		/* The knight may hit an obstacle in front, which is
		 * an action. The light infantry has none. */
		AJoA_Figure* Mover = bKeeps ? FrenchKnight : FrenchLight;
		AJoA_Figure* MoverC =
			bKeeps ? FrenchKnightC : FrenchLightC;
		if (Once())
		{
			ServerPC->RequestSelectFigure(Mover);
			break;
		}

		if (!bActionSent
			&& ServerPC->GetSelectedFigure() == Mover)
		{
			EStepDirection Direction = EStepDirection::Forward;
			if (!PickStep(Mover, Direction))
			{
				Fail(TEXT("French knight has no free step."));
				break;
			}

			bActionSent = true;
			TargetCell = Mover->GetStepTarget(Direction);
			if (bKeeps)
			{
				/* An enemy in front of the target square
				 * gives the knight an attack after the step. */
				EnemyCell = TargetCell + FIntPoint(-1, 0);
				EnglishKnight->SetBoardPlacement(
					EnemyCell.X,
					EnemyCell.Y,
					EFacing::PositiveX);
			}
			Mover->RequestStep(Direction);
			break;
		}

		const AJoA_GameState* ServerState =
			AJoA_GameState::Get(Server);
		const AJoA_GameState* ClientState =
			AJoA_GameState::Get(Client);
		const int32 Expected = bKeeps ? FrenchSeat : EnglishSeat;
		bool bDone = bActionSent
			&& IsAt(Mover, TargetCell)
			&& IsAt(MoverC, TargetCell)
			&& ServerState && ClientState
			&& ServerState->GetActiveSeat() == Expected
			&& ClientState->GetActiveSeat() == Expected
			&& !ServerState->IsActiveSeatBlocked()
			&& (bKeeps
				? ServerPC->GetSelectedFigure() == Mover
				: ServerPC->GetSelectedFigure() == nullptr);
		if (bDone && bKeeps && Elapsed() < 2.0)
		{
			// Still French after a settle period.
			break;
		}

		FinishWhen(
			bDone,
			bKeeps
				? TEXT("Timeout: move with an attack kept the turn.")
				: TEXT("Timeout: move with no option passed."));
		break;
	}

	case EStep::TurnExpires:
	{
		const AJoA_GameState* ServerState =
			AJoA_GameState::Get(Server);
		const AJoA_GameState* ClientState =
			AJoA_GameState::Get(Client);
		const UJoA_W_Play* ClientHud = ClientPC->GetPlayWidget();
		if (ServerState
			&& ServerState->GetActiveSeat() != SeenSeat)
		{
			if (SeenSeat != INDEX_NONE)
			{
				++SeatChanges;
			}
			SeenSeat = ServerState->GetActiveSeat();
		}

		/* Nobody plays, so the clock alone passes the turn
		 * from seat to seat. The client sees the deadline and
		 * a count in the HUD. */
		const FString Text = ClientHud
			? ClientHud->GetInfoText().ToString()
			: FString();
		bool bDone = SeatChanges >= 2
			&& ClientState
			&& ClientState->GetTurnDeadline() > 0.0
			&& (Text.StartsWith(TEXT("Your turn ("))
				|| Text.StartsWith(TEXT("Opponent's turn (")));
		FinishWhen(
			bDone,
			TEXT("Timeout: the turn clock passes the turn."));
		break;
	}

	case EStep::DropSeat:
	{
		const AJoA_GameState* ServerState =
			AJoA_GameState::Get(Server);
		if (Once())
		{
			UJoA_MatchSettings* Match =
				GetMutableDefault<UJoA_MatchSettings>();
			/* A watched run holds the paused match until
			 * Enter, so the client waits and the wait for
			 * the seat is long enough to read the HUD. */
			const bool bWatched = JoATest::IsWatched();
			if (Scenario == EScenario::SeatRejoin)
			{
				Match->ClientRetryIntervalSeconds =
					bWatched ? 1000.f : 1.f;
			}
			else
			{
				Match->ClientRetryIntervalSeconds = 1000.f;
				DropGrace = bWatched ? 20.f : 3.f;
				Match->ReconnectGraceSeconds = DropGrace;
			}

			UNetConnection* Link = RemoteForClient
				? RemoteForClient->GetNetConnection()
				: nullptr;
			if (!Link)
			{
				Fail(TEXT("No remote connection to cut."));
				break;
			}

			Link->Close();
			break;
		}

		/* The link is gone and the seat is held. The match
		 * is paused and the figures are still on the board. */
		bool bDropped = ServerState
			&& ServerState->GetSeatLink(EnglishSeat).State
				== ESeatConnection::Disconnected
			&& ServerState->IsPaused()
			&& CountLiving(Server) == 8;
		if (bDropped)
		{
			bLatched = true;
		}

		if (bLatched)
		{
			if (Settled(5.0))
			{
				GoTo(
					Scenario == EScenario::SeatRejoin
						? EStep::WaitRejoin
						: EStep::WaitForfeit);
			}
		}
		else if (Elapsed() > 15.0)
		{
			Fail(TEXT("Timeout: dropped seat is held."));
		}
		break;
	}

	case EStep::WaitForfeit:
	{
		const AJoA_GameState* ServerState =
			AJoA_GameState::Get(Server);
		const UJoA_W_Play* ServerHud = ServerPC->GetPlayWidget();
		bool bDone = ServerState
			&& ServerState->IsMatchOver()
			&& ServerState->GetWinnerSeat() == FrenchSeat
			&& ServerState->GetMatchEndReason()
				== EMatchEndReason::Forfeit
			&& ServerHud
			&& ServerHud->GetInfoText().ToString()
				== TEXT("You have won! The opponent left.");
		FinishWhen(
			bDone,
			TEXT("Timeout: grace ran out, French won."),
			DropGrace + 15.0);
		break;
	}

	case EStep::WaitRejoin:
	{
		const AJoA_GameState* ServerState =
			AJoA_GameState::Get(Server);
		const AJoA_PlayerState* Returned = RemoteForClient
			? RemoteForClient->GetPlayerState<AJoA_PlayerState>()
			: nullptr;

		if (Once() && JoATest::IsWatched())
		{
			// The client was kept waiting. Let it try now.
			for (const FWorldContext& Context :
				GEditor->GetWorldContexts())
			{
				UJoA_GameInstance* Instance =
					Cast<UJoA_GameInstance>(
						Context.OwningGameInstance);
				if (Context.WorldType == EWorldType::PIE
					&& Instance)
				{
					Instance->ReconnectNow();
				}
			}
		}

		/* The client came back on its own: same seat, same
		 * nation, the match runs again and the figures
		 * were kept. */
		bool bBack = ServerState
			&& ServerState->GetSeatLink(EnglishSeat).State
				== ESeatConnection::Connected
			&& !ServerState->IsPaused()
			&& !ServerState->IsMatchOver()
			&& CountLiving(Server) == 8
			&& Returned
			&& Returned->GetSeatIndex() == EnglishSeat
			&& Returned->GetNation() == ENation::English;
		FinishWhen(
			bBack,
			TEXT("Timeout: the client got its seat back."),
			60.0);
		break;
	}

	case EStep::ThirdPlayer:
	{
		const AJoA_GameState* ServerState =
			AJoA_GameState::Get(Server);
		int32 Controllers = 0;
		for (FConstPlayerControllerIterator It =
				Server->GetPlayerControllerIterator();
			It;
			++It)
		{
			++Controllers;
		}

		/* Both clients knock at once. One takes the second
		 * seat and the other is put out. Checked after the
		 * refusal has had time to land. */
		if (Elapsed() < 6.0)
		{
			break;
		}

		bool bDone = ServerState
			&& Controllers == 2
			&& ServerState->GetSeatLink(FrenchSeat).State
				== ESeatConnection::Connected
			&& ServerState->GetSeatLink(EnglishSeat).State
				== ESeatConnection::Connected
			&& !ServerState->IsPaused();
		FinishWhen(
			bDone,
			TEXT("Timeout: a third player stays out."),
			10.0);
		break;
	}

	case EStep::StalemateFrenchSkipsAgain:
	{
		if (Once())
		{
			ServerPC->RequestSelectFigure(FrenchKnight);
			break;
		}

		if (!bActionSent
			&& ServerPC->GetSelectedFigure() == FrenchKnight)
		{
			bActionSent = true;
			FrenchKnight->RequestSkipTurn();
		}

		const AJoA_GameState* ServerState =
			AJoA_GameState::Get(Server);
		const AJoA_GameState* ClientState =
			AJoA_GameState::Get(Client);
		const UJoA_W_Play* ServerHud = ServerPC->GetPlayWidget();
		const UJoA_W_Play* ClientHud = ClientPC->GetPlayWidget();
		bool bDone = ServerState && ClientState
			&& ServerState->IsMatchOver()
			&& ClientState->IsMatchOver()
			&& ServerState->GetWinnerSeat() == FrenchSeat
			&& ClientState->GetWinnerSeat() == FrenchSeat
			&& ServerHud
			&& ClientHud
			&& ServerHud->GetInfoText().ToString()
				== TEXT("You have won!")
			&& ClientHud->GetInfoText().ToString()
				== TEXT("You have lost!");
		FinishWhen(
			bDone,
			TEXT("Timeout: stuck English seat lost."));
		break;
	}

	case EStep::FlagCaptureSetup:
	{
		if (Once())
		{
			AJoA_GridManager* Grid =
				AJoA_GameMode::GetGridManager(Server);
			AJoA_Flag* EnemyFlag =
				FindFlag(Server, EnglishSeat);
			if (!Grid || !FrenchKnight || !EnemyFlag)
			{
				Fail(TEXT("No knight or English flag."));
				break;
			}

			Grid->SetObstacleHeight(6, 5, 0);
			FrenchKnight->SetBoardPlacement(
				EnemyFlag->GetCellX(),
				EnemyFlag->GetCellY(),
				EFacing::PositiveX);
			if (!FrenchKnight->ResolvePickupFlag())
			{
				Fail(TEXT("Knight could not pick the flag."));
				break;
			}

			FrenchKnight->SetBoardPlacement(
				6,
				5,
				EFacing::PositiveX);
			ServerPC->RequestSelectFigure(FrenchKnight);
			break;
		}

		Wait(
			FrenchKnight
				&& FrenchKnight->GetHeldFlag()
				&& FrenchKnight->CellX == 6
				&& FrenchKnight->CellY == 5
				&& FrenchKnightC
				&& FrenchKnightC->GetHeldFlag()
				&& FrenchKnightC->CellX == 6,
			10.0,
			TEXT("knight holds the English flag one cell")
			TEXT(" before the French home row."));
		break;
	}

	case EStep::FlagCaptureStep:
	{
		if (Once())
		{
			FrenchKnight->RequestStep(
				EStepDirection::Forward);
			break;
		}

		const AJoA_GameState* ServerState =
			AJoA_GameState::Get(Server);
		const AJoA_GameState* ClientState =
			AJoA_GameState::Get(Client);
		const UJoA_W_Play* ServerHud =
			ServerPC->GetPlayWidget();
		FinishWhen(
			ServerState && ClientState
				&& ServerState->IsMatchOver()
				&& ClientState->IsMatchOver()
				&& ServerState->GetWinnerSeat()
					== FrenchSeat
				&& ClientState->GetWinnerSeat()
					== FrenchSeat
				&& ServerState->GetMatchEndReason()
					== EMatchEndReason::FlagCapture
				&& ClientState->GetMatchEndReason()
					== EMatchEndReason::FlagCapture
				&& ServerHud
				&& ServerHud->GetInfoText().ToString()
					== TEXT("You have won!"),
			TEXT("Timeout: flag capture did not win."));
		break;
	}

	case EStep::FlagStaminaSetup:
	{
		if (Once())
		{
			AJoA_GridManager* Grid =
				AJoA_GameMode::GetGridManager(Server);
			AJoA_Flag* EnemyFlag =
				FindFlag(Server, EnglishSeat);
			if (!Grid || !FrenchKnight || !EnemyFlag)
			{
				Fail(TEXT("No knight or English flag."));
				break;
			}

			Grid->SetObstacleHeight(6, 5, 0);
			FrenchKnight->SetBoardPlacement(
				EnemyFlag->GetCellX(),
				EnemyFlag->GetCellY(),
				EFacing::NegativeX);
			if (!FrenchKnight->ResolvePickupFlag())
			{
				Fail(TEXT("Knight could not pick the flag."));
				break;
			}

			FrenchKnight->SetBoardPlacement(
				7,
				5,
				EFacing::NegativeX);
			SavedStamina = Attr(
				FrenchKnight,
				UJoA_GAS_StaminaSet::GetStaminaAttribute());
			ServerPC->RequestSelectFigure(FrenchKnight);
			break;
		}

		Wait(
			FrenchKnight
				&& FrenchKnight->GetHeldFlag()
				&& FrenchKnight->UnitType
					!= EUnitType::Medic
				&& FrenchKnight->CellX == 7
				&& FrenchKnightC
				&& FrenchKnightC->GetHeldFlag()
				&& SavedStamina > 0.f,
			10.0,
			TEXT("knight holds a flag on the home row"));
		break;
	}

	case EStep::FlagStaminaStep:
	{
		if (Once())
		{
			FrenchKnight->RequestStep(
				EStepDirection::Forward);
			break;
		}

		const float Now = Attr(
			FrenchKnight,
			UJoA_GAS_StaminaSet::GetStaminaAttribute());
		const float NowC = Attr(
			FrenchKnightC,
			UJoA_GAS_StaminaSet::GetStaminaAttribute());
		FinishWhen(
			FrenchKnight
				&& FrenchKnight->CellX == 6
				&& FrenchKnightC
				&& FrenchKnightC->CellX == 6
				&& FMath::IsNearlyEqual(
					Now,
					SavedStamina - 2.f)
				&& FMath::IsNearlyEqual(
					NowC,
					SavedStamina - 2.f),
			TEXT("Timeout: step with a flag did not")
			TEXT(" spend 2 stamina."));
		break;
	}

	case EStep::FlagCaptureEnglandSkip:
	{
		if (Once())
		{
			ServerPC->RequestSelectFigure(FrenchKnight);
			break;
		}

		if (!bActionSent
			&& ServerPC->GetSelectedFigure()
				== FrenchKnight)
		{
			bActionSent = true;
			FrenchKnight->RequestSkipTurn();
		}

		Wait(
			ActiveSeat(Server) == EnglishSeat
				&& ActiveSeat(Client) == EnglishSeat,
			10.0,
			TEXT("English seat after the French skip."));
		break;
	}

	case EStep::FlagCaptureEnglandSetup:
	{
		if (Once())
		{
			AJoA_GridManager* Grid =
				AJoA_GameMode::GetGridManager(Server);
			AJoA_Flag* EnemyFlag =
				FindFlag(Server, FrenchSeat);
			if (!Grid
				|| !EnglishKnight
				|| !EnglishKnightC
				|| !EnemyFlag)
			{
				Fail(TEXT("No knight or French flag."));
				break;
			}

			Grid->SetObstacleHeight(1, 2, 0);
			Grid->SetObstacleHeight(0, 2, 0);
			EnglishKnight->SetBoardPlacement(
				EnemyFlag->GetCellX(),
				EnemyFlag->GetCellY(),
				EFacing::NegativeX);
			if (!EnglishKnight->ResolvePickupFlag())
			{
				Fail(TEXT("Knight could not pick the flag."));
				break;
			}

			EnglishKnight->SetBoardPlacement(
				1,
				2,
				EFacing::NegativeX);
			ClientPC->RequestSelectFigure(EnglishKnightC);
			break;
		}

		Wait(
			EnglishKnight
				&& EnglishKnight->GetHeldFlag()
				&& EnglishKnight->CellX == 1
				&& EnglishKnight->CellY == 2
				&& EnglishKnightC
				&& EnglishKnightC->GetHeldFlag()
				&& EnglishKnightC->CellX == 1
				&& ClientPC->GetSelectedFigure()
					== EnglishKnightC,
			10.0,
			TEXT("English knight holds the French flag")
			TEXT(" one cell before the English home row."));
		break;
	}

	case EStep::FlagCaptureEnglandStep:
	{
		if (Once())
		{
			EnglishKnightC->RequestStep(
				EStepDirection::Forward);
			break;
		}

		const AJoA_GameState* ServerState =
			AJoA_GameState::Get(Server);
		const AJoA_GameState* ClientState =
			AJoA_GameState::Get(Client);
		const UJoA_W_Play* ServerHud =
			ServerPC->GetPlayWidget();
		const UJoA_W_Play* ClientHud =
			ClientPC->GetPlayWidget();
		FinishWhen(
			ServerState && ClientState
				&& ServerState->IsMatchOver()
				&& ClientState->IsMatchOver()
				&& ServerState->GetWinnerSeat()
					== EnglishSeat
				&& ClientState->GetWinnerSeat()
					== EnglishSeat
				&& ServerState->GetMatchEndReason()
					== EMatchEndReason::FlagCapture
				&& ClientState->GetMatchEndReason()
					== EMatchEndReason::FlagCapture
				&& ServerHud
				&& ServerHud->GetInfoText().ToString()
					== TEXT("You have lost! Your flag")
						TEXT(" was captured.")
				&& ClientHud
				&& ClientHud->GetInfoText().ToString()
					== TEXT("You have won! Enemy flag")
						TEXT(" captured."),
			TEXT("Timeout: English flag capture did not")
			TEXT(" win."));
		break;
	}

	case EStep::RestGrant:
	{
		const FGameplayAttribute Stamina =
			UJoA_GAS_StaminaSet::GetStaminaAttribute();
		const FGameplayAttribute Mana =
			UJoA_GAS_ManaSet::GetManaAttribute();
		const EUnitType Types[] = {
			EUnitType::HeavyKnight,
			EUnitType::LightInfantry,
			EUnitType::Archer,
			EUnitType::Medic
		};
		/* Both resources start under the cap, so a
		 * missed figure cannot hide at its maximum. */
		auto SeatMatches = [&](
			float StaminaValue,
			float ManaValue)
		{
			for (const EUnitType Type : Types)
			{
				const AJoA_Figure* ServerFigure =
					FindFigure(Server, Type, FrenchSeat);
				const AJoA_Figure* ClientFigure =
					FindFigure(Client, Type, FrenchSeat);
				if (!ServerFigure
					|| !ClientFigure
					|| !FMath::IsNearlyEqual(
						Attr(ServerFigure, Stamina),
						StaminaValue)
					|| !FMath::IsNearlyEqual(
						Attr(ServerFigure, Mana),
						ManaValue)
					|| !SameAttributes(
						ServerFigure,
						ClientFigure))
				{
					return false;
				}
			}
			return true;
		};
		if (Once())
		{
			for (const EUnitType Type : Types)
			{
				AJoA_Figure* Figure =
					FindFigure(Server, Type, FrenchSeat);
				UAbilitySystemComponent* ASC = Figure
					? Figure->GetAbilitySystemComponent()
					: nullptr;
				if (!ASC)
				{
					Fail(TEXT(
						"No French attributes."));
					break;
				}

				ASC->SetNumericAttributeBase(
					UJoA_GAS_StaminaSet::
						GetMaxStaminaAttribute(),
					10.f);
				ASC->SetNumericAttributeBase(Stamina, 4.f);
				ASC->SetNumericAttributeBase(
					UJoA_GAS_ManaSet::GetMaxManaAttribute(),
					10.f);
				ASC->SetNumericAttributeBase(Mana, 2.f);
			}
			if (bFailed)
			{
				break;
			}
			ServerPC->RequestSelectFigure(FrenchKnight);
			break;
		}

		const bool bReady =
			ServerPC->GetSelectedFigure() == FrenchKnight
			&& FrenchKnight->CanRest()
			&& SeatMatches(4.f, 2.f);
		if (!bActionSent && bReady)
		{
			bActionSent = true;
			FrenchKnight->RequestRest();
			break;
		}

		if (!bActionSent && Elapsed() > 10.0)
		{
			Fail(TEXT("French seat could not rest."));
			break;
		}

		Wait(
			bActionSent
				&& SeatMatches(5.f, 3.f)
				&& ActiveSeat(Server) == EnglishSeat
				&& ActiveSeat(Client) == EnglishSeat,
			10.0,
			TEXT("rest paid every French figure,")
			TEXT(" then passed the turn."));
		break;
	}

	case EStep::RestBlocked:
	{
		if (Once())
		{
			EStepDirection Direction =
				EStepDirection::Forward;
			if (!PickStep(EnglishKnight, Direction))
			{
				Fail(TEXT(
					"English knight has no free step."));
				break;
			}

			TargetCell = EnglishKnight->GetStepTarget(
				Direction);
			const FIntPoint Here(
				EnglishKnight->CellX,
				EnglishKnight->CellY);
			const FIntPoint Front =
				EnglishKnight->GetStepTarget(
					EStepDirection::Forward);
			EnemyCell = TargetCell + (Front - Here);
			if (EnemyCell.X < 0
				|| EnemyCell.Y < 0
				|| EnemyCell.X >= GridSize
				|| EnemyCell.Y >= GridSize)
			{
				Fail(TEXT(
					"No square in front after the step."));
				break;
			}

			if (AJoA_GridManager* Grid =
				AJoA_GameMode::GetGridManager(Server))
			{
				Grid->SetObstacleHeight(
					EnemyCell.X,
					EnemyCell.Y,
					0);
			}
			FrenchKnight->SetBoardPlacement(
				EnemyCell.X,
				EnemyCell.Y,
				EFacing::NegativeX);
			EnglishKnight->RequestStep(Direction);
			break;
		}

		const AJoA_GameState* ServerState =
			AJoA_GameState::Get(Server);
		const bool bStepped = ServerState
			&& IsAt(EnglishKnight, TargetCell)
			&& ServerState->HasActingFigureMoved()
			&& ServerState->GetActiveSeat() == EnglishSeat
			&& !EnglishKnight->CanRest();
		if (bStepped && !bActionSent)
		{
			bActionSent = true;
			SavedStamina = Attr(
				EnglishKnight,
				UJoA_GAS_StaminaSet::GetStaminaAttribute());
			EnglishKnight->RequestRest();
			break;
		}

		if (!bActionSent && Elapsed() > 10.0)
		{
			Fail(TEXT(
				"English knight did not stay committed."));
			break;
		}

		const bool bHeld = bActionSent
			&& ServerState
			&& ServerState->GetActiveSeat() == EnglishSeat
			&& ServerState->HasActingFigureMoved()
			&& !EnglishKnight->CanRest()
			&& FMath::IsNearlyEqual(
				Attr(
					EnglishKnight,
					UJoA_GAS_StaminaSet::
						GetStaminaAttribute()),
				SavedStamina);
		FinishWhen(
			bHeld,
			TEXT("Timeout: rest ran after a step."));
		break;
	}

	case EStep::BuildMark:
	{
		if (Once())
		{
			AJoA_Figure* Infantry = FindFigure(
				Server,
				EUnitType::LightInfantry,
				FrenchSeat);
			AJoA_Figure* Knight = FindFigure(
				Server,
				EUnitType::HeavyKnight,
				FrenchSeat);
			AJoA_Figure* Blocker = FindFigure(
				Server,
				EUnitType::HeavyKnight,
				EnglishSeat);
			/* Own flag. The enemy flag on the home
			 * rank would end the match. */
			AJoA_Flag* Flag =
				FindFlag(Server, FrenchSeat);
			AJoA_GridManager* Grid =
				AJoA_GameMode::GetGridManager(Server);
			if (!Infantry
				|| !Knight
				|| !Blocker
				|| !Flag
				|| !Grid)
			{
				Fail(TEXT(
					"Build setup is missing a piece."));
				break;
			}

			TargetCell = Infantry->GetStepTarget(
				EStepDirection::Forward);
			const int32 HomeX = Blocker->CellX;
			const int32 HomeY = Blocker->CellY;
			const EFacing HomeFacing = Blocker->Facing;
			const int32 FlagX = Flag->GetCellX();
			const int32 FlagY = Flag->GetCellY();

			auto CanRaise = [&]()
			{
				FIntPoint Found = FIntPoint::ZeroValue;
				return Infantry->FindBuildableCell(Found)
					&& Found == TargetCell;
			};

			Grid->SetObstacleHeight(
				TargetCell.X,
				TargetCell.Y,
				2);
			Test->TestFalse(
				TEXT("Height 2 cannot be raised"),
				CanRaise());
			Grid->SetObstacleHeight(
				TargetCell.X,
				TargetCell.Y,
				1);
			Test->TestTrue(
				TEXT("Height 1 can be raised"),
				CanRaise());
			Grid->SetObstacleHeight(
				TargetCell.X,
				TargetCell.Y,
				0);
			Test->TestTrue(
				TEXT("Empty square can be raised"),
				CanRaise());

			Flag->SetBoardCell(
				TargetCell.X,
				TargetCell.Y);
			Test->TestFalse(
				TEXT("A flag on the square blocks it"),
				CanRaise());
			Test->TestTrue(
				TEXT("Pickup of that flag"),
				Infantry->ResolvePickupFlag());
			Test->TestFalse(
				TEXT("A carried flag blocks building"),
				CanRaise());
			Test->TestTrue(
				TEXT("Drop of the carried flag"),
				Infantry->ResolveDropFlag());
			Flag->SetBoardCell(FlagX, FlagY);
			Test->TestTrue(
				TEXT("The square is free again"),
				CanRaise());

			Blocker->SetBoardPlacement(
				TargetCell.X,
				TargetCell.Y,
				EFacing::PositiveX);
			Test->TestFalse(
				TEXT("A unit blocks the build"),
				CanRaise());
			Blocker->SetBoardPlacement(
				HomeX,
				HomeY,
				HomeFacing);

			FIntPoint KnightCell = FIntPoint::ZeroValue;
			Test->TestFalse(
				TEXT("Heavy knight cannot build"),
				Knight->FindBuildableCell(KnightCell));
			ServerPC->RequestSelectFigure(Infantry);
		}

		AJoA_Figure* Infantry = FindFigure(
			Server,
			EUnitType::LightInfantry,
			FrenchSeat);
		const AJoA_FigureMarker* Ring =
			ServerPC
			? ServerPC->GetObstacleMarker()
			: nullptr;
		const AJoA_FigureMarker* StepRing =
			ServerPC
			? ServerPC->GetStepMarker()
			: nullptr;
		const bool bBrown = Infantry
			&& ServerPC
			&& ServerPC->GetSelectedFigure() == Infantry
			&& Ring
			&& Ring->GetKind()
				== EFigureMarkerKind::Obstacle
			&& (!StepRing
				|| StepRing->GetKind()
					!= EFigureMarkerKind::StepTarget);
		Wait(
			bBrown,
			10.0,
			TEXT("Brown ring on the square in front."));
		break;
	}

	case EStep::BuildRaise:
	{
		if (Once())
		{
			AJoA_Figure* Infantry = FindFigure(
				Server,
				EUnitType::LightInfantry,
				FrenchSeat);
			if (!Infantry)
			{
				Fail(TEXT("Light infantry is missing."));
				break;
			}
			Infantry->RequestBuildObstacle();
		}

		AJoA_GridManager* ServerGrid =
			AJoA_GameMode::GetGridManager(Server);
		AJoA_GridManager* ClientGrid =
			AJoA_GameMode::GetGridManager(Client);
		FGridCell ServerCell;
		FGridCell ClientCell;
		const bool bRaised = ServerGrid
			&& ClientGrid
			&& ServerGrid->GetCell(
				TargetCell.X,
				TargetCell.Y,
				ServerCell)
			&& ClientGrid->GetCell(
				TargetCell.X,
				TargetCell.Y,
				ClientCell)
			&& ServerCell.ObstacleHeight == 1
			&& ClientCell.ObstacleHeight == 1
			&& ActiveSeat(Server) == EnglishSeat
			&& ActiveSeat(Client) == EnglishSeat;
		FinishWhen(
			bRaised,
			TEXT("Timeout: the obstacle was not built."));
		break;
	}

	case EStep::JumpRules:
	{
		AJoA_Figure* Infantry = FindFigure(
			Server,
			EUnitType::LightInfantry,
			FrenchSeat);
		AJoA_Figure* Knight = FindFigure(
			Server,
			EUnitType::HeavyKnight,
			FrenchSeat);
		AJoA_GridManager* Grid =
			AJoA_GameMode::GetGridManager(Server);
		if (Once())
		{
			if (!Infantry || !Knight || !Grid || !ServerPC)
			{
				Fail(TEXT(
					"Jump setup is missing a piece."));
				break;
			}

			const FIntPoint Start(4, 4);
			const FIntPoint Front(3, 4);
			TargetCell = Front;
			Grid->SetObstacleHeight(Start.X, Start.Y, 0);
			Grid->SetObstacleHeight(Front.X, Front.Y, 1);
			Infantry->SetBoardPlacement(
				Start.X,
				Start.Y,
				EFacing::NegativeX);
			Test->TestTrue(
				TEXT("Infantry jumps onto height 1"),
				Infantry->CanJump());
			Test->TestFalse(
				TEXT("Infantry has no fall from ground"),
				Infantry->CanFall());

			Grid->SetObstacleHeight(4, 5, 1);
			Knight->SetBoardPlacement(
				5,
				5,
				EFacing::NegativeX);
			Test->TestFalse(
				TEXT("Heavy knight cannot jump"),
				Knight->CanJump());

			Grid->SetObstacleHeight(Front.X, Front.Y, 2);
			Test->TestFalse(
				TEXT("Cannot jump height 2 from ground"),
				Infantry->CanJump());

			Grid->SetObstacleHeight(Start.X, Start.Y, 1);
			Test->TestTrue(
				TEXT("Jump height 2 from height 1"),
				Infantry->CanJump());
			Test->TestFalse(
				TEXT("No step off an obstacle"),
				Infantry->CanStep(
					EStepDirection::Forward));

			Grid->SetObstacleHeight(Front.X, Front.Y, 0);
			Test->TestTrue(
				TEXT("Fall from height 1"),
				Infantry->CanFall());
			Test->TestFalse(
				TEXT("No jump onto empty ground"),
				Infantry->CanJump());
			Test->TestFalse(
				TEXT("No walk off an obstacle"),
				Infantry->CanStep(
					EStepDirection::Forward));

			Grid->SetObstacleHeight(Start.X, Start.Y, 0);
			Grid->SetObstacleHeight(Front.X, Front.Y, 1);
			Test->TestTrue(
				TEXT("Climb is ready"),
				Infantry->CanJump());
			ServerPC->RequestSelectFigure(Infantry);
		}

		const bool bSelected = Infantry
			&& ServerPC
			&& ServerPC->GetSelectedFigure() == Infantry
			&& Infantry->CanJump();
		Wait(
			bSelected,
			10.0,
			TEXT("Infantry selected and can jump."));
		break;
	}

	case EStep::JumpClimb:
	{
		if (Once())
		{
			AJoA_Figure* Infantry = FindFigure(
				Server,
				EUnitType::LightInfantry,
				FrenchSeat);
			if (!Infantry)
			{
				Fail(TEXT("Light infantry is missing."));
				break;
			}
			Infantry->RequestJump();
		}

		const bool bClimbed =
			IsAt(FrenchLight, TargetCell)
			&& IsAt(FrenchLightC, TargetCell)
			&& ActiveSeat(Server) == EnglishSeat
			&& ActiveSeat(Client) == EnglishSeat;
		Wait(
			bClimbed,
			10.0,
			TEXT("jump onto height 1."));
		break;
	}

	case EStep::JumpSkipEnglish:
	{
		if (Once()
			&& EnglishKnight
			&& ActiveSeat(Server) == EnglishSeat)
		{
			EnglishKnight->RequestSkipTurn();
		}

		Wait(
			ActiveSeat(Server) == FrenchSeat
				&& ActiveSeat(Client) == FrenchSeat,
			10.0,
			TEXT("French turn after the jump."));
		break;
	}

	case EStep::JumpFall:
	{
		if (Once())
		{
			AJoA_Figure* Infantry = FindFigure(
				Server,
				EUnitType::LightInfantry,
				FrenchSeat);
			AJoA_GridManager* Grid =
				AJoA_GameMode::GetGridManager(Server);
			if (!Infantry || !Grid)
			{
				Fail(TEXT("Fall setup is missing."));
				break;
			}

			const FIntPoint Start(4, 4);
			const FIntPoint Front(3, 4);
			TargetCell = Front;
			Grid->SetObstacleHeight(Start.X, Start.Y, 2);
			Grid->SetObstacleHeight(Front.X, Front.Y, 0);
			Infantry->SetBoardPlacement(
				Start.X,
				Start.Y,
				EFacing::NegativeX);
			SavedHealth = Attr(
				Infantry,
				UJoA_GAS_HealthSet::GetHealthAttribute());
			Test->TestTrue(
				TEXT("Fall from height 2"),
				Infantry->CanFall());
			Test->TestFalse(
				TEXT("No jump onto empty ground"),
				Infantry->CanJump());
			Infantry->RequestFall();
		}

		const bool bFell =
			IsAt(FrenchLight, TargetCell)
			&& IsAt(FrenchLightC, TargetCell)
			&& FMath::IsNearlyEqual(
				Attr(
					FrenchLight,
					UJoA_GAS_HealthSet::
						GetHealthAttribute()),
				SavedHealth - 1.f)
			&& FMath::IsNearlyEqual(
				Attr(
					FrenchLightC,
					UJoA_GAS_HealthSet::
						GetHealthAttribute()),
				SavedHealth - 1.f)
			&& ActiveSeat(Server) == EnglishSeat
			&& ActiveSeat(Client) == EnglishSeat;
		FinishWhen(
			bFell,
			TEXT("Timeout: fall from height 2."));
		break;
	}

	case EStep::PromotedJumpPromote:
	{
		if (Once())
		{
			AJoA_Figure* Infantry = FindFigure(
				Server,
				EUnitType::LightInfantry,
				FrenchSeat);
			AJoA_GridManager* Grid =
				AJoA_GameMode::GetGridManager(Server);
			if (!Infantry || !Grid)
			{
				Fail(TEXT("Promote setup is missing."));
				break;
			}

			/* English home is X = 0. Y = 1 is
			 * empty, away from the flag. */
			const FIntPoint Home(0, 1);
			Grid->SetObstacleHeight(Home.X, Home.Y, 0);
			Infantry->SetBoardPlacement(
				Home.X,
				Home.Y,
				EFacing::NegativeX);
		}

		Wait(
			FrenchLight
				&& FrenchLight->IsPromoted()
				&& FrenchLightC
				&& FrenchLightC->IsPromoted(),
			10.0,
			TEXT("French infantry promoted."));
		break;
	}

	case EStep::PromotedJumpRules:
	{
		AJoA_Figure* Infantry = FindFigure(
			Server,
			EUnitType::LightInfantry,
			FrenchSeat);
		AJoA_Figure* Archer = FindFigure(
			Server,
			EUnitType::Archer,
			FrenchSeat);
		AJoA_Figure* Knight = FindFigure(
			Server,
			EUnitType::HeavyKnight,
			FrenchSeat);
		AJoA_GridManager* Grid =
			AJoA_GameMode::GetGridManager(Server);
		if (Once())
		{
			if (!Infantry
				|| !Archer
				|| !Knight
				|| !Grid
				|| !ServerPC)
			{
				Fail(TEXT(
					"Promoted jump setup is missing."));
				break;
			}

			const FIntPoint Start(4, 4);
			const FIntPoint Front(3, 4);
			TargetCell = Front;
			Grid->SetObstacleHeight(Start.X, Start.Y, 0);
			Grid->SetObstacleHeight(Front.X, Front.Y, 2);
			Infantry->SetBoardPlacement(
				Start.X,
				Start.Y,
				EFacing::NegativeX);
			Test->TestTrue(
				TEXT("Promoted jumps empty onto 2"),
				Infantry->CanJump());
			Test->TestFalse(
				TEXT("Promoted has no fall from ground"),
				Infantry->CanFall());

			Grid->SetObstacleHeight(4, 3, 0);
			Grid->SetObstacleHeight(3, 3, 2);
			Archer->SetBoardPlacement(
				4,
				3,
				EFacing::NegativeX);
			Test->TestFalse(
				TEXT("Unpromoted cannot jump onto 2"),
				Archer->CanJump());

			Grid->SetObstacleHeight(5, 5, 0);
			Grid->SetObstacleHeight(4, 5, 2);
			Knight->SetBoardPlacement(
				5,
				5,
				EFacing::NegativeX);
			Test->TestFalse(
				TEXT("Heavy knight cannot jump onto 2"),
				Knight->CanJump());

			ServerPC->RequestSelectFigure(Infantry);
		}

		const bool bSelected = Infantry
			&& ServerPC
			&& ServerPC->GetSelectedFigure() == Infantry
			&& Infantry->CanJump();
		Wait(
			bSelected,
			10.0,
			TEXT("Promoted infantry can jump onto 2."));
		break;
	}

	case EStep::PromotedJumpClimb:
	{
		if (Once())
		{
			AJoA_Figure* Infantry = FindFigure(
				Server,
				EUnitType::LightInfantry,
				FrenchSeat);
			if (!Infantry)
			{
				Fail(TEXT("Light infantry is missing."));
				break;
			}
			Infantry->RequestJump();
		}

		const bool bClimbed =
			IsAt(FrenchLight, TargetCell)
			&& IsAt(FrenchLightC, TargetCell)
			&& ActiveSeat(Server) == EnglishSeat
			&& ActiveSeat(Client) == EnglishSeat;
		Wait(
			bClimbed,
			10.0,
			TEXT("promoted jump onto height 2."));
		break;
	}

	case EStep::PromotedJumpSkipEnglish:
	{
		if (Once()
			&& EnglishKnight
			&& ActiveSeat(Server) == EnglishSeat)
		{
			EnglishKnight->RequestSkipTurn();
		}

		Wait(
			ActiveSeat(Server) == FrenchSeat
				&& ActiveSeat(Client) == FrenchSeat,
			10.0,
			TEXT("French turn after the promoted jump."));
		break;
	}

	case EStep::PromotedJumpFall:
	{
		if (Once())
		{
			AJoA_Figure* Infantry = FindFigure(
				Server,
				EUnitType::LightInfantry,
				FrenchSeat);
			AJoA_GridManager* Grid =
				AJoA_GameMode::GetGridManager(Server);
			if (!Infantry || !Grid)
			{
				Fail(TEXT(
					"Promoted fall setup is missing."));
				break;
			}

			const FIntPoint Front(2, 4);
			TargetCell = Front;
			Grid->SetObstacleHeight(Front.X, Front.Y, 0);
			SavedHealth = Attr(
				Infantry,
				UJoA_GAS_HealthSet::GetHealthAttribute());
			Test->TestTrue(
				TEXT("Promoted fall from height 2"),
				Infantry->CanFall());
			Test->TestFalse(
				TEXT("No jump onto empty ground"),
				Infantry->CanJump());
			Infantry->RequestFall();
		}

		const bool bFell =
			IsAt(FrenchLight, TargetCell)
			&& IsAt(FrenchLightC, TargetCell)
			&& FMath::IsNearlyEqual(
				Attr(
					FrenchLight,
					UJoA_GAS_HealthSet::
						GetHealthAttribute()),
				SavedHealth)
			&& FMath::IsNearlyEqual(
				Attr(
					FrenchLightC,
					UJoA_GAS_HealthSet::
						GetHealthAttribute()),
				SavedHealth)
			&& ActiveSeat(Server) == EnglishSeat
			&& ActiveSeat(Client) == EnglishSeat;
		FinishWhen(
			bFell,
			TEXT("Timeout: promoted fall from height 2."));
		break;
	}

	case EStep::PromotionSetup:
	{
		if (Once())
		{
			AJoA_GridManager* Grid =
				AJoA_GameMode::GetGridManager(Server);
			if (!Grid || !FrenchKnight)
			{
				Fail(TEXT("No French knight."));
				break;
			}

			/* English home is X = 0. X = 1 is one
			 * step away. Y = 1 keeps the flag cells
			 * and the opening line clear. */
			const FIntPoint Start(1, 1);
			Grid->SetObstacleHeight(Start.X, Start.Y, 0);
			Grid->SetObstacleHeight(0, Start.Y, 0);
			FrenchKnight->SetBoardPlacement(
				Start.X,
				Start.Y,
				EFacing::NegativeX);
			ServerPC->RequestSelectFigure(FrenchKnight);
			break;
		}

		Wait(
			FrenchKnight
				&& FrenchKnight->CellX == 1
				&& FrenchKnight->CellY == 1
				&& !FrenchKnight->IsPromoted()
				&& FrenchKnightC
				&& FrenchKnightC->CellX == 1
				&& FrenchKnightC->CellY == 1
				&& !FrenchKnightC->IsPromoted(),
			10.0,
			TEXT("knight one file before English")
			TEXT(" home."));
		break;
	}

	case EStep::PromotionStep:
	{
		if (Once())
		{
			if (FrenchKnight
				&& !FrenchKnight->IsPromoted())
			{
				FrenchKnight->RequestStep(
					EStepDirection::Forward);
			}
			break;
		}

		const FUnitStats Defaults =
			FUnitStats::GetDefaults(
				EUnitType::HeavyKnight);
		const float ExpectedHealth =
			Defaults.MaxHealth + PromotionVitalBonus;
		const float ExpectedStamina =
			Defaults.MaxStamina
			+ PromotionVitalBonus
			- 1.f;
		const float ExpectedAttack =
			Defaults.Attack
			* PromotionCombatMultiplier;
		const float ExpectedDefense =
			Defaults.Defense
			* PromotionCombatMultiplier;
		const float ExpectedScale =
			0.5f * PromotionMeshMultiplier;

		const USkeletalMeshComponent* Mesh =
			FrenchKnight
				? FrenchKnight->GetBodyMesh()
				: nullptr;
		const USkeletalMeshComponent* MeshC =
			FrenchKnightC
				? FrenchKnightC->GetBodyMesh()
				: nullptr;
		const bool bScale =
			Mesh
			&& MeshC
			&& FMath::IsNearlyEqual(
				Mesh->GetRelativeScale3D().X,
				ExpectedScale)
			&& FMath::IsNearlyEqual(
				MeshC->GetRelativeScale3D().X,
				ExpectedScale);

		FinishWhen(
			FrenchKnight
				&& FrenchKnightC
				&& FrenchKnight->IsPromoted()
				&& FrenchKnightC->IsPromoted()
				&& FrenchKnight->CellX == 0
				&& FrenchKnightC->CellX == 0
				&& FMath::IsNearlyEqual(
					Attr(
						FrenchKnight,
						UJoA_GAS_HealthSet::
							GetHealthAttribute()),
					ExpectedHealth)
				&& FMath::IsNearlyEqual(
					Attr(
						FrenchKnightC,
						UJoA_GAS_HealthSet::
							GetHealthAttribute()),
					ExpectedHealth)
				&& FMath::IsNearlyEqual(
					Attr(
						FrenchKnight,
						UJoA_GAS_HealthSet::
							GetMaxHealthAttribute()),
					ExpectedHealth)
				&& FMath::IsNearlyEqual(
					Attr(
						FrenchKnight,
						UJoA_GAS_StaminaSet::
							GetStaminaAttribute()),
					ExpectedStamina)
				&& FMath::IsNearlyEqual(
					Attr(
						FrenchKnightC,
						UJoA_GAS_StaminaSet::
							GetStaminaAttribute()),
					ExpectedStamina)
				&& FMath::IsNearlyEqual(
					Attr(
						FrenchKnight,
						UJoA_GAS_CombatSet::
							GetAttackAttribute()),
					ExpectedAttack)
				&& FMath::IsNearlyEqual(
					Attr(
						FrenchKnightC,
						UJoA_GAS_CombatSet::
							GetAttackAttribute()),
					ExpectedAttack)
				&& FMath::IsNearlyEqual(
					Attr(
						FrenchKnight,
						UJoA_GAS_CombatSet::
							GetDefenseAttribute()),
					ExpectedDefense)
				&& bScale,
			TEXT("Timeout: reaching English home")
			TEXT(" did not promote the knight."));
		break;
	}

	case EStep::PromotionEnglandSkip:
	{
		if (Once())
		{
			ServerPC->RequestSelectFigure(FrenchKnight);
			break;
		}

		if (!bActionSent
			&& ServerPC->GetSelectedFigure()
				== FrenchKnight)
		{
			bActionSent = true;
			FrenchKnight->RequestSkipTurn();
		}

		Wait(
			ActiveSeat(Server) == EnglishSeat
				&& ActiveSeat(Client) == EnglishSeat,
			10.0,
			TEXT("English seat after the French skip."));
		break;
	}

	case EStep::PromotionEnglandSetup:
	{
		if (Once())
		{
			AJoA_GridManager* Grid =
				AJoA_GameMode::GetGridManager(Server);
			if (!Grid
				|| !EnglishMedic
				|| !EnglishMedicC
				|| !ClientPC)
			{
				Fail(TEXT("No English mage."));
				break;
			}

			/* French home is X = 7. X = 6 is one
			 * step away. Y = 6 keeps the flag
			 * cells and the opening line clear. */
			const FIntPoint Start(6, 6);
			Grid->SetObstacleHeight(Start.X, Start.Y, 0);
			Grid->SetObstacleHeight(7, Start.Y, 0);
			EnglishMedic->SetBoardPlacement(
				Start.X,
				Start.Y,
				EFacing::PositiveX);
			ClientPC->RequestSelectFigure(EnglishMedicC);
			break;
		}

		Wait(
			EnglishMedic
				&& EnglishMedic->CellX == 6
				&& EnglishMedic->CellY == 6
				&& !EnglishMedic->IsPromoted()
				&& EnglishMedicC
				&& EnglishMedicC->CellX == 6
				&& EnglishMedicC->CellY == 6
				&& !EnglishMedicC->IsPromoted()
				&& ClientPC
				&& ClientPC->GetSelectedFigure()
					== EnglishMedicC,
			10.0,
			TEXT("mage one file before French")
			TEXT(" home."));
		break;
	}

	case EStep::PromotionEnglandStep:
	{
		if (Once())
		{
			if (EnglishMedicC
				&& !EnglishMedicC->IsPromoted())
			{
				EnglishMedicC->RequestStep(
					EStepDirection::Forward);
			}
			break;
		}

		const FUnitStats Defaults =
			FUnitStats::GetDefaults(EUnitType::Medic);
		const float ExpectedHealth =
			Defaults.MaxHealth + PromotionVitalBonus;
		const float ExpectedMana =
			Defaults.MaxMana + PromotionVitalBonus;
		const float ExpectedHeal =
			Defaults.Heal * PromotionCombatMultiplier;
		const float ExpectedScale =
			0.5f * PromotionMeshMultiplier;

		const USkeletalMeshComponent* Mesh =
			EnglishMedic
				? EnglishMedic->GetBodyMesh()
				: nullptr;
		const USkeletalMeshComponent* MeshC =
			EnglishMedicC
				? EnglishMedicC->GetBodyMesh()
				: nullptr;
		const bool bScale =
			Mesh
			&& MeshC
			&& FMath::IsNearlyEqual(
				Mesh->GetRelativeScale3D().X,
				ExpectedScale)
			&& FMath::IsNearlyEqual(
				MeshC->GetRelativeScale3D().X,
				ExpectedScale);

		FinishWhen(
			EnglishMedic
				&& EnglishMedicC
				&& EnglishMedic->IsPromoted()
				&& EnglishMedicC->IsPromoted()
				&& EnglishMedic->CellX == 7
				&& EnglishMedicC->CellX == 7
				&& FMath::IsNearlyEqual(
					Attr(
						EnglishMedic,
						UJoA_GAS_HealthSet::
							GetHealthAttribute()),
					ExpectedHealth)
				&& FMath::IsNearlyEqual(
					Attr(
						EnglishMedicC,
						UJoA_GAS_HealthSet::
							GetHealthAttribute()),
					ExpectedHealth)
				&& FMath::IsNearlyEqual(
					Attr(
						EnglishMedic,
						UJoA_GAS_ManaSet::
							GetManaAttribute()),
					ExpectedMana)
				&& FMath::IsNearlyEqual(
					Attr(
						EnglishMedicC,
						UJoA_GAS_ManaSet::
							GetManaAttribute()),
					ExpectedMana)
				&& FMath::IsNearlyEqual(
					Attr(
						EnglishMedic,
						UJoA_GAS_CombatSet::
							GetHealAttribute()),
					ExpectedHeal)
				&& FMath::IsNearlyEqual(
					Attr(
						EnglishMedicC,
						UJoA_GAS_CombatSet::
							GetHealAttribute()),
					ExpectedHeal)
				&& bScale,
			TEXT("Timeout: reaching French home")
			TEXT(" did not promote the mage."));
		break;
	}

	case EStep::FigureAnimIdle:
	{
		AJoA_Figure* Medic = FindFigure(
			Server,
			EUnitType::Medic,
			FrenchSeat);
		AJoA_Figure* MedicC = FindFigure(
			Client,
			EUnitType::Medic,
			FrenchSeat);
		Wait(
			JoATest::Plays(Medic, EFigureAnim::Idle)
				&& JoATest::Plays(
					MedicC,
					EFigureAnim::Idle)
				&& JoATest::SeatPlays(
					Server,
					FrenchSeat,
					EFigureAnim::Idle)
				&& JoATest::SeatPlays(
					Client,
					FrenchSeat,
					EFigureAnim::Idle),
			10.0,
			TEXT("opening idle on every figure."));
		break;
	}

	case EStep::FigureAnimSit:
		if (Once())
		{
			JoATest::SkipActive(Server);
		}
		Wait(
			ActiveSeat(Server) == EnglishSeat
				&& JoATest::SeatPlays(
					Server,
					FrenchSeat,
					EFigureAnim::Sit)
				&& JoATest::SeatPlays(
					Client,
					FrenchSeat,
					EFigureAnim::Sit),
			10.0,
			TEXT("unused French figures sit."));
		break;

	case EStep::FigureAnimSitEnglish:
		if (Once())
		{
			JoATest::SkipActive(Server);
		}
		Wait(
			ActiveSeat(Server) == FrenchSeat
				&& JoATest::SeatPlays(
					Server,
					EnglishSeat,
					EFigureAnim::Sit)
				&& JoATest::SeatPlays(
					Client,
					EnglishSeat,
					EFigureAnim::Sit),
			10.0,
			TEXT("unused English figures sit."));
		break;

	case EStep::FigureAnimLie:
		if (Once())
		{
			JoATest::SkipActive(Server);
		}
		Wait(
			ActiveSeat(Server) == EnglishSeat
				&& JoATest::SeatPlays(
					Server,
					FrenchSeat,
					EFigureAnim::Lie)
				&& JoATest::SeatPlays(
					Client,
					FrenchSeat,
					EFigureAnim::Lie),
			10.0,
			TEXT("second unused French turn lies."));
		break;

	case EStep::FigureAnimLieEnglish:
		if (Once())
		{
			JoATest::SkipActive(Server);
		}
		Wait(
			ActiveSeat(Server) == FrenchSeat
				&& JoATest::SeatPlays(
					Server,
					EnglishSeat,
					EFigureAnim::Lie)
				&& JoATest::SeatPlays(
					Client,
					EnglishSeat,
					EFigureAnim::Lie),
			10.0,
			TEXT("second unused English turn lies."));
		break;

	case EStep::FigureAnimPick:
		if (Once())
		{
			AJoA_Figure* Medic = FindFigure(
				Server,
				EUnitType::Medic,
				FrenchSeat);
			if (!Medic || !ServerPC)
			{
				Fail(TEXT("Medic pick is missing."));
				break;
			}
			ServerPC->RequestSelectFigure(Medic);
		}
		{
			AJoA_Figure* Medic = FindFigure(
				Server,
				EUnitType::Medic,
				FrenchSeat);
			AJoA_Figure* MedicC = FindFigure(
				Client,
				EUnitType::Medic,
				FrenchSeat);
			Wait(
				JoATest::Plays(Medic, EFigureAnim::Idle)
					&& JoATest::Plays(
						MedicC,
						EFigureAnim::Idle),
				10.0,
				TEXT("click stands the medic."));
		}
		break;

	case EStep::FigureAnimClipRun:
	case EStep::FigureAnimClipAttack:
	case EStep::FigureAnimClipHeal:
	case EStep::FigureAnimClipShoot:
	case EStep::FigureAnimClipDodge:
	case EStep::FigureAnimClipJump:
	case EStep::FigureAnimClipFall:
	{
		EFigureAnim Clip = EFigureAnim::Run;
		const TCHAR* What = TEXT("run clip");
		switch (Step)
		{
		case EStep::FigureAnimClipAttack:
			Clip = EFigureAnim::Attack;
			What = TEXT("attack clip");
			break;
		case EStep::FigureAnimClipHeal:
			Clip = EFigureAnim::Heal;
			What = TEXT("heal clip");
			break;
		case EStep::FigureAnimClipShoot:
			Clip = EFigureAnim::Shoot;
			What = TEXT("shoot clip");
			break;
		case EStep::FigureAnimClipDodge:
			Clip = EFigureAnim::Dodge;
			What = TEXT("dodge clip");
			break;
		case EStep::FigureAnimClipJump:
			Clip = EFigureAnim::Jump;
			What = TEXT("jump clip");
			break;
		case EStep::FigureAnimClipFall:
			Clip = EFigureAnim::Fall;
			What = TEXT("fall clip");
			break;
		default:
			break;
		}

		if (Once())
		{
			AJoA_Figure* Medic = FindFigure(
				Server,
				EUnitType::Medic,
				FrenchSeat);
			if (!Medic)
			{
				Fail(TEXT("Clip medic is missing."));
				break;
			}
			Medic->MulticastBodyAnim(Clip);
		}

		AJoA_Figure* Medic = FindFigure(
			Server,
			EUnitType::Medic,
			FrenchSeat);
		AJoA_Figure* MedicC = FindFigure(
			Client,
			EUnitType::Medic,
			FrenchSeat);
		Wait(
			JoATest::Plays(Medic, Clip)
				&& JoATest::Plays(MedicC, Clip),
			10.0,
			What);
		break;
	}

	case EStep::FigureAnimWound:
		if (Once())
		{
			AJoA_Figure* Knight = FindFigure(
				Server,
				EUnitType::HeavyKnight,
				FrenchSeat);
			AJoA_Figure* Target = FindFigure(
				Server,
				EUnitType::HeavyKnight,
				EnglishSeat);
			if (!Knight || !Target || !ServerPC)
			{
				Fail(TEXT("Wound setup is missing."));
				break;
			}

			Target->SetBoardPlacement(
				4,
				4,
				EFacing::PositiveX);
			Knight->SetBoardPlacement(
				5,
				4,
				EFacing::NegativeX);
			ServerPC->RequestSelectFigure(Knight);
			AJoA_Figure::bForceCombatHit = true;
			AJoA_Figure::bForceCombatMiss = false;
			const FAttackResult Result =
				Knight->ResolveAttack(Target);
			AJoA_Figure::bForceCombatHit = false;
			if (!Result.bResolved || !Result.bHit)
			{
				Fail(TEXT("Forced hit did not resolve."));
				break;
			}
		}
		Wait(
			JoATest::Plays(EnglishKnight, EFigureAnim::Hit)
				&& JoATest::Plays(
					EnglishKnightC,
					EFigureAnim::Hit)
				&& JoATest::Plays(
					FindFigure(
						Server,
						EUnitType::Medic,
						EnglishSeat),
					EFigureAnim::Cry)
				&& JoATest::Plays(
					FindFigure(
						Client,
						EUnitType::Medic,
						EnglishSeat),
					EFigureAnim::Cry)
				&& JoATest::Plays(
					FindFigure(
						Server,
						EUnitType::Medic,
						FrenchSeat),
					EFigureAnim::Clap)
				&& JoATest::Plays(
					FindFigure(
						Client,
						EUnitType::Medic,
						FrenchSeat),
					EFigureAnim::Clap)
				&& JoATest::Plays(
					FrenchKnight,
					EFigureAnim::Clap)
				&& JoATest::Plays(
					FrenchKnightC,
					EFigureAnim::Clap),
			10.0,
			TEXT("wound: we clap, they cry."));
		break;

	case EStep::FigureAnimNewsIdle:
		Wait(
			JoATest::LivingNewsDone(Server)
				&& JoATest::LivingNewsDone(Client),
			15.0,
			TEXT("clap and cry stay idle."));
		break;

	case EStep::FigureAnimMiss:
		if (Once())
		{
			AJoA_Figure* Knight = FindFigure(
				Server,
				EUnitType::HeavyKnight,
				FrenchSeat);
			AJoA_Figure* Target = FindFigure(
				Server,
				EUnitType::Medic,
				EnglishSeat);
			if (!Knight || !Target || !ServerPC)
			{
				Fail(TEXT("Miss setup is missing."));
				break;
			}

			if (ActiveSeat(Server) != FrenchSeat)
			{
				JoATest::SkipActive(Server);
			}

			Target->SetBoardPlacement(
				4,
				4,
				EFacing::PositiveX);
			Knight->SetBoardPlacement(
				5,
				4,
				EFacing::NegativeX);
			ServerPC->RequestSelectFigure(Knight);
			AJoA_Figure::bForceCombatHit = false;
			AJoA_Figure::bForceCombatMiss = true;
			const FAttackResult Result =
				Knight->ResolveAttack(Target);
			AJoA_Figure::bForceCombatMiss = false;
			if (!Result.bResolved || Result.bHit)
			{
				Fail(TEXT("Forced miss did not resolve."));
				break;
			}
		}
		Wait(
			JoATest::Plays(EnglishMedic, EFigureAnim::Dodge)
				&& JoATest::Plays(
					EnglishMedicC,
					EFigureAnim::Dodge)
				&& JoATest::Plays(
					EnglishKnight,
					EFigureAnim::Cheer)
				&& JoATest::Plays(
					EnglishKnightC,
					EFigureAnim::Cheer)
				&& JoATest::Plays(
					FindFigure(
						Server,
						EUnitType::Medic,
						FrenchSeat),
					EFigureAnim::Cry)
				&& JoATest::Plays(
					FindFigure(
						Client,
						EUnitType::Medic,
						FrenchSeat),
					EFigureAnim::Cry)
				&& JoATest::Plays(
					FrenchKnight,
					EFigureAnim::Cry)
				&& JoATest::Plays(
					FrenchKnightC,
					EFigureAnim::Cry)
				&& !JoATest::Plays(
					EnglishMedic,
					EFigureAnim::Cheer)
				&& !JoATest::Plays(
					EnglishMedic,
					EFigureAnim::Hit),
			10.0,
			TEXT("miss: they cheer, we cry."));
		break;

	case EStep::FigureAnimKill:
		if (Once())
		{
			Kill(Server, EUnitType::Archer);
		}
		Wait(
			JoATest::Plays(
				FindFigure(
					Server,
					EUnitType::Archer,
					EnglishSeat),
				EFigureAnim::Death)
				&& JoATest::Plays(
					FindFigure(
						Client,
						EUnitType::Archer,
						EnglishSeat),
					EFigureAnim::Death)
				&& FindFigure(
					Server,
					EUnitType::Archer,
					EnglishSeat)
				&& !FindFigure(
					Server,
					EUnitType::Archer,
					EnglishSeat)->IsOnBench()
				&& FindFigure(
					Client,
					EUnitType::Archer,
					EnglishSeat)
				&& !FindFigure(
					Client,
					EUnitType::Archer,
					EnglishSeat)->IsOnBench(),
			10.0,
			TEXT("death plays on the square."));
		break;

	case EStep::FigureAnimKillBench:
	{
		AJoA_Figure* Archer = FindFigure(
			Server,
			EUnitType::Archer,
			EnglishSeat);
		AJoA_Figure* ArcherC = FindFigure(
			Client,
			EUnitType::Archer,
			EnglishSeat);
		Wait(
			Archer
				&& ArcherC
				&& Archer->IsOnBench()
				&& ArcherC->IsOnBench()
				&& (JoATest::Plays(
						Archer,
						EFigureAnim::Wave)
					|| JoATest::Plays(
						Archer,
						EFigureAnim::Idle))
				&& (JoATest::Plays(
						ArcherC,
						EFigureAnim::Wave)
					|| JoATest::Plays(
						ArcherC,
						EFigureAnim::Idle)),
			15.0,
			TEXT("dead figure moves to the edge."));
		break;
	}

	case EStep::FigureAnimBenchSit:
		if (Once())
		{
			JoATest::SkipActive(Server);
		}
		Wait(
			ActiveSeat(Server) == FrenchSeat
				&& JoATest::Plays(
					FindFigure(
						Server,
						EUnitType::Archer,
						EnglishSeat),
					EFigureAnim::Idle),
			10.0,
			TEXT("fresh bench skips one turn."));
		break;

	case EStep::FigureAnimBenchLie:
		if (Once())
		{
			JoATest::SkipActive(Server);
		}
		Wait(
			JoATest::Plays(
				FindFigure(
					Server,
					EUnitType::Archer,
					EnglishSeat),
				EFigureAnim::Sit)
				&& JoATest::Plays(
					FindFigure(
						Client,
						EUnitType::Archer,
						EnglishSeat),
					EFigureAnim::Sit),
			10.0,
			TEXT("quiet turn sits the bench."));
		break;

	case EStep::FigureAnimBenchNews:
		if (Once())
		{
			JoATest::SkipActive(Server);
		}

		if (JoATest::Plays(
				FindFigure(
					Server,
					EUnitType::Archer,
					EnglishSeat),
				EFigureAnim::Lie)
			&& !bActionSent)
		{
			bActionSent = true;
			AJoA_Figure::SetForcedBenchStart(0);
			AJoA_Figure::BroadcastGoodNews(
				Server,
				FrenchSeat);
		}

		Wait(
			bActionSent
				&& JoATest::Plays(
					FindFigure(
						Server,
						EUnitType::Archer,
						EnglishSeat),
					EFigureAnim::Idle)
				&& JoATest::Plays(
					FindFigure(
						Client,
						EUnitType::Archer,
						EnglishSeat),
					EFigureAnim::Idle)
				&& JoATest::Plays(
					FindFigure(
						Server,
						EUnitType::Medic,
						FrenchSeat),
					EFigureAnim::Clap),
			10.0,
			TEXT("bench lies, then cry resets to idle."));
		break;

	case EStep::FigureAnimFlag:
		if (Once())
		{
			AJoA_Figure* Medic = FindFigure(
				Server,
				EUnitType::Medic,
				FrenchSeat);
			AJoA_Flag* Flag = JoATest::FindFlag(
				Server,
				EnglishSeat);
			if (!Medic || !Flag || !ServerPC)
			{
				Fail(TEXT("Flag pickup setup is missing."));
				break;
			}

			if (ActiveSeat(Server) != FrenchSeat)
			{
				JoATest::SkipActive(Server);
			}

			Flag->SetBoardCell(
				Medic->CellX,
				Medic->CellY);
			ServerPC->RequestSelectFigure(Medic);
			if (!Medic->ResolvePickupFlag())
			{
				Fail(TEXT("Flag pickup failed."));
				break;
			}
		}
		Wait(
			!JoATest::Plays(
				FindFigure(
					Server,
					EUnitType::Medic,
					FrenchSeat),
				EFigureAnim::Clap)
				&& !JoATest::Plays(
					FindFigure(
						Server,
						EUnitType::Medic,
						FrenchSeat),
					EFigureAnim::Cheer)
				&& JoATest::Plays(
					FindFigure(
						Server,
						EUnitType::HeavyKnight,
						FrenchSeat),
					EFigureAnim::Clap)
				&& JoATest::SeatPlays(
					Server,
					EnglishSeat,
					EFigureAnim::Cry),
			10.0,
			TEXT("flag pickup: mates clap, picker stays."));
		break;

	/* The medic still carries the flag. Cry, cheer,
	 * and clap must change nothing on the carrier,
	 * on the server or on the client. */
	case EStep::FigureAnimFlagBlock:
	{
		AJoA_Figure* Medic = FindFigure(
			Server,
			EUnitType::Medic,
			FrenchSeat);
		AJoA_Figure* MedicC = FindFigure(
			Client,
			EUnitType::Medic,
			FrenchSeat);
		if (Once())
		{
			if (!Medic
				|| !MedicC
				|| !Medic->GetHeldFlag())
			{
				Fail(TEXT("Carrier setup is missing."));
				break;
			}

			Medic->MulticastBodyAnim(EFigureAnim::Cry);
			Medic->MulticastBodyAnim(EFigureAnim::Cheer);
			Medic->MulticastBodyAnim(EFigureAnim::Clap);
			AJoA_Figure::BroadcastBadNews(
				Server,
				FrenchSeat);
			AJoA_Figure::BroadcastGoodNews(
				Server,
				FrenchSeat);
		}

		auto Reacts = [](const AJoA_Figure* Figure)
		{
			using JoATest::Plays;
			return Plays(Figure, EFigureAnim::Cry)
				|| Plays(Figure, EFigureAnim::Cheer)
				|| Plays(Figure, EFigureAnim::Clap);
		};
		if (Reacts(Medic) || Reacts(MedicC))
		{
			Fail(TEXT("Flag carrier reacted."));
			break;
		}
		Wait(
			Medic && MedicC,
			10.0,
			TEXT("carrier ignores cry, cheer, and clap."),
			3.0);
		break;
	}

	case EStep::FigureAnimFall:
		if (Once())
		{
			AJoA_Figure* Infantry = FindFigure(
				Server,
				EUnitType::LightInfantry,
				FrenchSeat);
			AJoA_GridManager* Grid =
				AJoA_GameMode::GetGridManager(Server);
			if (!Infantry || !Grid || !ServerPC)
			{
				Fail(TEXT("Fall setup is missing."));
				break;
			}

			if (ActiveSeat(Server) != FrenchSeat)
			{
				JoATest::SkipActive(Server);
			}

			const FIntPoint Start(4, 3);
			const FIntPoint Front(3, 3);
			Grid->SetObstacleHeight(Start.X, Start.Y, 2);
			Grid->SetObstacleHeight(Front.X, Front.Y, 0);
			Infantry->SetBoardPlacement(
				Start.X,
				Start.Y,
				EFacing::NegativeX);
			ServerPC->RequestSelectFigure(Infantry);
			if (!Infantry->ResolveFall())
			{
				Fail(TEXT("Fall did not resolve."));
				break;
			}
		}
		FinishWhen(
			JoATest::Plays(
				FindFigure(
					Server,
					EUnitType::LightInfantry,
					EnglishSeat),
					EFigureAnim::Cheer)
				&& JoATest::Plays(
					FindFigure(
						Server,
						EUnitType::HeavyKnight,
						FrenchSeat),
					EFigureAnim::Cry),
			TEXT("Timeout: fall reactions missing."));
		break;

	case EStep::MeleeRules:
	{
		if (Once())
		{
			AJoA_Figure* Knight = FindFigure(
				Server,
				EUnitType::HeavyKnight,
				FrenchSeat);
			AJoA_Figure* Infantry = FindFigure(
				Server,
				EUnitType::LightInfantry,
				FrenchSeat);
			AJoA_GridManager* Grid =
				AJoA_GameMode::GetGridManager(Server);
			if (!Knight
				|| !Infantry
				|| !Grid
				|| !ServerPC)
			{
				Fail(TEXT("Melee setup is missing."));
				break;
			}

			if (ActiveSeat(Server) != FrenchSeat)
			{
				JoATest::SkipActive(Server);
			}

			JoATest::ClearObstacles(Server);
			JoATest::ParkSeat(
				Server,
				FrenchSeat,
				7);
			JoATest::ParkSeat(
				Server,
				EnglishSeat,
				0);

			const FIntPoint Atk(5, 4);
			const FIntPoint Front1(4, 4);
			const FIntPoint Front2(3, 4);
			const FIntPoint Front3(2, 4);
			const FIntPoint Side(5, 3);
			const EUnitType Enemies[] = {
				EUnitType::HeavyKnight,
				EUnitType::LightInfantry,
				EUnitType::Archer,
				EUnitType::Medic
			};

			bool bOk = true;
			auto Expect = [&](
				const TCHAR* What,
				bool bGot)
			{
				if (!Test->TestTrue(What, bGot))
				{
					bOk = false;
				}
			};

			ServerPC->RequestSelectFigure(Knight);
			Knight->SetBoardPlacement(
				Atk.X,
				Atk.Y,
				EFacing::NegativeX);

			for (const EUnitType Type : Enemies)
			{
				AJoA_Figure* Target = FindFigure(
					Server,
					Type,
					EnglishSeat);
				if (!Target)
				{
					Fail(TEXT("Melee target missing."));
					break;
				}

				Target->SetBoardPlacement(
					Front1.X,
					Front1.Y,
					EFacing::PositiveX);
				Expect(
					TEXT("HK hits any adjacent foe"),
					Knight->CanAttackTarget(Target));

				Target->SetBoardPlacement(
					Front2.X,
					Front2.Y,
					EFacing::PositiveX);
				Expect(
					TEXT("HK cannot hit range 2"),
					!Knight->CanAttackTarget(Target));

				Target->SetBoardPlacement(
					Side.X,
					Side.Y,
					EFacing::PositiveX);
				Expect(
					TEXT("HK needs the facing line"),
					!Knight->CanAttackTarget(Target));

				Target->SetBoardPlacement(
					Front1.X,
					Front1.Y,
					EFacing::PositiveX);
				Grid->SetObstacleHeight(
					Front1.X,
					Front1.Y,
					1);
				Expect(
					TEXT("HK cannot hit height 1"),
					!Knight->CanAttackTarget(Target));
				Grid->SetObstacleHeight(
					Front1.X,
					Front1.Y,
					2);
				Expect(
					TEXT("HK cannot hit height 2"),
					!Knight->CanAttackTarget(Target));
				Grid->SetObstacleHeight(
					Front1.X,
					Front1.Y,
					0);

				JoATest::ParkSeat(
					Server,
					EnglishSeat,
					0);
			}

			if (bFailed)
			{
				break;
			}

			ServerPC->RequestSelectFigure(Infantry);
			Infantry->SetBoardPlacement(
				Atk.X,
				Atk.Y,
				EFacing::NegativeX);

			for (const EUnitType Type : Enemies)
			{
				AJoA_Figure* Target = FindFigure(
					Server,
					Type,
					EnglishSeat);
				if (!Target)
				{
					Fail(TEXT("LI target missing."));
					break;
				}

				Target->SetBoardPlacement(
					Front1.X,
					Front1.Y,
					EFacing::PositiveX);
				Expect(
					TEXT("LI hits any adjacent foe"),
					Infantry->CanAttackTarget(Target));

				Target->SetBoardPlacement(
					Front2.X,
					Front2.Y,
					EFacing::PositiveX);
				Expect(
					TEXT("LI hits range 2"),
					Infantry->CanAttackTarget(Target));

				Target->SetBoardPlacement(
					Front3.X,
					Front3.Y,
					EFacing::PositiveX);
				Expect(
					TEXT("LI cannot hit range 3"),
					!Infantry->CanAttackTarget(Target));

				Target->SetBoardPlacement(
					Front1.X,
					Front1.Y,
					EFacing::PositiveX);
				Grid->SetObstacleHeight(
					Front1.X,
					Front1.Y,
					1);
				Expect(
					TEXT("LI hits a foe on height 1"),
					Infantry->CanAttackTarget(Target));
				Grid->SetObstacleHeight(
					Front1.X,
					Front1.Y,
					2);
				Expect(
					TEXT("LI cannot hit height 2"),
					!Infantry->CanAttackTarget(Target));
				Grid->SetObstacleHeight(
					Front1.X,
					Front1.Y,
					0);

				Target->SetBoardPlacement(
					Front2.X,
					Front2.Y,
					EFacing::PositiveX);
				Grid->SetObstacleHeight(
					Front1.X,
					Front1.Y,
					1);
				Expect(
					TEXT("LI cannot hit behind 1"),
					!Infantry->CanAttackTarget(Target));
				Grid->SetObstacleHeight(
					Front1.X,
					Front1.Y,
					2);
				Expect(
					TEXT("LI cannot hit behind 2"),
					!Infantry->CanAttackTarget(Target));
				Grid->SetObstacleHeight(
					Front1.X,
					Front1.Y,
					0);

				JoATest::ParkSeat(
					Server,
					EnglishSeat,
					0);
			}

			AJoA_Figure* Last = FindFigure(
				Server,
				EUnitType::Medic,
				EnglishSeat);
			if (Last)
			{
				Last->SetBoardPlacement(
					Front1.X,
					Front1.Y,
					EFacing::PositiveX);
			}

			if (!bOk)
			{
				Fail(TEXT("Melee reach rules failed."));
			}
		}

		AJoA_Figure* Infantry = FindFigure(
			Server,
			EUnitType::LightInfantry,
			FrenchSeat);
		AJoA_Figure* InfantryC = FindFigure(
			Client,
			EUnitType::LightInfantry,
			FrenchSeat);
		AJoA_Figure* Medic = FindFigure(
			Server,
			EUnitType::Medic,
			EnglishSeat);
		AJoA_Figure* MedicC = FindFigure(
			Client,
			EUnitType::Medic,
			EnglishSeat);
		Wait(
			Infantry
				&& InfantryC
				&& Medic
				&& MedicC
				&& JoATest::IsAt(Infantry, FIntPoint(5, 4))
				&& JoATest::IsAt(InfantryC, FIntPoint(5, 4))
				&& JoATest::IsAt(Medic, FIntPoint(4, 4))
				&& JoATest::IsAt(MedicC, FIntPoint(4, 4))
				&& Infantry->CanAttackTarget(Medic)
				&& InfantryC->CanAttackTarget(MedicC),
			10.0,
			TEXT("LI reach matches on the client."));
		break;
	}

	case EStep::MeleeClientHit:
	{
		if (Once())
		{
			AJoA_Figure* AttackerC = FindFigure(
				Client,
				EUnitType::HeavyKnight,
				EnglishSeat);
			AJoA_Figure* Target = FindFigure(
				Server,
				EUnitType::HeavyKnight,
				FrenchSeat);
			AJoA_Figure* TargetC = FindFigure(
				Client,
				EUnitType::HeavyKnight,
				FrenchSeat);
			if (!AttackerC
				|| !Target
				|| !TargetC
				|| !ClientPC
				|| !ServerPC)
			{
				Fail(TEXT("Client hit setup missing."));
				break;
			}

			if (ActiveSeat(Server) == FrenchSeat)
			{
				JoATest::SkipActive(Server);
			}

			JoATest::ClearObstacles(Server);
			JoATest::ParkSeat(
				Server,
				FrenchSeat,
				7);
			JoATest::ParkSeat(
				Server,
				EnglishSeat,
				0);
			Target->SetBoardPlacement(
				4,
				4,
				EFacing::PositiveX);
			AJoA_Figure* Attacker = FindFigure(
				Server,
				EUnitType::HeavyKnight,
				EnglishSeat);
			if (!Attacker)
			{
				Fail(TEXT("English HK missing."));
				break;
			}

			Attacker->SetBoardPlacement(
				5,
				4,
				EFacing::NegativeX);
			ClientPC->RequestSelectFigure(AttackerC);
			AJoA_Figure::bForceCombatHit = true;
			AJoA_Figure::bForceCombatMiss = false;
		}

		AJoA_Figure* AttackerC = FindFigure(
			Client,
			EUnitType::HeavyKnight,
			EnglishSeat);
		AJoA_Figure* Target = FindFigure(
			Server,
			EUnitType::HeavyKnight,
			FrenchSeat);
		AJoA_Figure* TargetC = FindFigure(
			Client,
			EUnitType::HeavyKnight,
			FrenchSeat);
		if (AttackerC
			&& TargetC
			&& ClientPC
			&& ClientPC->GetSelectedFigure() == AttackerC
			&& JoATest::IsAt(Target, FIntPoint(4, 4))
			&& JoATest::IsAt(TargetC, FIntPoint(4, 4))
			&& AttackerC->CanAttackTarget(TargetC)
			&& !bActionSent)
		{
			bActionSent = true;
			ClientPC->RequestAttack(AttackerC, TargetC);
		}

		FinishWhen(
			bActionSent
				&& JoATest::Plays(Target, EFigureAnim::Hit)
				&& JoATest::Plays(TargetC, EFigureAnim::Hit),
			TEXT("Timeout: client slash missed Hit."));
		AJoA_Figure::bForceCombatHit = false;
		break;
	}

	case EStep::MedicHealRules:
	{
		if (Once())
		{
			AJoA_Figure* Medic = FindFigure(
				Server,
				EUnitType::Medic,
				FrenchSeat);
			AJoA_Figure* Ally = FindFigure(
				Server,
				EUnitType::LightInfantry,
				FrenchSeat);
			AJoA_Figure* Foe = FindFigure(
				Server,
				EUnitType::HeavyKnight,
				EnglishSeat);
			AJoA_GridManager* Grid =
				AJoA_GameMode::GetGridManager(Server);
			if (!Medic
				|| !Ally
				|| !Foe
				|| !Grid
				|| !ServerPC)
			{
				Fail(TEXT("Heal setup is missing."));
				break;
			}

			if (ActiveSeat(Server) != FrenchSeat)
			{
				JoATest::SkipActive(Server);
			}

			JoATest::ClearObstacles(Server);
			JoATest::ParkSeat(
				Server,
				FrenchSeat,
				7);
			JoATest::ParkSeat(
				Server,
				EnglishSeat,
				0);
			JoATest::Wound(Medic, 6.f);
			JoATest::Wound(Ally, 6.f);
			ServerPC->RequestSelectFigure(Medic);

			const FIntPoint At(4, 4);
			Medic->SetBoardPlacement(
				At.X,
				At.Y,
				EFacing::NegativeX);

			bool bOk = true;
			auto Expect = [&](
				const TCHAR* What,
				bool bGot)
			{
				if (!Test->TestTrue(What, bGot))
				{
					bOk = false;
				}
			};

			Expect(
				TEXT("Medic heals himself"),
				Medic->CanHealTarget(Medic));

			const FIntPoint Axis[] = {
				FIntPoint(-1, 0),
				FIntPoint(1, 0),
				FIntPoint(0, -1),
				FIntPoint(0, 1)
			};
			for (const FIntPoint& Dir : Axis)
			{
				const FIntPoint Near(
					At.X + Dir.X,
					At.Y + Dir.Y);
				const FIntPoint Far(
					At.X + Dir.X * 2,
					At.Y + Dir.Y * 2);
				const FIntPoint Row(
					At.X + Dir.X * 3,
					At.Y + Dir.Y * 3);
				Ally->SetBoardPlacement(
					Near.X,
					Near.Y,
					EFacing::PositiveX);
				Expect(
					TEXT("Heal adjacent on an axis"),
					Medic->CanHealTarget(Ally));
				Ally->SetBoardPlacement(
					Far.X,
					Far.Y,
					EFacing::PositiveX);
				Expect(
					TEXT("Heal range 2 on an axis"),
					Medic->CanHealTarget(Ally));
				Ally->SetBoardPlacement(
					Row.X,
					Row.Y,
					EFacing::PositiveX);
				Expect(
					TEXT("No heal beyond range 2"),
					!Medic->CanHealTarget(Ally));
				Expect(
					TEXT("Server refuses a far wounded ally"),
					!Medic->ResolveHeal(Ally).bInRange);
			}

			Medic->SetBoardPlacement(
				At.X,
				At.Y,
				EFacing::NegativeXNegativeY);
			Ally->SetBoardPlacement(
				2,
				2,
				EFacing::PositiveX);
			Expect(
				TEXT("Heal diagonal he faces"),
				Medic->CanHealTarget(Ally));

			Medic->SetBoardPlacement(
				At.X,
				At.Y,
				EFacing::NegativeX);
			Expect(
				TEXT("No diagonal without facing"),
				!Medic->CanHealTarget(Ally));

			Medic->SetBoardPlacement(
				At.X,
				At.Y,
				EFacing::NegativeX);
			Ally->SetBoardPlacement(
				3,
				4,
				EFacing::PositiveX);
			Grid->SetObstacleHeight(3, 4, 1);
			Expect(
				TEXT("Heal ally on height 1"),
				Medic->CanHealTarget(Ally));
			Grid->SetObstacleHeight(3, 4, 2);
			Expect(
				TEXT("No heal on height 2"),
				!Medic->CanHealTarget(Ally));
			Grid->SetObstacleHeight(3, 4, 0);

			Ally->SetBoardPlacement(
				2,
				4,
				EFacing::PositiveX);
			Grid->SetObstacleHeight(3, 4, 1);
			Expect(
				TEXT("No heal behind height 1"),
				!Medic->CanHealTarget(Ally));
			Grid->SetObstacleHeight(3, 4, 2);
			Expect(
				TEXT("No heal behind height 2"),
				!Medic->CanHealTarget(Ally));
			Grid->SetObstacleHeight(3, 4, 0);

			Foe->SetBoardPlacement(
				3,
				4,
				EFacing::PositiveX);
			Expect(
				TEXT("Medic cannot heal a foe"),
				!Medic->CanHealTarget(Foe));

			Ally->SetBoardPlacement(
				3,
				4,
				EFacing::PositiveX);
			if (!bOk)
			{
				Fail(TEXT("Medic heal rules failed."));
			}
		}

		AJoA_Figure* Medic = FindFigure(
			Server,
			EUnitType::Medic,
			FrenchSeat);
		AJoA_Figure* MedicC = FindFigure(
			Client,
			EUnitType::Medic,
			FrenchSeat);
		AJoA_Figure* Ally = FindFigure(
			Server,
			EUnitType::LightInfantry,
			FrenchSeat);
		AJoA_Figure* AllyC = FindFigure(
			Client,
			EUnitType::LightInfantry,
			FrenchSeat);
		Wait(
			Medic
				&& MedicC
				&& Ally
				&& AllyC
				&& JoATest::IsAt(Medic, FIntPoint(4, 4))
				&& JoATest::IsAt(MedicC, FIntPoint(4, 4))
				&& JoATest::IsAt(Ally, FIntPoint(3, 4))
				&& JoATest::IsAt(AllyC, FIntPoint(3, 4))
				&& Medic->CanHealTarget(Ally)
				&& MedicC->CanHealTarget(AllyC),
			10.0,
			TEXT("client sees the open heal."));
		break;
	}

	case EStep::MedicHealFlag:
	{
		if (Once())
		{
			AJoA_Figure* Medic = FindFigure(
				Server,
				EUnitType::Medic,
				FrenchSeat);
			AJoA_Flag* Flag = JoATest::FindFlag(
				Server,
				FrenchSeat);
			if (!Medic || !Flag || !ServerPC)
			{
				Fail(TEXT("Heal flag setup missing."));
				break;
			}

			if (ActiveSeat(Server) != FrenchSeat)
			{
				JoATest::SkipActive(Server);
			}

			ServerPC->RequestSelectFigure(Medic);
			Flag->SetBoardCell(
				Medic->CellX,
				Medic->CellY);
			if (!Medic->ResolvePickupFlag())
			{
				Fail(TEXT("Medic flag pickup failed."));
			}
		}

		AJoA_Figure* Medic = FindFigure(
			Server,
			EUnitType::Medic,
			FrenchSeat);
		AJoA_Figure* Ally = FindFigure(
			Server,
			EUnitType::LightInfantry,
			FrenchSeat);
		FinishWhen(
			Medic
				&& Ally
				&& Medic->GetHeldFlag()
				&& !Medic->CanHealTarget(Medic)
				&& !Medic->CanHealTarget(Ally),
			TEXT("Timeout: flag still allows heal."));
		break;
	}

	case EStep::ArcherShotRules:
	{
		if (Once())
		{
			AJoA_Figure* Archer = FindFigure(
				Server,
				EUnitType::Archer,
				FrenchSeat);
			AJoA_Figure* Foe = FindFigure(
				Server,
				EUnitType::LightInfantry,
				EnglishSeat);
			AJoA_Figure* Mate = FindFigure(
				Server,
				EUnitType::Medic,
				FrenchSeat);
			AJoA_GridManager* Grid =
				AJoA_GameMode::GetGridManager(Server);
			if (!Archer
				|| !Foe
				|| !Mate
				|| !Grid
				|| !ServerPC)
			{
				Fail(TEXT("Shot setup is missing."));
				break;
			}

			if (ActiveSeat(Server) != FrenchSeat)
			{
				JoATest::SkipActive(Server);
			}

			JoATest::ClearObstacles(Server);
			JoATest::ParkSeat(
				Server,
				FrenchSeat,
				7);
			JoATest::ParkSeat(
				Server,
				EnglishSeat,
				0);
			ServerPC->RequestSelectFigure(Archer);
			Archer->SetBoardPlacement(
				6,
				4,
				EFacing::NegativeX);

			bool bOk = true;
			auto Expect = [&](
				const TCHAR* What,
				bool bGot)
			{
				if (!Test->TestTrue(What, bGot))
				{
					bOk = false;
				}
			};

			Foe->SetBoardPlacement(
				5,
				4,
				EFacing::PositiveX);
			Expect(
				TEXT("Archer hits the next square"),
				Archer->CanAttackTarget(Foe));

			Foe->SetBoardPlacement(
				1,
				4,
				EFacing::PositiveX);
			Expect(
				TEXT("Archer hits a far open file"),
				Archer->CanAttackTarget(Foe));

			Foe->SetBoardPlacement(
				5,
				4,
				EFacing::PositiveX);
			Grid->SetObstacleHeight(5, 4, 1);
			Expect(
				TEXT("Archer hits a foe on height 1"),
				Archer->CanAttackTarget(Foe));
			Grid->SetObstacleHeight(5, 4, 2);
			Expect(
				TEXT("Archer hits a foe on height 2"),
				Archer->CanAttackTarget(Foe));
			Grid->SetObstacleHeight(5, 4, 0);

			Foe->SetBoardPlacement(
				4,
				4,
				EFacing::PositiveX);
			Grid->SetObstacleHeight(5, 4, 1);
			Expect(
				TEXT("Archer hits just behind 1"),
				Archer->CanAttackTarget(Foe));
			Grid->SetObstacleHeight(5, 4, 2);
			Expect(
				TEXT("Archer cannot hit behind 2"),
				!Archer->CanAttackTarget(Foe));
			Grid->SetObstacleHeight(5, 4, 1);
			Foe->SetBoardPlacement(
				3,
				4,
				EFacing::PositiveX);
			Expect(
				TEXT("Archer cannot hit two behind 1"),
				!Archer->CanAttackTarget(Foe));
			Grid->SetObstacleHeight(5, 4, 0);

			Mate->SetBoardPlacement(
				5,
				4,
				EFacing::NegativeX);
			Expect(
				TEXT("Archer cannot shoot an ally"),
				!Archer->CanAttackTarget(Mate));

			Mate->SetBoardPlacement(
				7,
				2,
				EFacing::NegativeX);
			Foe->SetBoardPlacement(
				4,
				4,
				EFacing::PositiveX);
			Grid->SetObstacleHeight(5, 4, 1);
			if (!bOk)
			{
				Fail(TEXT("Archer shot rules failed."));
			}
		}

		AJoA_Figure* Archer = FindFigure(
			Server,
			EUnitType::Archer,
			FrenchSeat);
		AJoA_Figure* ArcherC = FindFigure(
			Client,
			EUnitType::Archer,
			FrenchSeat);
		AJoA_Figure* Foe = FindFigure(
			Server,
			EUnitType::LightInfantry,
			EnglishSeat);
		AJoA_Figure* FoeC = FindFigure(
			Client,
			EUnitType::LightInfantry,
			EnglishSeat);
		FinishWhen(
			Archer
				&& ArcherC
				&& Foe
				&& FoeC
				&& JoATest::IsAt(Archer, FIntPoint(6, 4))
				&& JoATest::IsAt(ArcherC, FIntPoint(6, 4))
				&& JoATest::IsAt(Foe, FIntPoint(4, 4))
				&& JoATest::IsAt(FoeC, FIntPoint(4, 4))
				&& Archer->CanAttackTarget(Foe)
				&& ArcherC->CanAttackTarget(FoeC),
			TEXT("Timeout: client missed the shot."));
		break;
	}

	case EStep::ObstacleBoxSetup:
	{
		if (Once())
		{
			if (!EnglishLight || !FrenchKnight)
			{
				Fail(TEXT(
					"Boxed infantry setup is missing."));
				break;
			}

			Kill(Server, EUnitType::HeavyKnight);
			Kill(Server, EUnitType::Archer);
			Kill(Server, EUnitType::Medic);
			ClearObstacles(Server);
			EnglishLight->SetBoardPlacement(
				0,
				0,
				EFacing::PositiveX);
			ParkSeat(Server, FrenchSeat, 7);
			AJoA_GridManager* Grid =
				AJoA_GameMode::GetGridManager(Server);
			if (!Grid)
			{
				Fail(TEXT("Boxed infantry has no grid."));
				break;
			}

			Grid->SetObstacleHeight(1, 0, 2);
			Grid->SetObstacleHeight(0, 1, 2);
			Grid->SetObstacleHeight(1, 1, 2);
			break;
		}

		const bool bBoxed = CountLiving(Server) == 5
			&& CountLiving(Client) == 5
			&& IsAt(EnglishLight, FIntPoint(0, 0))
			&& IsAt(EnglishLightC, FIntPoint(0, 0))
			&& EnglishLight
			&& EnglishLightC
			&& !EnglishLight->IsPromoted()
			&& !EnglishLightC->IsPromoted()
			&& CellHeightIs(Server, 1, 0, 2)
			&& CellHeightIs(Client, 1, 0, 2)
			&& CellHeightIs(Server, 0, 1, 2)
			&& CellHeightIs(Client, 0, 1, 2)
			&& CellHeightIs(Server, 1, 1, 2)
			&& CellHeightIs(Client, 1, 1, 2)
			&& ActiveSeat(Server) == FrenchSeat;
		Wait(
			bBoxed,
			10.0,
			TEXT("English infantry boxed by height 2."));
		break;
	}

	case EStep::ObstacleBoxFrenchSkips:
	{
		if (Once())
		{
			ServerPC->RequestSelectFigure(FrenchKnight);
			break;
		}

		if (!bActionSent
			&& ServerPC->GetSelectedFigure() == FrenchKnight)
		{
			if (!FrenchKnight->HasAnyAction())
			{
				Fail(TEXT("French knight has no action."));
				break;
			}

			bActionSent = true;
			FrenchKnight->RequestSkipTurn();
		}

		const AJoA_GameState* ServerState =
			AJoA_GameState::Get(Server);
		const AJoA_GameState* ClientState =
			AJoA_GameState::Get(Client);
		const UJoA_W_Play* ClientHud =
			ClientPC->GetPlayWidget();
		const bool bNoMove = EnglishLight
			&& !EnglishLight->IsPromoted()
			&& !EnglishLight->CanJump()
			&& !EnglishLight->HasAnyAction()
			&& !EnglishLight->CanStep(
				EStepDirection::Forward)
			&& !EnglishLight->CanStep(
				EStepDirection::Back)
			&& !EnglishLight->CanStep(
				EStepDirection::Left)
			&& !EnglishLight->CanStep(
				EStepDirection::Right);
		Wait(
			ServerState && ClientState
				&& bNoMove
				&& ServerState->GetActiveSeat()
					== EnglishSeat
				&& ClientState->GetActiveSeat()
					== EnglishSeat
				&& ServerState->IsActiveSeatBlocked()
				&& ClientState->IsActiveSeatBlocked()
				&& !ServerState->IsMatchOver()
				&& ClientHud
				&& ClientHud->GetInfoText().ToString()
					.StartsWith(TEXT("You can't move")),
			10.0,
			TEXT("boxed infantry must skip."));
		break;
	}

	case EStep::ObstacleBoxEnglishSkips:
	{
		if (Once())
		{
			ClientPC->RequestSelectFigure(EnglishLightC);
			break;
		}

		if (!bActionSent
			&& ClientPC->GetSelectedFigure()
				== EnglishLightC)
		{
			bActionSent = true;
			EnglishLightC->RequestSkipTurn();
		}

		const AJoA_GameState* ServerState =
			AJoA_GameState::Get(Server);
		if (ServerState
			&& ServerState->GetActiveSeat() == FrenchSeat
			&& !ServerState->IsActiveSeatBlocked()
			&& !ServerState->IsMatchOver())
		{
			GoTo(EStep::ObstacleBoxFrenchSkipsAgain);
		}
		else if (Elapsed() > 10.0)
		{
			Fail(TEXT(
				"Timeout: boxed infantry did not skip."));
		}
		break;
	}

	case EStep::ObstacleBoxFrenchSkipsAgain:
	{
		if (Once())
		{
			ServerPC->RequestSelectFigure(FrenchKnight);
			break;
		}

		if (!bActionSent
			&& ServerPC->GetSelectedFigure() == FrenchKnight)
		{
			bActionSent = true;
			FrenchKnight->RequestSkipTurn();
		}

		const AJoA_GameState* ServerState =
			AJoA_GameState::Get(Server);
		const AJoA_GameState* ClientState =
			AJoA_GameState::Get(Client);
		const UJoA_W_Play* ServerHud =
			ServerPC->GetPlayWidget();
		const UJoA_W_Play* ClientHud =
			ClientPC->GetPlayWidget();
		const bool bLost = ServerState && ClientState
			&& ServerState->IsMatchOver()
			&& ClientState->IsMatchOver()
			&& ServerState->GetWinnerSeat() == FrenchSeat
			&& ClientState->GetWinnerSeat() == FrenchSeat
			&& ServerState->GetMatchEndReason()
				== EMatchEndReason::Stalemate
			&& ClientState->GetMatchEndReason()
				== EMatchEndReason::Stalemate
			&& ServerHud
			&& ClientHud
			&& ServerHud->GetInfoText().ToString()
				== TEXT("You have won!")
			&& ClientHud->GetInfoText().ToString()
				== TEXT("You have lost!");
		FinishWhen(
			bLost,
			TEXT("Timeout: boxed infantry did not lose."));
		break;
	}

	case EStep::SplitBoardSetup:
	{
		if (Once())
		{
			if (!FrenchKnight
				|| !EnglishLight
				|| !FrenchLight
				|| !FrenchArcher
				|| !EnglishArcher)
			{
				Fail(TEXT("Split board setup is missing."));
				break;
			}

			ClearObstacles(Server);
			ParkSeat(Server, FrenchSeat, 7);
			ParkSeat(Server, EnglishSeat, 0);
			FrenchLight->SetBoardPlacement(
				5,
				3,
				EFacing::NegativeX);
			FrenchArcher->SetBoardPlacement(
				7,
				4,
				EFacing::NegativeX);
			EnglishLight->SetBoardPlacement(
				2,
				4,
				EFacing::PositiveX);
			EnglishArcher->SetBoardPlacement(
				0,
				4,
				EFacing::PositiveX);
			AJoA_GridManager* Grid =
				AJoA_GameMode::GetGridManager(Server);
			if (!Grid)
			{
				Fail(TEXT("Split board has no grid."));
				break;
			}

			for (int32 Y = 0; Y < GridSize; ++Y)
			{
				Grid->SetObstacleHeight(3, Y, 2);
				Grid->SetObstacleHeight(4, Y, 2);
			}
			Kill(Server, EUnitType::HeavyKnight);
			KillFigure(FrenchKnight);
			break;
		}

		const AJoA_GameState* ServerState =
			AJoA_GameState::Get(Server);
		const AJoA_GameState* ClientState =
			AJoA_GameState::Get(Client);
		const UJoA_W_Play* ServerHud = ServerPC
			? ServerPC->GetPlayWidget()
			: nullptr;
		const UJoA_W_Play* ClientHud = ClientPC
			? ClientPC->GetPlayWidget()
			: nullptr;
		const bool bDraw = CountLiving(Server) == 6
			&& CountLiving(Client) == 6
			&& FrenchKnight && FrenchKnight->IsDead()
			&& FrenchKnightC && FrenchKnightC->IsDead()
			&& EnglishKnight && EnglishKnight->IsDead()
			&& EnglishKnightC
			&& EnglishKnightC->IsDead()
			&& NonePromoted(Server)
			&& NonePromoted(Client)
			&& IsAt(FrenchLight, FIntPoint(5, 3))
			&& IsAt(FrenchLightC, FIntPoint(5, 3))
			&& IsAt(FrenchArcher, FIntPoint(7, 4))
			&& IsAt(FrenchArcherC, FIntPoint(7, 4))
			&& IsAt(EnglishLight, FIntPoint(2, 4))
			&& IsAt(EnglishLightC, FIntPoint(2, 4))
			&& IsAt(EnglishArcher, FIntPoint(0, 4))
			&& IsAt(EnglishArcherC, FIntPoint(0, 4))
			&& CellHeightIs(Server, 3, 0, 2)
			&& CellHeightIs(Client, 3, 7, 2)
			&& CellHeightIs(Server, 4, 4, 2)
			&& CellHeightIs(Client, 4, 1, 2)
			&& CellHeightIs(Server, 5, 3, 0)
			&& CellHeightIs(Client, 2, 4, 0)
			&& ServerState
			&& ClientState
			&& ServerState->IsMatchOver()
			&& ClientState->IsMatchOver()
			&& ServerState->GetWinnerSeat()
				== INDEX_NONE
			&& ClientState->GetWinnerSeat()
				== INDEX_NONE
			&& ServerState->GetMatchEndReason()
				== EMatchEndReason::Draw
			&& ClientState->GetMatchEndReason()
				== EMatchEndReason::Draw
			&& ServerHud
			&& ClientHud
			&& ServerHud->GetInfoText().ToString()
				== TEXT("The game ended in a draw.")
			&& ClientHud->GetInfoText().ToString()
				== TEXT("The game ended in a draw.");
		FinishWhen(
			bDraw,
			TEXT("Timeout: split board did not draw."));
		break;
	}

	case EStep::StaminaPulseQuiet:
	{
		UJoA_W_CharacterStat* Stat =
			FindStat(ServerPC, FrenchKnight);
		if (Once())
		{
			if (!FrenchKnight || !Stat)
			{
				Fail(TEXT(
					"Knight stat row is missing."));
				break;
			}

			ServerPC->RequestSelectFigure(FrenchKnight);
			break;
		}

		if (!Stat)
		{
			Fail(TEXT("Knight stat row is missing."));
			break;
		}

		if (Stat->IsStaminaRowPulsing())
		{
			Fail(TEXT(
				"Full stamina pulsed the row."));
			break;
		}

		Wait(
			Elapsed() >= 0.5
				&& ServerPC->GetSelectedFigure()
					== FrenchKnight
				&& !Stat->IsStaminaRowPulsing()
				&& FMath::IsNearlyEqual(
					Stat->GetStaminaRowScale(),
					1.f,
					0.05f),
			2.0,
			TEXT("full stamina did not stay still."));
		break;
	}

	case EStep::StaminaPulseSpent:
	{
		UJoA_W_CharacterStat* Stat =
			FindStat(ServerPC, FrenchKnight);
		if (Once())
		{
			UAbilitySystemComponent* ASC = FrenchKnight
				? FrenchKnight->GetAbilitySystemComponent()
				: nullptr;
			if (!ASC || !Stat)
			{
				Fail(TEXT(
					"Knight stamina could not be spent."));
				break;
			}

			ASC->SetNumericAttributeBase(
				UJoA_GAS_StaminaSet::GetStaminaAttribute(),
				0.f);
			break;
		}

		if (Stat && Stat->IsStaminaRowPulsing())
		{
			Fail(TEXT(
				"Empty stamina pulsed without a new pick."));
			break;
		}

		const bool bSpent = FrenchKnight
			&& Stat
			&& FMath::IsNearlyEqual(
				Attr(
					FrenchKnight,
					UJoA_GAS_StaminaSet::
						GetStaminaAttribute()),
				0.f)
			&& !Stat->IsStaminaRowPulsing();
		Wait(
			Elapsed() >= 0.5 && bSpent,
			2.0,
			TEXT("empty stamina pulsed on its own."));
		break;
	}

	case EStep::StaminaPulseRun:
	{
		UJoA_W_CharacterStat* Stat =
			FindStat(ServerPC, FrenchKnight);
		UJoA_W_CharacterStat* ClientStat =
			FindStat(ClientPC, FrenchKnightC);
		if (Once())
		{
			if (!Stat)
			{
				Fail(TEXT("Knight stat row is missing."));
				break;
			}

			ServerPC->RequestSelectFigure(FrenchKnight);
			break;
		}

		if (!Stat)
		{
			Fail(TEXT("Knight stat row is missing."));
			break;
		}

		if (Stat->GetStaminaRowScale() > 1.2f)
		{
			bActionSent = true;
		}

		if (ClientStat && ClientStat->IsStaminaRowPulsing())
		{
			Fail(TEXT(
				"Opponent stamina row pulsed."));
			break;
		}

		if (bActionSent
			&& !Stat->IsStaminaRowPulsing()
			&& Elapsed() < 2.8)
		{
			Fail(TEXT(
				"Stamina pulse ended before 3 seconds."));
			break;
		}

		const bool bDone = bActionSent
			&& !Stat->IsStaminaRowPulsing()
			&& Elapsed() >= 2.8
			&& FMath::IsNearlyEqual(
				Stat->GetStaminaRowScale(),
				1.f,
				0.05f);
		Wait(
			bDone,
			5.0,
			TEXT("stamina row did not pulse for 3 seconds."));
		break;
	}

	case EStep::StaminaPulseMage:
	{
		AJoA_Figure* Medic = FindFigure(
			Server,
			EUnitType::Medic,
			FrenchSeat);
		UJoA_W_CharacterStat* MageStat =
			FindStat(ServerPC, Medic);
		UJoA_W_CharacterStat* KnightStat =
			FindStat(ServerPC, FrenchKnight);
		if (Once())
		{
			UAbilitySystemComponent* ASC = Medic
				? Medic->GetAbilitySystemComponent()
				: nullptr;
			if (!ASC || !MageStat)
			{
				Fail(TEXT("Mage stat row is missing."));
				break;
			}

			ASC->SetNumericAttributeBase(
				UJoA_GAS_ManaSet::GetManaAttribute(),
				0.f);
			ServerPC->RequestSelectFigure(Medic);
			break;
		}

		if (MageStat && MageStat->IsStaminaRowPulsing())
		{
			Fail(TEXT("Mage stamina row pulsed."));
			break;
		}

		if (KnightStat && KnightStat->IsStaminaRowPulsing())
		{
			Fail(TEXT(
				"Knight kept pulsing after the mage."));
			break;
		}

		Wait(
			Elapsed() >= 0.5
				&& Medic
				&& ServerPC->GetSelectedFigure() == Medic
				&& MageStat
				&& !MageStat->IsStaminaRowPulsing()
				&& FMath::IsNearlyEqual(
					MageStat->GetStaminaRowScale(),
					1.f,
					0.05f),
			2.0,
			TEXT("mage row did not stay still."));
		break;
	}

	case EStep::StaminaPulseAgain:
	{
		UJoA_W_CharacterStat* Stat =
			FindStat(ServerPC, FrenchKnight);
		if (Once())
		{
			if (!Stat)
			{
				Fail(TEXT("Knight stat row is missing."));
				break;
			}

			ServerPC->RequestSelectFigure(FrenchKnight);
			break;
		}

		const bool bPulsing = Stat
			&& Stat->IsStaminaRowPulsing()
			&& Stat->GetStaminaRowScale() > 1.2f;
		FinishWhen(
			bPulsing,
			TEXT("Picking the knight again did not pulse."));
		break;
	}

	case EStep::EndPlay:
		if (Once())
		{
			if (!JoATest::IsWatched())
			{
				GEditor->RequestEndPlayMap();
			}
			else
			{
				/* Watched run: PIE stays open for the result
				 * until the user stops it. */
				UE_LOG(
					LogTemp,
					Display,
					TEXT("JoA test finished. PIE stays open. ")
					TEXT("Press Esc or Stop to close it."));
				GEngine->AddOnScreenDebugMessage(
					9001,
					3600.f,
					FColor::Green,
					TEXT("Test finished. Press Esc to close."));
			}
			break;
		}

		if (!GEditor->IsPlaySessionInProgress()
			|| (!JoATest::IsWatched() && Elapsed() > 30.0))
		{
			if (Settings)
			{
				Settings->RemoveFromRoot();
			}
			Next();
		}
		break;

	case EStep::Done:
		break;
	}

	return Step == EStep::Done;
}

DEFINE_LATENT_AUTOMATION_COMMAND_ONE_PARAMETER(
	FJoARunScript,
	TSharedPtr<FJoAScript>,
	Script);

bool FJoARunScript::Update()
{
	return Script->Tick();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FJoAMatchFlowTest,
	"JoA.Match.TwoPlayerFlow",
	EAutomationTestFlags::EditorContext
		| EAutomationTestFlags::ProductFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FJoAStalemateTest,
	"JoA.Match.Stalemate",
	EAutomationTestFlags::EditorContext
		| EAutomationTestFlags::ProductFilter)

static bool StartScenario(
	FAutomationTestBase* Test,
	EScenario Scenario)
{
	if (!FEditorFileUtils::LoadMap(
			TEXT("/Game/Map/Arena"),
			false,
			true))
	{
		Test->AddError(
			TEXT("Map /Game/Map/Arena did not load."));
		return false;
	}

	ADD_LATENT_AUTOMATION_COMMAND(
		FJoARunScript(
			MakeShared<FJoAScript>(Test, Scenario)));
	return true;
}

bool FJoAStalemateTest::RunTest(const FString& Parameters)
{
	return StartScenario(this, EScenario::Stalemate);
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FJoAObstacleBoxTest,
	"JoA.Match.ObstacleBox",
	EAutomationTestFlags::EditorContext
		| EAutomationTestFlags::ProductFilter)

bool FJoAObstacleBoxTest::RunTest(const FString& Parameters)
{
	return StartScenario(this, EScenario::ObstacleBox);
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FJoASplitBoardTest,
	"JoA.Match.SplitBoard",
	EAutomationTestFlags::EditorContext
		| EAutomationTestFlags::ProductFilter)

bool FJoASplitBoardTest::RunTest(const FString& Parameters)
{
	return StartScenario(this, EScenario::SplitBoard);
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FJoABlockedRecoversTest,
	"JoA.Match.BlockedRecovers",
	EAutomationTestFlags::EditorContext
		| EAutomationTestFlags::ProductFilter)

bool FJoABlockedRecoversTest::RunTest(
	const FString& Parameters)
{
	return StartScenario(this, EScenario::BlockedRecovers);
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FJoAMovePassesTurnTest,
	"JoA.Match.MovePassesTurn",
	EAutomationTestFlags::EditorContext
		| EAutomationTestFlags::ProductFilter)

bool FJoAMovePassesTurnTest::RunTest(
	const FString& Parameters)
{
	return StartScenario(this, EScenario::MovePassesTurn);
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FJoAMoveKeepsTurnTest,
	"JoA.Match.MoveKeepsTurn",
	EAutomationTestFlags::EditorContext
		| EAutomationTestFlags::ProductFilter)

bool FJoAMoveKeepsTurnTest::RunTest(
	const FString& Parameters)
{
	return StartScenario(this, EScenario::MoveKeepsTurn);
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FJoATurnExpiresTest,
	"JoA.Match.TurnExpires",
	EAutomationTestFlags::EditorContext
		| EAutomationTestFlags::ProductFilter)

bool FJoATurnExpiresTest::RunTest(const FString& Parameters)
{
	return StartScenario(this, EScenario::TurnExpires);
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FJoASeatDropTest,
	"JoA.Match.SeatDrop",
	EAutomationTestFlags::EditorContext
		| EAutomationTestFlags::ProductFilter)

bool FJoASeatDropTest::RunTest(const FString& Parameters)
{
	/* The engine logs a cut link as an error, and an error
	 * log fails an automation test. */
	AddExpectedError(
		TEXT("BroadcastNetworkFailure"),
		EAutomationExpectedErrorFlags::Contains,
		0);
	return StartScenario(this, EScenario::SeatDrop);
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FJoASeatRejoinTest,
	"JoA.Match.SeatRejoin",
	EAutomationTestFlags::EditorContext
		| EAutomationTestFlags::ProductFilter)

bool FJoASeatRejoinTest::RunTest(const FString& Parameters)
{
	/* The engine logs a cut link as an error, and an error
	 * log fails an automation test. */
	AddExpectedError(
		TEXT("BroadcastNetworkFailure"),
		EAutomationExpectedErrorFlags::Contains,
		0);
	return StartScenario(this, EScenario::SeatRejoin);
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FJoAThirdPlayerTest,
	"JoA.Match.ThirdPlayer",
	EAutomationTestFlags::EditorContext
		| EAutomationTestFlags::ProductFilter)

bool FJoAThirdPlayerTest::RunTest(const FString& Parameters)
{
	/* The engine logs the link of the player put out as an
	 * error, and an error log fails an automation test. */
	AddExpectedError(
		TEXT("BroadcastNetworkFailure"),
		EAutomationExpectedErrorFlags::Contains,
		0);
	return StartScenario(this, EScenario::ThirdPlayer);
}

bool FJoAMatchFlowTest::RunTest(const FString& Parameters)
{
	if (!FEditorFileUtils::LoadMap(
			TEXT("/Game/Map/Arena"),
			false,
			true))
	{
		AddError(TEXT("Map /Game/Map/Arena did not load."));
		return false;
	}

	ADD_LATENT_AUTOMATION_COMMAND(
		FJoARunScript(MakeShared<FJoAScript>(this)));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FJoAFlagCaptureTest,
	"JoA.Match.FlagCapture",
	EAutomationTestFlags::EditorContext
		| EAutomationTestFlags::ProductFilter)

bool FJoAFlagCaptureTest::RunTest(const FString& Parameters)
{
	return StartScenario(this, EScenario::FlagCapture);
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FJoAFlagStaminaTest,
	"JoA.Match.FlagStamina",
	EAutomationTestFlags::EditorContext
		| EAutomationTestFlags::ProductFilter)

bool FJoAFlagStaminaTest::RunTest(const FString& Parameters)
{
	return StartScenario(this, EScenario::FlagStamina);
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FJoAStaminaPulseTest,
	"JoA.Match.StaminaPulse",
	EAutomationTestFlags::EditorContext
		| EAutomationTestFlags::ProductFilter)

bool FJoAStaminaPulseTest::RunTest(const FString& Parameters)
{
	return StartScenario(this, EScenario::StaminaPulse);
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FJoAFlagCaptureEnglandTest,
	"JoA.Match.FlagCaptureEngland",
	EAutomationTestFlags::EditorContext
		| EAutomationTestFlags::ProductFilter)

bool FJoAFlagCaptureEnglandTest::RunTest(
	const FString& Parameters)
{
	return StartScenario(
		this,
		EScenario::FlagCaptureEngland);
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FJoARestTest,
	"JoA.Match.Rest",
	EAutomationTestFlags::EditorContext
		| EAutomationTestFlags::ProductFilter)

bool FJoARestTest::RunTest(const FString& Parameters)
{
	return StartScenario(this, EScenario::Rest);
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FJoABuildObstacleTest,
	"JoA.Match.BuildObstacle",
	EAutomationTestFlags::EditorContext
		| EAutomationTestFlags::ProductFilter)

bool FJoABuildObstacleTest::RunTest(
	const FString& Parameters)
{
	return StartScenario(this, EScenario::BuildObstacle);
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FJoAJumpFallTest,
	"JoA.Match.JumpFall",
	EAutomationTestFlags::EditorContext
		| EAutomationTestFlags::ProductFilter)

bool FJoAJumpFallTest::RunTest(
	const FString& Parameters)
{
	return StartScenario(this, EScenario::JumpFall);
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FJoAPromotedJumpFallTest,
	"JoA.Match.PromotedJumpFall",
	EAutomationTestFlags::EditorContext
		| EAutomationTestFlags::ProductFilter)

bool FJoAPromotedJumpFallTest::RunTest(
	const FString& Parameters)
{
	return StartScenario(
		this,
		EScenario::PromotedJumpFall);
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FJoAPromotionTest,
	"JoA.Match.Promotion",
	EAutomationTestFlags::EditorContext
		| EAutomationTestFlags::ProductFilter)

bool FJoAPromotionTest::RunTest(
	const FString& Parameters)
{
	return StartScenario(this, EScenario::Promotion);
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FJoAPromotionEnglandTest,
	"JoA.Match.PromotionEngland",
	EAutomationTestFlags::EditorContext
		| EAutomationTestFlags::ProductFilter)

bool FJoAPromotionEnglandTest::RunTest(
	const FString& Parameters)
{
	return StartScenario(
		this,
		EScenario::PromotionEngland);
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FJoASteamJoinUrlTest,
	"JoA.Match.SteamJoinUrl",
	EAutomationTestFlags::EditorContext
		| EAutomationTestFlags::ProductFilter)

bool FJoASteamJoinUrlTest::RunTest(
	const FString& Parameters)
{
	UJoA_GameInstance* Instance =
		NewObject<UJoA_GameInstance>(
			GetTransientPackage());
	if (!Instance)
	{
		AddError(TEXT("No game instance."));
		return false;
	}

	TestEqual(
		TEXT("plain address"),
		Instance->BuildJoinUrl(TEXT("127.0.0.1:7777")),
		TEXT("127.0.0.1:7777"));

	Instance->StoreReconnectTicket(
		TEXT("steam.1:7777"),
		TEXT("seat"),
		1);
	TestEqual(
		TEXT("steam url plus token"),
		Instance->BuildJoinUrl(TEXT("steam.1:7777")),
		TEXT("steam.1:7777?ReconnectToken=seat"));
	TestEqual(
		TEXT("steam url already has query"),
		Instance->BuildJoinUrl(
			TEXT("steam.1:7777?x=1")),
		TEXT("steam.1:7777?x=1&ReconnectToken=seat"));

	FString Parsed;
	TestFalse(
		TEXT("ip is not a steam id"),
		AJoA_GameMode::ParseSteamSocketId(
			TEXT("127.0.0.1:7777"),
			Parsed));
	TestTrue(
		TEXT("steam address parses"),
		AJoA_GameMode::ParseSteamSocketId(
			TEXT("steam.76561198000000000:7777"),
			Parsed));
	TestEqual(
		TEXT("parsed steam id"),
		Parsed,
		TEXT("76561198000000000"));
	TestTrue(
		TEXT("bare steam id parses"),
		AJoA_GameMode::ParseSteamSocketId(
			TEXT("76561198000000000"),
			Parsed));

	FSeatRecord SeatRows[2];
	SeatRows[0].Link = ESeatConnection::Connected;
	SeatRows[0].bHost = true;
	SeatRows[0].UniqueKey = TEXT("76561198000000001");
	SeatRows[1].Link = ESeatConnection::Disconnected;
	SeatRows[1].UniqueKey = TEXT("76561198000000000");
	SeatRows[1].Token = TEXT("seat");
	TestEqual(
		TEXT("same steam id reclaims the seat"),
		AJoA_GameMode::FindSteamPeerSeat(
			SeatRows,
			TEXT("76561198000000000"),
			FString()),
		1);
	TestEqual(
		TEXT("other steam id keeps the token out"),
		AJoA_GameMode::FindSteamPeerSeat(
			SeatRows,
			TEXT("76561198000000002"),
			TEXT("seat")),
		static_cast<int32>(INDEX_NONE));

	SeatRows[1].Link = ESeatConnection::Empty;
	SeatRows[1].UniqueKey.Reset();
	SeatRows[1].Token.Reset();
	TestEqual(
		TEXT("first steam peer takes the empty seat"),
		AJoA_GameMode::FindSteamPeerSeat(
			SeatRows,
			TEXT("76561198000000002"),
			FString()),
		1);
	return true;
}

/* Overlay Play needs two Steam accounts and a click.
 * This run checks New Game: Menu PIE, HostMatch, Arena
 * listen, and a Steam lobby when Steam is logged in. */
class FJoASteamHostScript
{
public:
	explicit FJoASteamHostScript(
		FAutomationTestBase* InTest)
		: Test(InTest)
	{
	}

	bool Tick()
	{
		switch (Step)
		{
		case EStep::Start:
			return StartPie();
		case EStep::WaitMenu:
			return WaitMenu();
		case EStep::WaitHost:
			return WaitHost();
		case EStep::EndPlay:
			return EndPlay();
		case EStep::Done:
			return true;
		}
		return true;
	}

private:
	enum class EStep : uint8
	{
		Start,
		WaitMenu,
		WaitHost,
		EndPlay,
		Done
	};

	void GoTo(EStep NextStep)
	{
		Step = NextStep;
		StepStart = FPlatformTime::Seconds();
		bStepActed = false;
	}

	bool Once()
	{
		if (bStepActed)
		{
			return false;
		}
		bStepActed = true;
		return true;
	}

	double Elapsed() const
	{
		return FPlatformTime::Seconds() - StepStart;
	}

	void Fail(const FString& Message)
	{
		Test->AddError(Message);
		Step = bPlayStarted ? EStep::EndPlay : EStep::Done;
		StepStart = FPlatformTime::Seconds();
	}

	UJoA_GameInstance* PieInstance() const
	{
		UWorld* World = JoATest::FindPieAny();
		return World
			? World->GetGameInstance<UJoA_GameInstance>()
			: nullptr;
	}

	bool StartPie()
	{
		if (!GEditor)
		{
			Fail(TEXT("No editor."));
			return Step == EStep::Done;
		}

		Settings = NewObject<ULevelEditorPlaySettings>(
			GetTransientPackage());
		Settings->AddToRoot();
		Settings->SetPlayNetMode(PIE_Standalone);
		Settings->SetPlayNumberOfClients(1);
		Settings->SetRunUnderOneProcess(true);
		Settings->bLaunchSeparateServer = false;

		FRequestPlaySessionParams Params;
		Params.WorldType =
			EPlaySessionWorldType::PlayInEditor;
		Params.EditorPlaySettings = Settings;
		GEditor->RequestPlaySession(Params);
		bPlayStarted = true;
		GoTo(EStep::WaitMenu);
		return false;
	}

	bool WaitMenu()
	{
		UWorld* World = JoATest::FindPieAny();
		UJoA_GameInstance* Instance = PieInstance();
		if (World && Instance && Once())
		{
			Instance->HostMatch();
		}

		UWorld* Host = JoATest::FindPieWorld(true);
		if (Host
			&& Host->GetMapName().Contains(TEXT("Arena")))
		{
			GoTo(EStep::WaitHost);
			return false;
		}

		if (Elapsed() > 25.0)
		{
			Fail(TEXT(
				"Timeout: Arena listen did not start."));
		}
		return Step == EStep::Done;
	}

	bool WaitHost()
	{
		UJoA_GameInstance* Instance = PieInstance();
		if (Once() && Instance)
		{
			if (Instance->HasOnlineSession())
			{
				Test->AddInfo(
					TEXT("Steam lobby is up."));
			}
			else if (Instance->CanUseSteamSession())
			{
				Test->AddWarning(
					TEXT("Steam is up, lobby is not. ")
					TEXT("Hosted on this machine."));
			}
			else
			{
				Test->AddInfo(
					TEXT("No Steam. Hosted locally."));
			}
		}

		if (JoATest::IsWatched()
			&& Elapsed()
				< JoATest::CVarHoldSeconds
					.GetValueOnGameThread())
		{
			return false;
		}

		if (Instance)
		{
			Instance->LeaveOnlineSession();
		}
		GoTo(EStep::EndPlay);
		return Step == EStep::Done;
	}

	bool EndPlay()
	{
		if (Once())
		{
			if (!JoATest::IsWatched())
			{
				GEditor->RequestEndPlayMap();
			}
			return false;
		}

		if (!GEditor->IsPlaySessionInProgress()
			|| (!JoATest::IsWatched()
				&& Elapsed() > 30.0))
		{
			if (Settings)
			{
				Settings->RemoveFromRoot();
			}
			GoTo(EStep::Done);
		}
		return Step == EStep::Done;
	}

	FAutomationTestBase* Test = nullptr;
	ULevelEditorPlaySettings* Settings = nullptr;
	EStep Step = EStep::Start;
	double StepStart = 0.0;
	bool bStepActed = false;
	bool bPlayStarted = false;
};

DEFINE_LATENT_AUTOMATION_COMMAND_ONE_PARAMETER(
	FJoARunSteamHost,
	TSharedPtr<FJoASteamHostScript>,
	Script);

bool FJoARunSteamHost::Update()
{
	return Script->Tick();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FJoASteamHostTest,
	"JoA.Match.SteamHost",
	EAutomationTestFlags::EditorContext
		| EAutomationTestFlags::ProductFilter)

bool FJoASteamHostTest::RunTest(
	const FString& Parameters)
{
	if (!FEditorFileUtils::LoadMap(
			TEXT("/Game/Map/Menu"),
			false,
			true))
	{
		AddError(TEXT("Map /Game/Map/Menu did not load."));
		return false;
	}

	ADD_LATENT_AUTOMATION_COMMAND(
		FJoARunSteamHost(
			MakeShared<FJoASteamHostScript>(this)));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FJoAFigureAnimsTest,
	"JoA.Match.FigureAnims",
	EAutomationTestFlags::EditorContext
		| EAutomationTestFlags::ProductFilter)

bool FJoAFigureAnimsTest::RunTest(
	const FString& Parameters)
{
	return StartScenario(this, EScenario::FigureAnims);
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FJoAMeleeReachTest,
	"JoA.Match.MeleeReach",
	EAutomationTestFlags::EditorContext
		| EAutomationTestFlags::ProductFilter)

bool FJoAMeleeReachTest::RunTest(
	const FString& Parameters)
{
	return StartScenario(this, EScenario::MeleeReach);
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FJoAMedicHealTest,
	"JoA.Match.MedicHeal",
	EAutomationTestFlags::EditorContext
		| EAutomationTestFlags::ProductFilter)

bool FJoAMedicHealTest::RunTest(
	const FString& Parameters)
{
	return StartScenario(this, EScenario::MedicHeal);
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FJoAArcherShotTest,
	"JoA.Match.ArcherShot",
	EAutomationTestFlags::EditorContext
		| EAutomationTestFlags::ProductFilter)

bool FJoAArcherShotTest::RunTest(
	const FString& Parameters)
{
	return StartScenario(this, EScenario::ArcherShot);
}

#endif
