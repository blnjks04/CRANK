// Overwind rupture: the doll pops into 5 parts; a partner revives it by bringing head + key to the torso.
// Design doc 3.7 / 7.4.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CrankTypes.h"
#include "ScatterReviveComponent.generated.h"

class AWindupCharacter;
class ABodyPartPickup;

UCLASS(ClassGroup = (Crank), meta = (BlueprintSpawnableComponent))
class CRANK_API UScatterReviveComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UScatterReviveComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** Server: burst into parts. Instigator (the winder) gets knocked back. */
	void Rupture(AWindupCharacter* Instigator);

	/** Server: a required part reached the torso. */
	void AttachPart(EBodyPartType Type, bool bInverted);

	/** Server: put the doll back together. */
	void Revive();

	ABodyPartPickup* GetTorso() const { return Torso.Get(); }
	bool IsScattered() const;

	/** Parts still missing for the HUD (bit 0 head, bit 1 key). */
	uint8 GetMissingMask() const { return (bHeadAttached ? 0 : 1) | (bKeyAttached ? 0 : 2); }

protected:
	AWindupCharacter* GetDoll() const;

	TArray<TWeakObjectPtr<ABodyPartPickup>> Parts;
	TWeakObjectPtr<ABodyPartPickup> Torso;
	bool bHeadAttached = false;
	bool bKeyAttached = false;
	bool bHeadInverted = false;
};
