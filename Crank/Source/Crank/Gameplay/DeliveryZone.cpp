#include "Gameplay/DeliveryZone.h"
#include "Gameplay/CarryableActor.h"
#include "Game/RaidGameMode.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Engine/World.h"

ADeliveryZone::ADeliveryZone()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;

	Visual = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Visual"));
	SetRootComponent(Visual);
	Visual->SetCollisionProfileName(TEXT("BlockAll"));

	Box = CreateDefaultSubobject<UBoxComponent>(TEXT("Box"));
	Box->SetupAttachment(Visual);
	Box->SetBoxExtent(FVector(90.f, 90.f, 90.f));
	Box->SetRelativeLocation(FVector(0.f, 0.f, 90.f));
	Box->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Box->SetCollisionResponseToAllChannels(ECR_Ignore);
	Box->SetCollisionResponseToChannel(ECC_PhysicsBody, ECR_Overlap);
	Box->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Overlap);

	Beacon = CreateDefaultSubobject<UPointLightComponent>(TEXT("Beacon"));
	Beacon->SetupAttachment(Visual);
	Beacon->SetRelativeLocation(FVector(0.f, 0.f, 160.f));
	Beacon->SetIntensity(1800.f);
	Beacon->SetAttenuationRadius(450.f);
	Beacon->SetLightColor(FLinearColor(1.f, 0.8f, 0.35f));
	Beacon->SetCastShadows(false);

	Label = NSLOCTEXT("Crank", "DeliveryLabel", "부품 수거함");
}

void ADeliveryZone::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	Pulse += DeltaSeconds;
	Beacon->SetIntensity(1400.f + 600.f * FMath::Sin(Pulse * 3.f));

	if (!HasAuthority())
	{
		return;
	}

	TArray<AActor*> Inside;
	Box->GetOverlappingActors(Inside, ACarryableActor::StaticClass());
	for (AActor* Actor : Inside)
	{
		ACarryableActor* Part = Cast<ACarryableActor>(Actor);
		if (!Part || Part->IsDelivered() || !Accepts(Part->GetPartType()))
		{
			continue;
		}
		const ECrankPartType Type = Part->GetPartType();
		Part->Deliver();
		if (ARaidGameMode* GM = GetWorld()->GetAuthGameMode<ARaidGameMode>())
		{
			GM->OnPartDelivered(Type, this);
		}
	}
}
