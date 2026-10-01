#include "Gameplay/PullDrawer.h"
#include "Character/WindupCharacter.h"
#include "Character/GrabComponent.h"
#include "FX/CrankSound.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Net/UnrealNetwork.h"

APullDrawer::APullDrawer()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	Drawer = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Drawer"));
	Drawer->SetupAttachment(Root);
	Drawer->SetCollisionProfileName(TEXT("BlockAll"));
	Drawer->SetMobility(EComponentMobility::Movable);

	HandleMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("HandleMesh"));
	HandleMesh->SetupAttachment(Drawer);
	HandleMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	HandleBox = CreateDefaultSubobject<UBoxComponent>(TEXT("HandleBox"));
	HandleBox->SetupAttachment(Drawer);
	HandleBox->SetBoxExtent(FVector(20.f, 40.f, 16.f));
	HandleBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	HandleBox->SetCollisionObjectType(ECC_WorldDynamic);
	HandleBox->SetCollisionResponseToAllChannels(ECR_Ignore);
	HandleBox->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
}

void APullDrawer::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(APullDrawer, Extent);
}

void APullDrawer::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	Extent = FMath::Clamp(StartExtent, 0.f, MaxExtent);
	VisualExtent = Extent;
	DesiredExtent = Extent;
	Drawer->SetRelativeLocation(FVector(Extent, 0.f, 0.f));
}

FText APullDrawer::GetGrabLabel(const UPrimitiveComponent* Comp) const
{
	return NSLOCTEXT("Crank", "DrawerLabel", "서랍 손잡이 (뒤로 걸어 열기)");
}

void APullDrawer::OnGrabbed(AWindupCharacter* By, ECrankHand Hand, UPrimitiveComponent* Comp)
{
	if (!Holder.IsValid())
	{
		Holder = By;
		const FVector Out = GetActorForwardVector();
		HoldOffset = FVector::DotProduct(By->GetActorLocation() - HandleBox->GetComponentLocation(), Out);
		HoldOffset = FMath::Clamp(HoldOffset, 25.f, 80.f);
		DesiredExtent = Extent;
		CrankSound::PlayAt(this, TEXT("SFX_Grab"), HandleBox->GetComponentLocation(), 0.6f, 0.8f);
	}
}

void APullDrawer::OnReleased(AWindupCharacter* By, ECrankHand Hand, UPrimitiveComponent* Comp, const FVector& ThrowVelocity)
{
	if (Holder.Get() == By && !By->GetGrab()->IsHoldingActor(this))
	{
		Holder.Reset();
		DesiredExtent = Extent;
	}
}

bool APullDrawer::TickHeld(AWindupCharacter* By, ECrankHand Hand, UPrimitiveComponent* Comp, float DeltaTime)
{
	if (Holder.Get() != By)
	{
		return true;
	}
	const FVector Out = GetActorForwardVector();
	// Closed handle position + current extent = where the handle is; follow the doll along the axis.
	const FVector ClosedHandle = HandleBox->GetComponentLocation() - Out * Extent;
	const float AlongAxis = FVector::DotProduct(By->GetActorLocation() - ClosedHandle, Out) - HoldOffset;
	DesiredExtent = FMath::Clamp(AlongAxis, 0.f, MaxExtent);

	// Too far sideways / away: let go.
	const FVector Lateral = (By->GetActorLocation() - HandleBox->GetComponentLocation()) - Out * FVector::DotProduct(By->GetActorLocation() - HandleBox->GetComponentLocation(), Out);
	return Lateral.Size2D() < 120.f && FVector::Dist(By->GetActorLocation(), HandleBox->GetComponentLocation()) < 200.f;
}

void APullDrawer::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (HasAuthority())
	{
		if (Holder.IsValid() && !Holder->GetGrab()->IsHoldingActor(this))
		{
			Holder.Reset();
			DesiredExtent = Extent;
		}
		const float NewExtent = FMath::FInterpConstantTo(Extent, DesiredExtent, DeltaSeconds, SlideSpeed);
		const bool bMoving = !FMath::IsNearlyEqual(NewExtent, Extent, 0.5f);
		Extent = NewExtent;
		if (bMoving && !bWasMoving)
		{
			CrankSound::PlayAt(this, TEXT("SFX_Drawer"), HandleBox->GetComponentLocation(), 0.9f);
		}
		bWasMoving = bMoving;
	}

	VisualExtent = HasAuthority() ? Extent : FMath::FInterpTo(VisualExtent, Extent, DeltaSeconds, 12.f);
	Drawer->SetRelativeLocation(FVector(VisualExtent, 0.f, 0.f));
}
