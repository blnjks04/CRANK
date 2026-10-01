#include "Boss/ChargingDock.h"
#include "Gameplay/CarryableActor.h"
#include "FX/CrankFX.h"
#include "FX/CrankSound.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Net/UnrealNetwork.h"
#include "Engine/World.h"

AChargingDock::AChargingDock()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	bAlwaysRelevant = true;

	Base = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Base"));
	SetRootComponent(Base);
	Base->SetCollisionProfileName(TEXT("BlockAll"));

	ParkPoint = CreateDefaultSubobject<USceneComponent>(TEXT("ParkPoint"));
	ParkPoint->SetupAttachment(Base);
	ParkPoint->SetRelativeLocation(FVector(170.f, 0.f, 0.f));

	ClothZone = CreateDefaultSubobject<UBoxComponent>(TEXT("ClothZone"));
	ClothZone->SetupAttachment(Base);
	ClothZone->SetBoxExtent(FVector(110.f, 130.f, 90.f));
	ClothZone->SetRelativeLocation(FVector(0.f, 0.f, 60.f));
	ClothZone->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	ClothZone->SetCollisionResponseToAllChannels(ECR_Ignore);
	ClothZone->SetCollisionResponseToChannel(ECC_PhysicsBody, ECR_Overlap);
	ClothZone->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Overlap);

	StatusLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("StatusLight"));
	StatusLight->SetupAttachment(Base);
	StatusLight->SetRelativeLocation(FVector(0.f, 0.f, 120.f));
	StatusLight->SetIntensity(2500.f);
	StatusLight->SetAttenuationRadius(500.f);
	StatusLight->SetLightColor(FLinearColor(0.2f, 1.f, 0.35f));
	StatusLight->SetCastShadows(false);
}

void AChargingDock::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AChargingDock, bDisabled);
	DOREPLIFETIME(AChargingDock, bCharging);
}

FVector AChargingDock::GetDockPoint() const
{
	return ParkPoint->GetComponentLocation();
}

FVector AChargingDock::GetDockFacing() const
{
	return -GetActorForwardVector();
}

void AChargingDock::SetCharging(bool bInCharging)
{
	if (bCharging != bInCharging)
	{
		bCharging = bInCharging;
		OnRep_State();
	}
}

void AChargingDock::OnRep_State()
{
	if (bDisabled)
	{
		StatusLight->SetLightColor(FLinearColor(0.6f, 0.6f, 0.6f));
		StatusLight->SetIntensity(300.f);
	}
	else if (bCharging)
	{
		StatusLight->SetLightColor(FLinearColor(0.3f, 0.6f, 1.f));
		CrankSound::PlayAt(this, TEXT("SFX_Dock"), GetActorLocation(), 1.f);
	}
	else
	{
		StatusLight->SetLightColor(FLinearColor(0.2f, 1.f, 0.35f));
		StatusLight->SetIntensity(2500.f);
	}
}

void AChargingDock::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	Blink += DeltaSeconds;
	if (bCharging && !bDisabled)
	{
		StatusLight->SetIntensity(2000.f + 2000.f * FMath::Abs(FMath::Sin(Blink * 8.f)));
		if (FMath::Fmod(Blink, 0.25f) < DeltaSeconds)
		{
			UCrankFXSubsystem::Spawn(this, ECrankFX::CellZap, GetActorLocation() + FVector(0, 0, 60.f), FRotator::ZeroRotator, 0.6f);
		}
	}

	if (!HasAuthority())
	{
		return;
	}

	const float Now = GetWorld()->GetTimeSeconds();
	if (bDisabled)
	{
		ACarryableActor* Cloth = CoveringCloth.Get();
		// Someone picked the cloth back up, or time ran out: the dock wakes up.
		if (!Cloth || Cloth->IsCarried() || Now >= DisabledUntil)
		{
			bDisabled = false;
			if (Cloth && !Cloth->IsCarried())
			{
				// Spit the cloth off to the side so it can be reused.
				Cloth->SetActorLocation(GetActorLocation() + GetActorRightVector() * 260.f + FVector(0, 0, 80.f), false, nullptr, ETeleportType::ResetPhysics);
			}
			CoveringCloth.Reset();
			OnRep_State();
			CrankSound::PlayAt(this, TEXT("SFX_Dock"), GetActorLocation(), 0.8f, 1.3f);
		}
		return;
	}

	TArray<AActor*> Inside;
	ClothZone->GetOverlappingActors(Inside, ACarryableActor::StaticClass());
	for (AActor* Actor : Inside)
	{
		ACarryableActor* Cloth = Cast<ACarryableActor>(Actor);
		if (Cloth && Cloth->GetPartType() == ECrankPartType::Dishcloth && !Cloth->IsCarried())
		{
			bDisabled = true;
			DisabledUntil = Now + DisableTime;
			CoveringCloth = Cloth;
			// Drape it neatly over the dock.
			Cloth->SetActorLocationAndRotation(GetActorLocation() + FVector(0, 0, 70.f), FRotator(0.f, GetActorRotation().Yaw, 0.f), false, nullptr, ETeleportType::ResetPhysics);
			OnRep_State();
			CrankSound::PlayAt(this, TEXT("SFX_ClothFall"), GetActorLocation(), 1.f, 1.2f);
			UCrankFXSubsystem::Spawn(this, ECrankFX::Puff, GetActorLocation() + FVector(0, 0, 60.f), FRotator::ZeroRotator, 2.f);
			break;
		}
	}
}
