// Respawn checkpoint per zone; also used by cheats ("Crank.Goto B") and the HUD zone name.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CrankCheckpoint.generated.h"

class UBoxComponent;
class UArrowComponent;

UCLASS(Blueprintable)
class CRANK_API ACrankCheckpoint : public AActor
{
	GENERATED_BODY()

public:
	ACrankCheckpoint();

	virtual void BeginPlay() override;

	/** Order along the raid route (higher = further). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Checkpoint")
	int32 Order = 0;

	/** Short id used by cheats: Start, A, B, C, D, E. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Checkpoint")
	FName ZoneId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Checkpoint")
	FText ZoneName;

	FTransform GetSpawnTransform(int32 PlayerIndex) const;

protected:
	UFUNCTION()
	void OnBoxBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Checkpoint")
	TObjectPtr<UBoxComponent> Box;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Checkpoint")
	TObjectPtr<UArrowComponent> Arrow;
};
