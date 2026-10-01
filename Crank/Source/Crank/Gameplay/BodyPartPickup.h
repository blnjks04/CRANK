// One of the five parts a ruptured doll scatters into (design doc 3.7).
// The torso is the anchor: bring head + key to it to revive the doll.

#pragma once

#include "CoreMinimal.h"
#include "Gameplay/CarryableActor.h"
#include "BodyPartPickup.generated.h"

class UStaticMesh;
class UMaterialInstanceDynamic;

UCLASS(Blueprintable)
class CRANK_API ABodyPartPickup : public ACarryableActor
{
	GENERATED_BODY()

public:
	ABodyPartPickup();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void BeginPlay() override;
	virtual void OnConstruction(const FTransform& Transform) override;

	virtual bool CanBeGrabbed(const AWindupCharacter* By, ECrankHand Hand, const UPrimitiveComponent* Comp) const override;
	virtual FText GetGrabLabel(const UPrimitiveComponent* Comp) const override;

	/** Server: configure after spawning. */
	void InitPart(EBodyPartType InType, AWindupCharacter* InOwnerDoll);

	EBodyPartType GetBodyPartType() const { return BodyPartType; }
	AWindupCharacter* GetOwnerDoll() const { return OwnerDoll; }
	bool IsRequiredPart() const { return BodyPartType == EBodyPartType::Head || BodyPartType == EBodyPartType::Key; }

	/** Meshes per part type (assigned in the blueprint). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Body Part")
	TMap<EBodyPartType, TObjectPtr<UStaticMesh>> PartMeshes;

	/** Material slot index that receives the owner's paint color. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Body Part")
	int32 PaintMaterialIndex = 0;

	/** Distance at which a head / key snaps onto the torso. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Body Part")
	float AttachRadius = 85.f;

protected:
	UFUNCTION()
	void OnRep_PartSetup();

	void ApplyPartVisuals();

	UPROPERTY(ReplicatedUsing = OnRep_PartSetup)
	EBodyPartType BodyPartType = EBodyPartType::Torso;

	UPROPERTY(ReplicatedUsing = OnRep_PartSetup)
	TObjectPtr<AWindupCharacter> OwnerDoll;

	/** Torso only: which required parts are already on. */
	UPROPERTY(ReplicatedUsing = OnRep_PartSetup)
	uint8 AttachedMask = 0;

	UPROPERTY(VisibleAnywhere, Category = "Body Part")
	TObjectPtr<UStaticMeshComponent> AttachedHead;

	UPROPERTY(VisibleAnywhere, Category = "Body Part")
	TObjectPtr<UStaticMeshComponent> AttachedKey;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> PaintMID;

	float NextBlinkTime = 0.f;
	float BlinkEndTime = 0.f;
};
