// Boss charging dock (design doc 6.5). Phase 3: the boss returns here; 3 s on the dock restores a cell.
// Covering it with the dishcloth disables it for 15 s.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ChargingDock.generated.h"

class UBoxComponent;
class UStaticMeshComponent;
class UPointLightComponent;
class ACarryableActor;

UCLASS(Blueprintable)
class CRANK_API AChargingDock : public AActor
{
	GENERATED_BODY()

public:
	AChargingDock();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void Tick(float DeltaSeconds) override;

	bool IsDisabled() const { return bDisabled; }

	/** Where the boss parks (world). */
	FVector GetDockPoint() const;

	/** Direction the boss should face when parked. */
	FVector GetDockFacing() const;

	/** Server: visual / audio feedback while the boss charges. */
	void SetCharging(bool bInCharging);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dock")
	float DisableTime = 15.f;

protected:
	UFUNCTION()
	void OnRep_State();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Dock")
	TObjectPtr<UStaticMeshComponent> Base;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Dock")
	TObjectPtr<UBoxComponent> ClothZone;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Dock")
	TObjectPtr<USceneComponent> ParkPoint;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Dock")
	TObjectPtr<UPointLightComponent> StatusLight;

	UPROPERTY(ReplicatedUsing = OnRep_State)
	bool bDisabled = false;

	UPROPERTY(ReplicatedUsing = OnRep_State)
	bool bCharging = false;

	float DisabledUntil = 0.f;
	TWeakObjectPtr<ACarryableActor> CoveringCloth;
	float Blink = 0.f;
};
