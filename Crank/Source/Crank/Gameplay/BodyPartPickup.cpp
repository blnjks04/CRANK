#include "Gameplay/BodyPartPickup.h"
#include "Character/WindupCharacter.h"
#include "Character/ScatterReviveComponent.h"
#include "CrankAssets.h"
#include "FX/CrankFX.h"
#include "FX/CrankSound.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/StaticMesh.h"
#include "Net/UnrealNetwork.h"
#include "EngineUtils.h"

ABodyPartPickup::ABodyPartPickup()
{
	AttachedHead = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("AttachedHead"));
	AttachedHead->SetupAttachment(Mesh);
	AttachedHead->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	AttachedHead->SetRelativeLocation(FVector(0.f, 0.f, 26.f));
	AttachedHead->SetVisibility(false);

	AttachedKey = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("AttachedKey"));
	AttachedKey->SetupAttachment(Mesh);
	AttachedKey->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	AttachedKey->SetRelativeLocation(FVector(-18.f, 0.f, 6.f));
	AttachedKey->SetVisibility(false);

	MassKg = 2.f;
	GrabPriority = 6;
}

void ABodyPartPickup::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ABodyPartPickup, BodyPartType);
	DOREPLIFETIME(ABodyPartPickup, OwnerDoll);
	DOREPLIFETIME(ABodyPartPickup, AttachedMask);
}

void ABodyPartPickup::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	ApplyPartVisuals();
}

void ABodyPartPickup::BeginPlay()
{
	ApplyPartVisuals();
	Super::BeginPlay();
	NextBlinkTime = GetWorld()->GetTimeSeconds() + FMath::FRandRange(1.f, 3.f);
}

void ABodyPartPickup::InitPart(EBodyPartType InType, AWindupCharacter* InOwnerDoll)
{
	BodyPartType = InType;
	OwnerDoll = InOwnerDoll;
	Weight = InType == EBodyPartType::Torso ? ECarryWeight::Heavy : ECarryWeight::Light;
	GrabPriority = IsRequiredPart() ? 7 : 4;
	MassKg = InType == EBodyPartType::Torso ? 6.f : 1.5f;
	ApplyPartVisuals();
}

void ABodyPartPickup::OnRep_PartSetup()
{
	ApplyPartVisuals();
}

void ABodyPartPickup::ApplyPartVisuals()
{
	if (!Mesh)
	{
		return;
	}

	auto MeshFor = [this](EBodyPartType Type) -> UStaticMesh*
	{
		if (const TObjectPtr<UStaticMesh>* Found = PartMeshes.Find(Type))
		{
			if (*Found)
			{
				return *Found;
			}
		}
		switch (Type)
		{
		case EBodyPartType::Head:	return CrankAssets::Mesh(CrankPaths::DollPartHead, CrankPaths::Sphere);
		case EBodyPartType::Key:	return CrankAssets::Mesh(CrankPaths::DollKeyMesh, CrankPaths::Cube);
		case EBodyPartType::Torso:	return CrankAssets::Mesh(CrankPaths::DollPartTorso, CrankPaths::Cylinder);
		case EBodyPartType::Arm:	return CrankAssets::Mesh(CrankPaths::DollPartArm, CrankPaths::Cylinder);
		default:					return CrankAssets::Mesh(CrankPaths::DollPartLeg, CrankPaths::Cylinder);
		}
	};

	UStaticMesh* Desired = MeshFor(BodyPartType);
	if (Desired && Mesh->GetStaticMesh() != Desired)
	{
		Mesh->SetStaticMesh(Desired);
		if (Desired->GetPathName().StartsWith(TEXT("/Engine/")))
		{
			Mesh->SetWorldScale3D(FVector(0.25f));
		}
	}

	if (OwnerDoll && Mesh->GetNumMaterials() > PaintMaterialIndex && BodyPartType != EBodyPartType::Key)
	{
		if (!PaintMID)
		{
			PaintMID = Mesh->CreateDynamicMaterialInstance(PaintMaterialIndex);
		}
		if (PaintMID)
		{
			PaintMID->SetVectorParameterValue(TEXT("PaintColor"), OwnerDoll->GetPlayerColor());
		}
	}

	const bool bTorso = BodyPartType == EBodyPartType::Torso;
	AttachedHead->SetStaticMesh(MeshFor(EBodyPartType::Head));
	AttachedKey->SetStaticMesh(MeshFor(EBodyPartType::Key));
	AttachedHead->SetVisibility(bTorso && (AttachedMask & 1) != 0);
	AttachedKey->SetVisibility(bTorso && (AttachedMask & 2) != 0);
	if (bTorso && OwnerDoll && AttachedHead->GetNumMaterials() > PaintMaterialIndex)
	{
		if (UMaterialInstanceDynamic* HeadMID = AttachedHead->CreateDynamicMaterialInstance(PaintMaterialIndex))
		{
			HeadMID->SetVectorParameterValue(TEXT("PaintColor"), OwnerDoll->GetPlayerColor());
		}
	}
}

bool ABodyPartPickup::CanBeGrabbed(const AWindupCharacter* By, ECrankHand Hand, const UPrimitiveComponent* Comp) const
{
	return By != OwnerDoll && Super::CanBeGrabbed(By, Hand, Comp);
}

FText ABodyPartPickup::GetGrabLabel(const UPrimitiveComponent* Comp) const
{
	switch (BodyPartType)
	{
	case EBodyPartType::Head:	return NSLOCTEXT("Crank", "PartHead", "머리 (필수)");
	case EBodyPartType::Key:	return NSLOCTEXT("Crank", "PartKey", "태엽 (필수)");
	case EBodyPartType::Torso:	return NSLOCTEXT("Crank", "PartTorso", "몸통");
	case EBodyPartType::Arm:	return NSLOCTEXT("Crank", "PartArm", "팔");
	default:					return NSLOCTEXT("Crank", "PartLeg", "다리");
	}
}

void ABodyPartPickup::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// Comedy: the lonely head blinks.
	if (BodyPartType == EBodyPartType::Head && GetNetMode() != NM_DedicatedServer)
	{
		const float Now = GetWorld()->GetTimeSeconds();
		if (Now >= NextBlinkTime)
		{
			BlinkEndTime = Now + 0.12f;
			NextBlinkTime = Now + FMath::FRandRange(1.2f, 3.5f);
		}
		const float BlinkScale = Now < BlinkEndTime ? 0.8f : 1.f;
		const FVector Base = Mesh->GetStaticMesh() && Mesh->GetStaticMesh()->GetPathName().StartsWith(TEXT("/Engine/")) ? FVector(0.25f) : FVector(1.f);
		const FVector Desired(Base.X, Base.Y, Base.Z * BlinkScale);
		if (!Mesh->GetRelativeScale3D().Equals(Desired, 0.01f))
		{
			Mesh->SetRelativeScale3D(Desired);
		}
	}

	if (!HasAuthority() || BodyPartType != EBodyPartType::Torso || !OwnerDoll)
	{
		return;
	}

	// Torso: absorb the head / key when they are brought close.
	for (TActorIterator<ABodyPartPickup> It(GetWorld()); It; ++It)
	{
		ABodyPartPickup* Part = *It;
		if (Part == this || Part->OwnerDoll != OwnerDoll || !Part->IsRequiredPart() || Part->IsDelivered() || Part->IsActorBeingDestroyed())
		{
			continue;
		}
		if (FVector::Dist(Part->GetActorLocation(), GetActorLocation()) > AttachRadius)
		{
			continue;
		}

		const bool bHead = Part->BodyPartType == EBodyPartType::Head;
		const bool bInverted = bHead && FVector::DotProduct(Part->GetActorUpVector(), FVector::UpVector) < 0.f;
		AttachedMask |= bHead ? 1 : 2;
		OnRep_PartSetup();

		Part->DropFromAllCarriers();
		Part->Destroy();
		CrankSound::PlayAt(this, TEXT("SFX_Assemble"), GetActorLocation(), 1.f, bHead ? 1.f : 1.2f);
		UCrankFXSubsystem::Spawn(this, ECrankFX::WindClick, GetActorLocation() + FVector(0, 0, 20.f));

		if (UScatterReviveComponent* Scatter = OwnerDoll->GetScatter())
		{
			Scatter->AttachPart(bHead ? EBodyPartType::Head : EBodyPartType::Key, bInverted);
		}
		break;
	}
}
