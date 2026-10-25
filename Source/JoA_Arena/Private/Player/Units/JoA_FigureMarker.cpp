// Copyright Epic Games, Inc. All Rights Reserved.

#include "Player/Units/JoA_FigureMarker.h"

// --- Engine ---
#include "Engine/StaticMesh.h"

// --- GAME ---
#include "Player/Units/JoA_Figure.h"

// --- Components ---
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "ProceduralMeshComponent.h"

// --- Materials ---
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"

// --- Helpers ---
#include "UObject/ConstructorHelpers.h"

namespace
{
	const FName MarkerColorParam(TEXT("Color"));
	const FName MarkerEmissiveParam(TEXT("EmissiveColor"));

	const FLinearColor MarkerSelectedColor(1.f, 0.9f, 0.05f);
	const FLinearColor MarkerAttackColor(1.f, 0.05f, 0.05f);
	const FLinearColor MarkerHealColor(0.55f, 1.f, 0.55f);
	const FLinearColor MarkerObstacleColor(0.45f, 0.22f, 0.05f);

	// Ring on the square. Tile is 100 wide.
	const int32 MarkerRingSegments = 32;
	const float MarkerRingRadius = 47.f;
	const float MarkerRingWidth = 5.f;
	const float MarkerRingZ = 3.f;
}

AJoA_FigureMarker::AJoA_FigureMarker()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = false;

	USceneComponent* Root =
		CreateDefaultSubobject<USceneComponent>(
			TEXT("Root"));
	SetRootComponent(Root);

	static ConstructorHelpers::FObjectFinder<UStaticMesh>
		ConeAsset(TEXT(
			"/Engine/BasicShapes/Cone.Cone"));
	static ConstructorHelpers::FObjectFinder<
		UMaterialInterface> UnlitAsset(TEXT(
			"/Engine/EngineMaterials/"
			"EmissiveMeshMaterial.EmissiveMeshMaterial"));

	Ring = CreateDefaultSubobject<UProceduralMeshComponent>(
		TEXT("Ring"));
	Ring->SetupAttachment(Root);
	Ring->SetCollisionEnabled(
		ECollisionEnabled::NoCollision);
	Ring->SetGenerateOverlapEvents(false);
	Ring->SetCastShadow(false);
	Ring->SetCanEverAffectNavigation(false);

	Cone = CreateDefaultSubobject<UStaticMeshComponent>(
		TEXT("Cone"));
	Cone->SetupAttachment(Root);
	Cone->SetStaticMesh(ConeAsset.Object);
	Cone->SetCollisionEnabled(
		ECollisionEnabled::NoCollision);
	Cone->SetCastShadow(false);
	const FVector ConeScale(0.35f, 0.35f, 0.4f);
	Cone->SetRelativeScale3D(ConeScale);
	Cone->SetRelativeLocation(FVector(0.f, 0.f, 135.f));

	/* The engine cone points up. A pitch of 180 turns
	 * the tip down at the head. */
	Cone->SetRelativeRotation(FRotator(180.f, 0.f, 0.f));

	/* A lit material turned the markers dark gray in the
	 * shadow of the tavern. The unlit one keeps the color
	 * the same everywhere. */
	if (UnlitAsset.Succeeded())
	{
		UnlitBase = UnlitAsset.Object;
	}
}

void AJoA_FigureMarker::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	/* One ribbon, not a ring of cube components.
	 * The engine unlit material has no instancing
	 * flag, so the ribbon is a single mesh. */
	if (UnlitBase)
	{
		RingMaterial = UMaterialInstanceDynamic::Create(
			UnlitBase,
			this);
		ConeMaterial = UMaterialInstanceDynamic::Create(
			UnlitBase,
			this);
		Cone->SetMaterial(0, ConeMaterial);
		Ring->SetMaterial(0, RingMaterial);
	}

	BuildRing();
	HideMarker();
}

void AJoA_FigureMarker::BuildRing()
{
	if (!Ring)
	{
		return;
	}

	TArray<FVector> Vertices;
	TArray<int32> Triangles;
	TArray<FVector> Normals;
	TArray<FVector2D> UVs;
	TArray<FLinearColor> Colors;
	TArray<FProcMeshTangent> Tangents;
	const int32 VertexCount = MarkerRingSegments * 2;
	Vertices.Reserve(VertexCount);
	Normals.Reserve(VertexCount);
	UVs.Reserve(VertexCount);
	Colors.Reserve(VertexCount);
	Tangents.Reserve(VertexCount);

	const float Inner =
		MarkerRingRadius - MarkerRingWidth * 0.5f;
	const float Outer =
		MarkerRingRadius + MarkerRingWidth * 0.5f;
	const FVector Up(0.f, 0.f, 1.f);
	const FProcMeshTangent Tangent(1.f, 0.f, 0.f);

	for (int32 Index = 0; Index < MarkerRingSegments; ++Index)
	{
		const float Angle =
			2.f * PI * static_cast<float>(Index)
			/ static_cast<float>(MarkerRingSegments);
		const float C = FMath::Cos(Angle);
		const float S = FMath::Sin(Angle);
		Vertices.Add(FVector(
			C * Inner,
			S * Inner,
			MarkerRingZ));
		Vertices.Add(FVector(
			C * Outer,
			S * Outer,
			MarkerRingZ));
		Normals.Add(Up);
		Normals.Add(Up);
		UVs.Add(FVector2D::ZeroVector);
		UVs.Add(FVector2D::ZeroVector);
		Colors.Add(FLinearColor::White);
		Colors.Add(FLinearColor::White);
		Tangents.Add(Tangent);
		Tangents.Add(Tangent);
	}

	Triangles.Reserve(MarkerRingSegments * 12);
	for (int32 Index = 0; Index < MarkerRingSegments; ++Index)
	{
		const int32 Next =
			(Index + 1) % MarkerRingSegments;
		const int32 Inner0 = Index * 2;
		const int32 Outer0 = Inner0 + 1;
		const int32 Inner1 = Next * 2;
		const int32 Outer1 = Inner1 + 1;

		Triangles.Add(Inner0);
		Triangles.Add(Outer0);
		Triangles.Add(Outer1);
		Triangles.Add(Inner0);
		Triangles.Add(Outer1);
		Triangles.Add(Inner1);

		Triangles.Add(Inner0);
		Triangles.Add(Outer1);
		Triangles.Add(Outer0);
		Triangles.Add(Inner0);
		Triangles.Add(Inner1);
		Triangles.Add(Outer1);
	}

	Ring->CreateMeshSection_LinearColor(
		0,
		Vertices,
		Triangles,
		Normals,
		UVs,
		Colors,
		Tangents,
		false);
}

void AJoA_FigureMarker::ShowOn(
	AJoA_Figure* Figure,
	EFigureMarkerKind Kind)
{
	if (!IsValid(Figure)
		|| Figure->IsDead()
		|| Kind == EFigureMarkerKind::None)
	{
		HideMarker();
		return;
	}

	if (GetAttachParentActor() != Figure)
	{
		AttachToActor(
			Figure,
			FAttachmentTransformRules::
				SnapToTargetNotIncludingScale);
	}

	CurrentKind = Kind;
	SetActorHiddenInGame(false);
	Cone->SetVisibility(true);

	switch (Kind)
	{
	case EFigureMarkerKind::Attackable:
		ApplyColor(MarkerAttackColor);
		break;
	case EFigureMarkerKind::Healable:
		ApplyColor(MarkerHealColor);
		break;
	default:
		ApplyColor(MarkerSelectedColor);
		break;
	}
}

void AJoA_FigureMarker::ShowOnCell(
	const FVector& Location,
	EFigureMarkerKind Kind)
{
	if (Kind == EFigureMarkerKind::None)
	{
		HideMarker();
		return;
	}

	if (GetAttachParentActor())
	{
		DetachFromActor(
			FDetachmentTransformRules::
				KeepWorldTransform);
	}

	SetActorLocation(Location);
	CurrentKind = Kind;
	SetActorHiddenInGame(false);
	Cone->SetVisibility(false);
	const bool bBrown =
		Kind == EFigureMarkerKind::Obstacle
		|| Kind == EFigureMarkerKind::FlagPickup;
	ApplyColor(
		bBrown
			? MarkerObstacleColor
			: MarkerHealColor);
}

void AJoA_FigureMarker::HideMarker()
{
	CurrentKind = EFigureMarkerKind::None;
	SetActorHiddenInGame(true);
}

void AJoA_FigureMarker::ApplyColor(const FLinearColor& Color)
{
	for (UMaterialInstanceDynamic* Material :
		{ RingMaterial.Get(), ConeMaterial.Get() })
	{
		if (Material)
		{
			/* Intensity 1 disappears on the lit board.
			 * Above 1 stays bright and can bloom. */
			const FLinearColor Glow = Color * 12.f;
			Material->SetVectorParameterValue(
				MarkerColorParam,
				Glow);
			Material->SetVectorParameterValue(
				MarkerEmissiveParam,
				Glow);
		}
	}
}
