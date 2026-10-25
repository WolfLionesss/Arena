// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/JoA_W_Menu.h"

// --- Engine ---
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/CheckBox.h"
#include "Components/EditableTextBox.h"
#include "Components/HorizontalBox.h"
#include "Components/SizeBox.h"
#include "Components/Slider.h"
#include "Components/Spacer.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/Widget.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/KismetSystemLibrary.h"

// --- GAME ---
#include "Match/JoA_GameInstance.h"
#include "UI/JoA_UIText.h"
#include "UI/JoA_W_About.h"

namespace
{
	const TCHAR* DefaultPort = TEXT("7777");
	const FName MenuColumnName(TEXT("VerticalBox_55"));
	const uint8 PageMain = 0;
	const uint8 PageSettings = 1;
	const uint8 PageAbout = 2;
	const TCHAR* AboutWidgetPath =
		TEXT("/Game/WBP/WBP_About.WBP_About_C");

	FText SeedRangeNotice()
	{
		/* Grouping would print 2,147,483,647. */
		const FNumberFormattingOptions& Plain =
			FNumberFormattingOptions::DefaultNoGrouping();
		return JoA_UIText(
			TEXT("Menu.SeedRange"),
			FFormatOrderedArguments{
				FText::AsNumber(
					UJoA_GameInstance::
						MinOpeningObstacleSeed,
					&Plain),
				FText::AsNumber(
					UJoA_GameInstance::
						MaxOpeningObstacleSeed,
					&Plain)
			});
	}
}

UJoA_W_Menu::UJoA_W_Menu(
	const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	/* The menu controller gives this widget the UI focus.
	 * A widget that is not focusable makes the engine log
	 * "Attempting to focus Non-Focusable widget". It can
	 * only be set at construction. */
	SetIsFocusable(true);
}

void UJoA_W_Menu::NativeConstruct()
{
	Super::NativeConstruct();

	Btn_NewGame->OnClicked.AddUniqueDynamic(
		this,
		&UJoA_W_Menu::OnNewGameClicked);
	Btn_JoinGame->OnClicked.AddUniqueDynamic(
		this,
		&UJoA_W_Menu::OnJoinGameClicked);
	Btn_QuitGame->OnClicked.AddUniqueDynamic(
		this,
		&UJoA_W_Menu::OnQuitGameClicked);
	if (Btn_Settings)
	{
		Btn_Settings->OnClicked.AddUniqueDynamic(
			this,
			&UJoA_W_Menu::OnSettingsClicked);
	}
	if (Btn_About)
	{
		Btn_About->OnClicked.AddUniqueDynamic(
			this,
			&UJoA_W_Menu::OnAboutClicked);
	}
	if (Btn_Back)
	{
		Btn_Back->OnClicked.AddUniqueDynamic(
			this,
			&UJoA_W_Menu::OnBackClicked);
	}
	if (Btn_Save)
	{
		Btn_Save->OnClicked.AddUniqueDynamic(
			this,
			&UJoA_W_Menu::OnSaveClicked);
	}

	MenuColumn = GetWidgetFromName(MenuColumnName);
	if (TB_Help)
	{
		TB_Help->SetText(JoA_UIText(TEXT("Menu.Help")));
		TB_Help->SetAutoWrapText(true);
	}

	if (UJoA_GameInstance* Instance =
		GetGameInstance<UJoA_GameInstance>())
	{
		if (CB_AutoPickup)
		{
			CB_AutoPickup->SetIsChecked(
				Instance->GetAutoPickupFlag());
		}
		ApplyTutorialText(Instance->GetShowTutorial());
		if (CB_SetTutorial)
		{
			CB_SetTutorial->SetIsChecked(
				Instance->GetShowTutorial());
			CB_SetTutorial->OnCheckStateChanged.
				AddUniqueDynamic(
					this,
					&UJoA_W_Menu::OnTutorialChanged);
		}
		if (Slider_Obstacles)
		{
			const int32 Pairs =
				Instance->GetOpeningObstaclePairs();
			Slider_Obstacles->SetMinValue(
				UJoA_GameInstance::MinOpeningObstaclePairs);
			Slider_Obstacles->SetMaxValue(
				UJoA_GameInstance::MaxOpeningObstaclePairs);
			Slider_Obstacles->SetStepSize(1.f);
			Slider_Obstacles->SetValue(
				static_cast<float>(Pairs));
			ApplyObstacleCountText(Pairs);
		}
		ApplySeedText(Instance->GetOpeningObstacleSeed());
	}
	if (ET_Seed)
	{
		ET_Seed->OnTextCommitted.AddUniqueDynamic(
			this,
			&UJoA_W_Menu::OnSeedCommitted);
	}
	if (Slider_Obstacles)
	{
		Slider_Obstacles->OnValueChanged.AddUniqueDynamic(
			this,
			&UJoA_W_Menu::OnObstaclePairsChanged);
	}
	BuildSoundRow();
	SetSettingsOpen(false);

	/* Designer: centred, 800 by 30, Pos Y 100.
	 * A rebuilt slot was leaving it elsewhere. */
	if (UCanvasPanelSlot* InfoSlot =
		Cast<UCanvasPanelSlot>(
			TB_Info ? TB_Info->Slot : nullptr))
	{
		InfoSlot->SetAlignment(FVector2D(0.5f, 0.5f));
		InfoSlot->SetPosition(FVector2D(0.f, 100.f));
		InfoSlot->SetSize(FVector2D(800.f, 30.f));
		InfoSlot->SetAutoSize(false);
		InfoSlot->SetZOrder(0);
	}

	/* A refused join reloads this map. Without the text
	 * here the click looks like it did nothing. */
	UJoA_GameInstance* Instance =
		GetGameInstance<UJoA_GameInstance>();
	ApplyMenuNotice();

	/* The travel notice can land after construct,
	 * on this same frame. One tick reads it. */
	bHasScriptImplementedTick = true;
	UpdateCanTick();

	if (Instance && TB_Status)
	{
		const FString Notice =
			Instance->ConsumeNetworkNotice();
		if (!Notice.IsEmpty())
		{
			TB_Status->SetText(JoA_UIText(
				TEXT("Menu.JoinFailed"),
				FFormatOrderedArguments{
					FText::FromString(Notice)
				}));
		}
	}
}

void UJoA_W_Menu::OnNewGameClicked()
{
	UJoA_GameInstance* Instance =
		GetGameInstance<UJoA_GameInstance>();
	if (!Instance)
	{
		return;
	}

	if (TB_Status)
	{
		TB_Status->SetText(
			JoA_UIText(TEXT("Menu.CreatingSession")));
	}

	/* Steam lobby first, then Arena listen, then the
	 * invite overlay. A friend Play on that invite
	 * joins. No Steam: listen on this machine. */
	Instance->HostMatch();
}

void UJoA_W_Menu::OnJoinGameClicked()
{
	APlayerController* Controller = GetOwningPlayer();
	if (!Controller)
	{
		return;
	}

	if (UJoA_GameInstance* Instance =
		GetGameInstance<UJoA_GameInstance>())
	{
		/* A won leave left the menu blank on
		 * purpose. This join may fail, and
		 * that failure has to be allowed
		 * to write its own line. */
		Instance->ReleaseMenuNoticeHold();
	}

	FString Address = ET_Address
		? ET_Address->GetText().ToString().TrimStartAndEnd()
		: FString();
	if (Address.IsEmpty())
	{
		Address = TEXT("127.0.0.1");
	}

	if (!Address.Contains(TEXT(":")))
	{
		Address += FString(TEXT(":")) + DefaultPort;
	}

	/* A refused or unreachable host comes back as a network
	 * failure, and the engine returns to this map. */
	if (TB_Status)
	{
		TB_Status->SetText(JoA_UIText(
			TEXT("Menu.Connecting"),
			FFormatOrderedArguments{
				FText::FromString(Address)
			}));
	}

	/* A player who left a held seat comes back to it with
	 * the saved token. Without it the server takes him for
	 * a third player. */
	UJoA_GameInstance* Instance =
		GetGameInstance<UJoA_GameInstance>();
	const FString Url = Instance
		? Instance->BuildJoinUrl(Address)
		: Address;
	Controller->ClientTravel(Url, TRAVEL_Absolute);
}

void UJoA_W_Menu::NativeTick(
	const FGeometry& MyGeometry,
	float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	ApplyMenuNotice();
	bHasScriptImplementedTick = false;
	UpdateCanTick();
}

void UJoA_W_Menu::ApplyMenuNotice()
{
	if (!TB_Info || bShowingSeedNotice)
	{
		return;
	}

	const UJoA_GameInstance* Instance =
		GetGameInstance<UJoA_GameInstance>();
	const FName NoticeKey = Instance
		? Instance->GetMenuNotice()
		: FName(NAME_None);
	const bool bHasNotice = !NoticeKey.IsNone();
	TB_Info->SetText(
		bHasNotice
			? JoA_UIText(NoticeKey)
			: FText::GetEmpty());
	TB_Info->SetVisibility(
		bHasNotice
			? ESlateVisibility::HitTestInvisible
			: ESlateVisibility::Collapsed);
}

void UJoA_W_Menu::OnSettingsClicked()
{
	SetSettingsOpen(true);
}

void UJoA_W_Menu::OnAboutClicked()
{
	ShowPage(PageAbout);
}

void UJoA_W_Menu::OnTutorialChanged(bool bIsChecked)
{
	ApplyTutorialText(bIsChecked);
	if (UJoA_GameInstance* Instance =
		GetGameInstance<UJoA_GameInstance>())
	{
		Instance->SetShowTutorial(bIsChecked);
	}
}

void UJoA_W_Menu::ApplyTutorialText(bool bOn)
{
	if (!TB_SetTutorial)
	{
		return;
	}

	TB_SetTutorial->SetText(JoA_UIText(
		bOn
			? TEXT("Menu.TutorialOn")
			: TEXT("Menu.TutorialOff")));
}

void UJoA_W_Menu::OnBackClicked()
{
	LoadControlsFromSaved();
	SetSettingsOpen(false);
}

void UJoA_W_Menu::OnSaveClicked()
{
	/* The controls only hold a draft. The game
	 * instance writes the settings file, so
	 * nothing is stored before this click. */
	int32 Seed = 0;
	if (ET_Seed && !TryReadSeed(
		ET_Seed->GetText().ToString(),
		Seed))
	{
		ShowSeedNotice(true);
		return;
	}

	if (UJoA_GameInstance* Instance =
		GetGameInstance<UJoA_GameInstance>())
	{
		if (CB_AutoPickup)
		{
			Instance->SetAutoPickupFlag(
				CB_AutoPickup->IsChecked());
		}
		if (CB_SetTutorial)
		{
			Instance->SetShowTutorial(
				CB_SetTutorial->IsChecked());
		}
		if (ET_Seed)
		{
			Instance->SetOpeningObstacleSeed(Seed);
			ShowSeedNotice(false);
		}
		if (Slider_Obstacles)
		{
			Instance->SetOpeningObstaclePairs(
				FMath::RoundToInt(
					Slider_Obstacles->GetValue()));
		}
		if (Slider_Sound)
		{
			Instance->SetSoundVolume(
				FMath::RoundToInt(
					Slider_Sound->GetValue()));
		}
	}
	SetSettingsOpen(false);
}

void UJoA_W_Menu::LoadControlsFromSaved()
{
	const UJoA_GameInstance* Instance =
		GetGameInstance<UJoA_GameInstance>();
	if (!Instance)
	{
		return;
	}

	if (CB_AutoPickup)
	{
		CB_AutoPickup->SetIsChecked(
			Instance->GetAutoPickupFlag());
	}
	ApplyTutorialText(Instance->GetShowTutorial());
	if (CB_SetTutorial)
	{
		CB_SetTutorial->SetIsChecked(
			Instance->GetShowTutorial());
	}
	if (Slider_Obstacles)
	{
		const int32 Pairs =
			Instance->GetOpeningObstaclePairs();
		Slider_Obstacles->SetValue(
			static_cast<float>(Pairs));
		ApplyObstacleCountText(Pairs);
	}
	ApplySeedText(Instance->GetOpeningObstacleSeed());
	ShowSeedNotice(false);
	if (Slider_Sound)
	{
		const int32 Volume = Instance->GetSoundVolume();
		Slider_Sound->SetValue(
			static_cast<float>(Volume));
		ApplySoundVolumeText(Volume);
	}
}

void UJoA_W_Menu::OnObstaclePairsChanged(float Value)
{
	const int32 Pairs = FMath::Clamp(
		FMath::RoundToInt(Value),
		UJoA_GameInstance::MinOpeningObstaclePairs,
		UJoA_GameInstance::MaxOpeningObstaclePairs);
	if (Slider_Obstacles)
	{
		const float Snapped = static_cast<float>(Pairs);
		if (!FMath::IsNearlyEqual(
			Slider_Obstacles->GetValue(),
			Snapped))
		{
			Slider_Obstacles->SetValue(Snapped);
		}
	}
	ApplyObstacleCountText(Pairs);
}

void UJoA_W_Menu::OnSeedCommitted(
	const FText& Text,
	ETextCommit::Type CommitMethod)
{
	if (CommitMethod == ETextCommit::OnCleared)
	{
		return;
	}

	int32 Seed = 0;
	ShowSeedNotice(!TryReadSeed(Text.ToString(), Seed));
}

bool UJoA_W_Menu::TryReadSeed(
	const FString& Raw,
	int32& OutSeed) const
{
	const FString Text = Raw.TrimStartAndEnd();
	if (Text.IsEmpty() || Text.Len() > 10)
	{
		return false;
	}

	for (const TCHAR Char : Text)
	{
		if (!FChar::IsDigit(Char))
		{
			return false;
		}
	}

	const int64 Value = FCString::Atoi64(*Text);
	if (Value < UJoA_GameInstance::MinOpeningObstacleSeed
		|| Value >
			UJoA_GameInstance::MaxOpeningObstacleSeed)
	{
		return false;
	}

	OutSeed = static_cast<int32>(Value);
	return true;
}

void UJoA_W_Menu::ShowSeedNotice(bool bInvalid)
{
	bShowingSeedNotice = bInvalid;
	if (!TB_Info)
	{
		return;
	}

	if (!bInvalid)
	{
		ApplyMenuNotice();
		return;
	}

	TB_Info->SetText(SeedRangeNotice());
	TB_Info->SetVisibility(
		ESlateVisibility::HitTestInvisible);
}

void UJoA_W_Menu::ApplySeedText(int32 Seed)
{
	if (!ET_Seed)
	{
		return;
	}

	const FText Shown =
		FText::FromString(FString::FromInt(Seed));
	if (!ET_Seed->GetText().EqualTo(Shown))
	{
		ET_Seed->SetText(Shown);
	}
}

void UJoA_W_Menu::OnSoundVolumeChanged(float Value)
{
	const int32 Volume = FMath::Clamp(
		FMath::RoundToInt(Value),
		0,
		100);
	if (Slider_Sound)
	{
		const float Snapped =
			static_cast<float>(Volume);
		if (!FMath::IsNearlyEqual(
			Slider_Sound->GetValue(),
			Snapped))
		{
			Slider_Sound->SetValue(Snapped);
		}
	}
	ApplySoundVolumeText(Volume);
	if (UJoA_GameInstance* Instance =
		GetGameInstance<UJoA_GameInstance>())
	{
		Instance->SetSoundVolume(Volume);
	}
}

void UJoA_W_Menu::ApplySoundVolumeText(int32 Volume)
{
	if (TB_SoundVolume)
	{
		TB_SoundVolume->SetText(FText::FromString(
			FString::Printf(TEXT("%d%%"), Volume)));
	}
}

void UJoA_W_Menu::BuildSoundRow()
{
	if (!WidgetTree || GetWidgetFromName(TEXT("HB_Sound")))
	{
		return;
	}

	UVerticalBox* Column =
		Cast<UVerticalBox>(VB_Settings);
	UWidget* Back = Btn_Back;
	if (!Back)
	{
		Back = GetWidgetFromName(TEXT("Btn_Back"));
	}

	/* Back sits in a horizontal box beside Save. The
	 * whole box is the last row of the column, so it is
	 * the box that moves, not the button. */
	while (Back && Back->GetParent() != Column)
	{
		Back = Back->GetParent();
	}
	if (!Column || !Back)
	{
		return;
	}

	/* The column is already on screen. Inserting
	 * only reorders the slot list. Slate keeps
	 * the new row last, under Back. */
	UVerticalBoxSlot* OldBack =
		Cast<UVerticalBoxSlot>(Back->Slot);
	const FMargin BackPadding = OldBack
		? OldBack->GetPadding()
		: FMargin();
	const EHorizontalAlignment BackHAlign = OldBack
		? OldBack->GetHorizontalAlignment()
		: HAlign_Fill;
	const EVerticalAlignment BackVAlign = OldBack
		? OldBack->GetVerticalAlignment()
		: VAlign_Fill;
	const FSlateChildSize BackSize = OldBack
		? OldBack->GetSize()
		: FSlateChildSize();
	Back->RemoveFromParent();

	UHorizontalBox* Row =
		WidgetTree->ConstructWidget<UHorizontalBox>(
			UHorizontalBox::StaticClass(),
			TEXT("HB_Sound"));
	UTextBlock* Label =
		WidgetTree->ConstructWidget<UTextBlock>(
			UTextBlock::StaticClass(),
			TEXT("TB_Sound"));
	Label->SetText(JoA_UIText(TEXT("Menu.Sound")));
	USpacer* GapBefore =
		WidgetTree->ConstructWidget<USpacer>(
			USpacer::StaticClass(),
			TEXT("SP_SoundBefore"));
	GapBefore->SetSize(FVector2D(15.f, 1.f));
	Slider_Sound =
		WidgetTree->ConstructWidget<USlider>(
			USlider::StaticClass(),
			TEXT("Slider_Sound"));
	USpacer* GapAfter =
		WidgetTree->ConstructWidget<USpacer>(
			USpacer::StaticClass(),
			TEXT("SP_SoundAfter"));
	GapAfter->SetSize(FVector2D(15.f, 1.f));
	TB_SoundVolume =
		WidgetTree->ConstructWidget<UTextBlock>(
			UTextBlock::StaticClass(),
			TEXT("TB_SoundVolume"));

	if (TB_ObstacleCount)
	{
		const FSlateFontInfo Font =
			TB_ObstacleCount->GetFont();
		Label->SetFont(Font);
		TB_SoundVolume->SetFont(Font);
		Label->SetColorAndOpacity(
			TB_ObstacleCount->GetColorAndOpacity());
		TB_SoundVolume->SetColorAndOpacity(
			TB_ObstacleCount->GetColorAndOpacity());
	}

	int32 Volume = 100;
	if (const UJoA_GameInstance* Instance =
		GetGameInstance<UJoA_GameInstance>())
	{
		Volume = Instance->GetSoundVolume();
	}
	Slider_Sound->SetMinValue(0.f);
	Slider_Sound->SetMaxValue(100.f);
	Slider_Sound->SetStepSize(1.f);
	Slider_Sound->SetValue(
		static_cast<float>(Volume));
	ApplySoundVolumeText(Volume);
	Slider_Sound->OnValueChanged.AddUniqueDynamic(
		this,
		&UJoA_W_Menu::OnSoundVolumeChanged);

	/* The settings column sizes to its
	 * children. A fill slot would be zero
	 * wide, so the slider gets a fixed box. */
	USizeBox* SoundSize =
		WidgetTree->ConstructWidget<USizeBox>(
			USizeBox::StaticClass(),
			TEXT("SZ_Sound"));
	SoundSize->SetWidthOverride(200.f);
	SoundSize->SetHeightOverride(24.f);
	SoundSize->AddChild(Slider_Sound);

	Row->AddChildToHorizontalBox(Label);
	Row->AddChildToHorizontalBox(GapBefore);
	Row->AddChildToHorizontalBox(SoundSize);
	Row->AddChildToHorizontalBox(GapAfter);
	Row->AddChildToHorizontalBox(TB_SoundVolume);

	if (UVerticalBoxSlot* RowSlot =
		Column->AddChildToVerticalBox(Row))
	{
		RowSlot->SetPadding(FMargin(0.f, 8.f));
		RowSlot->SetHorizontalAlignment(HAlign_Fill);
	}

	USpacer* GapBelow =
		WidgetTree->ConstructWidget<USpacer>(
			USpacer::StaticClass(),
			TEXT("SP_SoundBelow"));
	GapBelow->SetSize(FVector2D(1.f, 30.f));
	Column->AddChildToVerticalBox(GapBelow);
	if (UVerticalBoxSlot* NewBack =
		Column->AddChildToVerticalBox(Back))
	{
		NewBack->SetPadding(BackPadding);
		NewBack->SetHorizontalAlignment(BackHAlign);
		NewBack->SetVerticalAlignment(BackVAlign);
		NewBack->SetSize(BackSize);
	}
}

void UJoA_W_Menu::SetSettingsOpen(bool bOpen)
{
	ShowPage(bOpen ? PageSettings : PageMain);
}

void UJoA_W_Menu::ShowPage(uint8 Page)
{
	if (Page == PageAbout)
	{
		EnsureAbout();
		if (!AboutPage)
		{
			Page = PageMain;
		}
	}

	if (MenuColumn)
	{
		MenuColumn->SetVisibility(
			Page == PageMain
				? ESlateVisibility::SelfHitTestInvisible
				: ESlateVisibility::Collapsed);
	}
	if (VB_Settings)
	{
		VB_Settings->SetVisibility(
			Page == PageSettings
				? ESlateVisibility::SelfHitTestInvisible
				: ESlateVisibility::Collapsed);
	}
	if (AboutPage)
	{
		AboutPage->SetVisibility(
			Page == PageAbout
				? ESlateVisibility::SelfHitTestInvisible
				: ESlateVisibility::Collapsed);
	}
}

void UJoA_W_Menu::EnsureAbout()
{
	if (AboutPage || !WidgetTree)
	{
		return;
	}

	UClass* AboutClass = LoadClass<UJoA_W_About>(
		nullptr,
		AboutWidgetPath);
	UCanvasPanel* Canvas =
		Cast<UCanvasPanel>(WidgetTree->RootWidget);
	if (!AboutClass || !Canvas)
	{
		return;
	}

	AboutPage = CreateWidget<UJoA_W_About>(
		this,
		AboutClass);
	if (!AboutPage)
	{
		return;
	}

	AboutPage->OnBackToMenu.BindUObject(
		this,
		&UJoA_W_Menu::OnBackClicked);
	UCanvasPanelSlot* AboutSlot =
		Canvas->AddChildToCanvas(AboutPage);
	if (AboutSlot)
	{
		/* Top edge, centered. A middle anchor
		 * pushed the buttons off the left. */
		AboutSlot->SetAnchors(FAnchors(0.5f, 0.f));
		AboutSlot->SetAlignment(FVector2D(0.5f, 0.f));
		AboutSlot->SetAutoSize(true);
		AboutSlot->SetPosition(FVector2D(0.f, 100.f));
		AboutSlot->SetZOrder(1);
	}
	AboutPage->SetVisibility(
		ESlateVisibility::Collapsed);
}

void UJoA_W_Menu::ApplyObstacleCountText(int32 Pairs)
{
	if (TB_ObstacleCount)
	{
		TB_ObstacleCount->SetText(FText::AsNumber(Pairs));
	}
}

void UJoA_W_Menu::OnQuitGameClicked()
{
	UKismetSystemLibrary::QuitGame(
		this,
		GetOwningPlayer(),
		EQuitPreference::Quit,
		false);
}
