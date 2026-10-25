// Copyright Epic Games, Inc. All Rights Reserved.

#include "Player/Units/JoA_Figure.h"

// --- Engine ---
#include "Animation/AnimSequence.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "TimerManager.h"

// --- GAME ---
#include "Board/JoA_Flag.h"
#include "Match/JoA_CookedContent.h"
#include "Match/JoA_GameInstance.h"
#include "Player/Units/JoA_FigureHelpers.h"

// --- Components ---
#include "Components/SkeletalMeshComponent.h"

// --- Net ---
#include "Net/Core/PushModel/PushModel.h"

void AJoA_Figure::MulticastBodyAnim_Implementation(
	EFigureAnim Anim)
{
	/* A flag carrier never cheers, claps, or cries.
	 * The server must not replicate that clip, or
	 * every client would play it on the carrier. */
	if (HeldFlag
		&& (Anim == EFigureAnim::Clap
			|| Anim == EFigureAnim::Cheer
			|| Anim == EFigureAnim::Cry))
	{
		bReactedThisTurn = true;
		return;
	}

	if (HasAuthority())
	{
		NetBodyAnim = Anim;
		++BodyCue;
		MARK_PROPERTY_DIRTY_FROM_NAME(
			AJoA_Figure,
			NetBodyAnim,
			this);
		MARK_PROPERTY_DIRTY_FROM_NAME(
			AJoA_Figure,
			BodyCue,
			this);
		ForceNetUpdate();
	}

	/* A held pickup walks after the clip.
	 * Run is that walk, with the step cue. */
	if (Anim == EFigureAnim::Run
		&& bDeferStepArrival)
	{
		BeginArrivalWalk();
		return;
	}

	/* A real step multicasts Run.
	 * The client hears it here too. */
	if (Anim == EFigureAnim::Run)
	{
		bArmedStepRun = true;
	}
	PlayBodyAnim(Anim);
	bArmedStepRun = false;
}

void AJoA_Figure::BeginArrivalWalk()
{
	bDeferStepArrival = false;
	/* The pickup cue is still the replicated
	 * clip until Run arrives. A refresh would
	 * put the figure back into the bend. */
	NetBodyAnim = EFigureAnim::Run;
	if (PlayingBodyAnim != EFigureAnim::Run)
	{
		/* A predicted step may have spent
		 * the cue. This walk still sounds. */
		bMoveCuePlayed = false;
		bArmedStepRun = true;
		PlayBodyAnim(EFigureAnim::Run);
		bArmedStepRun = false;
	}
	ApplyBoardTransform();
}

void AJoA_Figure::PlayOrMulticastBodyAnim(EFigureAnim Anim)
{
	if (HasAuthority())
	{
		MulticastBodyAnim(Anim);
		return;
	}

	PlayBodyAnim(Anim);
}

void AJoA_Figure::OnRep_BodyCue()
{
	if (NetBodyAnim == EFigureAnim::Run
		&& bDeferStepArrival)
	{
		BeginArrivalWalk();
		return;
	}

	if (PlayingBodyAnim == NetBodyAnim)
	{
		return;
	}

	if (NetBodyAnim == EFigureAnim::Run)
	{
		bArmedStepRun = true;
	}
	PlayBodyAnim(NetBodyAnim);
	bArmedStepRun = false;
}

void AJoA_Figure::KeepNetBodyAnim()
{
	if (HasAuthority()
		|| PlayingBodyAnim != EFigureAnim::Run
		|| NetBodyAnim == EFigureAnim::Run
		|| NetBodyAnim == EFigureAnim::Idle)
	{
		return;
	}

	PlayBodyAnim(NetBodyAnim);
}

void AJoA_Figure::ResumeIdleAnim()
{
	/* The walk starts when pickup ends.
	 * Rest here would cancel that Run. */
	if (bDeferStepArrival)
	{
		bHasFollowUp = false;
		bHoldBodyAnim = false;
		return;
	}

	if (bHasFollowUp && !bIsDead)
	{
		const EFigureAnim Next = FollowUpAnim;
		bHasFollowUp = false;
		bHoldBodyAnim = false;
		/* Local follow-up never reached the
		 * client. The multicast does. */
		PlayOrMulticastBodyAnim(Next);
		return;
	}

	bHasFollowUp = false;

	bHoldBodyAnim = false;

	if (bIsDead)
	{
		if (PlayingBodyAnim == EFigureAnim::Death)
		{
			/* The edge used to open on Wave.
			 * That clip loops once the intro
			 * flag drops, so the body stayed
			 * in the cheer. Idle stands. */
			PlaceOnBench();
			bBenchIntro = false;
			BenchPhase = EBenchPhase::Idle;
			PlayBodyAnim(EFigureAnim::Idle, true);
			return;
		}

		if (bBenchIntro
			&& PlayingBodyAnim == EFigureAnim::Wave)
		{
			bBenchIntro = false;
			BenchPhase = EBenchPhase::Idle;
			PlayBodyAnim(EFigureAnim::Idle, true);
			return;
		}

		if (PlayingBodyAnim == EFigureAnim::Sit)
		{
			PlayBodyAnim(EFigureAnim::Sit, true);
			return;
		}

		if (PlayingBodyAnim == EFigureAnim::Lie)
		{
			PlayBodyAnim(EFigureAnim::Lie, true);
			return;
		}

		if (PlayingBodyAnim == EFigureAnim::Clap
			|| PlayingBodyAnim == EFigureAnim::Cheer
			|| PlayingBodyAnim == EFigureAnim::Cry)
		{
			/* News can replace the death clip.
			 * The first pose on the edge is
			 * idle. A later cheer still rests. */
			if (bBenchIntro)
			{
				PlaceOnBench();
				bBenchIntro = false;
				BenchPhase = EBenchPhase::Idle;
				PlayBodyAnim(
					EFigureAnim::Idle,
					true);
				return;
			}

			if (HasAuthority())
			{
				PlayBenchStart();
			}
			else
			{
				PlayBodyAnim(EFigureAnim::Idle, true);
			}
			return;
		}

		return;
	}

	if (PlayingBodyAnim == EFigureAnim::Sit)
	{
		PlayBodyAnim(EFigureAnim::Sit, true);
		return;
	}

	if (PlayingBodyAnim == EFigureAnim::Lie)
	{
		PlayBodyAnim(EFigureAnim::Lie, true);
		return;
	}

	/* A dodge, hit, clap, cheer, or cry must
	 * not sit again. Unused rest would put
	 * them back on the floor. */
	if (PlayingBodyAnim == EFigureAnim::Dodge
		|| PlayingBodyAnim == EFigureAnim::Hit
		|| PlayingBodyAnim == EFigureAnim::Clap
		|| PlayingBodyAnim == EFigureAnim::Cheer
		|| PlayingBodyAnim == EFigureAnim::Cry)
	{
		PlayBodyAnim(EFigureAnim::Idle, true);
		return;
	}

	if (HasAuthority())
	{
		PlayRestFromUnused();
	}
}

void AJoA_Figure::PlayActionCue(EFigureAnim Anim)
{
	const TCHAR* Path = nullptr;
	bool bMoveCue = false;
	switch (Anim)
	{
	case EFigureAnim::Heal:
		Path = TEXT(
			"/Game/Audio/SFX/SFX_Spell.SFX_Spell");
		break;
	case EFigureAnim::Attack:
		/* Knight and barbarian. The same
		 * clip lowers an obstacle. */
		if (UnitType != EUnitType::HeavyKnight
			&& UnitType != EUnitType::LightInfantry)
		{
			return;
		}
		Path = TEXT(
			"/Game/Audio/SFX/SFX_Slash.SFX_Slash");
		break;
	case EFigureAnim::Shoot:
		Path = TEXT(
			"/Game/Audio/SFX/SFX_Arrow.SFX_Arrow");
		break;
	case EFigureAnim::Run:
		bMoveCue = true;
		Path = TEXT(
			"/Game/Audio/SFX/SFX_Step.SFX_Step");
		break;
	case EFigureAnim::Hit:
		Path = TEXT(
			"/Game/Audio/SFX/SFX_Hurt.SFX_Hurt");
		break;
	case EFigureAnim::Death:
		Path = TEXT(
			"/Game/Audio/SFX/SFX_Death.SFX_Death");
		break;
	case EFigureAnim::Cheer:
	case EFigureAnim::Clap:
		/* A hit claps. A miss cheers the
		 * other seat. Both use one clip. */
		Path = TEXT(
			"/Game/Audio/SFX/SFX_Cheer.SFX_Cheer");
		break;
	case EFigureAnim::Fall:
	case EFigureAnim::Jump:
		bMoveCue = true;
		Path = TEXT(
			"/Game/Audio/SFX/SFX_Jump.SFX_Jump");
		break;
	case EFigureAnim::Interact:
		Path = TEXT(
			"/Game/Audio/SFX/SFX_Place.SFX_Place");
		break;
	default:
		return;
	}

	if (bMoveCue && bMoveCuePlayed
		&& !HasAuthority())
	{
		/* The client predicted this cue.
		 * A stuck step flag must not mute
		 * a jump on the host. */
		return;
	}

	USoundBase* Sound =
		UJoA_CookedContent::LoadSound(this, Path);
	if (!Sound)
	{
		return;
	}

	/* The device carries the slider. Push it
	 * again so a mute does not stick at 100%.
	 * The voice itself stays at 1. */
	if (UJoA_GameInstance* Instance =
		GetGameInstance<UJoA_GameInstance>())
	{
		Instance->ApplySoundVolume();
		if (Instance->GetSoundVolume() <= 0)
		{
			return;
		}
	}

	if (bMoveCue)
	{
		bMoveCuePlayed = true;
	}

	UGameplayStatics::SpawnSound2D(
		this,
		Sound,
		1.f,
		1.f,
		0.f,
		nullptr,
		false,
		true);
}

void AJoA_Figure::PlayBodyAnim(EFigureAnim Anim)
{
	const bool bLoop =
		Anim == EFigureAnim::Idle
		|| Anim == EFigureAnim::Run
		|| (Anim == EFigureAnim::Wave
			&& bIsDead
			&& !bBenchIntro);
	PlayBodyAnim(Anim, bLoop);
}

void AJoA_Figure::PlayBodyAnim(
	EFigureAnim Anim,
	bool bLoop)
{
	/* Local plays reach here without a multicast.
	 * The carrier still must not react. */
	if (HeldFlag
		&& (Anim == EFigureAnim::Clap
			|| Anim == EFigureAnim::Cheer
			|| Anim == EFigureAnim::Cry))
	{
		return;
	}

	/* Run is a square step. A hit used to
	 * start it on the server and the client. */
	if (Anim == EFigureAnim::Run
		&& !bArmedStepRun)
	{
		return;
	}

	/* Cry and cheer stay on the square.
	 * A refresh slide must not carry them. */
	if (JoAFigure::IsReactionAnim(Anim)
		&& Anim != EFigureAnim::Jump
		&& Anim != EFigureAnim::Fall)
	{
		const bool bStepSlide = bSliding
			&& PlayingBodyAnim != EFigureAnim::Jump
			&& PlayingBodyAnim != EFigureAnim::Fall;
		/* Auto pickup holds the step. The clip
		 * stays on the square just left. */
		const bool bArrivalPickup =
			Anim == EFigureAnim::PickUp
			&& (bDeferStepArrival || bStepSlide);
		if (bArrivalPickup)
		{
			RewindArrivalSlide();
			bDeferStepArrival = true;
		}
		else if (bSliding && !bAwaitingConfirm)
		{
			/* A fall from height 2 ends in a cry. The
			 * host lands first, so on the client the
			 * cry arrives mid-drop. Dropping the slide
			 * left the figure hanging in the air.
			 * Finish it on the cell instead. */
			SetActorLocation(SlideTo);
			if (HeldFlag)
			{
				HeldFlag->AttachToHolder(this);
			}
			bSliding = false;
		}
		else
		{
			bSliding = false;
		}
	}

	PlayingBodyAnim = Anim;
	/* Hold blocks Run for this clip. Do not
	 * tick a replay. That restarts frame 0
	 * every frame, so the clip never moves. */
	bHoldBodyAnim = !bLoop
		&& Anim != EFigureAnim::Run;
	if (Anim == EFigureAnim::Clap
		|| Anim == EFigureAnim::Cheer
		|| Anim == EFigureAnim::Cry)
	{
		bReactedThisTurn = true;
		if (!bIsDead)
		{
			UnusedOwnerTurns = 0;
		}
	}

	RefreshHeldItemPresentation();

	if (!Mesh
		|| GetNetMode() == NM_DedicatedServer)
	{
		bHoldBodyAnim = false;
		return;
	}

	UWorld* World = GetWorld();
	if (!World || !World->IsGameWorld())
	{
		return;
	}

	World->GetTimerManager().ClearTimer(
		BodyAnimHandle);

	UAnimSequence* Seq = FindBodyAnim(Anim, bLoop);
	if (!Seq)
	{
		bHoldBodyAnim = false;
		return;
	}

	Mesh->SetAnimationMode(
		EAnimationMode::AnimationSingleNode);
	Mesh->SetDisablePostProcessBlueprint(true);
	Mesh->PlayAnimation(Seq, bLoop);
	/* Sit and lie down clips are short. A 1x
	 * play looks like the pose spawned. */
	float Rate = 1.f;
	if (!bLoop
		&& (Anim == EFigureAnim::Sit
			|| Anim == EFigureAnim::Lie))
	{
		Rate = 0.55f;
	}
	Mesh->SetPlayRate(Rate);
	PlayActionCue(Anim);
	if (bLoop)
	{
		return;
	}

	const float Seconds = Seq->GetPlayLength()
		/ FMath::Max(0.1f, Rate);
	World->GetTimerManager().SetTimer(
		BodyAnimHandle,
		this,
		&AJoA_Figure::ResumeIdleAnim,
		FMath::Max(0.05f, Seconds),
		false);
}

UAnimSequence* AJoA_Figure::FindBodyAnim(
	EFigureAnim Anim,
	bool bLoopPose) const
{
	const TCHAR* Name = nullptr;
	switch (Anim)
	{
	case EFigureAnim::Idle:
		/* Flag carry uses the mage stance.
		 * Bow aim and unarmed idle keep a
		 * weapon grip the flag replaced. */
		if (HeldFlag)
		{
			Name = TEXT(
				"Rig_Medium_GeneralIdle_A");
			break;
		}
		switch (UnitType)
		{
		case EUnitType::Archer:
			Name = TEXT(
				"Rig_Medium_CombatRangedRanged_"
				"Bow_Aiming_Idle");
			break;
		case EUnitType::Medic:
			Name = TEXT(
				"Rig_Medium_GeneralIdle_A");
			break;
		/* 2H idle parks both hands on one
		 * grip. Sword and shield need 1H. */
		case EUnitType::LightInfantry:
		case EUnitType::HeavyKnight:
			Name = TEXT(
				"Rig_Medium_CombatMeleeMelee_"
				"Unarmed_Idle");
			break;
		default:
			Name = TEXT(
				"Rig_Medium_CombatMeleeMelee_2H_Idle");
			break;
		}
		break;
	case EFigureAnim::Run:
		Name = TEXT(
			"Rig_Medium_MovementBasicRunning_A");
		break;
	case EFigureAnim::Attack:
		Name = UnitType == EUnitType::LightInfantry
			? TEXT(
				"Rig_Medium_CombatMeleeMelee_"
				"1H_Attack_Chop")
			: TEXT(
				"Rig_Medium_CombatMeleeMelee_"
				"1H_Attack_Slice_Horizontal");
		break;
	case EFigureAnim::Heal:
		Name = TEXT(
			"Rig_Medium_CombatRangedRanged_"
			"Magic_Raise");
		break;
	case EFigureAnim::Shoot:
		Name = TEXT(
			"Rig_Medium_CombatRangedRanged_"
			"Bow_Release");
		break;
	case EFigureAnim::Jump:
		Name = TEXT(
			"Rig_Medium_MovementBasicJump_"
			"Full_Short");
		break;
	case EFigureAnim::Fall:
		Name = TEXT(
			"Rig_Medium_MovementBasicJump_Land");
		break;
	case EFigureAnim::Hit:
		Name = TEXT(
			"Rig_Medium_GeneralHit_A");
		break;
	case EFigureAnim::Dodge:
		Name = TEXT(
			"Rig_Medium_MovementAdvancedDodge_"
			"Forward");
		break;
	case EFigureAnim::Death:
		Name = TEXT(
			"Rig_Medium_GeneralDeath_A");
		break;
	case EFigureAnim::Sit:
		Name = bLoopPose
			? TEXT(
				"Rig_Medium_SimulationSit_Floor_Idle")
			: TEXT(
				"Rig_Medium_SimulationSit_Floor_Down");
		break;
	case EFigureAnim::Lie:
		Name = bLoopPose
			? TEXT(
				"Rig_Medium_SimulationLie_Idle")
			: TEXT(
				"Rig_Medium_SimulationLie_Down");
		break;
	case EFigureAnim::Wave:
		Name = TEXT(
			"Rig_Medium_SimulationWaving");
		break;
	case EFigureAnim::Clap:
		Name = TEXT(
			"Rig_Medium_SimulationCheering");
		break;
	case EFigureAnim::Cheer:
		Name = TEXT(
			"Rig_Medium_SimulationCheering");
		break;
	case EFigureAnim::Cry:
		/* KayKit has no cry. Block stands
		 * and raises both hands. */
		Name = TEXT(
			"Rig_Medium_CombatMeleeMelee_Block");
		break;
	case EFigureAnim::Interact:
		Name = TEXT(
			"Rig_Medium_GeneralInteract");
		break;
	case EFigureAnim::PickUp:
		Name = TEXT(
			"Rig_Medium_GeneralPickUp");
		break;
	default:
		break;
	}

	if (!Name)
	{
		return nullptr;
	}

	return UJoA_CookedContent::LoadAnim(this, Name);
}

float AJoA_Figure::BodyAnimSeconds(EFigureAnim Anim) const
{
	if (const UAnimSequence* Seq = FindBodyAnim(Anim))
	{
		return FMath::Max(0.12f, Seq->GetPlayLength());
	}

	return 0.4f;
}
