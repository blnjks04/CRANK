#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "RaidPlayerState.generated.h"

UCLASS()
class CRANK_API ARaidPlayerState : public APlayerState
{
	GENERATED_BODY()

public:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** 0 = host (red), 1 = guest (blue). */
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Raid")
	int32 PlayerIndex = 0;
};
