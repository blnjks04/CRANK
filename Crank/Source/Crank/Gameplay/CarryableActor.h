// Physics object dolls can pick up and carry (alone or together). Design doc 3.8 / 7.2 multipliers.
// Free: simulates physics (replicated). Carried: kinematic, placed between the carriers' hands on
// every machine from the replicated carrier list, so it stays smooth on clients.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CrankTypes.h"
#include "Gameplay/Grabbable.h"
#include "CarryableActor.generated.h"

class AWindupCharacter;
class UStaticMeshComponent;

USTRUCT()
struct FCarrierEntry
{
	GENERATED_BODY()

	UPROPERTY()
	TObjectPtr<AWindupCharacter> Doll = nullptr;

	UPROPERTY()
	uint8 HandMask = 0;
};

UCLASS(Blueprintable)
class CRANK_API ACarryableActor : public AActor, public IGrabbable
{
	GENERATED_BODY()

public:
	ACarryableActor();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void BeginPlay() override;

	// IGrabbable
	virtual bool CanBeGrabbed(const AWindupCharacter* By, ECrankHand Hand, const UPrimitiveComponent* Comp) const override;
	virtual int32 GetGrabPriority(const UPrimitiveComponent* Comp) const override { return GrabPriority; }
	virtual EGrabStyle GetGrabStyle(const UPrimitiveComponent* Comp) const override { return EGrabStyle::Carry; }
	virtual void OnGrabbed(AWindupCharacter* By, ECrankHand Hand, UPrimitiveComponent* Comp) override;
	virtual void OnReleased(AWindupCharacter* By, ECrankHand Hand, UPrimitiveComponent* Comp, const FVector& ThrowVelocity) override;
	virtual bool TickHeld(AWindupCharacter* By, ECrankHand Hand, UPrimitiveComponent* Comp, float DeltaTime) override;
	virtual FText GetGrabLabel(const UPrimitiveComponent* Comp) const override { return DisplayName; }

	bool IsCarried() const { return Carriers.Num() > 0; }
	int32 NumCarriers() const { return Carriers.Num(); }
	bool IsCarriedBy(const AWindupCharacter* Doll) const;
	const TArray<FCarrierEntry>& GetCarriers() const { return Carriers; }

	/** Torque drain multiplier for a carrier (7.2). */
	float GetDrainMultiplierFor(const AWindupCharacter* Doll) const;

	/** Max speed for a carrier, 0 = no cap. */
	float GetSpeedCapFor(const AWindupCharacter* Doll) const;

	/** Server: force every carrier to let go. */
	void DropFromAllCarriers();

	/** Server: delivered to a delivery zone. */
	virtual void Deliver();

	ECrankPartType GetPartType() const { return PartType; }
	ECarryWeight GetWeight() const { return Weight; }
	bool IsDelivered() const { return bDelivered; }

	UStaticMeshComponent* GetMesh() const { return Mesh; }

protected:
	UFUNCTION()
	void OnRep_Carriers();

	UFUNCTION(NetMulticast, Reliable)
	void MulticastDelivered();

	void ApplyCarriedPhysics(bool bCarried);
	void UpdateCarriedTransform();
	void ReleaseFree(const FVector& ThrowVelocity);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Carry")
	TObjectPtr<UStaticMeshComponent> Mesh;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Carry")
	ECarryWeight Weight = ECarryWeight::Light;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Carry")
	ECrankPartType PartType = ECrankPartType::None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Carry")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Carry")
	int32 GrabPriority = 5;

	/** Extra height above the carry point. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Carry")
	float HoldHeight = 0.f;

	/** Extra distance in front of a single carrier (big objects). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Carry")
	float HoldForward = 0.f;

	/** Two carriers farther apart than this lose their grip. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Carry")
	float MaxCarrierSeparation = 340.f;

	/** Mass override (kg) when simulating. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Carry")
	float MassKg = 4.f;

	UPROPERTY(ReplicatedUsing = OnRep_Carriers)
	TArray<FCarrierEntry> Carriers;

	/** Rotation relative to the first carrier's yaw, captured on pick up. */
	UPROPERTY(Replicated)
	FRotator GrabRelativeRotation = FRotator::ZeroRotator;

	UPROPERTY(Replicated)
	bool bDelivered = false;

	FVector SpawnLocation = FVector::ZeroVector;
};
