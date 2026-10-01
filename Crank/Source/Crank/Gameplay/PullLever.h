// Hold-to-pull lever (zone C crawl-gap lever, zone B spatula release). Activates mechanisms.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Gameplay/Grabbable.h"
#include "PullLever.generated.h"

class UBoxComponent;
class UStaticMeshComponent;
class ACrankMechanism;

UCLASS(Blueprintable)
class CRANK_API APullLever : public AActor, public IGrabbable
{
	GENERATED_BODY()

public:
	APullLever();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void Tick(float DeltaSeconds) override;

	// IGrabbable
	virtual bool CanBeGrabbed(const AWindupCharacter* By, ECrankHand Hand, const UPrimitiveComponent* Comp) const override;
	virtual int32 GetGrabPriority(const UPrimitiveComponent* Comp) const override { return 8; }
	virtual EGrabStyle GetGrabStyle(const UPrimitiveComponent* Comp) const override { return EGrabStyle::Anchor; }
	virtual void OnGrabbed(AWindupCharacter* By, ECrankHand Hand, UPrimitiveComponent* Comp) override;
	virtual void OnReleased(AWindupCharacter* By, ECrankHand Hand, UPrimitiveComponent* Comp, const FVector& ThrowVelocity) override;
	virtual bool TickHeld(AWindupCharacter* By, ECrankHand Hand, UPrimitiveComponent* Comp, float DeltaTime) override;
	virtual float GetHoldProgress(const UPrimitiveComponent* Comp) const override { return Progress; }
	virtual FText GetGrabLabel(const UPrimitiveComponent* Comp) const override { return Label; }

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Lever")
	TArray<TObjectPtr<ACrankMechanism>> Targets;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Lever")
	float HoldTime = 1.0f;

	/** Stays pulled forever once activated. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Lever")
	bool bOneShot = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Lever")
	FRotator PulledRotation = FRotator(-70.f, 0.f, 0.f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Lever")
	FText Label;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Lever")
	TObjectPtr<UStaticMeshComponent> Base;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Lever")
	TObjectPtr<USceneComponent> Pivot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Lever")
	TObjectPtr<UStaticMeshComponent> Handle;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Lever")
	TObjectPtr<UBoxComponent> GrabBox;

	UPROPERTY(Replicated)
	float Progress = 0.f;

	UPROPERTY(Replicated)
	bool bDone = false;

	int32 HoldersCount = 0;
};
