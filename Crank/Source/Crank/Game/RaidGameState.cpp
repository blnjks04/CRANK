#include "Game/RaidGameState.h"
#include "FX/CrankSound.h"
#include "Net/UnrealNetwork.h"
#include "Engine/World.h"

void ARaidGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ARaidGameState, RaidPhase);
	DOREPLIFETIME(ARaidGameState, RaidStartTime);
	DOREPLIFETIME(ARaidGameState, RaidEndTime);
	DOREPLIFETIME(ARaidGameState, CollectedMask);
	DOREPLIFETIME(ARaidGameState, bMainSpringDelivered);
	DOREPLIFETIME(ARaidGameState, bRequiresGoldenKey);
	DOREPLIFETIME(ARaidGameState, bGoldenKeyDelivered);
	DOREPLIFETIME(ARaidGameState, bGiantDefeated);
	DOREPLIFETIME(ARaidGameState, bBossDefeated);
	DOREPLIFETIME(ARaidGameState, bEmergencyUsed);
	DOREPLIFETIME(ARaidGameState, FailReason);
	DOREPLIFETIME(ARaidGameState, ResultTime);
	DOREPLIFETIME(ARaidGameState, ZoneName);
	DOREPLIFETIME(ARaidGameState, ZoneOrder);
}

float ARaidGameState::GetRemainingTime() const
{
	if (RaidPhase == ERaidPhase::Waiting)
	{
		return RaidEndTime - RaidStartTime;
	}
	if (RaidPhase != ERaidPhase::InProgress)
	{
		return FMath::Max(0.f, RaidEndTime - ResultTime);
	}
	return FMath::Max(0.f, RaidEndTime - GetServerWorldTimeSeconds());
}

float ARaidGameState::GetElapsedTime() const
{
	if (RaidPhase == ERaidPhase::Waiting)
	{
		return 0.f;
	}
	const float End = RaidPhase == ERaidPhase::InProgress ? GetServerWorldTimeSeconds() : ResultTime;
	return FMath::Max(0.f, End - RaidStartTime);
}

uint8 ARaidGameState::PartBit(ECrankPartType Type)
{
	switch (Type)
	{
	case ECrankPartType::Gear:			return 1;
	case ECrankPartType::SpringCoil:	return 2;
	case ECrankPartType::JewelBearing:	return 4;
	default:							return 0;
	}
}

int32 ARaidGameState::NumCollected() const
{
	return ((CollectedMask & 1) ? 1 : 0) + ((CollectedMask & 2) ? 1 : 0) + ((CollectedMask & 4) ? 1 : 0);
}

void ARaidGameState::MulticastToast_Implementation(const FText& Text, FLinearColor Color)
{
	FRaidToast Toast;
	Toast.Text = Text;
	Toast.Color = Color;
	Toast.LocalTime = GetWorld()->GetTimeSeconds();
	Toasts.Add(Toast);
	if (Toasts.Num() > 4)
	{
		Toasts.RemoveAt(0);
	}
	CrankSound::Play2D(this, TEXT("SFX_Toast"), 0.5f);
}

void ARaidGameState::PruneToasts(float Now)
{
	Toasts.RemoveAll([Now](const FRaidToast& T) { return Now - T.LocalTime > 4.5f; });
}
