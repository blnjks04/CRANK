// Torque (T): leg power of a windup doll. Server authoritative, replicated.
// Design doc 3.1 / 3.2 / 3.5 / 7.1 / 7.2

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CrankTypes.h"
#include "TorqueComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnTorqueBandChanged, ETorqueBand, OldBand, ETorqueBand, NewBand);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnTorqueChanged, float, NewTorque);

UCLASS(ClassGroup = (Crank), meta = (BlueprintSpawnableComponent))
class CRANK_API UTorqueComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UTorqueComponent();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	UFUNCTION(BlueprintPure, Category = "Torque")
	float GetTorque() const { return Torque; }

	UFUNCTION(BlueprintPure, Category = "Torque")
	ETorqueBand GetBand() const { return CrankUtil::BandForTorque(Torque); }

	UFUNCTION(BlueprintPure, Category = "Torque")
	bool IsDischarged() const { return Torque <= 0.f; }

	UFUNCTION(BlueprintPure, Category = "Torque")
	bool CanLaunch() const;

	/** Overwind launch bonus (0..0.5). */
	UFUNCTION(BlueprintPure, Category = "Torque")
	float GetOverwindBonus() const;

	/** Server: set torque (clamped to 0..120). */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Torque")
	void SetTorque(float NewTorque);

	/** Server: add (or remove) torque. Returns the applied delta. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Torque")
	float AddTorque(float Delta);

	/** Server: spend torque if available (jump, get up). Returns false if not enough. */
	bool TrySpend(float Amount);

	/** Max walk speed for the current band (before carry caps). */
	float GetBandSpeed() const;

	/** Drain per second while moving for the current band (before multipliers). */
	float GetBandDrain() const;

	/** Stop draining (e.g. while trapped the boss drains instead). */
	UPROPERTY(Transient)
	bool bSuspendMovementDrain = false;

	UPROPERTY(BlueprintAssignable, Category = "Torque")
	FOnTorqueBandChanged OnBandChanged;

	UPROPERTY(BlueprintAssignable, Category = "Torque")
	FOnTorqueChanged OnTorqueChanged;

	/** Seconds spent continuously at 0 T (server). */
	float GetTimeDischarged() const { return TimeDischarged; }

protected:
	UFUNCTION()
	void OnRep_Torque(float OldTorque);

	void HandleTorqueChanged(float OldTorque);
	void ApplyMovementSpeed();

	UPROPERTY(ReplicatedUsing = OnRep_Torque, VisibleInstanceOnly, Category = "Torque")
	float Torque = 0.f;

	float TimeDischarged = 0.f;
	float NetDirtyAccumulator = 0.f;
};
