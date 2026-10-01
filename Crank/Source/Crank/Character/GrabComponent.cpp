#include "Character/GrabComponent.h"
#include "Character/WindupCharacter.h"
#include "Character/WindInteractionComponent.h"
#include "FX/CrankSound.h"
#include "Net/UnrealNetwork.h"
#include "Engine/World.h"
#include "Engine/OverlapResult.h"
#include "Components/PrimitiveComponent.h"
#include "GameFramework/CharacterMovementComponent.h"

UGrabComponent::UGrabComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PrePhysics;
	SetIsReplicatedByDefault(true);
}

void UGrabComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UGrabComponent, LeftHand);
	DOREPLIFETIME(UGrabComponent, RightHand);
}

AWindupCharacter* UGrabComponent::GetDoll() const
{
	return Cast<AWindupCharacter>(GetOwner());
}

bool UGrabComponent::WantsGrab(ECrankHand Hand) const
{
	const AWindupCharacter* Doll = GetDoll();
	if (Doll && Doll->IsLocallyControlled())
	{
		return bLocalWants[(int32)Hand];
	}
	return GetHand(Hand).bWantsGrab;
}

void UGrabComponent::SetGrabInput(ECrankHand Hand, bool bPressed)
{
	bLocalButton[(int32)Hand] = bPressed;
	ApplyLocalWants(Hand);
}

void UGrabComponent::ReleaseStaleInputs(bool bLeftDown, bool bRightDown)
{
	if (bLocalButton[(int32)ECrankHand::Left] && !bLeftDown)
	{
		UE_LOG(LogCrank, Verbose, TEXT("%s releasing stale left grab input"), *GetNameSafe(GetOwner()));
		SetGrabInput(ECrankHand::Left, false);
	}
	if (bLocalButton[(int32)ECrankHand::Right] && !bRightDown)
	{
		UE_LOG(LogCrank, Verbose, TEXT("%s releasing stale right grab input"), *GetNameSafe(GetOwner()));
		SetGrabInput(ECrankHand::Right, false);
	}
}

void UGrabComponent::SetHoldSourceInput(bool bPressed)
{
	bLocalHoldSource = bPressed;
	ApplyLocalWants(ECrankHand::Left);
}

void UGrabComponent::ApplyLocalWants(ECrankHand Hand)
{
	// The steady-wind key (E) also holds the left hand; the hand opens only when both are released.
	const bool bPressed = bLocalButton[(int32)Hand] || (Hand == ECrankHand::Left && bLocalHoldSource);
	if (bLocalWants[(int32)Hand] == bPressed)
	{
		return;
	}
	bLocalWants[(int32)Hand] = bPressed;

	AWindupCharacter* Doll = GetDoll();
	if (!Doll)
	{
		return;
	}
	UE_LOG(LogCrank, Verbose, TEXT("%s grab input hand=%d pressed=%d (button=%d hold=%d)"), *Doll->GetName(),
		(int32)Hand, bPressed, bLocalButton[(int32)Hand], bLocalHoldSource);

	// Releasing the winding hand while aiming fires the slingshot (aim comes from this client).
	if (!bPressed)
	{
		Doll->GetWind()->OnLocalGrabReleased(Hand);
	}

	if (Doll->HasAuthority())
	{
		SetGrabInputServer(Hand, bPressed);
	}
	else
	{
		ServerSetGrabInput(Hand, bPressed);
	}
}

void UGrabComponent::ServerSetGrabInput_Implementation(ECrankHand Hand, bool bPressed)
{
	SetGrabInputServer(Hand, bPressed);
}

void UGrabComponent::SetGrabInputServer(ECrankHand Hand, bool bPressed)
{
	AWindupCharacter* Doll = GetDoll();
	if (!Doll)
	{
		return;
	}

	FCrankHandState& H = HandRef(Hand);
	H.bWantsGrab = bPressed;
	if (Doll->IsLocallyControlled())
	{
		bLocalWants[(int32)Hand] = bPressed;
	}
	UE_LOG(LogCrank, Verbose, TEXT("%s server grab wants hand=%d pressed=%d"), *Doll->GetName(), (int32)Hand, bPressed);

	if (!bPressed)
	{
		Doll->GetWind()->OnServerGrabReleased(Hand);
		if (H.IsHolding())
		{
			ReleaseHand(Hand, true);
		}
	}
}

bool UGrabComponent::CanUseHands() const
{
	const AWindupCharacter* Doll = GetDoll();
	return Doll && Doll->CanUseArms();
}

void UGrabComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	AWindupCharacter* Doll = GetDoll();
	if (!Doll || !Doll->HasAuthority())
	{
		return;
	}

	TickHand(ECrankHand::Left, DeltaTime);
	TickHand(ECrankHand::Right, DeltaTime);
	Doll->UpdateFriendGrab();
}

void UGrabComponent::TickHand(ECrankHand Hand, float DeltaTime)
{
	FCrankHandState& H = HandRef(Hand);
	AWindupCharacter* Doll = GetDoll();

	if (!CanUseHands())
	{
		if (H.IsHolding())
		{
			ReleaseHand(Hand, false);
		}
		return;
	}

	if (!H.bWantsGrab)
	{
		if (H.IsHolding())
		{
			ReleaseHand(Hand, true);
		}
		return;
	}

	// Hands on the partner's key are owned by the wind component.
	if (Doll->GetWind()->IsWinding())
	{
		return;
	}

	if (!H.IsHolding())
	{
		TryGrab(Hand);
		return;
	}

	if (!IsValid(H.Actor) || (H.Component && !IsValid(H.Component)))
	{
		const bool bWants = H.bWantsGrab;
		H = FCrankHandState();
		H.bWantsGrab = bWants;
		return;
	}

	if (IGrabbable* Grabbable = Cast<IGrabbable>(H.Actor))
	{
		if (!Grabbable->TickHeld(Doll, Hand, H.Component, DeltaTime))
		{
			ReleaseHand(Hand, false);
		}
	}
	else if (H.Component)
	{
		// Generic tagged anchor (chair legs, table legs...): let go when walking away.
		FVector Closest;
		H.Component->GetClosestPointOnCollision(GetReachPoint(Hand), Closest);
		if (FVector::Dist(Closest, Doll->GetActorLocation()) > 140.f)
		{
			ReleaseHand(Hand, false);
		}
	}
}

bool UGrabComponent::TryGrab(ECrankHand Hand)
{
	AWindupCharacter* Doll = GetDoll();

	// Second hand on a friend the other hand already holds = piggyback (never starts winding).
	const FCrankHandState& Other = GetHand(Hand == ECrankHand::Left ? ECrankHand::Right : ECrankHand::Left);
	if (Other.Style == EGrabStyle::Friend)
	{
		AWindupCharacter* Friend = Cast<AWindupCharacter>(Other.Actor);
		if (Friend && Friend->CanBeGrabbed(Doll, Hand, Other.Component)
			&& FVector::Dist(Friend->GetActorLocation(), Doll->GetActorLocation()) < 200.f)
		{
			DoGrab(Hand, Friend, Other.Component);
			return true;
		}
	}

	// Only the left hand (left click / E) takes a partner's key, so the right hand can grab the body.
	if (Hand == ECrankHand::Left && Doll->GetWind()->TryBeginWinding(Hand))
	{
		return true;
	}

	AActor* Actor = nullptr;
	UPrimitiveComponent* Comp = nullptr;
	if (FindBestTarget(Hand, Actor, Comp))
	{
		DoGrab(Hand, Actor, Comp);
		return true;
	}
	return false;
}

bool UGrabComponent::FindBestTarget(ECrankHand Hand, AActor*& OutActor, UPrimitiveComponent*& OutComp) const
{
	OutActor = nullptr;
	OutComp = nullptr;

	const AWindupCharacter* Doll = GetDoll();
	UWorld* World = GetWorld();
	if (!Doll || !World)
	{
		return false;
	}

	const FVector Reach = GetReachPoint(Hand);

	FCollisionObjectQueryParams ObjParams;
	ObjParams.AddObjectTypesToQuery(ECC_WorldDynamic);
	ObjParams.AddObjectTypesToQuery(ECC_WorldStatic);
	ObjParams.AddObjectTypesToQuery(ECC_PhysicsBody);
	ObjParams.AddObjectTypesToQuery(ECC_Pawn);
	ObjParams.AddObjectTypesToQuery(ECC_Boss);

	FCollisionQueryParams Params(SCENE_QUERY_STAT(CrankGrab), false, Doll);

	TArray<FOverlapResult> Overlaps;
	World->OverlapMultiByObjectType(Overlaps, Reach, FQuat::Identity, ObjParams, FCollisionShape::MakeSphere(ReachRadius), Params);

	int32 BestPriority = MIN_int32;
	float BestDist = TNumericLimits<float>::Max();

	for (const FOverlapResult& Overlap : Overlaps)
	{
		AActor* Actor = Overlap.GetActor();
		UPrimitiveComponent* Comp = Overlap.GetComponent();
		if (!Actor || !Comp || Actor == Doll)
		{
			continue;
		}

		int32 Priority = 0;
		FVector Point;
		if (IGrabbable* Grabbable = Cast<IGrabbable>(Actor))
		{
			if (!Grabbable->CanBeGrabbed(Doll, Hand, Comp))
			{
				continue;
			}
			Priority = Grabbable->GetGrabPriority(Comp);
			Point = Grabbable->GetGrabPoint(Comp, Reach);
		}
		else if (Comp->ComponentHasTag(TEXT("GrabAnchor")))
		{
			Priority = -10;
			if (Comp->GetClosestPointOnCollision(Reach, Point) < 0.f)
			{
				Point = Comp->GetComponentLocation();
			}
		}
		else
		{
			continue;
		}

		const float Dist = FVector::Dist(Point, Reach);
		if (Priority > BestPriority || (Priority == BestPriority && Dist < BestDist))
		{
			BestPriority = Priority;
			BestDist = Dist;
			OutActor = Actor;
			OutComp = Comp;
		}
	}
	return OutActor != nullptr;
}

void UGrabComponent::DoGrab(ECrankHand Hand, AActor* Actor, UPrimitiveComponent* Comp)
{
	AWindupCharacter* Doll = GetDoll();
	FCrankHandState& H = HandRef(Hand);
	H.Actor = Actor;
	H.Component = Comp;

	if (IGrabbable* Grabbable = Cast<IGrabbable>(Actor))
	{
		H.Style = Grabbable->GetGrabStyle(Comp);
		Grabbable->OnGrabbed(Doll, Hand, Comp);
	}
	else
	{
		H.Style = EGrabStyle::Anchor;
	}

	CrankSound::PlayAt(Doll, TEXT("SFX_Grab"), GetReachPoint(Hand), 0.6f, FMath::FRandRange(0.95f, 1.1f));
}

void UGrabComponent::ReleaseHand(ECrankHand Hand, bool bAllowThrow)
{
	FCrankHandState& H = HandRef(Hand);
	if (!H.IsHolding())
	{
		return;
	}

	AActor* Actor = H.Actor;
	UPrimitiveComponent* Comp = H.Component;
	const bool bWants = H.bWantsGrab;
	H = FCrankHandState();
	H.bWantsGrab = bWants;

	const FVector ThrowVelocity = bAllowThrow ? ComputeThrowVelocity() : FVector::ZeroVector;
	if (IGrabbable* Grabbable = Cast<IGrabbable>(Actor))
	{
		Grabbable->OnReleased(GetDoll(), Hand, Comp, ThrowVelocity);
	}
}

void UGrabComponent::ReleaseAll(bool bAllowThrow)
{
	ReleaseHand(ECrankHand::Left, bAllowThrow);
	ReleaseHand(ECrankHand::Right, bAllowThrow);
}

void UGrabComponent::ReleaseActor(AActor* Actor)
{
	if (LeftHand.Actor == Actor)
	{
		ReleaseHand(ECrankHand::Left, false);
	}
	if (RightHand.Actor == Actor)
	{
		ReleaseHand(ECrankHand::Right, false);
	}
}

FVector UGrabComponent::ComputeThrowVelocity() const
{
	const AWindupCharacter* Doll = GetDoll();
	if (!Doll)
	{
		return FVector::ZeroVector;
	}
	const FVector Vel = Doll->GetVelocity();
	const float Speed2D = Vel.Size2D();
	if (Speed2D < 120.f)
	{
		return FVector::ZeroVector;
	}
	const FVector Dir = FVector(Vel.X, Vel.Y, 0.f).GetSafeNormal();
	return Dir * ThrowSpeed + FVector::UpVector * ThrowUpSpeed + FVector(Vel.X, Vel.Y, 0.f) * 0.3f;
}

void UGrabComponent::OnRep_Hands()
{
	// Pose is read by the anim instance; nothing else to do.
}

bool UGrabComponent::IsHoldingActor(const AActor* Actor) const
{
	return Actor && (LeftHand.Actor == Actor || RightHand.Actor == Actor);
}

int32 UGrabComponent::CountHandsOn(const AActor* Actor) const
{
	return (Actor && LeftHand.Actor == Actor ? 1 : 0) + (Actor && RightHand.Actor == Actor ? 1 : 0);
}

bool UGrabComponent::IsCarrying() const
{
	return LeftHand.Style == EGrabStyle::Carry || RightHand.Style == EGrabStyle::Carry
		|| (LeftHand.Style == EGrabStyle::Friend && RightHand.Style == EGrabStyle::Friend);
}

bool UGrabComponent::IsAnchored() const
{
	return LeftHand.Style == EGrabStyle::Anchor || RightHand.Style == EGrabStyle::Anchor;
}

AActor* UGrabComponent::GetCarriedActor() const
{
	if (LeftHand.Style == EGrabStyle::Carry)
	{
		return LeftHand.Actor;
	}
	if (RightHand.Style == EGrabStyle::Carry)
	{
		return RightHand.Actor;
	}
	return nullptr;
}

FVector UGrabComponent::GetCarryPoint() const
{
	const AActor* Owner = GetOwner();
	if (!Owner)
	{
		return FVector::ZeroVector;
	}
	return Owner->GetActorLocation() + Owner->GetActorForwardVector() * 46.f + FVector(0.f, 0.f, 12.f);
}

FVector UGrabComponent::GetReachPoint(ECrankHand Hand) const
{
	const AActor* Owner = GetOwner();
	if (!Owner)
	{
		return FVector::ZeroVector;
	}
	const float Side = Hand == ECrankHand::Left ? -16.f : 16.f;
	return Owner->GetActorLocation() + Owner->GetActorForwardVector() * 40.f + Owner->GetActorRightVector() * Side + FVector(0.f, 0.f, 6.f);
}

float UGrabComponent::GetHoldProgress() const
{
	for (const FCrankHandState* H : { &LeftHand, &RightHand })
	{
		if (const IGrabbable* Grabbable = Cast<IGrabbable>(H->Actor))
		{
			const float Progress = Grabbable->GetHoldProgress(H->Component);
			if (Progress >= 0.f)
			{
				return Progress;
			}
		}
	}
	return -1.f;
}
