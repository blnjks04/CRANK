#include "Gameplay/WindStation.h"
#include "Character/WindupCharacter.h"
#include "Character/TorqueComponent.h"
#include "FX/CrankFX.h"
#include "FX/CrankSound.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/AudioComponent.h"
#include "Net/UnrealNetwork.h"

AWindStation::AWindStation()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;

	Body = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Body"));
	SetRootComponent(Body);
	Body->SetCollisionProfileName(TEXT("BlockAll"));

	Spinner = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Spinner"));
	Spinner->SetupAttachment(Body);
	Spinner->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	Crank = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Crank"));
	Crank->SetupAttachment(Body);
	Crank->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	Pad = CreateDefaultSubobject<UBoxComponent>(TEXT("Pad"));
	Pad->SetupAttachment(Body);
	// Covers the box top and a ring around it so crawling (discharged) dolls can recharge too.
	Pad->SetBoxExtent(FVector(190.f, 190.f, 110.f));
	Pad->SetRelativeLocation(FVector(0.f, 0.f, 80.f));
	Pad->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Pad->SetCollisionResponseToAllChannels(ECR_Ignore);
	Pad->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);

	NotesFX = CreateDefaultSubobject<UCrankFXEmitterComponent>(TEXT("NotesFX"));
	NotesFX->SetupAttachment(Body);
	NotesFX->SetRelativeLocation(FVector(0.f, 0.f, 140.f));
	NotesFX->Effect = ECrankFX::Notes;
	NotesFX->Rate = 2.5f;
	NotesFX->SpawnExtent = FVector(40.f, 40.f, 0.f);
}

void AWindStation::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AWindStation, bActive);
}

void AWindStation::BeginPlay()
{
	Super::BeginPlay();
	Music = CrankSound::CreateLoop(Body, TEXT("SFX_MusicBox"), 0.8f);
	OnRep_Active();
}

void AWindStation::OnRep_Active()
{
	NotesFX->SetEmitting(bActive);
	if (Music)
	{
		if (bActive && !Music->IsPlaying())
		{
			Music->FadeIn(0.3f);
		}
		else if (!bActive && Music->IsPlaying())
		{
			Music->FadeOut(0.6f, 0.f);
		}
	}
}

void AWindStation::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (bActive)
	{
		Spinner->AddLocalRotation(FRotator(0.f, SpinSpeed * DeltaSeconds, 0.f));
		Crank->AddLocalRotation(FRotator(SpinSpeed * 0.5f * DeltaSeconds, 0.f, 0.f));	// around its shaft (local Y)
	}

	if (!HasAuthority())
	{
		return;
	}

	bool bAnyCharging = false;
	TArray<AActor*> Inside;
	Pad->GetOverlappingActors(Inside, AWindupCharacter::StaticClass());
	for (AActor* Actor : Inside)
	{
		AWindupCharacter* Doll = Cast<AWindupCharacter>(Actor);
		if (!Doll || Doll->GetBodyState() != ECrankBodyState::Normal)
		{
			continue;
		}
		UTorqueComponent* Torque = Doll->GetTorque();
		if (Torque->GetTorque() < ChargeCap)
		{
			Torque->AddTorque(FMath::Min(ChargeRate * DeltaSeconds, ChargeCap - Torque->GetTorque()));
			bAnyCharging = true;
		}
	}

	if (bAnyCharging != bActive)
	{
		bActive = bAnyCharging;
		OnRep_Active();
	}
}
