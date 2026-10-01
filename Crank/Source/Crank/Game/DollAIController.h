// Simple AI for a dummy doll (M1 "wind a dummy and watch it walk", solo testing partner).
// Walks after the nearest player while it has torque, and winds a resting player whose torque is low.
// Can itself be wound, launched and carried like any doll.

#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "DollAIController.generated.h"

class AWindupCharacter;

UCLASS()
class CRANK_API ADollAIController : public AAIController
{
	GENERATED_BODY()

public:
	ADollAIController();

	virtual void Tick(float DeltaSeconds) override;

	UPROPERTY(EditAnywhere, Category = "Dummy")
	float FollowDistance = 240.f;

	/** Help a player who stands still with less torque than this. */
	UPROPERTY(EditAnywhere, Category = "Dummy")
	float HelpBelowTorque = 60.f;

	/** Let go of the key once the player reaches this torque. */
	UPROPERTY(EditAnywhere, Category = "Dummy")
	float HelpUntilTorque = 95.f;

	/** How long the player must rest before the dummy walks over to wind them. */
	UPROPERTY(EditAnywhere, Category = "Dummy")
	float HelpAfterStill = 1.0f;

protected:
	AWindupCharacter* FindClosestPlayer(const AWindupCharacter* Doll, float& OutDist) const;
	void StopHelping(AWindupCharacter* Doll);

	TWeakObjectPtr<AWindupCharacter> HelpTarget;
	float StillTime = 0.f;
	float HelpCooldown = 0.f;
};
