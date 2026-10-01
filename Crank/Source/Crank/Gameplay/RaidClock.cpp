#include "Gameplay/RaidClock.h"
#include "Game/RaidGameState.h"
#include "FX/CrankSound.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"

ARaidClock::ARaidClock()
{
	PrimaryActorTick.bCanEverTick = true;

	Body = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Body"));
	SetRootComponent(Body);
	Body->SetCollisionProfileName(TEXT("BlockAll"));

	FacePivot = CreateDefaultSubobject<USceneComponent>(TEXT("FacePivot"));
	FacePivot->SetupAttachment(Body);
	FacePivot->SetRelativeLocation(FVector(196.f, 0.f, 1880.f));

	HourHand = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("HourHand"));
	HourHand->SetupAttachment(FacePivot);
	HourHand->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	MinuteHand = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MinuteHand"));
	MinuteHand->SetupAttachment(FacePivot);
	MinuteHand->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	MinuteHand->SetRelativeLocation(FVector(4.f, 0.f, 0.f));

	PendulumPivot = CreateDefaultSubobject<USceneComponent>(TEXT("PendulumPivot"));
	PendulumPivot->SetupAttachment(Body);
	PendulumPivot->SetRelativeLocation(FVector(100.f, 0.f, 1450.f));

	Pendulum = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Pendulum"));
	Pendulum->SetupAttachment(PendulumPivot);
	Pendulum->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void ARaidClock::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	const ARaidGameState* GS = GetWorld()->GetGameState<ARaidGameState>();
	float Remaining = 25.f * 60.f;
	float Total = 25.f * 60.f;
	if (GS)
	{
		Total = FMath::Max(1.f, GS->RaidEndTime - GS->RaidStartTime);
		Remaining = GS->GetRemainingTime();
	}

	// Minutes past 11 o'clock.
	const float Minutes = 60.f - (Remaining / Total) * (60.f - StartMinutePastEleven);
	const float MinuteAngle = Minutes / 60.f * 360.f;
	const float HourAngle = (11.f + Minutes / 60.f) / 12.f * 360.f;
	MinuteHand->SetRelativeRotation(FRotator(0.f, 0.f, RotationSign * MinuteAngle));
	HourHand->SetRelativeRotation(FRotator(0.f, 0.f, RotationSign * HourAngle));

	TickPhase += DeltaSeconds;
	PendulumPivot->SetRelativeRotation(FRotator(0.f, 0.f, PendulumAmplitude * FMath::Sin(TickPhase * PI)));

	const int32 Second = FMath::FloorToInt(TickPhase);
	if (Second != LastSecond)
	{
		LastSecond = Second;
		CrankSound::PlayAt(this, TEXT("SFX_ClockTick"), FacePivot->GetComponentLocation(), 0.5f, Second % 2 == 0 ? 1.f : 0.9f);
	}
}
