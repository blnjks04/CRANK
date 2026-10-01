#include "Boss/GiantArena.h"
#include "Boss/BossGiantMonkey.h"
#include "Gameplay/CrankMechanism.h"
#include "Gameplay/CrankCheckpoint.h"
#include "Character/WindupCharacter.h"
#include "Components/BoxComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"

AGiantArena::AGiantArena()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	bAlwaysRelevant = true;

	Bounds = CreateDefaultSubobject<UBoxComponent>(TEXT("Bounds"));
	SetRootComponent(Bounds);
	Bounds->SetBoxExtent(FVector(3000.f, 3000.f, 800.f));	// the whole toy room, walls included
	Bounds->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	WakeTrigger = CreateDefaultSubobject<UBoxComponent>(TEXT("WakeTrigger"));
	WakeTrigger->SetupAttachment(Bounds);
	WakeTrigger->SetBoxExtent(FVector(2300.f, 2300.f, 600.f));
	WakeTrigger->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	WakeTrigger->SetCollisionResponseToAllChannels(ECR_Ignore);
	WakeTrigger->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
}

void AGiantArena::BeginPlay()
{
	Super::BeginPlay();
	if (HasAuthority())
	{
		WakeTrigger->OnComponentBeginOverlap.AddDynamic(this, &AGiantArena::OnWakeEnter);
		if (Boss)
		{
			Boss->Arena = this;
		}
	}
	for (TActorIterator<ACrankCheckpoint> It(GetWorld()); It; ++It)
	{
		if (ContainsPoint(It->GetActorLocation()))
		{
			SafeSpots.Add(It->GetActorLocation());
		}
	}
}

bool AGiantArena::IsInSafeSpot(const FVector& Location, float Extra) const
{
	for (const FVector& Spot : SafeSpots)
	{
		if (FVector::Dist2D(Spot, Location) < SafeRadius + Extra)
		{
			return true;
		}
	}
	return false;
}

bool AGiantArena::ContainsPoint(const FVector& Location, float Margin) const
{
	const FVector Local = GetActorTransform().InverseTransformPosition(Location);
	const FVector Extent = Bounds->GetUnscaledBoxExtent();
	return FMath::Abs(Local.X) <= Extent.X - Margin && FMath::Abs(Local.Y) <= Extent.Y - Margin;
}

FVector AGiantArena::ClampPoint(const FVector& Location, float Margin) const
{
	const FVector Extent = Bounds->GetUnscaledBoxExtent();
	auto Inside = [this, &Extent, Margin](const FVector& P)
	{
		const FVector L = GetActorTransform().InverseTransformPosition(P);
		return FMath::Abs(L.X) <= Extent.X - Margin + 1.f && FMath::Abs(L.Y) <= Extent.Y - Margin + 1.f;
	};
	FVector Local = GetActorTransform().InverseTransformPosition(Location);
	Local.X = FMath::Clamp(Local.X, -Extent.X + Margin, Extent.X - Margin);
	Local.Y = FMath::Clamp(Local.Y, -Extent.Y + Margin, Extent.Y - Margin);
	FVector World = GetActorTransform().TransformPosition(Local);
	World.Z = Location.Z;

	// Keep out of the respawn safe spots: the closest point on the safe circle that is still inside the walls.
	for (const FVector& Spot : SafeSpots)
	{
		if (FVector::Dist2D(World, Spot) >= SafeRadius)
		{
			continue;
		}
		FVector Best = World;
		float BestDist = TNumericLimits<float>::Max();
		for (int32 i = 0; i < 48; ++i)
		{
			const float Angle = 2.f * PI * i / 48.f;
			const FVector Candidate(Spot.X + FMath::Cos(Angle) * (SafeRadius + 5.f), Spot.Y + FMath::Sin(Angle) * (SafeRadius + 5.f), World.Z);
			const float Dist = FVector::Dist2D(Candidate, World);
			if (Inside(Candidate) && Dist < BestDist)
			{
				BestDist = Dist;
				Best = Candidate;
			}
		}
		World = Best;
	}
	return World;
}

void AGiantArena::OnWakeEnter(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	const AWindupCharacter* Doll = Cast<AWindupCharacter>(OtherActor);
	if (Boss && Doll && Doll->IsPlayerControlled())
	{
		Boss->Activate();
	}
}

void AGiantArena::OnGiantDefeated(ABossGiantMonkey* InBoss)
{
	for (ACrankMechanism* Mechanism : OpenOnDefeat)
	{
		if (Mechanism)
		{
			Mechanism->SetActivated(true);
		}
	}
}
