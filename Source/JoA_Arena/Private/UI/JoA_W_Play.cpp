// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/JoA_W_Play.h"

// --- Engine ---
#include "Algo/Sort.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "Layout/Geometry.h"
#include "TimerManager.h"

// --- GAME ---
#include "Player/JoA_GameState.h"
#include "Player/JoA_PlayerController.h"
#include "Player/JoA_PlayerState.h"
#include "Board/JoA_Flag.h"
#include "Match/JoA_CookedContent.h"
#include "Match/JoA_GameInstance.h"
#include "Match/JoA_MatchSettings.h"
#include "Player/JoA_Nation.h"
#include "Player/Units/JoA_Figure.h"
#include "Player/Units/JoA_FigureRegistry.h"
#include "Player/Units/JoA_UnitTypes.h"
#include "UI/JoA_W_ActionButton.h"
#include "UI/JoA_UIText.h"
#include "UI/JoA_W_CharacterStat.h"
#include "UI/JoA_WidgetHelpers.h"

// --- Components ---
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/PanelWidget.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"

namespace
{
	bool ByUnitType(
		const UJoA_W_CharacterStat& A,
		const UJoA_W_CharacterStat& B)
	{
		return static_cast<uint8>(A.GetFigure()->UnitType)
			< static_cast<uint8>(B.GetFigure()->UnitType);
	}

	bool PortraitClickable(
		const AJoA_PlayerController& Controller,
		const AJoA_Figure* Shown)
	{
		if (Controller.OwnsFigure(Shown))
		{
			return true;
		}

		const AJoA_Figure* Selected =
			Controller.GetSelectedFigure();
		return IsValid(Selected)
			&& Selected != Shown
			&& (Selected->CanAttackTarget(Shown)
				|| Selected->CanHealTarget(Shown));
	}

	void SetEnabled(UButton* Button, bool bEnabled)
	{
		if (Button)
		{
			Button->SetIsEnabled(bEnabled);
		}
	}

	/* Default canvas anchors stretch to the full
	 * HUD. That covers portraits and HitsHud then
	 * eats every world click on the client. */
	void PinCanvasControl(
		UWidget* Widget,
		UCanvasPanel* Canvas,
		const FVector2D& Alignment)
	{
		if (!Widget || !Canvas)
		{
			return;
		}

		if (Widget->GetParent() != Canvas)
		{
			Widget->RemoveFromParent();
			Canvas->AddChildToCanvas(Widget);
		}

		UCanvasPanelSlot* Slot =
			Cast<UCanvasPanelSlot>(Widget->Slot);
		if (!Slot)
		{
			return;
		}

		Slot->SetAnchors(FAnchors(0.5f, 1.f));
		Slot->SetAlignment(Alignment);
		Slot->SetAutoSize(true);
		Slot->SetZOrder(2);
	}

	const float StatHalfGap = 8.f;

	const FLinearColor ActionHealColor(0.55f, 1.f, 0.55f);

	/* Wood cell. Obstacle and drop labels must
	 * not take the blue or red team tint. */
	const FLinearColor ActionObstacleColor =
		FLinearColor(FColor::FromHex(TEXT("6B4423")));

	FText TiedThrowWord(EOpeningThrow Throw)
	{
		switch (Throw)
		{
		case EOpeningThrow::Rock:
			return JoA_UIText(TEXT("Play.DrawRock"));
		case EOpeningThrow::Paper:
			return JoA_UIText(TEXT("Play.DrawPaper"));
		case EOpeningThrow::Scissors:
			return JoA_UIText(TEXT("Play.DrawScissors"));
		default:
			return FText::GetEmpty();
		}
	}

	/* The line under "Waiting for the opponent."
	 * The same line fills TB_Info once both
	 * seats are in and the match has not started. */
	FText OpeningChoiceLine(
		const AJoA_GameState* State,
		const AJoA_PlayerController* Controller)
	{
		if (!State || !Controller)
		{
			return FText::GetEmpty();
		}

		const EOpeningThrow Chosen =
			Controller->GetLocalOpeningThrow();
		if (Chosen != EOpeningThrow::None)
		{
			FText ThrowText;
			switch (Chosen)
			{
			case EOpeningThrow::Rock:
				ThrowText =
					JoA_UIText(TEXT("Play.ChosenRock"));
				break;
			case EOpeningThrow::Paper:
				ThrowText =
					JoA_UIText(TEXT("Play.ChosenPaper"));
				break;
			case EOpeningThrow::Scissors:
				ThrowText = JoA_UIText(
					TEXT("Play.ChosenScissors"));
				break;
			default:
				break;
			}
			return JoA_UIText(
				TEXT("Play.OpeningThrowChosen"),
				FFormatOrderedArguments{ ThrowText });
		}

		const EOpeningThrow Tied =
			State->GetOpeningTiedThrow();
		if (Tied != EOpeningThrow::None)
		{
			return JoA_UIText(
				TEXT("Play.OpeningThrowDraw"),
				FFormatOrderedArguments{
					TiedThrowWord(Tied)
				});
		}

		return JoA_UIText(
			TEXT("Play.OpeningThrowPrompt"));
	}

	const TCHAR* ShortNameKey(EUnitType Type)
	{
		switch (Type)
		{
		case EUnitType::HeavyKnight:
			return TEXT("Play.Unit.Knight");
		case EUnitType::LightInfantry:
			return TEXT("Play.Unit.Infantry");
		case EUnitType::Archer:
			return TEXT("Play.Unit.Archer");
		case EUnitType::Medic:
			return TEXT("Play.Unit.Medic");
		default:
			return TEXT("Play.Unit.None");
		}
	}

	bool OwnsPickupFlag(const AJoA_Figure* Selected)
	{
		if (!IsValid(Selected))
		{
			return false;
		}

		const AJoA_Flag* Flag =
			Selected->FindPickupFlag();
		return IsValid(Flag)
			&& Flag->GetOwningPlayerIndex()
				== Selected->OwningPlayerIndex;
	}

	int32 OppositeSeat(int32 Seat)
	{
		return Seat == FrenchSeat
			? EnglishSeat
			: FrenchSeat;
	}

	/* Own flag keeps this seat's GetTeamColor.
	 * The enemy flag uses the other seat, so
	 * the light blue and light red stay the
	 * same colours the teams already use. */
	FLinearColor ActionTextColor(
		EActionKind Kind,
		const AJoA_Figure* Target,
		const AJoA_Figure* Selected)
	{
		if (Kind == EActionKind::Obstacle
			|| Kind == EActionKind::Build
			|| Kind == EActionKind::Drop)
		{
			return ActionObstacleColor;
		}

		if (Kind == EActionKind::Pickup
			&& IsValid(Selected))
		{
			const int32 Own =
				Selected->OwningPlayerIndex;
			const int32 Seat = OwnsPickupFlag(Selected)
				? Own
				: OppositeSeat(Own);
			return GetTeamColor(Seat);
		}

		/* A null target must not be read. That
		 * crashed the client (0x500). */
		const AJoA_Figure* ColorFigure =
			IsValid(Target) ? Target : Selected;
		if (IsValid(ColorFigure))
		{
			return GetTeamColor(
				ColorFigure->OwningPlayerIndex);
		}

		return FLinearColor::White;
	}

	FText ActionLabel(
		EActionKind Kind,
		const AJoA_Figure* Target,
		const AJoA_Figure* Selected)
	{
		if (Kind == EActionKind::Pickup)
		{
			return JoA_UIText(
				OwnsPickupFlag(Selected)
					? TEXT("Play.PickUpYourFlag")
					: TEXT("Play.PickUpEnemyFlag"));
		}

		if (Kind == EActionKind::Drop)
		{
			return JoA_UIText(TEXT("Play.DropFlag"));
		}

		if (Kind == EActionKind::Obstacle)
		{
			return JoA_UIText(
				TEXT("Play.DestroyObstacle"));
		}

		if (Kind == EActionKind::Build)
		{
			return JoA_UIText(
				TEXT("Play.BuildObstacle"));
		}

		return Target
			? JoA_UIText(ShortNameKey(Target->UnitType))
			: FText::GetEmpty();
	}
}

FText UJoA_W_Play::GetInfoText() const
{
	return TB_Info ? TB_Info->GetText() : FText::GetEmpty();
}

bool UJoA_W_Play::HitsHud(const FVector2D& DesktopPos) const
{
	auto Over = [&DesktopPos](const UWidget* Widget)
	{
		if (!Widget)
		{
			return false;
		}

		const ESlateVisibility Vis =
			Widget->GetVisibility();
		if (Vis == ESlateVisibility::Collapsed
			|| Vis == ESlateVisibility::Hidden
			|| Vis
				== ESlateVisibility::HitTestInvisible
			|| Vis
				== ESlateVisibility::SelfHitTestInvisible)
		{
			return false;
		}

		const FGeometry Geometry =
			Widget->GetCachedGeometry();
		const FVector2f Size = Geometry.GetLocalSize();
		if (Size.X < 1.f || Size.Y < 1.f)
		{
			return false;
		}

		return Geometry.IsUnderLocation(
			FVector2f(DesktopPos));
	};

	/* Only the controls themselves. The portrait
	 * rows and action boxes are stretch slots on
	 * a small client viewport, and their geometry
	 * would eat every world click. */
	if (Over(Btn_Go)
		|| Over(Btn_TurnLeft)
		|| Over(Btn_TurnRight)
		|| Over(Btn_SkipTurn)
		|| Over(Btn_Rest)
		|| Over(Btn_Jump)
		|| Over(Btn_Fall))
	{
		return true;
	}

	for (const UJoA_W_CharacterStat* Stat : StatWidgets)
	{
		if (Over(Stat))
		{
			return true;
		}
	}

	const UPanelWidget* ActionRow = HB_ActionButtons;
	if (!ActionRow)
	{
		return false;
	}

	const int32 Count = ActionRow->GetChildrenCount();
	for (int32 Index = 0; Index < Count; ++Index)
	{
		const UWidget* Child = ActionRow->GetChildAt(Index);
		if (Over(Child))
		{
			return true;
		}

		const UPanelWidget* Nested =
			Cast<UPanelWidget>(Child);
		if (!Nested)
		{
			continue;
		}

		const int32 NestedCount =
			Nested->GetChildrenCount();
		for (int32 NestedIndex = 0;
			NestedIndex < NestedCount;
			++NestedIndex)
		{
			if (Over(Nested->GetChildAt(NestedIndex)))
			{
				return true;
			}
		}
	}

	return false;
}

void UJoA_W_Play::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	JoA_DisableButtonFocus(Btn_Go);
	JoA_DisableButtonFocus(Btn_TurnLeft);
	JoA_DisableButtonFocus(Btn_TurnRight);
	JoA_DisableButtonFocus(Btn_SkipTurn);
	JoA_DisableButtonFocus(Btn_Rest);
	JoA_DisableButtonFocus(Btn_Jump);
	JoA_DisableButtonFocus(Btn_Fall);
}

void UJoA_W_Play::NativeConstruct()
{
	Super::NativeConstruct();
	/* Empty canvas must not swallow board clicks.
	 * Portraits and buttons stay Visible. */
	SetVisibility(
		ESlateVisibility::SelfHitTestInvisible);
	if (HB_LocalFigures)
	{
		HB_LocalFigures->SetVisibility(
			ESlateVisibility::SelfHitTestInvisible);
	}
	if (HB_OpponentFigures)
	{
		HB_OpponentFigures->SetVisibility(
			ESlateVisibility::SelfHitTestInvisible);
	}
	if (TB_Info)
	{
		TB_Info->SetVisibility(
			ESlateVisibility::HitTestInvisible);
	}

	/* The player controller builds this widget. A second one
	 * for the same player, for example from the Level
	 * Blueprint, would double every stat row. The later one
	 * leaves. */
	for (TObjectIterator<UJoA_W_Play> It; It; ++It)
	{
		const UJoA_W_Play* Other = *It;
		if (Other != this
			&& Other->IsInViewport()
			&& Other->GetOwningPlayer() == GetOwningPlayer())
		{
			RemoveFromParent();
			return;
		}
	}

	/* Stat widgets placed in the designer are samples.
	 * The real ones are built from the registry. A sample
	 * can sit in any panel, so the whole tree is swept. */
	if (WidgetTree)
	{
		TArray<UWidget*> Samples;
		WidgetTree->ForEachWidget(
			[&Samples](UWidget* Child)
			{
				if (Cast<UJoA_W_CharacterStat>(Child))
				{
					Samples.Add(Child);
				}
			});
		for (UWidget* Sample : Samples)
		{
			Sample->RemoveFromParent();
		}
	}

	if (HB_LocalFigures)
	{
		HB_LocalFigures->ClearChildren();
	}
	if (HB_OpponentFigures)
	{
		HB_OpponentFigures->ClearChildren();
	}

	if (Btn_Go)
	{
		Btn_Go->OnClicked.AddDynamic(
			this,
			&UJoA_W_Play::OnGoClicked);
	}
	if (Btn_TurnLeft)
	{
		Btn_TurnLeft->OnClicked.AddDynamic(
			this,
			&UJoA_W_Play::OnTurnLeftClicked);
	}
	if (Btn_TurnRight)
	{
		Btn_TurnRight->OnClicked.AddDynamic(
			this,
			&UJoA_W_Play::OnTurnRightClicked);
	}
	if (Btn_SkipTurn)
	{
		Btn_SkipTurn->OnClicked.AddDynamic(
			this,
			&UJoA_W_Play::OnSkipTurnClicked);
	}
	if (Btn_Rest)
	{
		Btn_Rest->OnClicked.AddDynamic(
			this,
			&UJoA_W_Play::OnRestClicked);
	}

	BindJumpFallClicks();

	if (!StatWidgetClass)
	{
		StatWidgetClass =
			UJoA_CookedContent::LoadWidget(
				this,
				TEXT("/Game/WBP/"
					"WBP_CharacterStat."
					"WBP_CharacterStat_C"));
	}

	if (UJoA_FigureRegistry* Registry =
		UJoA_FigureRegistry::Get(this))
	{
		/* Register and unregister also broadcast this.
		 * A second bind rebuilt the rows twice per
		 * figure at the start of the match. */
		BoardHandle = Registry->OnBoardStateChanged.AddUObject(
			this,
			&UJoA_W_Play::Rebuild);
	}

	if (AJoA_PlayerController* Controller =
		Cast<AJoA_PlayerController>(GetOwningPlayer()))
	{
		SelectionHandle =
			Controller->OnSelectedFigureChanged.AddUObject(
				this,
				&UJoA_W_Play::HandleSelectedFigureChanged);
	}

	/* NativeTick lines the action row up with the
	 * turn arrow once Slate has a size. */
	bHasScriptImplementedTick = true;
	Rebuild();
}

void UJoA_W_Play::NativeTick(
	const FGeometry& MyGeometry,
	float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	bool bWaiting = false;
	if (bActionButtonsPinned
		&& !PlaceActionButtonsNextToArrows())
	{
		bWaiting = true;
	}
	if (bJumpFallPinned && !PlaceJumpFallButtons())
	{
		bWaiting = true;
	}
	if (!bInfoBesidePortraits)
	{
		if (PlaceInfoBesidePortraits())
		{
			bInfoBesidePortraits = true;
		}
		else
		{
			bWaiting = true;
		}
	}
	if (!bKeyHintsPlaced)
	{
		if (PlaceKeyHints())
		{
			bKeyHintsPlaced = true;
		}
		else
		{
			bWaiting = true;
		}
	}

	/* Slate had no size on the first pass. Once
	 * both rows have one, stop polling the layout. */
	if (!bWaiting)
	{
		bHasScriptImplementedTick = false;
		UpdateCanTick();
	}
}

void UJoA_W_Play::KeepLayoutTick()
{
	if (bHasScriptImplementedTick)
	{
		return;
	}

	bHasScriptImplementedTick = true;
	UpdateCanTick();
}

void UJoA_W_Play::NativeDestruct()
{
	if (UWorld* World = GetWorld())
	{
		FTimerManager& Timers = World->GetTimerManager();
		Timers.ClearTimer(CountdownHandle);
		Timers.ClearTimer(OpeningThrowInfoHandle);
	}

	if (UJoA_FigureRegistry* Registry =
		UJoA_FigureRegistry::Get(this))
	{
		Registry->OnBoardStateChanged.Remove(BoardHandle);
	}

	if (AJoA_PlayerController* Controller =
		Cast<AJoA_PlayerController>(GetOwningPlayer()))
	{
		Controller->OnSelectedFigureChanged.Remove(
			SelectionHandle);
	}

	Super::NativeDestruct();
}

void UJoA_W_Play::Rebuild()
{
	SyncFigures();
	RefreshSelection();
	RefreshControls();
	RefreshTurnInfo();
}

void UJoA_W_Play::HandleSelectedFigureChanged(
	AJoA_Figure* Figure)
{
	RefreshSelection();
	RefreshControls();
	for (UJoA_W_CharacterStat* Widget : StatWidgets)
	{
		if (Widget)
		{
			Widget->NotifyPicked(
				Widget->GetFigure() == Figure);
		}
	}
}

void UJoA_W_Play::SyncFigures()
{
	const AJoA_PlayerController* Controller =
		Cast<AJoA_PlayerController>(GetOwningPlayer());
	const UJoA_FigureRegistry* Registry =
		UJoA_FigureRegistry::Get(this);
	if (!Controller || !Registry || !StatWidgetClass)
	{
		return;
	}

	TArray<AJoA_Figure*> Living;
	Registry->CollectFigures(Living);

	bool bChanged = false;

	// A fallen or removed figure loses its stat widget.
	for (int32 Index = StatWidgets.Num() - 1;
		Index >= 0;
		--Index)
	{
		UJoA_W_CharacterStat* Widget = StatWidgets[Index];
		AJoA_Figure* Shown = Widget->GetFigure();
		if (Shown && !Shown->IsDead()
			&& Living.Contains(Shown))
		{
			continue;
		}

		Widget->ClearFigure();
		Widget->RemoveFromParent();
		StatWidgets.RemoveAtSwap(Index);
		bChanged = true;
	}

	for (AJoA_Figure* Figure : Living)
	{
		/* Wait until this machine can tell whose figure
		 * it is. The owning-state pointer can still be
		 * unmapped on a client while the seat int is
		 * already there. */
		if (Figure->IsDead())
		{
			continue;
		}

		const AJoA_PlayerState* OwnerState =
			Controller->GetPlayerState<AJoA_PlayerState>();
		if (!OwnerState)
		{
			continue;
		}

		const bool bPointerKnown =
			Figure->GetOwningPlayerState() != nullptr;
		const bool bSeatKnown =
			OwnerState->GetSeatIndex() != INDEX_NONE
			&& Figure->OwningPlayerIndex != INDEX_NONE;
		if (!bPointerKnown && !bSeatKnown)
		{
			continue;
		}

		const bool bKnown = StatWidgets.ContainsByPredicate(
			[Figure](const UJoA_W_CharacterStat* Widget)
			{
				return Widget->GetFigure() == Figure;
			});
		if (bKnown)
		{
			continue;
		}

		UJoA_W_CharacterStat* Widget =
			CreateWidget<UJoA_W_CharacterStat>(
				this,
				StatWidgetClass);
		if (!Widget)
		{
			continue;
		}

		Widget->SetFigure(Figure);
		Widget->SetClickable(
			PortraitClickable(*Controller, Figure));
		Widget->OnFigureClicked.AddUObject(
			this,
			&UJoA_W_Play::HandleFigureClicked);
		StatWidgets.Add(Widget);
		bChanged = true;
	}

	/* Ownership can arrive after the widget was made. A
	 * widget made too early stayed in the wrong row and not
	 * clickable, so the portrait ignored the click. */
	for (UJoA_W_CharacterStat* Widget : StatWidgets)
	{
		const bool bOwn =
			Controller->OwnsFigure(Widget->GetFigure());
		Widget->SetClickable(
			PortraitClickable(
				*Controller,
				Widget->GetFigure()));

		const UPanelWidget* Wanted = bOwn
			? HB_LocalFigures.Get()
			: HB_OpponentFigures.Get();
		if (Widget->GetParent() != Wanted)
		{
			bChanged = true;
		}
	}

	if (!bChanged)
	{
		return;
	}

	/* Rows are rebuilt in unit type order, so the
	 * portraits match keys 1 to 4. */
	Algo::Sort(
		StatWidgets,
		[](const UJoA_W_CharacterStat* A,
			const UJoA_W_CharacterStat* B)
		{
			return ByUnitType(*A, *B);
		});

	if (HB_LocalFigures)
	{
		HB_LocalFigures->ClearChildren();
	}
	if (HB_OpponentFigures)
	{
		HB_OpponentFigures->ClearChildren();
	}

	for (UJoA_W_CharacterStat* Widget : StatWidgets)
	{
		UPanelWidget* Row =
			Controller->OwnsFigure(Widget->GetFigure())
			? HB_LocalFigures.Get()
			: HB_OpponentFigures.Get();
		if (!Row)
		{
			continue;
		}

		UPanelSlot* RowSlot = Row->AddChild(Widget);
		if (UHorizontalBoxSlot* BoxSlot =
			Cast<UHorizontalBoxSlot>(RowSlot))
		{
			BoxSlot->SetPadding(
				FMargin(StatHalfGap, 0.f, StatHalfGap, 0.f));
		}
	}

	/* Portraits arrive after the first layout.
	 * The info line has to measure them again. */
	bInfoBesidePortraits = false;
	bKeyHintsPlaced = false;
	KeepLayoutTick();
}

void UJoA_W_Play::RefreshSelection()
{
	const AJoA_PlayerController* Controller =
		Cast<AJoA_PlayerController>(GetOwningPlayer());
	if (!Controller)
	{
		return;
	}

	const AJoA_Figure* Selected =
		Controller->GetSelectedFigure();
	for (UJoA_W_CharacterStat* Widget : StatWidgets)
	{
		const AJoA_Figure* Shown = Widget->GetFigure();
		/* Only the local player has a selection. The
		 * opponent's portraits stay at full colour. */
		const bool bOwn = Controller->OwnsFigure(Shown);
		Widget->SetClickable(
			PortraitClickable(*Controller, Shown));
		Widget->SetSelected(!bOwn || Shown == Selected);
	}
}

void UJoA_W_Play::RefreshControls()
{
	const AJoA_PlayerController* Controller =
		Cast<AJoA_PlayerController>(GetOwningPlayer());
	const AJoA_Figure* Figure =
		Controller ? Controller->GetSelectedFigure() : nullptr;

	/* The same test the ability runs. A button is
	 * disabled when its move would be refused. */
	const bool bValid = IsValid(Figure);
	SetEnabled(
		Btn_Go,
		bValid && Figure->CanStep(EStepDirection::Forward));
	SetEnabled(Btn_TurnLeft, bValid && Figure->CanTurn());
	SetEnabled(Btn_TurnRight, bValid && Figure->CanTurn());
	SetEnabled(Btn_SkipTurn, bValid && Figure->CanSkipTurn());
	SetEnabled(Btn_Rest, bValid && Figure->CanRest());

	UpdateGoArrow(bValid ? Figure : nullptr);
	RefreshActions();
	RefreshJumpFall();
	RefreshKeyHints();
}

void UJoA_W_Play::UpdateGoArrow(const AJoA_Figure* Figure)
{
	if (!Btn_Go)
	{
		return;
	}

	/* The camera of a seat looks the way its figures face
	 * at the start. The arrow points up when the figure
	 * still faces that way, and turns by 45 degrees for
	 * every step of the free turn. */
	float Angle = 0.f;
	if (Figure)
	{
		const EFacing Home = Figure->OwningPlayerIndex
				== FrenchSeat
			? EFacing::NegativeX
			: EFacing::PositiveX;
		int32 Delta =
			AJoA_Figure::FacingRingIndex(Figure->Facing)
			- AJoA_Figure::FacingRingIndex(Home);
		Delta = ((Delta % 8) + 8) % 8;
		if (Delta > 4)
		{
			Delta -= 8;
		}
		Angle = 45.f * static_cast<float>(Delta);
	}

	Btn_Go->SetRenderTransformPivot(FVector2D(0.5, 0.5));
	Btn_Go->SetRenderTransformAngle(Angle);
}
void UJoA_W_Play::RefreshActions()
{
	if (!HB_ActionButtons && WidgetTree)
	{
		/* The Blueprint has no panel for the buttons yet.
		 * A box on the root canvas stands in, right of the
		 * arrows. */
		UCanvasPanel* Canvas =
			Cast<UCanvasPanel>(WidgetTree->RootWidget);
		if (Canvas)
		{
			UHorizontalBox* Box =
				WidgetTree->ConstructWidget<UHorizontalBox>();
			UCanvasPanelSlot* BoxSlot =
				Canvas->AddChildToCanvas(Box);
			BoxSlot->SetAnchors(FAnchors(0.5f, 1.f));
			BoxSlot->SetAlignment(FVector2D(0.0, 1.0));
			BoxSlot->SetPosition(FVector2D(260.0, -40.0));
			BoxSlot->SetAutoSize(true);
			HB_ActionButtons = Box;
		}
	}

	if (!HB_ActionButtons)
	{
		return;
	}

	PinActionButtons();

	const AJoA_PlayerController* Controller =
		Cast<AJoA_PlayerController>(GetOwningPlayer());
	const AJoA_Figure* Selected =
		Controller ? Controller->GetSelectedFigure() : nullptr;
	const UJoA_FigureRegistry* Registry =
		UJoA_FigureRegistry::Get(this);

	TArray<AJoA_Figure*> Targets;
	if (IsValid(Selected) && Registry)
	{
		Registry->CollectFigures(Targets);
	}

	Algo::Sort(
		Targets,
		[](const AJoA_Figure* A, const AJoA_Figure* B)
		{
			return static_cast<uint8>(A->UnitType)
				< static_cast<uint8>(B->UnitType);
		});

	TArray<TPair<AJoA_Figure*, EActionKind>> Wanted;
	TArray<uint32> Signature;
	for (AJoA_Figure* Target : Targets)
	{
		const bool bAttack = Selected->CanAttackTarget(Target);
		const bool bHeal =
			!bAttack && Selected->CanHealTarget(Target);
		if (bAttack || bHeal)
		{
			const EActionKind Kind = bHeal
				? EActionKind::Heal
				: EActionKind::Attack;
			Wanted.Emplace(Target, Kind);
			Signature.Add(
				Target->GetUniqueID() * 2u
				+ (bHeal ? 1u : 0u));
		}
	}

	FIntPoint ObstacleCell = FIntPoint::ZeroValue;
	if (IsValid(Selected)
		&& Selected->FindDestroyableObstacle(ObstacleCell))
	{
		Wanted.Emplace(nullptr, EActionKind::Obstacle);
		Signature.Add(
			0x80000000u
			| (static_cast<uint32>(ObstacleCell.X) << 8)
			| static_cast<uint32>(ObstacleCell.Y));
	}

	FIntPoint BuildCell = FIntPoint::ZeroValue;
	if (IsValid(Selected)
		&& Selected->FindBuildableCell(BuildCell))
	{
		Wanted.Emplace(nullptr, EActionKind::Build);
		Signature.Add(
			0x10000000u
			| (static_cast<uint32>(BuildCell.X) << 8)
			| static_cast<uint32>(BuildCell.Y));
	}

	const AJoA_Flag* PickupFlag = IsValid(Selected)
		? Selected->FindPickupFlag()
		: nullptr;
	if (PickupFlag)
	{
		Wanted.Emplace(nullptr, EActionKind::Pickup);
		Signature.Add(
			0x40000000u
			| (static_cast<uint32>(PickupFlag->GetCellX())
				<< 8)
			| static_cast<uint32>(PickupFlag->GetCellY()));
	}
	else if (IsValid(Selected) && Selected->CanDropFlag())
	{
		Wanted.Emplace(nullptr, EActionKind::Drop);
		Signature.Add(0x20000000u);
	}

	/* Rebuilding on every board change would drop the
	 * hover and press state of a button under the mouse. */
	if (Signature == ActionSignature
		&& HB_ActionButtons->GetChildrenCount()
			== Wanted.Num())
	{
		return;
	}

	ActionSignature = Signature;
	HB_ActionButtons->ClearChildren();
	if (!AttackIcon)
	{
		AttackIcon =
			UJoA_CookedContent::LoadAttackIcon(this);
	}

	auto MakeActionButton =
		[this, Selected](
			AJoA_Figure* Target,
			EActionKind Kind) -> UJoA_W_ActionButton*
	{
		UJoA_W_ActionButton* Button =
			WidgetTree->ConstructWidget<
				UJoA_W_ActionButton>();
		Button->Setup(Target, Kind);
		Button->OnActionClicked.AddUObject(
			this,
			&UJoA_W_Play::HandleActionClicked);

		UHorizontalBox* Content =
			WidgetTree->ConstructWidget<
				UHorizontalBox>();
		Button->AddChild(Content);

		const bool bHeal = Kind == EActionKind::Heal;
		if (Kind == EActionKind::Attack && AttackIcon)
		{
			UImage* Icon =
				WidgetTree->ConstructWidget<UImage>();
			Icon->SetBrushFromTexture(AttackIcon);
			Icon->SetDesiredSizeOverride(
				FVector2D(36.0, 36.0));
			UPanelSlot* IconSlot = Content->AddChild(Icon);
			Cast<UHorizontalBoxSlot>(IconSlot)
				->SetVerticalAlignment(VAlign_Center);
		}
		else if (bHeal)
		{
			UTextBlock* Plus =
				WidgetTree->ConstructWidget<
					UTextBlock>();
			Plus->SetText(FText::FromString(TEXT("+")));
			Plus->SetColorAndOpacity(
				FSlateColor(ActionHealColor));
			FSlateFontInfo Font = Plus->GetFont();
			Font.Size = 32;
			Plus->SetFont(Font);
			UPanelSlot* PlusSlot = Content->AddChild(Plus);
			Cast<UHorizontalBoxSlot>(PlusSlot)
				->SetVerticalAlignment(VAlign_Center);
		}

		UTextBlock* Name =
			WidgetTree->ConstructWidget<UTextBlock>(); 		
		Name->SetText(
			ActionLabel(Kind, Target, Selected));
		Name->SetColorAndOpacity(
			FSlateColor(ActionTextColor(
				Kind,
				Target,
				Selected)));
		UPanelSlot* NameSlot = Content->AddChild(Name);
		if (UHorizontalBoxSlot* NameBox =
			Cast<UHorizontalBoxSlot>(NameSlot))
		{
			NameBox->SetVerticalAlignment(
				VAlign_Center);
			NameBox->SetPadding(
				FMargin(8.f, 0.f, 0.f, 0.f));
		}

		return Button;
	};

	TArray<TPair<AJoA_Figure*, EActionKind>> Heals;
	TArray<TPair<AJoA_Figure*, EActionKind>> Others;
	for (const TPair<AJoA_Figure*, EActionKind>& Entry
		: Wanted)
	{
		if (Entry.Value == EActionKind::Heal)
		{
			Heals.Add(Entry);
		}
		else
		{
			Others.Add(Entry);
		}
	}

	if (Heals.Num() > 0)
	{
		UVerticalBox* HealStack =
			WidgetTree->ConstructWidget<UVerticalBox>();
		HealStack->SetVisibility(
			ESlateVisibility::SelfHitTestInvisible);
		for (const TPair<AJoA_Figure*, EActionKind>& Entry
			: Heals)
		{
			UJoA_W_ActionButton* Button = MakeActionButton(
				Entry.Key,
				Entry.Value);
			UPanelSlot* HealSlot =
				HealStack->AddChild(Button);
			if (UVerticalBoxSlot* StackSlot =
				Cast<UVerticalBoxSlot>(HealSlot))
			{
				StackSlot->SetPadding(
					FMargin(0.f, 0.f, 0.f, 6.f));
			}
		}

		UPanelSlot* StackSlot =
			HB_ActionButtons->AddChild(HealStack);
		if (UHorizontalBoxSlot* BoxSlot =
			Cast<UHorizontalBoxSlot>(StackSlot))
		{
			BoxSlot->SetPadding(
				FMargin(6.f, 0.f, 6.f, 0.f));
		}
	}

	for (const TPair<AJoA_Figure*, EActionKind>& Entry
		: Others)
	{
		UJoA_W_ActionButton* Button = MakeActionButton(
			Entry.Key,
			Entry.Value);
		UPanelSlot* ButtonSlot =
			HB_ActionButtons->AddChild(Button);
		if (UHorizontalBoxSlot* BoxSlot =
			Cast<UHorizontalBoxSlot>(ButtonSlot))
		{
			BoxSlot->SetPadding(
				FMargin(6.f, 0.f, 6.f, 0.f));
		}
	}
}

void UJoA_W_Play::PinActionButtons()
{
	if (!HB_ActionButtons || !WidgetTree)
	{
		return;
	}

	UCanvasPanel* Canvas =
		Cast<UCanvasPanel>(WidgetTree->RootWidget);
	if (!Canvas)
	{
		return;
	}

	/* HB_ActionButtons sits in the Blueprint vertical
	 * stack with the arrows. A pickup row then pushes
	 * the move buttons. The canvas slot is independent.
	 * AutoSize is set here, not after Place, because
	 * Place returns early before Slate has a size. */
	PinCanvasControl(
		HB_ActionButtons,
		Canvas,
		FVector2D(0.0, 0.5));
	HB_ActionButtons->SetVisibility(
		ESlateVisibility::SelfHitTestInvisible);

	KeepLayoutTick();
	PlaceActionButtonsNextToArrows();
	bActionButtonsPinned = true;
}

bool UJoA_W_Play::PlaceActionButtonsNextToArrows()
{
	if (!HB_ActionButtons)
	{
		return false;
	}

	UCanvasPanelSlot* CanvasSlot =
		Cast<UCanvasPanelSlot>(HB_ActionButtons->Slot);
	UWidget* Neighbour = Btn_TurnRight
		? static_cast<UWidget*>(Btn_TurnRight.Get())
		: static_cast<UWidget*>(Btn_Go.Get());
	UWidget* CanvasWidget =
		HB_ActionButtons->GetParent();
	if (!CanvasSlot || !Neighbour || !CanvasWidget)
	{
		return false;
	}

	const FGeometry CanvasGeo =
		CanvasWidget->GetCachedGeometry();
	const FGeometry ArrowGeo =
		Neighbour->GetCachedGeometry();
	const FVector2f CanvasSize = CanvasGeo.GetLocalSize();
	const FVector2f ArrowSize = ArrowGeo.GetLocalSize();
	if (CanvasSize.X < 1.f || ArrowSize.X < 1.f)
	{
		return false;
	}

	/* Left edge of the action row, a small gap
	 * after the right turn arrow. Vertical centre
	 * matches the arrow. */
	const FVector2f ArrowLocal =
		CanvasGeo.AbsoluteToLocal(
			ArrowGeo.GetAbsolutePosition());
	const float Gap = 8.f;
	const float Left =
		ArrowLocal.X + ArrowSize.X + Gap;
	const float MidY =
		ArrowLocal.Y + ArrowSize.Y * 0.5f;

	const FVector2D SlotPos(
		Left - CanvasSize.X * 0.5f,
		MidY - CanvasSize.Y);
	if (!CanvasSlot->GetPosition().Equals(SlotPos, 0.5f))
	{
		CanvasSlot->SetAnchors(FAnchors(0.5f, 1.f));
		CanvasSlot->SetAlignment(FVector2D(0.0, 0.5));
		CanvasSlot->SetPosition(SlotPos);
		CanvasSlot->SetAutoSize(true);
		CanvasSlot->SetZOrder(2);
	}
	return true;
}

void UJoA_W_Play::HandleActionClicked(
	AJoA_Figure* Target,
	EActionKind Kind)
{
	AJoA_PlayerController* Controller =
		Cast<AJoA_PlayerController>(GetOwningPlayer());
	AJoA_Figure* Selected =
		Controller ? Controller->GetSelectedFigure() : nullptr;
	if (!Selected)
	{
		return;
	}

	if (Kind == EActionKind::Pickup)
	{
		Selected->RequestPickupFlag();
		return;
	}

	if (Kind == EActionKind::Drop)
	{
		Selected->RequestDropFlag();
		return;
	}

	if (Kind == EActionKind::Obstacle)
	{
		FIntPoint ObstacleCell = FIntPoint::ZeroValue;
		if (Selected->FindDestroyableObstacle(ObstacleCell))
		{
			Controller->RequestDestroyObstacle(
				Selected,
				ObstacleCell.X,
				ObstacleCell.Y);
		}
		return;
	}

	if (Kind == EActionKind::Build)
	{
		Selected->RequestBuildObstacle();
		return;
	}

	if (Kind == EActionKind::Heal)
	{
		Controller->RequestHeal(Selected, Target);
		return;
	}

	Controller->RequestAttack(Selected, Target);
}

bool UJoA_W_Play::PlaceInfoBesidePortraits()
{
	if (!TB_Info || !WidgetTree)
	{
		return true;
	}

	UCanvasPanel* Canvas =
		Cast<UCanvasPanel>(WidgetTree->RootWidget);
	if (!Canvas)
	{
		return true;
	}

	/* Keep the line on the top centre. A long
	 * sentence wraps before the top portraits.
	 * The text does not take clicks. */
	if (TB_Info->GetParent() != Canvas)
	{
		TB_Info->RemoveFromParent();
		Canvas->AddChildToCanvas(TB_Info);
	}

	UCanvasPanelSlot* InfoSlot =
		Cast<UCanvasPanelSlot>(TB_Info->Slot);
	const FGeometry CanvasGeo =
		Canvas->GetCachedGeometry();
	const FVector2f CanvasSize =
		CanvasGeo.GetLocalSize();
	if (!InfoSlot || CanvasSize.X < 1.f)
	{
		return false;
	}

	float PortraitLeft = CanvasSize.X;
	bool bFoundPortrait = false;
	for (const UJoA_W_CharacterStat* Stat : StatWidgets)
	{
		if (!Stat)
		{
			continue;
		}

		const FGeometry StatGeo =
			Stat->GetCachedGeometry();
		if (StatGeo.GetLocalSize().X < 1.f)
		{
			continue;
		}

		const FVector2f Local =
			CanvasGeo.AbsoluteToLocal(
				StatGeo.GetAbsolutePosition());
		/* The own row sits on the bottom.
		 * Only the top row limits the line. */
		if (Local.Y > CanvasSize.Y * 0.4f)
		{
			continue;
		}

		PortraitLeft = FMath::Min(
			PortraitLeft,
			Local.X);
		bFoundPortrait = true;
	}

	const float Margin = 16.f;
	const float CenterX = CanvasSize.X * 0.5f;
	/* "Your turn (13 s)" is one line. Auto
	 * wrap was measuring the box it had just
	 * shrunk, so the last word dropped. */
	float WrapWidth = 900.f;
	if (bFoundPortrait
		&& PortraitLeft > CenterX + Margin)
	{
		WrapWidth = (PortraitLeft - CenterX - Margin)
			* 2.f;
	}
	WrapWidth = FMath::Clamp(
		WrapWidth,
		720.f,
		CanvasSize.X - Margin * 2.f);

	TB_Info->SetAutoWrapText(false);
	TB_Info->SetWrapTextAt(WrapWidth);
	TB_Info->SetJustification(ETextJustify::Center);
	TB_Info->SetVisibility(
		ESlateVisibility::HitTestInvisible);

	/* Center of the line is 100px under the top,
	 * the same slot as the menu designer.
	 * Alignment 0 pins the text to the ceiling. */
	InfoSlot->SetAnchors(FAnchors(0.5f, 0.f));
	InfoSlot->SetAlignment(FVector2D(0.5f, 0.5f));
	InfoSlot->SetPosition(FVector2D(0.f, 100.f));
	InfoSlot->SetAutoSize(true);
	/* Above the portrait row, so a long line
	 * is not drawn behind those widgets. */
	InfoSlot->SetZOrder(20);
	return bFoundPortrait;
}

void UJoA_W_Play::RefreshTurnInfo()
{
	const AJoA_PlayerController* Controller =
		Cast<AJoA_PlayerController>(GetOwningPlayer());
	const AJoA_GameState* State = AJoA_GameState::Get(this);
	if (!TB_Info || !Controller || !State)
	{
		return;
	}

	const AJoA_PlayerState* OwnerState =
		Controller->GetPlayerState<AJoA_PlayerState>();
	if (!OwnerState)
	{
		return;
	}

	const int32 Seat =
		OwnerState->GetNation() == ENation::French
		? FrenchSeat
		: EnglishSeat;

	if (!bInfoColorSaved)
	{
		bInfoColorSaved = true;
		InfoDefaultColor = TB_Info->GetColorAndOpacity();
		TB_Info->SetRenderTransformPivot(
			FVector2D(0.5, 0.5));
		TB_Info->SetJustification(ETextJustify::Center);
		TB_Info->SetVisibility(
			ESlateVisibility::HitTestInvisible);
	}

	/* Without a limit the deadline is 0 and the turn text
	 * stays as it was, with no count. */
	const double Deadline = State->GetTurnDeadline();
	const bool bTimed = Deadline > 0.0;
	const FText TurnLeft = FText::AsNumber(FMath::Max(
		0,
		FMath::CeilToInt(State->SecondsUntil(Deadline))));
	const int32 OpponentSeat =
		Seat == FrenchSeat ? EnglishSeat : FrenchSeat;
	const UWorld* World = GetWorld();
	const UJoA_GameInstance* Instance = World
		? World->GetGameInstance<UJoA_GameInstance>()
		: nullptr;

	FSlateColor Color = InfoDefaultColor;
	FText Message;
	bool bCountdown = false;
	bool bPulse = false;
	float FontScale = 1.f;
	if (Instance && Instance->IsReconnecting())
	{
		Color = FSlateColor(InfoWarningColor);
		Message = JoA_UIText(
			TEXT("Play.Reconnecting"),
			FFormatOrderedArguments{
				FText::AsNumber(
					Instance->GetReconnectAttempt())
			});
	}
	else if (State->IsMatchOver())
	{
		const EMatchEndReason EndReason =
			State->GetMatchEndReason();
		if (EndReason == EMatchEndReason::Draw)
		{
			Color = FSlateColor(InfoWhiteColor);
			Message = JoA_UIText(TEXT("Play.MatchDraw"));
		}
		else
		{
			const bool bWon =
				State->GetWinnerSeat() == Seat;
		const bool bForfeit =
			EndReason == EMatchEndReason::Forfeit;
		const bool bFlagCapture =
			EndReason == EMatchEndReason::FlagCapture;
		Color = FSlateColor(
			bWon ? InfoWonColor : InfoWarningColor);
		if (bForfeit)
		{
			Message = JoA_UIText(
				bWon
					? TEXT("Play.VictoryForfeit")
					: TEXT("Play.DefeatForfeit"));
		}
		else if (bFlagCapture)
		{
			Message = JoA_UIText(
				bWon
					? TEXT("Play.VictoryFlag")
					: TEXT("Play.DefeatFlag"));
		}
		else
		{
			Message = JoA_UIText(
				bWon
					? TEXT("Play.Victory")
					: TEXT("Play.Defeat"));
		}
		}
	}
	else if (State->IsPaused())
	{
		const FSeatLink Link =
			State->GetSeatLink(OpponentSeat);
		Color = FSlateColor(InfoWarningColor);
		if (Link.State == ESeatConnection::Disconnected)
		{
			bCountdown = true;
			const double Return = Link.ReconnectDeadline;
			Message = JoA_UIText(
				TEXT("Play.OpponentGone"),
				FFormatOrderedArguments{
					FText::AsNumber(FMath::Max(
						0,
						FMath::CeilToInt(
							State->SecondsUntil(Return))))
				});
		}
		else if (Link.State == ESeatConnection::Empty)
		{
			const FText Waiting =
				JoA_UIText(TEXT("Play.WaitingForOpponent"));
			/* The host picks rock, paper, or
			 * scissors before the other seat
			 * exists. Both lines stay up. */
			if (!State->HasMatchStarted())
			{
				Message = JoA_UIText(
					TEXT("Play.WaitAndThrow"),
					FFormatOrderedArguments{
						Waiting,
						OpeningChoiceLine(
							State,
							Controller)
					});
			}
			else
			{
				Message = Waiting;
			}
		}
		else
		{
			Message = JoA_UIText(TEXT("Play.MatchPaused"));
		}
	}
	else if (!State->HasMatchStarted())
	{
		bOpeningThrowInfoHidden = false;
		if (World)
		{
			World->GetTimerManager().ClearTimer(
				OpeningThrowInfoHandle);
		}
		Color = FSlateColor(InfoWhiteColor);
		Message = OpeningChoiceLine(State, Controller);
	}
	else if (State->GetActiveSeat() == Seat
		&& State->IsActiveSeatBlocked())
	{
		Color = FSlateColor(InfoWarningColor);
		Message = JoA_UIText(TEXT("Play.MustSkip"));
	}
	else if (!bOpeningThrowInfoHidden
		&& State->GetOpeningWinningThrow()
			!= EOpeningThrow::None
		&& State->GetActiveSeat()
			== State->GetOpeningWinnerSeat())
	{
		Color = FSlateColor(InfoWhiteColor);
		if (State->GetOpeningWinnerSeat() == Seat)
		{
			Message =
				JoA_UIText(TEXT("Play.OpeningThrowWon"));
		}
		else
		{
			const ENation WinnerNation =
				GetNation(
					State->GetOpeningWinnerSeat());
			const FText NationText = JoA_UIText(
				WinnerNation == ENation::French
					? TEXT("Play.NationFrench")
					: TEXT("Play.NationEnglish"));
			FText ThrowText;
			switch (State->GetOpeningWinningThrow())
			{
			case EOpeningThrow::Rock:
				ThrowText =
					JoA_UIText(TEXT("Play.ThrowRock"));
				break;
			case EOpeningThrow::Paper:
				ThrowText =
					JoA_UIText(TEXT("Play.ThrowPaper"));
				break;
			case EOpeningThrow::Scissors:
				ThrowText =
					JoA_UIText(TEXT("Play.ThrowScissors"));
				break;
			default:
				break;
			}

			Message = JoA_UIText(
				TEXT("Play.OpeningThrowLost"),
				FFormatOrderedArguments{
					NationText,
					ThrowText
				});
		}

		/* The result stays two seconds, then Your
		 * turn or Opponent's turn takes over. */
		if (World
			&& !World->GetTimerManager().IsTimerActive(
				OpeningThrowInfoHandle))
		{
			World->GetTimerManager().SetTimer(
				OpeningThrowInfoHandle,
				this,
				&UJoA_W_Play::HideOpeningThrowInfo,
				2.f,
				false);
		}
	}
	else if (State->GetActiveSeat() == Seat)
	{
		bCountdown = bTimed;
		Color = FSlateColor(InfoYourTurnColor);

		/* The player must notice the clock running out.
		 * Orange from half of the limit, red from a
		 * quarter, and in red the text jumps to 150 % each
		 * second and shrinks to its size. The scale is a
		 * render transform, since a changing font size
		 * relayouts the text and makes it flicker. */
		const float Total =
			GetDefault<UJoA_MatchSettings>()->TurnSeconds;
		if (bTimed && Total > 0.f)
		{
			const double Left = FMath::Max(
				0.0,
				State->SecondsUntil(Deadline));
			const float Fraction = Left / Total;
			if (Fraction <= 0.25f)
			{
				Color = FSlateColor(InfoWarningColor);
				bPulse = true;
				FontScale = 1.f + 0.5f * FMath::Frac(Left);
			}
			else if (Fraction <= 0.5f)
			{
				Color = FSlateColor(InfoCautionColor);
			}
		}

		Message = bTimed
			? JoA_UIText(
				TEXT("Play.YourTurnTimed"),
				FFormatOrderedArguments{ TurnLeft })
			: JoA_UIText(TEXT("Play.YourTurn"));
	}
	else
	{
		bCountdown = bTimed;
		Message = bTimed
			? JoA_UIText(
				TEXT("Play.OpponentTurnTimed"),
				FFormatOrderedArguments{ TurnLeft })
			: JoA_UIText(TEXT("Play.OpponentTurn"));
	}

	if (!bShownInfoReady || !Message.EqualTo(ShownInfoText))
	{
		TB_Info->SetText(Message);
		ShownInfoText = Message;
	}
	if (!bShownInfoReady || Color != ShownInfoColor)
	{
		TB_Info->SetColorAndOpacity(Color);
		ShownInfoColor = Color;
	}
	bShownInfoReady = true;

	if (!FMath::IsNearlyEqual(ShownInfoScale, FontScale))
	{
		ShownInfoScale = FontScale;
		TB_Info->SetRenderScale(
			FVector2D(FontScale, FontScale));
	}

	/* The text changes with the clock, not with a board
	 * event, so a timer redraws it while a count shows.
	 * The pulse needs a finer step to look smooth. */
	if (World)
	{
		FTimerManager& Timers = World->GetTimerManager();
		const bool bRunning =
			Timers.IsTimerActive(CountdownHandle);
		const float Interval = bPulse ? 0.04f : 0.25f;
		if (bCountdown
			&& (!bRunning || CountdownInterval != Interval))
		{
			CountdownInterval = Interval;
			Timers.SetTimer(
				CountdownHandle,
				this,
				&UJoA_W_Play::RefreshTurnInfo,
				Interval,
				true);
		}
		else if (!bCountdown && bRunning)
		{
			Timers.ClearTimer(CountdownHandle);
		}
	}
}

void UJoA_W_Play::HideOpeningThrowInfo()
{
	bOpeningThrowInfoHidden = true;
	RefreshTurnInfo();
}

void UJoA_W_Play::HandleFigureClicked(AJoA_Figure* Figure)
{
	AJoA_PlayerController* Controller =
		Cast<AJoA_PlayerController>(GetOwningPlayer());
	if (!Controller)
	{
		return;
	}

	AJoA_Figure* Selected =
		Controller->GetSelectedFigure();
	if (IsValid(Selected) && Selected != Figure)
	{
		if (Selected->CanAttackTarget(Figure))
		{
			Controller->RequestAttack(Selected, Figure);
			return;
		}

		if (Selected->CanHealTarget(Figure))
		{
			Controller->RequestHeal(Selected, Figure);
			return;
		}
	}

	Controller->RequestSelectFigure(Figure);
}

void UJoA_W_Play::OnGoClicked()
{
	if (const AJoA_PlayerController* Controller =
		Cast<AJoA_PlayerController>(GetOwningPlayer()))
	{
		if (AJoA_Figure* Figure = Controller->GetSelectedFigure())
		{
			Figure->RequestStep(EStepDirection::Forward);
		}
	}
}
void UJoA_W_Play::OnTurnLeftClicked()
{
	if (const AJoA_PlayerController* Controller =
		Cast<AJoA_PlayerController>(GetOwningPlayer()))
	{
		if (AJoA_Figure* Figure = Controller->GetSelectedFigure())
		{
			Figure->RequestTurn(ETurnDirection::Left);
		}
	}
}

void UJoA_W_Play::OnTurnRightClicked()
{
	if (const AJoA_PlayerController* Controller =
		Cast<AJoA_PlayerController>(GetOwningPlayer()))
	{
		if (AJoA_Figure* Figure = Controller->GetSelectedFigure())
		{
			Figure->RequestTurn(ETurnDirection::Right);
		}
	}
}

void UJoA_W_Play::OnSkipTurnClicked()
{
	if (const AJoA_PlayerController* Controller =
		Cast<AJoA_PlayerController>(GetOwningPlayer()))
	{
		if (AJoA_Figure* Figure = Controller->GetSelectedFigure())
		{
			Figure->RequestSkipTurn();
		}
	}
}

void UJoA_W_Play::OnRestClicked()
{
	if (const AJoA_PlayerController* Controller =
		Cast<AJoA_PlayerController>(GetOwningPlayer()))
	{
		if (AJoA_Figure* Figure =
			Controller->GetSelectedFigure())
		{
			Figure->RequestRest();
		}
	}
}

void UJoA_W_Play::OnJumpClicked()
{
	if (const AJoA_PlayerController* Controller =
		Cast<AJoA_PlayerController>(GetOwningPlayer()))
	{
		if (AJoA_Figure* Figure =
			Controller->GetSelectedFigure())
		{
			Figure->RequestJump();
		}
	}
}

void UJoA_W_Play::OnFallClicked()
{
	if (const AJoA_PlayerController* Controller =
		Cast<AJoA_PlayerController>(GetOwningPlayer()))
	{
		if (AJoA_Figure* Figure =
			Controller->GetSelectedFigure())
		{
			Figure->RequestFall();
		}
	}
}

void UJoA_W_Play::BindJumpFallClicks()
{
	if (Btn_Jump && !bJumpClickedBound)
	{
		Btn_Jump->OnClicked.AddDynamic(
			this,
			&UJoA_W_Play::OnJumpClicked);
		bJumpClickedBound = true;
	}

	if (Btn_Fall && !bFallClickedBound)
	{
		Btn_Fall->OnClicked.AddDynamic(
			this,
			&UJoA_W_Play::OnFallClicked);
		bFallClickedBound = true;
	}
}

void UJoA_W_Play::EnsureJumpFallButtons()
{
	if (!WidgetTree)
	{
		return;
	}

	UCanvasPanel* Canvas =
		Cast<UCanvasPanel>(WidgetTree->RootWidget);
	if (!HB_JumpFall && Canvas)
	{
		UHorizontalBox* Box =
			WidgetTree->ConstructWidget<UHorizontalBox>(
				UHorizontalBox::StaticClass(),
				TEXT("HB_JumpFall"));
		Box->SetVisibility(
			ESlateVisibility::Collapsed);
		PinCanvasControl(
			Box,
			Canvas,
			FVector2D(0.0, 0.0));
		HB_JumpFall = Box;
	}

	auto MakeButton =
		[this](const FName Name, const FText& Label)
	{
		UButton* Button =
			WidgetTree->ConstructWidget<UButton>(
				UButton::StaticClass(),
				Name);
		JoA_DisableButtonFocus(Button);
		UTextBlock* Caption =
			WidgetTree->ConstructWidget<UTextBlock>();
		Caption->SetText(Label);
		Button->AddChild(Caption);
		return Button;
	};

	if (!Btn_Jump)
	{
		Btn_Jump = MakeButton(
			TEXT("Btn_Jump"),
			JoA_UIText(TEXT("Play.Jump")));
	}

	if (!Btn_Fall)
	{
		Btn_Fall = MakeButton(
			TEXT("Btn_Fall"),
			JoA_UIText(TEXT("Play.Fall")));
	}

	BindJumpFallClicks();

	if (!HB_JumpFall)
	{
		return;
	}

	auto Adopt = [this](UWidget* Child)
	{
		if (Child && Child->GetParent() != HB_JumpFall)
		{
			Child->RemoveFromParent();
			UPanelSlot* Slot =
				HB_JumpFall->AddChild(Child);
			if (UHorizontalBoxSlot* BoxSlot =
				Cast<UHorizontalBoxSlot>(Slot))
			{
				BoxSlot->SetPadding(
					FMargin(6.f, 0.f, 6.f, 0.f));
			}
		}
	};

	Adopt(Btn_Jump);
	Adopt(Btn_Fall);

	UCanvasPanel* RootCanvas =
		Cast<UCanvasPanel>(WidgetTree->RootWidget);
	PinCanvasControl(
		HB_JumpFall,
		RootCanvas,
		FVector2D(0.0, 0.0));

	KeepLayoutTick();
	PlaceJumpFallButtons();
	bJumpFallPinned = true;
}

bool UJoA_W_Play::PlaceJumpFallButtons()
{
	if (!HB_JumpFall)
	{
		return false;
	}

	UCanvasPanelSlot* CanvasSlot =
		Cast<UCanvasPanelSlot>(HB_JumpFall->Slot);
	UWidget* Neighbour = nullptr;
	if (HB_ActionButtons)
	{
		Neighbour = HB_ActionButtons.Get();
	}
	else if (Btn_TurnRight)
	{
		Neighbour = Btn_TurnRight.Get();
	}
	else
	{
		Neighbour = Btn_Go.Get();
	}

	UWidget* CanvasWidget = HB_JumpFall->GetParent();
	if (!CanvasSlot || !Neighbour || !CanvasWidget)
	{
		return false;
	}

	const FGeometry CanvasGeo =
		CanvasWidget->GetCachedGeometry();
	const FGeometry NeighbourGeo =
		Neighbour->GetCachedGeometry();
	const FVector2f CanvasSize = CanvasGeo.GetLocalSize();
	const FVector2f NeighbourSize =
		NeighbourGeo.GetLocalSize();
	if (CanvasSize.X < 1.f)
	{
		return false;
	}

	const FVector2f NeighbourLocal =
		CanvasGeo.AbsoluteToLocal(
			NeighbourGeo.GetAbsolutePosition());
	const float Gap = 8.f;
	const float FallbackHeight = 40.f;
	const float RowHeight = NeighbourSize.Y > 1.f
		? NeighbourSize.Y
		: FallbackHeight;
	const float Left = NeighbourLocal.X;
	const float Top =
		NeighbourLocal.Y + RowHeight + Gap;

	const FVector2D SlotPos(
		Left - CanvasSize.X * 0.5f,
		Top - CanvasSize.Y);
	if (!CanvasSlot->GetPosition().Equals(SlotPos, 0.5f))
	{
		CanvasSlot->SetAnchors(FAnchors(0.5f, 1.f));
		CanvasSlot->SetAlignment(FVector2D(0.0, 0.0));
		CanvasSlot->SetPosition(SlotPos);
		CanvasSlot->SetAutoSize(true);
		CanvasSlot->SetZOrder(2);
	}
	return true;
}

void UJoA_W_Play::RefreshJumpFall()
{
	EnsureJumpFallButtons();

	const AJoA_PlayerController* Controller =
		Cast<AJoA_PlayerController>(GetOwningPlayer());
	const AJoA_Figure* Figure =
		Controller ? Controller->GetSelectedFigure() : nullptr;
	const bool bValid = IsValid(Figure);
	const bool bShowJump =
		bValid && Figure->CanJump();
	const bool bShowFall =
		bValid && Figure->CanFall();

	auto Show = [](UWidget* Widget, bool bShow)
	{
		if (Widget)
		{
			Widget->SetVisibility(
				bShow
					? ESlateVisibility::Visible
					: ESlateVisibility::Collapsed);
		}
	};

	Show(Btn_Jump, bShowJump);
	Show(Btn_Fall, bShowFall);
	if (HB_JumpFall)
	{
		HB_JumpFall->SetVisibility(
			(bShowJump || bShowFall)
				? ESlateVisibility::SelfHitTestInvisible
				: ESlateVisibility::Collapsed);
	}
}

void UJoA_W_Play::EnsureKeyHints()
{
	if (VB_KeyHints || !WidgetTree)
	{
		return;
	}

	UCanvasPanel* Canvas =
		Cast<UCanvasPanel>(WidgetTree->RootWidget);
	if (!Canvas)
	{
		return;
	}

	UVerticalBox* Box =
		WidgetTree->ConstructWidget<UVerticalBox>(
			UVerticalBox::StaticClass(),
			TEXT("VB_KeyHints"));
	/* The box must not take clicks. A hit here
	 * would eat the board under the hint. */
	Box->SetVisibility(
		ESlateVisibility::HitTestInvisible);
	UCanvasPanelSlot* BoxSlot =
		Canvas->AddChildToCanvas(Box);
	BoxSlot->SetAnchors(FAnchors(0.f, 0.f));
	BoxSlot->SetAlignment(FVector2D(0.f, 1.f));
	BoxSlot->SetAutoSize(true);
	BoxSlot->SetPosition(FVector2D(50.f, 0.f));
	BoxSlot->SetZOrder(1);
	VB_KeyHints = Box;
	bKeyHintsPlaced = false;
	KeepLayoutTick();
}

void UJoA_W_Play::RefreshKeyHints()
{
	EnsureKeyHints();
	if (!VB_KeyHints || !WidgetTree)
	{
		return;
	}

	/* The menu checkbox is the only switch.
	 * Off hides the key lines for this player. */
	const UJoA_GameInstance* Instance =
		GetGameInstance<UJoA_GameInstance>();
	if (Instance && !Instance->GetShowTutorial())
	{
		ShownKeyHints.Reset();
		VB_KeyHints->ClearChildren();
		VB_KeyHints->SetVisibility(
			ESlateVisibility::Collapsed);
		return;
	}

	const AJoA_PlayerController* Controller =
		Cast<AJoA_PlayerController>(
			GetOwningPlayer());
	const AJoA_Figure* Figure = Controller
		? Controller->GetSelectedFigure()
		: nullptr;
	const bool bValid =
		IsValid(Figure) && !Figure->IsDead();

	bool bMyTurn = false;
	if (Controller)
	{
		const AJoA_GameState* State =
			AJoA_GameState::Get(this);
		const AJoA_PlayerState* OwnerState =
			Controller->GetPlayerState<
				AJoA_PlayerState>();
		if (State
			&& OwnerState
			&& State->IsPlayable())
		{
			const int32 Seat =
				OwnerState->GetNation()
					== ENation::French
				? FrenchSeat
				: EnglishSeat;
			bMyTurn =
				State->GetActiveSeat() == Seat;
		}
	}

	/* Lines are compared by key. The text is looked up
	 * once, when a text block is built. */
	TArray<FName> Lines;
	auto AddLine = [&Lines](
		bool bShow,
		const TCHAR* Key)
	{
		if (bShow)
		{
			Lines.Add(FName(Key));
		}
	};

	int32 OwnCount = 0;
	if (Controller)
	{
		TArray<AJoA_Figure*> Living;
		if (const UJoA_FigureRegistry* Registry =
			UJoA_FigureRegistry::Get(this))
		{
			Registry->CollectFigures(Living);
		}

		for (const AJoA_Figure* Owned : Living)
		{
			if (Controller->CanSelectFigure(Owned))
			{
				++OwnCount;
			}
		}
	}

	const bool bChoose = bMyTurn && OwnCount >= 2;
	AddLine(bChoose, TEXT("Play.Key.Choose"));
	AddLine(bChoose, TEXT("Play.Key.Scroll"));
	AddLine(
		bMyTurn && bValid && Figure->CanTurn(),
		TEXT("Play.Key.TurnLeft"));
	AddLine(
		bMyTurn && bValid
			&& Figure->CanStep(EStepDirection::Forward),
		TEXT("Play.Key.Forward"));
	AddLine(
		bMyTurn && bValid && Figure->CanTurn(),
		TEXT("Play.Key.TurnRight"));
	AddLine(
		bMyTurn && bValid && Figure->CanJump(),
		TEXT("Play.Key.Jump"));

	FIntPoint BuildCell = FIntPoint::ZeroValue;
	AddLine(
		bMyTurn && bValid
			&& Figure->FindBuildableCell(BuildCell),
		TEXT("Play.Key.Obstacle"));
	AddLine(
		bMyTurn && bValid && Figure->CanPickupFlag(),
		TEXT("Play.Key.PickUp"));
	AddLine(
		bMyTurn && bValid && Figure->CanDropFlag(),
		TEXT("Play.Key.PutDown"));
	AddLine(
		bMyTurn && bValid && Figure->CanRest(),
		TEXT("Play.Key.Rest"));

	if (Lines == ShownKeyHints
		&& VB_KeyHints->GetChildrenCount()
			== Lines.Num())
	{
		return;
	}

	ShownKeyHints = Lines;
	VB_KeyHints->ClearChildren();
	VB_KeyHints->SetVisibility(
		Lines.IsEmpty()
			? ESlateVisibility::Collapsed
			: ESlateVisibility::HitTestInvisible);

	for (const FName& Line : Lines)
	{
		UTextBlock* Block =
			WidgetTree->ConstructWidget<UTextBlock>();
		Block->SetText(JoA_UIText(Line));
		Block->SetJustification(ETextJustify::Left);
		Block->SetColorAndOpacity(
			FSlateColor(FLinearColor::White));
		FSlateFontInfo Font = Block->GetFont();
		Font.Size = 20;
		Block->SetFont(Font);
		Block->SetVisibility(
			ESlateVisibility::HitTestInvisible);

		UPanelSlot* LineSlot =
			VB_KeyHints->AddChild(Block);
		if (UVerticalBoxSlot* BoxSlot =
			Cast<UVerticalBoxSlot>(LineSlot))
		{
			BoxSlot->SetPadding(
				FMargin(0.f, 0.f, 0.f, 2.f));
			BoxSlot->SetHorizontalAlignment(
				HAlign_Left);
		}
	}

	/* New lines change the height. Place again
	 * so the block stays above the portraits. */
	bKeyHintsPlaced = false;
	KeepLayoutTick();
}

bool UJoA_W_Play::PlaceKeyHints()
{
	if (!WidgetTree)
	{
		return true;
	}

	if (!VB_KeyHints)
	{
		EnsureKeyHints();
	}

	if (!VB_KeyHints)
	{
		return true;
	}

	UCanvasPanel* Canvas =
		Cast<UCanvasPanel>(WidgetTree->RootWidget);
	if (!Canvas || !HB_LocalFigures)
	{
		return true;
	}

	if (VB_KeyHints->GetParent() != Canvas)
	{
		VB_KeyHints->RemoveFromParent();
		Canvas->AddChildToCanvas(VB_KeyHints);
	}

	UCanvasPanelSlot* HintSlot =
		Cast<UCanvasPanelSlot>(VB_KeyHints->Slot);
	const FGeometry CanvasGeo =
		Canvas->GetCachedGeometry();
	const FVector2f CanvasSize =
		CanvasGeo.GetLocalSize();
	if (!HintSlot || CanvasSize.X < 1.f)
	{
		return false;
	}

	float PortraitTop = CanvasSize.Y;
	bool bFound = false;
	for (const UJoA_W_CharacterStat* Stat : StatWidgets)
	{
		if (!Stat
			|| Stat->GetParent() != HB_LocalFigures)
		{
			continue;
		}

		const FGeometry StatGeo =
			Stat->GetCachedGeometry();
		if (StatGeo.GetLocalSize().Y < 1.f)
		{
			continue;
		}

		const FVector2f Local =
			CanvasGeo.AbsoluteToLocal(
				StatGeo.GetAbsolutePosition());
		PortraitTop = FMath::Min(PortraitTop, Local.Y);
		bFound = true;
	}

	if (!bFound)
	{
		return false;
	}

	if (VB_KeyHints->GetVisibility()
		== ESlateVisibility::Collapsed)
	{
		return true;
	}

	VB_KeyHints->ForceLayoutPrepass();
	const float HintHeight =
		VB_KeyHints->GetDesiredSize().Y;
	if (HintHeight < 1.f)
	{
		return false;
	}

	/* Top alignment plus the real height. A bottom
	 * pivot was ignored while the box had no size,
	 * so the lines drew down over the portraits. */
	const float Gap = 50.f;
	HintSlot->SetAnchors(FAnchors(0.f, 0.f));
	HintSlot->SetAlignment(FVector2D(0.f, 0.f));
	HintSlot->SetAutoSize(true);
	HintSlot->SetPosition(FVector2D(
		50.f,
		PortraitTop - Gap - HintHeight));
	HintSlot->SetZOrder(1);
	return true;
}
