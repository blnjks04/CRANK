#include "Gameplay/SurfaceZone.h"
#include "Character/WindupCharacter.h"
#include "FX/CrankFX.h"
#include "Components/BoxComponent.h"

ASurfaceZone::ASurfaceZone()
{
	PrimaryActorTick.bCanEverTick = false;

	Box = CreateDefaultSubobject<UBoxComponent>(TEXT("Box"));
	SetRootComponent(Box);
	Box->SetBoxExtent(FVector(400.f, 400.f, 60.f));
	Box->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Box->SetCollisionResponseToAllChannels(ECR_Ignore);
	Box->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	Box->SetGenerateOverlapEvents(true);

	MistFX = CreateDefaultSubobject<UCrankFXEmitterComponent>(TEXT("MistFX"));
	MistFX->SetupAttachment(Box);
	MistFX->Effect = ECrankFX::ColdMist;
	MistFX->Rate = 3.f;
}

void ASurfaceZone::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	const FVector Extent = Box->GetUnscaledBoxExtent();
	MistFX->SpawnExtent = FVector(Extent.X, Extent.Y, 0.f);
	MistFX->SetRelativeLocation(FVector(0.f, 0.f, -Extent.Z + 10.f));
	MistFX->Rate = FMath::Clamp(Extent.X * Extent.Y / 60000.f, 1.f, 12.f);
}

void ASurfaceZone::BeginPlay()
{
	Super::BeginPlay();
	Box->OnComponentBeginOverlap.AddDynamic(this, &ASurfaceZone::OnBoxBegin);
	Box->OnComponentEndOverlap.AddDynamic(this, &ASurfaceZone::OnBoxEnd);
	MistFX->SetEmitting(bColdMist);

	// Register dolls already inside.
	TArray<AActor*> Inside;
	Box->GetOverlappingActors(Inside, AWindupCharacter::StaticClass());
	for (AActor* Actor : Inside)
	{
		Cast<AWindupCharacter>(Actor)->RegisterSurfaceZone(this, true);
	}
}

void ASurfaceZone::OnBoxBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (AWindupCharacter* Doll = Cast<AWindupCharacter>(OtherActor))
	{
		Doll->RegisterSurfaceZone(this, true);
	}
}

void ASurfaceZone::OnBoxEnd(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	if (AWindupCharacter* Doll = Cast<AWindupCharacter>(OtherActor))
	{
		Doll->RegisterSurfaceZone(this, false);
	}
}
