#include "Boss/BossArenaManager.h"
#include "Boss/BossDustEater.h"
#include "Gameplay/CrankMechanism.h"
#include "Character/WindupCharacter.h"
#include "Game/RaidGameMode.h"
#include "Components/BoxComponent.h"
#include "Engine/World.h"

ABossArenaManager::ABossArenaManager()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	bAlwaysRelevant = true;

	Bounds = CreateDefaultSubobject<UBoxComponent>(TEXT("Bounds"));
	SetRootComponent(Bounds);
	Bounds->SetBoxExtent(FVector(1250.f, 1250.f, 300.f));
	Bounds->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	WakeTrigger = CreateDefaultSubobject<UBoxComponent>(TEXT("WakeTrigger"));
	WakeTrigger->SetupAttachment(Bounds);
	WakeTrigger->SetBoxExtent(FVector(1050.f, 1050.f, 250.f));
	WakeTrigger->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	WakeTrigger->SetCollisionResponseToAllChannels(ECR_Ignore);
	WakeTrigger->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
}

void ABossArenaManager::BeginPlay()
{
	Super::BeginPlay();
	if (HasAuthority())
	{
		WakeTrigger->OnComponentBeginOverlap.AddDynamic(this, &ABossArenaManager::OnArenaEnter);
		if (Boss)
		{
			Boss->Arena = this;
		}
	}
}

bool ABossArenaManager::ContainsPoint(const FVector& Location, float Margin) const
{
	const FVector Local = GetActorTransform().InverseTransformPosition(Location);
	const FVector Extent = Bounds->GetUnscaledBoxExtent();
	return FMath::Abs(Local.X) <= Extent.X - Margin && FMath::Abs(Local.Y) <= Extent.Y - Margin;
}

void ABossArenaManager::OnArenaEnter(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (Boss && Cast<AWindupCharacter>(OtherActor))
	{
		Boss->Activate();
	}
}

void ABossArenaManager::OnBossDefeated(ABossDustEater* InBoss)
{
	for (ACrankMechanism* Mechanism : OpenOnDefeat)
	{
		if (Mechanism)
		{
			Mechanism->SetActivated(true);
		}
	}
	if (ARaidGameMode* GM = GetWorld()->GetAuthGameMode<ARaidGameMode>())
	{
		GM->OnBossDefeated();
	}
}
