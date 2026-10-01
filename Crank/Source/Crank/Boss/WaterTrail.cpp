#include "Boss/WaterTrail.h"
#include "CrankAssets.h"
#include "CrankBalance.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "GameFramework/GameStateBase.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"

AWaterTrail::AWaterTrail()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.1f;
	bReplicates = true;
	bAlwaysRelevant = true;

	Puddles = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("Puddles"));
	SetRootComponent(Puddles);
	Puddles->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Puddles->SetCastShadow(false);
	Puddles->NumCustomDataFloats = 1;
	Puddles->SetMobility(EComponentMobility::Movable);

	Lifetime = CrankBalance::BossWaterLifetime;
}

void AWaterTrail::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AWaterTrail, Points);
}

float AWaterTrail::Now() const
{
	const UWorld* World = GetWorld();
	if (const AGameStateBase* GS = World ? World->GetGameState() : nullptr)
	{
		return GS->GetServerWorldTimeSeconds();
	}
	return World ? World->GetTimeSeconds() : 0.f;
}

void AWaterTrail::AddPoint(const FVector& Location)
{
	FWaterPoint Point;
	Point.Location = Location;
	Point.SpawnTime = Now();
	Points.Add(Point);
	if (Points.Num() > 96)
	{
		Points.RemoveAt(0);
	}
	RebuildVisuals();
}

bool AWaterTrail::IsWetAt(const FVector& FootLocation) const
{
	const float T = Now();
	const float RadiusSq = PuddleRadius * PuddleRadius;
	for (const FWaterPoint& Point : Points)
	{
		if (T - Point.SpawnTime > Lifetime)
		{
			continue;
		}
		if (FMath::Abs(FootLocation.Z - Point.Location.Z) < 60.f && FVector::DistSquared2D(FootLocation, Point.Location) < RadiusSq)
		{
			return true;
		}
	}
	return false;
}

void AWaterTrail::OnRep_Points()
{
	RebuildVisuals();
}

void AWaterTrail::RebuildVisuals()
{
	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	if (!Puddles->GetStaticMesh())
	{
		Puddles->SetStaticMesh(CrankAssets::Mesh(CrankPaths::FXPuff, CrankPaths::Sphere));
		if (UMaterialInterface* Mat = CrankAssets::Material(CrankPaths::MatWater, true))
		{
			Puddles->SetMaterial(0, Mat);
		}
	}

	const float T = Now();
	TArray<FTransform> Transforms;
	TArray<float> Fade;
	for (const FWaterPoint& Point : Points)
	{
		const float Age = T - Point.SpawnTime;
		if (Age > Lifetime)
		{
			continue;
		}
		const float Life = 1.f - Age / Lifetime;
		const float Size = PuddleRadius / 50.f * FMath::Lerp(0.6f, 1.f, FMath::Min(1.f, Life * 3.f));
		Transforms.Add(FTransform(FRotator(0.f, (float)(FMath::Abs(Point.Location.X * 13.f + Point.Location.Y * 7.f)), 0.f), FVector(Point.Location) + FVector(0, 0, 1.5f), FVector(Size, Size * 0.85f, 0.02f)));
		Fade.Add(Life);
	}

	Puddles->ClearInstances();
	if (Transforms.Num() > 0)
	{
		Puddles->AddInstances(Transforms, false, true, false);
		for (int32 i = 0; i < Fade.Num(); ++i)
		{
			Puddles->SetCustomDataValue(i, 0, Fade[i], false);
		}
		Puddles->MarkRenderStateDirty();
	}
}

void AWaterTrail::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (HasAuthority())
	{
		const float T = Now();
		const int32 Before = Points.Num();
		Points.RemoveAll([this, T](const FWaterPoint& P) { return T - P.SpawnTime > Lifetime; });
		if (Points.Num() != Before)
		{
			RebuildVisuals();
		}
	}

	VisualRefresh += DeltaSeconds;
	if (VisualRefresh > 0.5f)
	{
		VisualRefresh = 0.f;
		RebuildVisuals();
	}
}
