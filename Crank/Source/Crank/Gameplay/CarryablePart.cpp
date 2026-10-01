#include "Gameplay/CarryablePart.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"

ACarryablePart::ACarryablePart()
{
	Glow = CreateDefaultSubobject<UPointLightComponent>(TEXT("Glow"));
	Glow->SetupAttachment(Mesh);
	Glow->SetIntensity(900.f);
	Glow->SetAttenuationRadius(260.f);
	Glow->SetLightColor(FLinearColor(1.f, 0.85f, 0.45f));
	Glow->SetCastShadows(false);
	Glow->SetRelativeLocation(FVector(0.f, 0.f, 30.f));
}

void ACarryablePart::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!bGlow)
	{
		Glow->SetVisibility(false);
		return;
	}
	GlowTime += DeltaSeconds;
	Glow->SetVisibility(!IsCarried());
	Glow->SetIntensity(700.f + 400.f * FMath::Sin(GlowTime * 2.5f));
	Glow->SetWorldLocation(GetActorLocation() + FVector(0.f, 0.f, 40.f));
}

ABatteryCellPickup::ABatteryCellPickup()
{
	PartType = ECrankPartType::BatteryCell;
	Weight = ECarryWeight::Light;
	MassKg = 1.f;
	GrabPriority = 1;
	DisplayName = NSLOCTEXT("Crank", "SpentCell", "다 쓴 배터리");
}
