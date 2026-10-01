// Interface for anything a doll can grab with its hands (design doc 3.8).
// All calls happen on the server unless stated otherwise.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "CrankTypes.h"
#include "Grabbable.generated.h"

class AWindupCharacter;
class UPrimitiveComponent;

/** How a grab behaves for the grabbing doll. */
UENUM(BlueprintType)
enum class EGrabStyle : uint8
{
	None,
	Carry,		// object is lifted and follows the doll (parts, body parts, dishcloth, main spring)
	Anchor,		// doll holds on to a fixed/moving handle (lever, drawer, battery cell, chair leg)
	Friend,		// partner's body (one hand = tug, two hands = piggyback)
};

UINTERFACE(MinimalAPI, meta = (CannotImplementInterfaceInBlueprint))
class UGrabbable : public UInterface
{
	GENERATED_BODY()
};

class CRANK_API IGrabbable
{
	GENERATED_BODY()

public:
	/** Server: may this doll grab the given component with the given hand right now? */
	virtual bool CanBeGrabbed(const AWindupCharacter* By, ECrankHand Hand, const UPrimitiveComponent* Comp) const { return true; }

	/** Higher priority wins when several grabbables are in reach. */
	virtual int32 GetGrabPriority(const UPrimitiveComponent* Comp) const { return 0; }

	/** World point used to measure reach / attach the hand. */
	virtual FVector GetGrabPoint(const UPrimitiveComponent* Comp, const FVector& HandLocation) const;

	virtual EGrabStyle GetGrabStyle(const UPrimitiveComponent* Comp) const { return EGrabStyle::Carry; }

	/** Server: a hand took hold. */
	virtual void OnGrabbed(AWindupCharacter* By, ECrankHand Hand, UPrimitiveComponent* Comp) {}

	/** Server: a hand let go. bThrow is true when the doll was moving while releasing. */
	virtual void OnReleased(AWindupCharacter* By, ECrankHand Hand, UPrimitiveComponent* Comp, const FVector& ThrowVelocity) {}

	/** Server: called every tick while held. Return false to force the hand to let go. */
	virtual bool TickHeld(AWindupCharacter* By, ECrankHand Hand, UPrimitiveComponent* Comp, float DeltaTime) { return true; }

	/** 0..1 progress for hold-to-activate grabbables (lever, cell, dust bin lever); <0 when not applicable. */
	virtual float GetHoldProgress(const UPrimitiveComponent* Comp) const { return -1.f; }

	/** Short HUD label (Korean). */
	virtual FText GetGrabLabel(const UPrimitiveComponent* Comp) const;
};
