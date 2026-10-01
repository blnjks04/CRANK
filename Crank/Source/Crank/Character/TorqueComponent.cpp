#include "Character/TorqueComponent.h"
#include "Character/WindupCharacter.h"
#include "CrankBalance.h"
#include "Net/UnrealNetwork.h"
#include "GameFramework/CharacterMovementComponent.h"

UTorqueComponent::UTorqueComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PostPhysics;
	SetIsReplicatedByDefault(true);
}

void UTorqueComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UTorqueComponent, Torque);
}

void UTorqueComponent::BeginPlay()
{
	Super::BeginPlay();
	ApplyMovementSpeed();
}

bool UTorqueComponent::CanLaunch() const
{
	return Torque >= CrankBalance::LaunchMinTorque;
}

float UTorqueComponent::GetOverwindBonus() const
{
	return FMath::Clamp((Torque - CrankBalance::TorqueMax) * CrankBalance::OverwindBonusPerT, 0.f, CrankBalance::OverwindBonusMax);
}

void UTorqueComponent::SetTorque(float NewTorque)
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}
	const float Old = Torque;
	Torque = FMath::Clamp(NewTorque, 0.f, CrankBalance::TorqueOverMax);
	if (!FMath::IsNearlyEqual(Old, Torque))
	{
		HandleTorqueChanged(Old);
	}
}

float UTorqueComponent::AddTorque(float Delta)
{
	const float Old = Torque;
	SetTorque(Torque + Delta);
	return Torque - Old;
}

bool UTorqueComponent::TrySpend(float Amount)
{
	if (Torque < Amount)
	{
		return false;
	}
	AddTorque(-Amount);
	return true;
}

float UTorqueComponent::GetBandSpeed() const
{
	switch (GetBand())
	{
	case ETorqueBand::Discharged:	return CrankBalance::SpeedCrawl;
	case ETorqueBand::Toddle:		return CrankBalance::SpeedWalk;
	case ETorqueBand::Run:			return CrankBalance::SpeedRun;
	default:						return CrankBalance::SpeedSprint;
	}
}

float UTorqueComponent::GetBandDrain() const
{
	switch (GetBand())
	{
	case ETorqueBand::Discharged:	return 0.f;
	case ETorqueBand::Toddle:		return CrankBalance::DrainWalk;
	case ETorqueBand::Run:			return CrankBalance::DrainRun;
	default:						return CrankBalance::DrainSprint;
	}
}

void UTorqueComponent::OnRep_Torque(float OldTorque)
{
	HandleTorqueChanged(OldTorque);
}

void UTorqueComponent::HandleTorqueChanged(float OldTorque)
{
	const ETorqueBand OldBand = CrankUtil::BandForTorque(OldTorque);
	const ETorqueBand NewBand = GetBand();

	ApplyMovementSpeed();

	if (AWindupCharacter* Doll = Cast<AWindupCharacter>(GetOwner()))
	{
		Doll->OnTorqueUpdated(OldTorque, Torque);
	}

	OnTorqueChanged.Broadcast(Torque);
	if (OldBand != NewBand)
	{
		OnBandChanged.Broadcast(OldBand, NewBand);
	}
}

void UTorqueComponent::ApplyMovementSpeed()
{
	if (AWindupCharacter* Doll = Cast<AWindupCharacter>(GetOwner()))
	{
		Doll->RefreshMovementSpeed();
	}
}

void UTorqueComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	AWindupCharacter* Doll = Cast<AWindupCharacter>(GetOwner());
	if (!Doll || !Doll->HasAuthority())
	{
		return;
	}

	// Discharge timer (used for the emergency wind rule).
	if (Torque <= 0.f && Doll->GetBodyState() != ECrankBodyState::Scattered)
	{
		TimeDischarged += DeltaTime;
	}
	else
	{
		TimeDischarged = 0.f;
	}

	if (bSuspendMovementDrain || Torque <= 0.f || Doll->GetBodyState() != ECrankBodyState::Normal)
	{
		return;
	}

	const UCharacterMovementComponent* Move = Doll->GetCharacterMovement();
	if (!Move || !Move->IsMovingOnGround())
	{
		return;
	}

	// Drain only while actually moving (standing still is free).
	const float Speed2D = Move->Velocity.Size2D();
	if (Speed2D < 25.f)
	{
		return;
	}

	// Drain scales with how hard the legs work: full band drain at band speed.
	const float SpeedRatio = FMath::Clamp(Speed2D / FMath::Max(1.f, GetBandSpeed()), 0.35f, 1.f);
	const float Drain = GetBandDrain() * SpeedRatio * Doll->GetDrainMultiplier() * DeltaTime;
	if (Drain > 0.f)
	{
		AddTorque(-Drain);
	}
}
