#include "Gameplay/PullLever.h"
#include "Gameplay/CrankMechanism.h"
#include "Character/WindupCharacter.h"
#include "FX/CrankFX.h"
#include "FX/CrankSound.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Net/UnrealNetwork.h"

APullLever::APullLever()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;

	Base = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Base"));
	SetRootComponent(Base);
	Base->SetCollisionProfileName(TEXT("BlockAll"));

	Pivot = CreateDefaultSubobject<USceneComponent>(TEXT("Pivot"));
	Pivot->SetupAttachment(Base);

	Handle = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Handle"));
	Handle->SetupAttachment(Pivot);
	Handle->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	GrabBox = CreateDefaultSubobject<UBoxComponent>(TEXT("GrabBox"));
	GrabBox->SetupAttachment(Handle);
	GrabBox->SetBoxExtent(FVector(18.f, 18.f, 18.f));
	GrabBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	GrabBox->SetCollisionObjectType(ECC_WorldDynamic);
	GrabBox->SetCollisionResponseToAllChannels(ECR_Ignore);
	GrabBox->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);

	Label = NSLOCTEXT("Crank", "LeverLabel", "레버 당기기");
}

void APullLever::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(APullLever, Progress);
	DOREPLIFETIME(APullLever, bDone);
}

bool APullLever::CanBeGrabbed(const AWindupCharacter* By, ECrankHand Hand, const UPrimitiveComponent* Comp) const
{
	return !(bOneShot && bDone);
}

void APullLever::OnGrabbed(AWindupCharacter* By, ECrankHand Hand, UPrimitiveComponent* Comp)
{
	HoldersCount++;
}

void APullLever::OnReleased(AWindupCharacter* By, ECrankHand Hand, UPrimitiveComponent* Comp, const FVector& ThrowVelocity)
{
	HoldersCount = FMath::Max(0, HoldersCount - 1);
}

bool APullLever::TickHeld(AWindupCharacter* By, ECrankHand Hand, UPrimitiveComponent* Comp, float DeltaTime)
{
	if (bOneShot && bDone)
	{
		return false;
	}
	if (FVector::Dist2D(By->GetActorLocation(), GrabBox->GetComponentLocation()) > 150.f)
	{
		return false;
	}
	// Two hands do not pull twice as fast; the lever ticks per hand so halve when both hold it.
	const float Rate = 1.f / FMath::Max(0.1f, HoldTime) / FMath::Max(1, HoldersCount);
	Progress = FMath::Min(1.f, Progress + Rate * DeltaTime);
	if (Progress >= 1.f && !bDone)
	{
		bDone = true;
		for (ACrankMechanism* Target : Targets)
		{
			if (Target)
			{
				Target->SetActivated(bOneShot ? true : !Target->IsActivated());
			}
		}
		CrankSound::PlayAt(this, TEXT("SFX_Lever"), GrabBox->GetComponentLocation(), 1.f);
		return false;
	}
	return true;
}

void APullLever::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (HasAuthority() && HoldersCount == 0 && !bDone && Progress > 0.f)
	{
		Progress = FMath::Max(0.f, Progress - DeltaSeconds * 1.5f);
	}
	if (HasAuthority() && !bOneShot && bDone && HoldersCount == 0)
	{
		bDone = false;
		Progress = 0.f;
	}

	const float Alpha = bDone ? 1.f : Progress;
	Pivot->SetRelativeRotation(FQuat::Slerp(FQuat::Identity, PulledRotation.Quaternion(), Alpha));
}
