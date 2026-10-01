// Phase 2+ mop mode: slippery water puddles left behind the boss for 8 s (friction 0.2).

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "WaterTrail.generated.h"

class UInstancedStaticMeshComponent;

USTRUCT()
struct FWaterPoint
{
	GENERATED_BODY()

	UPROPERTY()
	FVector_NetQuantize Location = FVector::ZeroVector;

	UPROPERTY()
	float SpawnTime = 0.f;	// server world time
};

UCLASS(Blueprintable)
class CRANK_API AWaterTrail : public AActor
{
	GENERATED_BODY()

public:
	AWaterTrail();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void Tick(float DeltaSeconds) override;

	/** Server. */
	void AddPoint(const FVector& Location);

	/** Any machine. */
	bool IsWetAt(const FVector& FootLocation) const;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Water")
	float PuddleRadius = 75.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Water")
	float Lifetime = 8.f;

protected:
	UFUNCTION()
	void OnRep_Points();

	void RebuildVisuals();
	float Now() const;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Water")
	TObjectPtr<UInstancedStaticMeshComponent> Puddles;

	UPROPERTY(ReplicatedUsing = OnRep_Points)
	TArray<FWaterPoint> Points;

	float VisualRefresh = 0.f;
};
