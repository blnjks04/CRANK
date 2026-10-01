// Base for level mechanisms switched by levers / events (drawbridges, tablecloth shortcut).

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CrankMechanism.generated.h"

class UStaticMeshComponent;

UCLASS(Abstract, Blueprintable)
class CRANK_API ACrankMechanism : public AActor
{
	GENERATED_BODY()

public:
	ACrankMechanism();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Server. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Mechanism")
	virtual void SetActivated(bool bNewActivated);

	UFUNCTION(BlueprintPure, Category = "Mechanism")
	bool IsActivated() const { return bActivated; }

	/** Start activated (e.g. a bridge that is already down). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mechanism")
	bool bStartActivated = false;

protected:
	virtual void BeginPlay() override;

	UFUNCTION()
	void OnRep_Activated();

	/** Called on every machine when the state flips. */
	virtual void OnActivationChanged(bool bInstant) {}

	UPROPERTY(ReplicatedUsing = OnRep_Activated)
	bool bActivated = false;

	float ActivationTime = -100.f;
};

/** A plank that swings down on a hinge: spatula bridges in zone B, cutting-board ramp to zone D. */
UCLASS(Blueprintable)
class CRANK_API ADrawbridge : public ACrankMechanism
{
	GENERATED_BODY()

public:
	ADrawbridge();

	virtual void Tick(float DeltaSeconds) override;

	/** Hinge rotation when closed (raised) / open (lowered). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Drawbridge")
	FRotator ClosedRotation = FRotator(80.f, 0.f, 0.f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Drawbridge")
	FRotator OpenRotation = FRotator::ZeroRotator;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Drawbridge")
	float SwingTime = 1.2f;

protected:
	virtual void OnActivationChanged(bool bInstant) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Drawbridge")
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Drawbridge")
	TObjectPtr<USceneComponent> Hinge;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Drawbridge")
	TObjectPtr<UStaticMeshComponent> Plank;

	float Alpha = 0.f;
	bool bLandedFX = true;
};

/** The tablecloth curtain that blocks the way home until the boss is beaten (design doc 4.1 / 6.7). */
UCLASS(Blueprintable)
class CRANK_API ATableclothShortcut : public ACrankMechanism
{
	GENERATED_BODY()

public:
	ATableclothShortcut();

	virtual void Tick(float DeltaSeconds) override;

protected:
	virtual void OnActivationChanged(bool bInstant) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Shortcut")
	TObjectPtr<USceneComponent> Root;

	/** Hanging cloth that blocks the passage. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Shortcut")
	TObjectPtr<UStaticMeshComponent> Curtain;

	/** Cloth lying on the floor once it fell (the slide). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Shortcut")
	TObjectPtr<UStaticMeshComponent> FallenCloth;

	float Alpha = 0.f;
	FVector CurtainStart = FVector::ZeroVector;
};
