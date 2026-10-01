#include "Gameplay/CarryableActor.h"
#include "Character/WindupCharacter.h"
#include "Character/GrabComponent.h"
#include "CrankBalance.h"
#include "FX/CrankFX.h"
#include "FX/CrankSound.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Net/UnrealNetwork.h"
#include "EngineUtils.h"
#include "TimerManager.h"

ACarryableActor::ACarryableActor()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PostPhysics;
	bReplicates = true;
	SetReplicateMovement(true);
	SetNetUpdateFrequency(60.f);

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	SetRootComponent(Mesh);
	Mesh->SetCollisionProfileName(UCollisionProfile::PhysicsActor_ProfileName);
	Mesh->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	Mesh->SetSimulatePhysics(true);
	Mesh->SetUseCCD(true);
	Mesh->SetGenerateOverlapEvents(true);
	Mesh->SetLinearDamping(0.3f);
	Mesh->SetAngularDamping(0.6f);

	DisplayName = NSLOCTEXT("Crank", "PartName", "부품");
}

void ACarryableActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ACarryableActor, Carriers);
	DOREPLIFETIME(ACarryableActor, GrabRelativeRotation);
	DOREPLIFETIME(ACarryableActor, bDelivered);
}

void ACarryableActor::BeginPlay()
{
	Super::BeginPlay();
	SpawnLocation = GetActorLocation();
	if (Mesh && Mesh->GetStaticMesh())
	{
		Mesh->SetMassOverrideInKg(NAME_None, MassKg, true);
	}
	if (!Mesh || !Mesh->GetStaticMesh())
	{
		// No mesh assigned in the blueprint: cannot simulate.
		Mesh->SetSimulatePhysics(false);
	}
}

void ACarryableActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (IsCarried())
	{
		UpdateCarriedTransform();
	}
	else if (HasAuthority() && GetActorLocation().Z < -1500.f)
	{
		// Fell out of the kitchen: put it back where it started.
		Mesh->SetPhysicsLinearVelocity(FVector::ZeroVector);
		SetActorLocation(SpawnLocation + FVector(0, 0, 50.f), false, nullptr, ETeleportType::ResetPhysics);
	}
}

bool ACarryableActor::IsCarriedBy(const AWindupCharacter* Doll) const
{
	for (const FCarrierEntry& Entry : Carriers)
	{
		if (Entry.Doll == Doll)
		{
			return true;
		}
	}
	return false;
}

void ACarryableActor::UpdateCarriedTransform()
{
	FVector Sum = FVector::ZeroVector;
	int32 Count = 0;
	AWindupCharacter* First = nullptr;
	for (const FCarrierEntry& Entry : Carriers)
	{
		if (Entry.Doll)
		{
			Sum += Entry.Doll->GetGrab()->GetCarryPoint();
			++Count;
			if (!First)
			{
				First = Entry.Doll;
			}
		}
	}
	if (Count == 0 || !First)
	{
		return;
	}

	FVector Loc = Sum / Count + FVector(0, 0, HoldHeight);
	if (Count == 1)
	{
		Loc += First->GetActorForwardVector() * HoldForward;
	}
	const FQuat BaseRot = FRotator(0.f, First->GetActorRotation().Yaw, 0.f).Quaternion();
	const FQuat Rot = BaseRot * GrabRelativeRotation.Quaternion();
	SetActorLocationAndRotation(Loc, Rot, false, nullptr, ETeleportType::TeleportPhysics);
}

bool ACarryableActor::CanBeGrabbed(const AWindupCharacter* By, ECrankHand Hand, const UPrimitiveComponent* Comp) const
{
	if (bDelivered || !By)
	{
		return false;
	}
	if (Carriers.Num() >= 2 && !IsCarriedBy(By))
	{
		return false;
	}
	// One carried object at a time.
	if (const AActor* Held = By->GetGrab()->GetCarriedActor())
	{
		if (Held != this)
		{
			return false;
		}
	}
	return true;
}

void ACarryableActor::OnGrabbed(AWindupCharacter* By, ECrankHand Hand, UPrimitiveComponent* Comp)
{
	FCarrierEntry* Entry = Carriers.FindByPredicate([By](const FCarrierEntry& E) { return E.Doll == By; });
	if (!Entry)
	{
		if (Carriers.Num() == 0)
		{
			const FQuat CarrierYaw = FRotator(0.f, By->GetActorRotation().Yaw, 0.f).Quaternion();
			GrabRelativeRotation = (CarrierYaw.Inverse() * GetActorQuat()).Rotator();
		}
		FCarrierEntry NewEntry;
		NewEntry.Doll = By;
		Carriers.Add(NewEntry);
		Entry = &Carriers.Last();
	}
	Entry->HandMask |= (1 << (int32)Hand);

	ApplyCarriedPhysics(true);
	OnRep_Carriers();
	CrankSound::PlayAt(this, TEXT("SFX_Pickup"), GetActorLocation(), 0.7f, Weight == ECarryWeight::Light ? 1.2f : 0.85f);
}

void ACarryableActor::OnReleased(AWindupCharacter* By, ECrankHand Hand, UPrimitiveComponent* Comp, const FVector& ThrowVelocity)
{
	const int32 Index = Carriers.IndexOfByPredicate([By](const FCarrierEntry& E) { return E.Doll == By; });
	if (Index == INDEX_NONE)
	{
		return;
	}
	Carriers[Index].HandMask &= ~(1 << (int32)Hand);
	if (Carriers[Index].HandMask == 0)
	{
		Carriers.RemoveAt(Index);
	}
	if (Carriers.Num() == 0)
	{
		ReleaseFree(ThrowVelocity);
	}
	OnRep_Carriers();
}

bool ACarryableActor::TickHeld(AWindupCharacter* By, ECrankHand Hand, UPrimitiveComponent* Comp, float DeltaTime)
{
	if (bDelivered)
	{
		return false;
	}
	if (Carriers.Num() >= 2 && Carriers[0].Doll && Carriers[1].Doll)
	{
		const float Separation = FVector::Dist(Carriers[0].Doll->GetGrab()->GetCarryPoint(), Carriers[1].Doll->GetGrab()->GetCarryPoint());
		if (Separation > MaxCarrierSeparation && Carriers[1].Doll == By)
		{
			return false;
		}
	}
	return true;
}

void ACarryableActor::ReleaseFree(const FVector& ThrowVelocity)
{
	ApplyCarriedPhysics(false);
	if (Mesh->IsSimulatingPhysics())
	{
		const float Scale = Weight == ECarryWeight::Light ? 1.f : 0.55f;
		Mesh->SetPhysicsLinearVelocity(ThrowVelocity * Scale);
		Mesh->SetPhysicsAngularVelocityInDegrees(FMath::VRand() * (ThrowVelocity.IsNearlyZero() ? 30.f : 300.f));
		if (!ThrowVelocity.IsNearlyZero())
		{
			CrankSound::PlayAt(this, TEXT("SFX_Throw"), GetActorLocation(), 0.8f);
		}
	}
}

void ACarryableActor::ApplyCarriedPhysics(bool bCarried)
{
	if (!Mesh || !Mesh->GetStaticMesh())
	{
		return;
	}
	if (bCarried)
	{
		Mesh->SetSimulatePhysics(false);
		Mesh->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
	}
	else
	{
		Mesh->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
		Mesh->SetSimulatePhysics(true);
		Mesh->WakeAllRigidBodies();
	}
}

void ACarryableActor::OnRep_Carriers()
{
	ApplyCarriedPhysics(IsCarried());

	// Carry multipliers changed: refresh every doll's speed (only two players).
	if (UWorld* World = GetWorld())
	{
		for (TActorIterator<AWindupCharacter> It(World); It; ++It)
		{
			It->RefreshMovementSpeed();
		}
	}
}

float ACarryableActor::GetDrainMultiplierFor(const AWindupCharacter* Doll) const
{
	const bool bDuo = Carriers.Num() >= 2;
	switch (Weight)
	{
	case ECarryWeight::Heavy:		return bDuo ? 1.f : CrankBalance::MulHeavySolo;
	case ECarryWeight::TwoPerson:	return bDuo ? CrankBalance::MulMainSpringDuo : CrankBalance::MulMainSpringSolo;
	case ECarryWeight::Cloth:		return bDuo ? 1.f : CrankBalance::MulHeavySolo;
	default:						return 1.f;
	}
}

float ACarryableActor::GetSpeedCapFor(const AWindupCharacter* Doll) const
{
	const bool bDuo = Carriers.Num() >= 2;
	switch (Weight)
	{
	case ECarryWeight::Heavy:		return bDuo ? 0.f : CrankBalance::SpeedRun;
	case ECarryWeight::TwoPerson:	return bDuo ? CrankBalance::SpeedRun : CrankBalance::SpeedWalk;
	case ECarryWeight::Cloth:		return bDuo ? CrankBalance::SpeedRun : 80.f;
	default:						return 0.f;
	}
}

void ACarryableActor::DropFromAllCarriers()
{
	TArray<FCarrierEntry> Copy = Carriers;
	for (const FCarrierEntry& Entry : Copy)
	{
		if (Entry.Doll)
		{
			Entry.Doll->GetGrab()->ReleaseActor(this);
		}
	}
	Carriers.Reset();
	OnRep_Carriers();
}

void ACarryableActor::Deliver()
{
	if (!HasAuthority() || bDelivered)
	{
		return;
	}
	bDelivered = true;
	DropFromAllCarriers();
	MulticastDelivered();
	SetLifeSpan(0.5f);
}

void ACarryableActor::MulticastDelivered_Implementation()
{
	UCrankFXSubsystem::Spawn(this, ECrankFX::Deliver, GetActorLocation());
	CrankSound::PlayAt(this, TEXT("SFX_Deliver"), GetActorLocation(), 1.f);
	SetActorHiddenInGame(true);
	SetActorEnableCollision(false);
	if (Mesh)
	{
		Mesh->SetSimulatePhysics(false);
	}
}
