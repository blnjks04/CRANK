#include "Character/LaunchComponent.h"
#include "Character/WindupCharacter.h"
#include "Character/TorqueComponent.h"
#include "Boss/BossDustEater.h"
#include "Boss/BossGiantMonkey.h"
#include "CrankBalance.h"
#include "FX/CrankFX.h"
#include "FX/CrankSound.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"
#include "Engine/World.h"

ULaunchComponent::ULaunchComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	SetIsReplicatedByDefault(true);
}

AWindupCharacter* ULaunchComponent::GetDoll() const
{
	return Cast<AWindupCharacter>(GetOwner());
}

float ULaunchComponent::GetFlightTime() const
{
	return GetWorld() ? GetWorld()->GetTimeSeconds() - FlightStartTime : 0.f;
}

void ULaunchComponent::Launch(const FVector& Velocity, AWindupCharacter* InstigatorDoll, bool bSlingshot, bool bResetTorque)
{
	AWindupCharacter* Doll = GetDoll();
	if (!Doll || !Doll->HasAuthority())
	{
		return;
	}

	Doll->EndAllInteractions();
	if (bResetTorque)
	{
		Doll->GetTorque()->AddTorque(-CrankBalance::LaunchTorqueCost);
	}

	bSlingFlight = bSlingshot;
	FlightStartTime = GetWorld()->GetTimeSeconds();
	LastFlightVelocity = Velocity;
	LastInstigator = InstigatorDoll;

	// Body state carries the launch velocity so the owning client can predict the same launch.
	Doll->SetBodyState(ECrankBodyState::Flying, Velocity);
	Doll->LaunchCharacter(Velocity, true, true);
}

void ULaunchComponent::RequestSelfLaunch()
{
	AWindupCharacter* Doll = GetDoll();
	if (!Doll || !Doll->CanSelfLaunch())
	{
		return;
	}

	FRotator Rot = Doll->GetActorRotation();
	if (const AController* Controller = Doll->GetController())
	{
		Rot = Controller->GetControlRotation();
	}
	Rot.Pitch = FMath::Clamp(FRotator::NormalizeAxis(Rot.Pitch) + 10.f, 8.f, 50.f);
	Rot.Roll = 0.f;

	if (Doll->HasAuthority())
	{
		ServerSelfLaunch_Implementation(Rot.Vector());
	}
	else
	{
		ServerSelfLaunch(Rot.Vector());
	}
}

void ULaunchComponent::ServerSelfLaunch_Implementation(FVector_NetQuantizeNormal Direction)
{
	AWindupCharacter* Doll = GetDoll();
	if (!Doll || !Doll->CanSelfLaunch())
	{
		return;
	}

	// Self release aims roughly: a few degrees of random spread.
	const FVector Dir = FMath::VRandCone(FVector(Direction).GetSafeNormal(), FMath::DegreesToRadians(6.f));
	const float Speed = CrankBalance::SelfLaunchSpeed * (1.f + Doll->GetTorque()->GetOverwindBonus());
	Launch(Dir * Speed, Doll, false);
}

void ULaunchComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	AWindupCharacter* Doll = GetDoll();
	if (!Doll || !Doll->HasAuthority())
	{
		return;
	}

	if (Doll->GetBodyState() == ECrankBodyState::Flying)
	{
		const FVector Vel = Doll->GetVelocity();
		if (!Vel.IsNearlyZero())
		{
			LastFlightVelocity = Vel;
		}
		if (GetFlightTime() > 6.f)
		{
			Doll->StartRagdoll(FVector::ZeroVector, CrankBalance::LandRagdollTime);
		}
	}
}

void ULaunchComponent::HandleFlightHit(AActor* Other, UPrimitiveComponent* OtherComp, const FHitResult& Hit)
{
	AWindupCharacter* Doll = GetDoll();
	if (!Doll || Doll->GetBodyState() != ECrankBodyState::Flying || GetFlightTime() < 0.06f)
	{
		return;
	}

	const FVector V = LastFlightVelocity;
	const float Speed = V.Size();

	if (ABossDustEater* Boss = Cast<ABossDustEater>(Other))
	{
		Boss->HandleDollImpact(Doll, V, Hit);
		Doll->StartRagdoll(-V * 0.25f + FVector(0, 0, 260.f), CrankBalance::LandRagdollTime);
		return;
	}

	if (ABossGiantMonkey* Giant = Cast<ABossGiantMonkey>(Other))
	{
		Giant->HandleDollImpact(Doll, V, Hit);
		Doll->StartRagdoll(-V * 0.3f + FVector(0, 0, 220.f), CrankBalance::LandRagdollTime);
		return;
	}

	if (AWindupCharacter* Friend = Cast<AWindupCharacter>(Other))
	{
		Friend->StartRagdoll(V * 0.4f + FVector(0, 0, 160.f), CrankBalance::LandRagdollTime);
		Doll->StartRagdoll(-V * 0.2f + FVector(0, 0, 120.f), CrankBalance::LandRagdollTime);
		Doll->MulticastImpactFX(Hit.ImpactPoint, Hit.ImpactNormal, Speed);
		return;
	}

	const bool bWall = FMath::Abs(Hit.ImpactNormal.Z) < 0.45f;
	if (bWall && Speed >= SplatSpeed)
	{
		BeginSplat(Hit);
		return;
	}

	Doll->MulticastImpactFX(Hit.ImpactPoint, Hit.ImpactNormal, Speed);
	// Bounce off what we hit (pushing the ragdoll into it could tunnel through thin walls).
	Doll->StartRagdoll(FMath::GetReflectionVector(V, Hit.ImpactNormal) * 0.3f + FVector(0, 0, 100.f), CrankBalance::LandRagdollTime);
}

void ULaunchComponent::HandleFlightLanded(const FHitResult& Hit)
{
	AWindupCharacter* Doll = GetDoll();
	if (!Doll)
	{
		return;
	}

	if (Doll->GetBodyState() == ECrankBodyState::Flying)
	{
		const FVector V = LastFlightVelocity;
		Doll->MulticastImpactFX(Hit.ImpactPoint, FVector::UpVector, V.Size() * 0.6f);
		Doll->StartRagdoll(FVector(V.X, V.Y, 0.f) * 0.45f + FVector(0, 0, 90.f), CrankBalance::LandRagdollTime);
	}
	else if (Doll->GetBodyState() == ECrankBodyState::Splat)
	{
		Doll->StartRagdoll(Doll->GetSplatNormal() * 90.f + FVector(0, 0, 60.f), 0.6f);
	}
}

void ULaunchComponent::BeginSplat(const FHitResult& Hit)
{
	AWindupCharacter* Doll = GetDoll();
	const FVector Normal = Hit.ImpactNormal.GetSafeNormal2D();
	Doll->SetActorRotation(FRotator(0.f, (-Normal).Rotation().Yaw, 0.f));
	Doll->SetBodyState(ECrankBodyState::Splat, Normal);
	Doll->MulticastImpactFX(Hit.ImpactPoint, Hit.ImpactNormal, LastFlightVelocity.Size());
}
