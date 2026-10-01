// Toy room arena of "짝짝이 대왕" (zone F): wakes the giant when a doll walks in and keeps its hops inside.
// Checkpoints inside the bounds become safe spots: the giant never stands / lands near a respawn point.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GiantArena.generated.h"

class UBoxComponent;
class ABossGiantMonkey;
class ACrankMechanism;

UCLASS(Blueprintable)
class CRANK_API AGiantArena : public AActor
{
	GENERATED_BODY()

public:
	AGiantArena();

	virtual void BeginPlay() override;

	bool ContainsPoint(const FVector& Location, float Margin = 0.f) const;

	/** Clamp a ground point into the arena (XY, keeps Z) and out of the safe spots. */
	FVector ClampPoint(const FVector& Location, float Margin) const;

	/** Is this point near a respawn checkpoint? */
	bool IsInSafeSpot(const FVector& Location, float Extra = 0.f) const;

	/** Server. */
	void OnGiantDefeated(ABossGiantMonkey* InBoss);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Arena")
	TObjectPtr<ABossGiantMonkey> Boss;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Arena")
	TArray<TObjectPtr<ACrankMechanism>> OpenOnDefeat;

	/** Radius around checkpoints inside the arena that the giant keeps out of (its own capsule included). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Arena")
	float SafeRadius = 1300.f;

protected:
	UFUNCTION()
	void OnWakeEnter(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Arena")
	TObjectPtr<UBoxComponent> Bounds;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Arena")
	TObjectPtr<UBoxComponent> WakeTrigger;

	/** Checkpoint spawn points inside the bounds (gathered at BeginPlay). */
	TArray<FVector> SafeSpots;
};
