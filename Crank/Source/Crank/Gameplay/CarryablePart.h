// Raid parts (gear, spring coil, jewel bearing, main spring) and arena tools (dishcloth).
// Type / weight / mesh are set in the BP_Part_* blueprints.

#pragma once

#include "CoreMinimal.h"
#include "Gameplay/CarryableActor.h"
#include "CarryablePart.generated.h"

class UPointLightComponent;

UCLASS(Blueprintable)
class CRANK_API ACarryablePart : public ACarryableActor
{
	GENERATED_BODY()

public:
	ACarryablePart();

	virtual void Tick(float DeltaSeconds) override;

	/** Gentle glow so parts read from afar. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Part")
	TObjectPtr<UPointLightComponent> Glow;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Part")
	bool bGlow = true;

protected:
	float GlowTime = 0.f;
};

/** Spent battery cell that pops out of the boss (cosmetic carryable). */
UCLASS(Blueprintable)
class CRANK_API ABatteryCellPickup : public ACarryableActor
{
	GENERATED_BODY()

public:
	ABatteryCellPickup();
};
