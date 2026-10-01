#include "Character/ScatterReviveComponent.h"
#include "Character/WindupCharacter.h"
#include "Character/TorqueComponent.h"
#include "Gameplay/BodyPartPickup.h"
#include "Game/RaidGameMode.h"
#include "CrankBalance.h"
#include "FX/CrankSound.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"

UScatterReviveComponent::UScatterReviveComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickInterval = 0.05f;
}

AWindupCharacter* UScatterReviveComponent::GetDoll() const
{
	return Cast<AWindupCharacter>(GetOwner());
}

bool UScatterReviveComponent::IsScattered() const
{
	const AWindupCharacter* Doll = GetDoll();
	return Doll && Doll->GetBodyState() == ECrankBodyState::Scattered;
}

void UScatterReviveComponent::Rupture(AWindupCharacter* Instigator)
{
	AWindupCharacter* Doll = GetDoll();
	if (!Doll || !Doll->HasAuthority() || IsScattered() || Doll->GetBodyState() == ECrankBodyState::Frozen)
	{
		return;
	}

	const FVector Origin = Doll->GetActorLocation();
	Doll->EndAllInteractions();
	Doll->GetTorque()->SetTorque(0.f);
	Doll->SetBodyState(ECrankBodyState::Scattered, Origin);

	bHeadAttached = false;
	bKeyAttached = false;
	bHeadInverted = false;
	Parts.Reset();
	Torso.Reset();

	UClass* PartClass = Doll->BodyPartClass ? Doll->BodyPartClass.Get() : ABodyPartPickup::StaticClass();
	static const EBodyPartType Types[] = { EBodyPartType::Torso, EBodyPartType::Head, EBodyPartType::Key, EBodyPartType::Arm, EBodyPartType::Leg };
	for (EBodyPartType Type : Types)
	{
		const FTransform SpawnTM(FRotator(FMath::FRandRange(-30.f, 30.f), FMath::FRandRange(0.f, 360.f), 0.f), Origin + FVector(0, 0, Type == EBodyPartType::Head ? 30.f : 0.f));
		ABodyPartPickup* Part = GetWorld()->SpawnActorDeferred<ABodyPartPickup>(PartClass, SpawnTM, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		if (!Part)
		{
			continue;
		}
		Part->InitPart(Type, Doll);
		Part->FinishSpawning(SpawnTM);

		// Scatter within ~300 uu: horizontal speed * flight time (~0.8 s).
		FVector Dir = FMath::VRand();
		Dir.Z = 0.f;
		Dir.Normalize();
		const float Horizontal = Type == EBodyPartType::Torso ? FMath::FRandRange(30.f, 90.f) : FMath::FRandRange(120.f, CrankBalance::ScatterRadius * 1.15f);
		const FVector Velocity = Dir * Horizontal + FVector(0, 0, FMath::FRandRange(320.f, 480.f));
		if (UStaticMeshComponent* PartMesh = Part->GetMesh())
		{
			if (PartMesh->IsSimulatingPhysics())
			{
				PartMesh->SetPhysicsLinearVelocity(Velocity);
				PartMesh->SetPhysicsAngularVelocityInDegrees(FMath::VRand() * 540.f);
			}
		}

		Parts.Add(Part);
		if (Type == EBodyPartType::Torso)
		{
			Torso = Part;
		}
	}

	if (Instigator && Instigator != Doll)
	{
		const FVector Away = (Instigator->GetActorLocation() - Origin).GetSafeNormal2D();
		Instigator->StartRagdoll(Away * 320.f + FVector(0, 0, 220.f), 1.2f);
	}

	if (ARaidGameMode* GM = GetWorld()->GetAuthGameMode<ARaidGameMode>())
	{
		GM->OnDollScattered(Doll);
	}
}

void UScatterReviveComponent::AttachPart(EBodyPartType Type, bool bInverted)
{
	if (!IsScattered())
	{
		return;
	}
	if (Type == EBodyPartType::Head)
	{
		bHeadAttached = true;
		bHeadInverted = bInverted;
	}
	else if (Type == EBodyPartType::Key)
	{
		bKeyAttached = true;
	}

	if (bHeadAttached && bKeyAttached)
	{
		Revive();
	}
}

void UScatterReviveComponent::Revive()
{
	AWindupCharacter* Doll = GetDoll();
	if (!Doll || !IsScattered())
	{
		return;
	}

	FVector Location = Doll->GetActorLocation();
	if (ABodyPartPickup* TorsoPart = Torso.Get())
	{
		Location = TorsoPart->GetActorLocation();
	}

	for (const TWeakObjectPtr<ABodyPartPickup>& Part : Parts)
	{
		if (ABodyPartPickup* P = Part.Get())
		{
			P->DropFromAllCarriers();
			P->Destroy();
		}
	}
	Parts.Reset();
	Torso.Reset();

	Doll->ReviveAt(Location, bHeadInverted);

	if (ARaidGameMode* GM = GetWorld()->GetAuthGameMode<ARaidGameMode>())
	{
		GM->OnDollRevived(Doll);
	}
}

void UScatterReviveComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	AWindupCharacter* Doll = GetDoll();
	if (!Doll || !Doll->HasAuthority() || !IsScattered())
	{
		return;
	}

	// Keep the (hidden) doll - and therefore its camera - over the torso.
	ABodyPartPickup* TorsoPart = Torso.Get();
	if (TorsoPart)
	{
		Doll->SetActorLocation(TorsoPart->GetActorLocation() + FVector(0, 0, 60.f), false, nullptr, ETeleportType::TeleportPhysics);
	}

	// Lost parts (fell out of the world): bring them back next to the torso.
	for (const TWeakObjectPtr<ABodyPartPickup>& Part : Parts)
	{
		ABodyPartPickup* P = Part.Get();
		if (P && TorsoPart && P != TorsoPart && P->GetActorLocation().Z < -1000.f)
		{
			P->SetActorLocation(TorsoPart->GetActorLocation() + FVector(FMath::FRandRange(-60.f, 60.f), FMath::FRandRange(-60.f, 60.f), 80.f), false, nullptr, ETeleportType::ResetPhysics);
		}
	}
}
