// Floor area that changes how dolls move: cold fridge floor (drain x1.5), slippery cloth / water.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SurfaceZone.generated.h"

class UBoxComponent;
class UCrankFXEmitterComponent;

UCLASS(Blueprintable)
class CRANK_API ASurfaceZone : public AActor
{
	GENERATED_BODY()

public:
	ASurfaceZone();

	virtual void BeginPlay() override;
	virtual void OnConstruction(const FTransform& Transform) override;

	/** Torque drain multiplier (cold = 1.5). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Surface")
	float DrainMultiplier = 1.f;

	/** Ground friction (default floor = 8, slippery = 0.2). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Surface")
	float GroundFriction = 8.f;

	/** Speed multiplier while inside (tablecloth slide). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Surface")
	float SpeedBoost = 1.f;

	/** Emit cold mist puffs across the area. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Surface")
	bool bColdMist = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Surface")
	FText ZoneLabel;

	UBoxComponent* GetBox() const { return Box; }

protected:
	UFUNCTION()
	void OnBoxBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void OnBoxEnd(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Surface")
	TObjectPtr<UBoxComponent> Box;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Surface")
	TObjectPtr<UCrankFXEmitterComponent> MistFX;
};
