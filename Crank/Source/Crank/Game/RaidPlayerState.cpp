#include "Game/RaidPlayerState.h"
#include "Net/UnrealNetwork.h"

void ARaidPlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ARaidPlayerState, PlayerIndex);
}
