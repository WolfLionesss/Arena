// Copyright Epic Games, Inc. All Rights Reserved.

#include "Player/JoA_MenuPlayerController.h"

// --- Engine ---
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "Framework/Application/SlateApplication.h"
#include "TimerManager.h"

// --- GAME ---
#include "Match/JoA_GameInstance.h"
#include "UI/JoA_W_Menu.h"

AJoA_MenuPlayerController::AJoA_MenuPlayerController()
{
	/* No pawn on this map. FailedToSpawnPawn
	 * marks the controller Inactive, and
	 * SetPawn(null) would snap the view back
	 * to the origin, inside the tavern floor. */
	bAutoManageActiveCameraTarget = false;
}

void AJoA_MenuPlayerController::BeginPlay()
{
	Super::BeginPlay();

	if (!IsLocalController())
	{
		return;
	}

	if (UJoA_GameInstance* Instance =
		GetGameInstance<UJoA_GameInstance>())
	{
		Instance->PullSavedSoundVolume();
	}

	ApplyMenuCamera();

	if (!MenuWidget)
	{
		TSubclassOf<UJoA_W_Menu> WidgetClass =
			MenuWidgetClass;
		if (!WidgetClass)
		{
			WidgetClass = LoadClass<UJoA_W_Menu>(
				nullptr,
				TEXT(
					"/Game/WBP/WBP_Menu.WBP_Menu_C"));
		}

		if (!WidgetClass)
		{
			UE_LOG(
				LogTemp,
				Warning,
				TEXT("WBP_Menu is missing or its ")
				TEXT("parent is not UJoA_W_Menu. ")
				TEXT("Reparent it."));
		}
		else
		{
			MenuWidget = CreateWidget<UJoA_W_Menu>(
				this,
				WidgetClass);
			if (MenuWidget)
			{
				MenuWidget->AddToViewport();
			}
		}
	}

	bShowMouseCursor = true;
	ApplyMenuInput();
	GetWorldTimerManager().SetTimer(
		InputTimer,
		this,
		&AJoA_MenuPlayerController::ApplyMenuInput,
		0.3f,
		false);
}

void AJoA_MenuPlayerController::EndPlay(
	const EEndPlayReason::Type Reason)
{
	GetWorldTimerManager().ClearTimer(InputTimer);
	Super::EndPlay(Reason);
}

void AJoA_MenuPlayerController::ApplyMenuCamera()
{
	UWorld* World = GetWorld();
	if (!World || !IsLocalController())
	{
		return;
	}

	/* Stands in the room. The hearth is on the
	 * left, the window and the moon ahead. */
	const FVector ViewLocation(-880.f, 1840.f, 867.f);
	const FRotator ViewRotation(0.f, -78.f, 0.f);

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride =
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ACameraActor* MenuCamera =
		World->SpawnActor<ACameraActor>(
			ACameraActor::StaticClass(),
			ViewLocation,
			ViewRotation,
			Params);
	if (!MenuCamera)
	{
		return;
	}

	if (UCameraComponent* Cam =
		MenuCamera->GetCameraComponent())
	{
		Cam->bUsePawnControlRotation = false;
		Cam->SetFieldOfView(90.f);
	}

	SetViewTarget(MenuCamera);
}

void AJoA_MenuPlayerController::ApplyMenuInput()
{
	FInputModeUIOnly Mode;
	Mode.SetLockMouseToViewportBehavior(
		EMouseLockMode::DoNotLock);
	if (MenuWidget && MenuWidget->IsFocusable())
	{
		Mode.SetWidgetToFocus(MenuWidget->TakeWidget());
	}
	SetInputMode(Mode);

	UWorld* World = GetWorld();
	UGameViewportClient* Viewport = World
		? World->GetGameViewport()
		: nullptr;
	if (Viewport
		&& Viewport->GetGameViewportWidget().IsValid()
		&& FSlateApplication::IsInitialized())
	{
		FSlateApplication::Get().ActivateGameViewport();
	}
}
