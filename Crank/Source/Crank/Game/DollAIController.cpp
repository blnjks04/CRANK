#include "Game/DollAIController.h"
#include "Character/WindupCharacter.h"
#include "Character/TorqueComponent.h"
#include "Character/GrabComponent.h"
#include "Character/WindInteractionComponent.h"
#include "EngineUtils.h"

ADollAIController::ADollAIController()
{
	PrimaryActorTick.bCanEverTick = true;
	bWantsPlayerState = false;
}

AWindupCharacter* ADollAIController::FindClosestPlayer(const AWindupCharacter* Doll, float& OutDist) const
{
	AWindupCharacter* Closest = nullptr;
	OutDist = TNumericLimits<float>::Max();
	for (TActorIterator<AWindupCharacter> It(GetWorld()); It; ++It)
	{
		if (*It == Doll || !It->IsPlayerControlled())
		{
			continue;
		}
		const float Dist = FVector::Dist2D(It->GetActorLocation(), Doll->GetActorLocation());
		if (Dist < OutDist)
		{
			OutDist = Dist;
			Closest = *It;
		}
	}
	return Closest;
}

void ADollAIController::StopHelping(AWindupCharacter* Doll)
{
	Doll->GetWind()->SetHoldWindServer(false);
	Doll->GetGrab()->SetGrabInputServer(ECrankHand::Left, false);
	HelpTarget.Reset();
	HelpCooldown = 4.f;
	StillTime = 0.f;
}

void ADollAIController::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	AWindupCharacter* Doll = GetPawn<AWindupCharacter>();
	if (!Doll)
	{
		return;
	}
	HelpCooldown = FMath::Max(0.f, HelpCooldown - DeltaSeconds);

	UWindInteractionComponent* Wind = Doll->GetWind();
	if (Wind->IsWinding())
	{
		// Keep a steady hand until the partner is wound up (they can also walk off to break free).
		const AWindupCharacter* Target = Wind->GetWindTarget();
		if (!Target || Target->GetTorque()->GetTorque() >= HelpUntilTorque)
		{
			StopHelping(Doll);
		}
		return;
	}
	if (HelpTarget.IsValid())
	{
		// Winding ended from the outside (break free, knocked over...).
		StopHelping(Doll);
	}

	if (!Doll->CanMoveByInput() || Doll->GetTorque()->IsDischarged())
	{
		StillTime = 0.f;
		return;
	}

	float Dist = 0.f;
	AWindupCharacter* Player = FindClosestPlayer(Doll, Dist);
	if (!Player)
	{
		return;
	}

	const bool bDischarged = Player->GetTorque()->IsDischarged();
	const bool bResting = Player->CanBeWound() && (bDischarged || Player->GetVelocity().Size2D() < 15.f);
	StillTime = bResting ? StillTime + DeltaSeconds : 0.f;

	if (StillTime >= HelpAfterStill && HelpCooldown <= 0.f && Player->GetTorque()->GetTorque() < HelpBelowTorque)
	{
		// Walk around to the key...
		const FVector PlayerFwd = Player->GetActorForwardVector().GetSafeNormal2D();
		const FVector Spot = Player->GetActorLocation() - PlayerFwd * 62.f;
		FVector ToSpot = Spot - Doll->GetActorLocation();
		ToSpot.Z = 0.f;
		if (ToSpot.Size() > 20.f)
		{
			FVector Dir = ToSpot.GetSafeNormal();
			const FVector Rel = (Doll->GetActorLocation() - Player->GetActorLocation()).GetSafeNormal2D();
			if (FVector::DotProduct(Rel, PlayerFwd) > -0.3f && Dist < 160.f)
			{
				// In front of or beside the partner: circle around them instead of walking through.
				FVector Side = FVector::CrossProduct(FVector::UpVector, PlayerFwd);
				if (FVector::DotProduct(Rel, Side) < 0.f)
				{
					Side = -Side;
				}
				Dir = (Side - PlayerFwd * 0.6f).GetSafeNormal();
			}
			Doll->AddMovementInput(Dir, ToSpot.Size() > 120.f ? 1.f : 0.6f);
			return;
		}

		// ...face it and wind with a steady hand.
		const FVector Face = (Player->GetKeyWorldLocation() - Doll->GetActorLocation()).GetSafeNormal2D();
		Doll->SetActorRotation(FRotator(0.f, Face.Rotation().Yaw, 0.f));
		if (Wind->FindWindablePartner() == Player)
		{
			HelpTarget = Player;
			Wind->SetHoldWindServer(true);
			Doll->GetGrab()->SetGrabInputServer(ECrankHand::Left, true);
		}
		return;
	}

	if (Dist > FollowDistance)
	{
		const FVector Dir = (Player->GetActorLocation() - Doll->GetActorLocation()).GetSafeNormal2D();
		Doll->AddMovementInput(Dir, 1.f);
	}
}
