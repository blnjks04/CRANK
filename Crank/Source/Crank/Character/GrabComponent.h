// Left / right hand grabbing (Human Fall Flat style inputs), carrying, throwing. Design doc 3.8.
// Server authoritative. Hand states replicate for poses.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CrankTypes.h"
#include "Gameplay/Grabbable.h"
#include "GrabComponent.generated.h"

class AWindupCharacter;
class UPrimitiveComponent;

USTRUCT(BlueprintType)
struct FCrankHandState
{
	GENERATED_BODY()

	UPROPERTY()
	TObjectPtr<AActor> Actor = nullptr;

	UPROPERTY()
	TObjectPtr<UPrimitiveComponent> Component = nullptr;

	UPROPERTY()
	EGrabStyle Style = EGrabStyle::None;

	/** Input held (server view). */
	UPROPERTY()
	bool bWantsGrab = false;

	bool IsHolding() const { return Actor != nullptr; }
};

UCLASS(ClassGroup = (Crank), meta = (BlueprintSpawnableComponent))
class CRANK_API UGrabComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UGrabComponent();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** Owning client input entry point (mouse buttons / triggers). */
	void SetGrabInput(ECrankHand Hand, bool bPressed);

	/** Owning client: the steady-wind key also closes the left hand while held. */
	void SetHoldSourceInput(bool bPressed);

	/** Owning client: open any hand whose buttons are physically up (lost release events). */
	void ReleaseStaleInputs(bool bLeftDown, bool bRightDown);

	/** Server: AI / scripted input. */
	void SetGrabInputServer(ECrankHand Hand, bool bPressed);

	/** Server: let go of everything. */
	void ReleaseAll(bool bAllowThrow);

	/** Server: let go of one hand. */
	void ReleaseHand(ECrankHand Hand, bool bAllowThrow);

	/** Server: drop any hand holding this actor. */
	void ReleaseActor(AActor* Actor);

	const FCrankHandState& GetHand(ECrankHand Hand) const { return Hand == ECrankHand::Left ? LeftHand : RightHand; }

	bool IsHoldingActor(const AActor* Actor) const;
	bool IsHoldingAnything() const { return LeftHand.IsHolding() || RightHand.IsHolding(); }
	bool IsCarrying() const;
	bool IsAnchored() const;
	AActor* GetCarriedActor() const;
	int32 CountHandsOn(const AActor* Actor) const;

	/** Input currently held (local prediction for the owner, replicated for others). */
	bool WantsGrab(ECrankHand Hand) const;

	/** Where carried objects float in front of the chest (world). */
	FVector GetCarryPoint() const;

	/** Hand reach probe (world). */
	FVector GetReachPoint(ECrankHand Hand) const;

	/** Best grabbable for a hand (any machine, used for prompts too). */
	bool FindBestTarget(ECrankHand Hand, AActor*& OutActor, UPrimitiveComponent*& OutComp) const;

	/** Hold progress of whatever a hand holds (lever / cell) or -1. */
	float GetHoldProgress() const;

	UPROPERTY(EditAnywhere, Category = "Grab")
	float ReachRadius = 55.f;

	UPROPERTY(EditAnywhere, Category = "Grab")
	float ThrowSpeed = 650.f;

	UPROPERTY(EditAnywhere, Category = "Grab")
	float ThrowUpSpeed = 330.f;

protected:
	UFUNCTION(Server, Reliable)
	void ServerSetGrabInput(ECrankHand Hand, bool bPressed);

	UFUNCTION()
	void OnRep_Hands();

	FCrankHandState& HandRef(ECrankHand Hand) { return Hand == ECrankHand::Left ? LeftHand : RightHand; }

	void TickHand(ECrankHand Hand, float DeltaTime);
	bool TryGrab(ECrankHand Hand);
	void DoGrab(ECrankHand Hand, AActor* Actor, UPrimitiveComponent* Comp);
	bool CanUseHands() const;
	FVector ComputeThrowVelocity() const;

	AWindupCharacter* GetDoll() const;

	UPROPERTY(ReplicatedUsing = OnRep_Hands)
	FCrankHandState LeftHand;

	UPROPERTY(ReplicatedUsing = OnRep_Hands)
	FCrankHandState RightHand;

	void ApplyLocalWants(ECrankHand Hand);

	/** Owner-local raw inputs and the combined, predicted per-hand state. */
	bool bLocalButton[2] = { false, false };
	bool bLocalHoldSource = false;
	bool bLocalWants[2] = { false, false };
};
