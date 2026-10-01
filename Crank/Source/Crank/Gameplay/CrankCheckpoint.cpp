#include "Gameplay/CrankCheckpoint.h"
#include "Character/WindupCharacter.h"
#include "Game/RaidGameMode.h"
#include "Components/BoxComponent.h"
#include "Components/ArrowComponent.h"
#include "Engine/World.h"

ACrankCheckpoint::ACrankCheckpoint()
{
	PrimaryActorTick.bCanEverTick = false;

	Box = CreateDefaultSubobject<UBoxComponent>(TEXT("Box"));
	SetRootComponent(Box);
	Box->SetBoxExtent(FVector(300.f, 300.f, 200.f));
	Box->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Box->SetCollisionResponseToAllChannels(ECR_Ignore);
	Box->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);

	Arrow = CreateDefaultSubobject<UArrowComponent>(TEXT("Arrow"));
	Arrow->SetupAttachment(Box);
}

void ACrankCheckpoint::BeginPlay()
{
	Super::BeginPlay();
	if (HasAuthority())
	{
		Box->OnComponentBeginOverlap.AddDynamic(this, &ACrankCheckpoint::OnBoxBegin);
	}
}

void ACrankCheckpoint::OnBoxBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (AWindupCharacter* Doll = Cast<AWindupCharacter>(OtherActor))
	{
		if (ARaidGameMode* GM = GetWorld()->GetAuthGameMode<ARaidGameMode>())
		{
			GM->OnCheckpointReached(this, Doll);
		}
	}
}

FTransform ACrankCheckpoint::GetSpawnTransform(int32 PlayerIndex) const
{
	const FVector Side = GetActorRightVector() * (PlayerIndex % 2 == 0 ? -70.f : 70.f);
	return FTransform(FRotator(0.f, GetActorRotation().Yaw, 0.f), Arrow->GetComponentLocation() + Side + FVector(0.f, 0.f, 60.f));
}
