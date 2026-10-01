// Kitchen drawer that dolls pull open by grabbing the handle and walking backwards (zone C stairs).

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Gameplay/Grabbable.h"
#include "PullDrawer.generated.h"

class UBoxComponent;
class UStaticMeshComponent;

UCLASS(Blueprintable)
class CRANK_API APullDrawer : public AActor, public IGrabbable
{
	GENERATED_BODY()

public:
	APullDrawer();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void OnConstruction(const FTransform& Transform) override;

	// IGrabbable
	virtual int32 GetGrabPriority(const UPrimitiveComponent* Comp) const override { return 8; }
	virtual EGrabStyle GetGrabStyle(const UPrimitiveComponent* Comp) const override { return EGrabStyle::Anchor; }
	virtual void OnGrabbed(AWindupCharacter* By, ECrankHand Hand, UPrimitiveComponent* Comp) override;
	virtual void OnReleased(AWindupCharacter* By, ECrankHand Hand, UPrimitiveComponent* Comp, const FVector& ThrowVelocity) override;
	virtual bool TickHeld(AWindupCharacter* By, ECrankHand Hand, UPrimitiveComponent* Comp, float DeltaTime) override;
	virtual FText GetGrabLabel(const UPrimitiveComponent* Comp) const override;

	/** How far the drawer slides out (local +X). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Drawer")
	float MaxExtent = 320.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Drawer")
	float SlideSpeed = 260.f;

	/** Initial opening (0..MaxExtent). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Drawer")
	float StartExtent = 0.f;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Drawer")
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Drawer")
	TObjectPtr<UStaticMeshComponent> Drawer;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Drawer")
	TObjectPtr<UStaticMeshComponent> HandleMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Drawer")
	TObjectPtr<UBoxComponent> HandleBox;

	UPROPERTY(Replicated)
	float Extent = 0.f;

	TWeakObjectPtr<AWindupCharacter> Holder;
	float DesiredExtent = 0.f;
	float VisualExtent = 0.f;
	float HoldOffset = 0.f;
	bool bWasMoving = false;
};
