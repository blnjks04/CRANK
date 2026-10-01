// Part drop-off (zone tubes for optional parts, the workbench for the main spring).

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CrankTypes.h"
#include "DeliveryZone.generated.h"

class UBoxComponent;
class UStaticMeshComponent;
class UPointLightComponent;

UCLASS(Blueprintable)
class CRANK_API ADeliveryZone : public AActor
{
	GENERATED_BODY()

public:
	ADeliveryZone();

	virtual void Tick(float DeltaSeconds) override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Delivery")
	TArray<ECrankPartType> AcceptedParts;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Delivery")
	FText Label;

	bool Accepts(ECrankPartType Type) const { return AcceptedParts.Contains(Type); }

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Delivery")
	TObjectPtr<UStaticMeshComponent> Visual;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Delivery")
	TObjectPtr<UBoxComponent> Box;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Delivery")
	TObjectPtr<UPointLightComponent> Beacon;

	float Pulse = 0.f;
};
