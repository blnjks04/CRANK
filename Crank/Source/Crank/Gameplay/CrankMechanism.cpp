#include "Gameplay/CrankMechanism.h"
#include "FX/CrankFX.h"
#include "FX/CrankSound.h"
#include "Components/StaticMeshComponent.h"
#include "Net/UnrealNetwork.h"
#include "Engine/World.h"

ACrankMechanism::ACrankMechanism()
{
	bReplicates = true;
	bAlwaysRelevant = true;
}

void ACrankMechanism::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ACrankMechanism, bActivated);
}

void ACrankMechanism::BeginPlay()
{
	Super::BeginPlay();
	if (HasAuthority() && bStartActivated)
	{
		bActivated = true;
	}
	OnActivationChanged(true);
}

void ACrankMechanism::SetActivated(bool bNewActivated)
{
	if (!HasAuthority() || bActivated == bNewActivated)
	{
		return;
	}
	bActivated = bNewActivated;
	OnRep_Activated();
}

void ACrankMechanism::OnRep_Activated()
{
	ActivationTime = GetWorld()->GetTimeSeconds();
	OnActivationChanged(false);
}

// ------------------------------------------------------------------------------------------

ADrawbridge::ADrawbridge()
{
	PrimaryActorTick.bCanEverTick = true;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	Hinge = CreateDefaultSubobject<USceneComponent>(TEXT("Hinge"));
	Hinge->SetupAttachment(Root);

	Plank = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Plank"));
	Plank->SetupAttachment(Hinge);
	Plank->SetCollisionProfileName(TEXT("BlockAll"));
	Plank->SetMobility(EComponentMobility::Movable);
}

void ADrawbridge::OnActivationChanged(bool bInstant)
{
	if (bInstant)
	{
		Alpha = bActivated ? 1.f : 0.f;
		Hinge->SetRelativeRotation(bActivated ? OpenRotation : ClosedRotation);
		bLandedFX = true;
		return;
	}
	bLandedFX = false;
	CrankSound::PlayAt(this, TEXT("SFX_Bridge"), GetActorLocation(), 1.f);
}

void ADrawbridge::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	const float Target = bActivated ? 1.f : 0.f;
	if (FMath::IsNearlyEqual(Alpha, Target))
	{
		return;
	}
	Alpha = FMath::FInterpConstantTo(Alpha, Target, DeltaSeconds, 1.f / FMath::Max(0.1f, SwingTime));
	// Gravity-like ease in, little bounce at the end.
	float Eased = Alpha * Alpha;
	if (bActivated && Alpha >= 1.f)
	{
		Eased = 1.f;
	}
	const FQuat Rot = FQuat::Slerp(ClosedRotation.Quaternion(), OpenRotation.Quaternion(), Eased);
	Hinge->SetRelativeRotation(Rot);

	if (!bLandedFX && FMath::IsNearlyEqual(Alpha, Target))
	{
		bLandedFX = true;
		CrankSound::PlayAt(this, TEXT("SFX_Thud"), Plank->Bounds.Origin, 1.f, 0.7f);
		UCrankFXSubsystem::Spawn(this, ECrankFX::Land, Plank->Bounds.Origin - FVector(0, 0, Plank->Bounds.BoxExtent.Z), FRotator::ZeroRotator, 2.f);
	}
}

// ------------------------------------------------------------------------------------------

ATableclothShortcut::ATableclothShortcut()
{
	PrimaryActorTick.bCanEverTick = true;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	Curtain = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Curtain"));
	Curtain->SetupAttachment(Root);
	Curtain->SetCollisionProfileName(TEXT("BlockAll"));
	Curtain->SetMobility(EComponentMobility::Movable);

	FallenCloth = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("FallenCloth"));
	FallenCloth->SetupAttachment(Root);
	FallenCloth->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	FallenCloth->SetVisibility(false);
	FallenCloth->SetMobility(EComponentMobility::Movable);
}

void ATableclothShortcut::OnActivationChanged(bool bInstant)
{
	if (CurtainStart.IsZero())
	{
		CurtainStart = Curtain->GetRelativeLocation();
	}
	if (bActivated)
	{
		Curtain->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		if (bInstant)
		{
			Alpha = 1.f;
			Curtain->SetVisibility(false);
			FallenCloth->SetVisibility(true);
		}
		else
		{
			CrankSound::PlayAt(this, TEXT("SFX_ClothFall"), GetActorLocation(), 1.f);
		}
	}
}

void ATableclothShortcut::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!bActivated || Alpha >= 1.f)
	{
		return;
	}
	Alpha = FMath::Min(1.f, Alpha + DeltaSeconds / 1.4f);
	// The curtain slips down and crumples to the floor.
	const float Drop = Alpha * Alpha;
	Curtain->SetRelativeLocation(CurtainStart - FVector(0.f, 0.f, Drop * 700.f));
	Curtain->SetRelativeScale3D(FVector(1.f, 1.f, FMath::Max(0.05f, 1.f - Drop)));
	if (Alpha >= 1.f)
	{
		Curtain->SetVisibility(false);
		FallenCloth->SetVisibility(true);
		UCrankFXSubsystem::Spawn(this, ECrankFX::DustBurst, GetActorLocation(), FRotator::ZeroRotator, 2.f);
		CrankSound::PlayAt(this, TEXT("SFX_Thud"), GetActorLocation(), 1.f, 0.6f);
	}
}
