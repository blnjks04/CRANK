#include "Gameplay/Grabbable.h"
#include "Components/PrimitiveComponent.h"

FVector IGrabbable::GetGrabPoint(const UPrimitiveComponent* Comp, const FVector& HandLocation) const
{
	if (!Comp)
	{
		return HandLocation;
	}
	FVector Closest;
	if (Comp->GetClosestPointOnCollision(HandLocation, Closest) >= 0.f)
	{
		return Closest;
	}
	return Comp->GetComponentLocation();
}

FText IGrabbable::GetGrabLabel(const UPrimitiveComponent* Comp) const
{
	return NSLOCTEXT("Crank", "GrabGeneric", "잡기");
}
