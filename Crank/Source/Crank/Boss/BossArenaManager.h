// Under-table arena (design doc 6.6): wakes the boss when a doll walks in, keeps the boss inside,
// opens the tablecloth shortcut after the fight.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BossArenaManager.generated.h"

class UBoxComponent;
class ABossDustEater;
class ACrankMechanism;

UCLASS(Blueprintable)
class CRANK_API ABossArenaManager : public AActor
{
	GENERATED_BODY()

public:
	ABossArenaManager();

	virtual void BeginPlay() override;

	bool ContainsPoint(const FVector& Location, float Margin = 0.f) const;

	/** Server. */
	void OnBossDefeated(ABossDustEater* InBoss);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Arena")
	TObjectPtr<ABossDustEater> Boss;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Arena")
	TArray<TObjectPtr<ACrankMechanism>> OpenOnDefeat;

protected:
	UFUNCTION()
	void OnArenaEnter(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Arena")
	TObjectPtr<UBoxComponent> Bounds;

	/** Smaller trigger: walking this far in wakes the boss. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Arena")
	TObjectPtr<UBoxComponent> WakeTrigger;
};
