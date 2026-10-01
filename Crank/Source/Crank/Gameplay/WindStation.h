// Music box wind station (design doc 3.9): stand on it to be wound slowly up to 60 T.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "WindStation.generated.h"

class UBoxComponent;
class UStaticMeshComponent;
class UAudioComponent;
class UCrankFXEmitterComponent;

UCLASS(Blueprintable)
class CRANK_API AWindStation : public AActor
{
	GENERATED_BODY()

public:
	AWindStation();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Station")
	float ChargeRate = 5.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Station")
	float ChargeCap = 60.f;

	/** Degrees per second the cylinder / dancer spins while active. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Station")
	float SpinSpeed = 90.f;

	bool IsActive() const { return bActive; }

protected:
	UFUNCTION()
	void OnRep_Active();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Station")
	TObjectPtr<UStaticMeshComponent> Body;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Station")
	TObjectPtr<UStaticMeshComponent> Spinner;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Station")
	TObjectPtr<UStaticMeshComponent> Crank;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Station")
	TObjectPtr<UBoxComponent> Pad;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Station")
	TObjectPtr<UCrankFXEmitterComponent> NotesFX;

	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> Music;

	UPROPERTY(ReplicatedUsing = OnRep_Active)
	bool bActive = false;
};
