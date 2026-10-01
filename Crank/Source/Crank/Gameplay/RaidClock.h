// Grandfather clock: its hands show the raid timer (11:35 -> midnight), pendulum swings, ticks and chimes.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RaidClock.generated.h"

class UStaticMeshComponent;

UCLASS(Blueprintable)
class CRANK_API ARaidClock : public AActor
{
	GENERATED_BODY()

public:
	ARaidClock();

	virtual void Tick(float DeltaSeconds) override;

	/** Clock time shown when the raid starts (minutes before midnight shown = raid length). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Clock")
	float StartMinutePastEleven = 30.f;

	/** Flip if the hands turn the wrong way for the mesh orientation. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Clock")
	float RotationSign = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Clock")
	float PendulumAmplitude = 10.f;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Clock")
	TObjectPtr<UStaticMeshComponent> Body;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Clock")
	TObjectPtr<USceneComponent> FacePivot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Clock")
	TObjectPtr<UStaticMeshComponent> HourHand;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Clock")
	TObjectPtr<UStaticMeshComponent> MinuteHand;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Clock")
	TObjectPtr<USceneComponent> PendulumPivot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Clock")
	TObjectPtr<UStaticMeshComponent> Pendulum;

	float TickPhase = 0.f;
	int32 LastSecond = -1;
	bool bChimed = false;
};
