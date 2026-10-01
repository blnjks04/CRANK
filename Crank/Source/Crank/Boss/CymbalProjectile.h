// Boomerang cymbal thrown by "짝짝이 대왕" (phase 2+): curves out to the target and comes back to the hand.
// The path is a pure function of the replicated launch parameters, so every machine flies it locally;
// the server alone knocks over the dolls it passes through.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CymbalProjectile.generated.h"

class UStaticMeshComponent;
class UAudioComponent;
class AWindupCharacter;

UCLASS(Blueprintable)
class CRANK_API ACymbalProjectile : public AActor
{
	GENERATED_BODY()

public:
	ACymbalProjectile();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	/** Server, before FinishSpawning. CurveSign bends the path to one side (+1 / -1). */
	void InitFlight(const FVector& InFrom, const FVector& InTo, float InCurveSign, float InOutTime);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cymbal")
	float HitRadius = 130.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cymbal")
	float SpinRate = 1080.f;

protected:
	/** ReturnTo: where the thrower's hand is now (the way back homes in on it). */
	FVector EvalPath(float T, const FVector& ReturnTo) const;
	FVector GetReturnPoint() const;
	float ServerNow() const;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Cymbal")
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Cymbal")
	TObjectPtr<UStaticMeshComponent> Disc;

	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> WhooshAudio;

	UPROPERTY(Replicated)
	FVector_NetQuantize From;

	UPROPERTY(Replicated)
	FVector_NetQuantize To;

	UPROPERTY(Replicated)
	float CurveSign = 1.f;

	UPROPERTY(Replicated)
	float OutTime = 1.15f;

	UPROPERTY(Replicated)
	float LaunchTime = 0.f;

	TArray<TWeakObjectPtr<AWindupCharacter>> AlreadyHit;
	float SpinAngle = 0.f;
	bool bDone = false;
};
