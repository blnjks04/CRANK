// Launching a fully wound doll (design doc 3.4 / 7.3): slingshot (winder aims) or self release (F).
// Flight keeps the capsule; impacts / landing turn into a 1 s ragdoll, walls can "splat".

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "LaunchComponent.generated.h"

class AWindupCharacter;

UCLASS(ClassGroup = (Crank), meta = (BlueprintSpawnableComponent))
class CRANK_API ULaunchComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	ULaunchComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** Server: launch this doll. */
	void Launch(const FVector& Velocity, AWindupCharacter* InstigatorDoll, bool bSlingshot, bool bResetTorque = true);

	/** Owning client: F key. */
	void RequestSelfLaunch();

	/** Server: capsule hit something while flying. */
	void HandleFlightHit(AActor* Other, UPrimitiveComponent* OtherComp, const FHitResult& Hit);

	/** Server: landed on the floor while flying / splatted. */
	void HandleFlightLanded(const FHitResult& Hit);

	bool IsSlingshotFlight() const { return bSlingFlight; }
	FVector GetFlightVelocity() const { return LastFlightVelocity; }
	float GetFlightTime() const;

	/** Minimum wall impact speed for the comedy splat. */
	UPROPERTY(EditAnywhere, Category = "Launch")
	float SplatSpeed = 700.f;

	/** How long the doll stays flattened on a wall before sliding. */
	UPROPERTY(EditAnywhere, Category = "Launch")
	float SplatStickTime = 0.8f;

protected:
	UFUNCTION(Server, Reliable)
	void ServerSelfLaunch(FVector_NetQuantizeNormal Direction);

	void BeginSplat(const FHitResult& Hit);

	AWindupCharacter* GetDoll() const;

	float FlightStartTime = 0.f;
	FVector LastFlightVelocity = FVector::ZeroVector;
	bool bSlingFlight = false;
	float SplatStartTime = 0.f;
	bool bSplatSliding = false;
	TWeakObjectPtr<AWindupCharacter> LastInstigator;
};
