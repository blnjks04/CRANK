#include "Boss/BossDustEater.h"
#include "Boss/ChargingDock.h"
#include "Boss/WaterTrail.h"
#include "Boss/BossArenaManager.h"
#include "Character/WindupCharacter.h"
#include "Character/TorqueComponent.h"
#include "Character/LaunchComponent.h"
#include "Gameplay/CarryablePart.h"
#include "CrankBalance.h"
#include "FX/CrankFX.h"
#include "FX/CrankSound.h"
#include "Components/StaticMeshComponent.h"
#include "Components/BoxComponent.h"
#include "Components/SphereComponent.h"
#include "Components/AudioComponent.h"
#include "Components/PointLightComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "GameFramework/GameStateBase.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Net/UnrealNetwork.h"

namespace BossEvent
{
	enum : uint8
	{
		SmallBonk, BigBonk, Suck, Brush, Eject, CellPulled, PhaseUp, Recharged, Defeated, MiniStun, Wake
	};
}

namespace
{
	float BossNow(const UWorld* World)
	{
		if (const AGameStateBase* GS = World ? World->GetGameState() : nullptr)
		{
			return GS->GetServerWorldTimeSeconds();
		}
		return World ? World->GetTimeSeconds() : 0.f;
	}

	UBoxComponent* MakeGrabBox(UObject* Outer, const TCHAR* Name, USceneComponent* Parent, const FVector& Extent)
	{
		UBoxComponent* Box = CastChecked<AActor>(Outer)->CreateDefaultSubobject<UBoxComponent>(Name);
		Box->SetupAttachment(Parent);
		Box->SetBoxExtent(Extent);
		Box->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		Box->SetCollisionObjectType(ECC_WorldDynamic);
		Box->SetCollisionResponseToAllChannels(ECR_Ignore);
		Box->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
		return Box;
	}
}

ABossDustEater::ABossDustEater()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	SetReplicateMovement(false);
	bAlwaysRelevant = true;
	SetNetUpdateFrequency(60.f);
	AutoPossessAI = EAutoPossessAI::Disabled;

	Body = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Body"));
	SetRootComponent(Body);
	Body->SetMobility(EComponentMobility::Movable);
	Body->SetCollisionObjectType(ECC_Boss);
	Body->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	Body->SetCollisionResponseToAllChannels(ECR_Block);
	Body->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	Body->SetCollisionResponseToChannel(ECC_BossBlocker, ECR_Block);

	Bumper = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Bumper"));
	Bumper->SetupAttachment(Body);
	Bumper->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	HatchPivot = CreateDefaultSubobject<USceneComponent>(TEXT("HatchPivot"));
	HatchPivot->SetupAttachment(Body);
	HatchPivot->SetRelativeLocation(FVector(-60.f, 0.f, 80.f));

	Hatch = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Hatch"));
	Hatch->SetupAttachment(HatchPivot);
	Hatch->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	RampPivot = CreateDefaultSubobject<USceneComponent>(TEXT("RampPivot"));
	RampPivot->SetupAttachment(Body);
	RampPivot->SetRelativeLocation(FVector(-118.f, 0.f, 78.f));
	RampPivot->SetRelativeRotation(FRotator(0.f, 180.f, 0.f));

	Ramp = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Ramp"));
	Ramp->SetupAttachment(RampPivot);
	Ramp->SetCollisionProfileName(TEXT("BlockAll"));
	Ramp->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	BrushL = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BrushL"));
	BrushL->SetupAttachment(Body);
	BrushL->SetRelativeLocation(FVector(88.f, -92.f, 4.f));
	BrushL->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	BrushR = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BrushR"));
	BrushR->SetupAttachment(Body);
	BrushR->SetRelativeLocation(FVector(88.f, 92.f, 4.f));
	BrushR->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	BrushZoneL = CreateDefaultSubobject<USphereComponent>(TEXT("BrushZoneL"));
	BrushZoneL->SetupAttachment(BrushL);
	BrushZoneL->SetSphereRadius(55.f);
	BrushZoneL->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	BrushZoneL->SetCollisionResponseToAllChannels(ECR_Ignore);
	BrushZoneL->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);

	BrushZoneR = CreateDefaultSubobject<USphereComponent>(TEXT("BrushZoneR"));
	BrushZoneR->SetupAttachment(BrushR);
	BrushZoneR->SetSphereRadius(55.f);
	BrushZoneR->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	BrushZoneR->SetCollisionResponseToAllChannels(ECR_Ignore);
	BrushZoneR->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);

	IntakeZone = CreateDefaultSubobject<UBoxComponent>(TEXT("IntakeZone"));
	IntakeZone->SetupAttachment(Body);
	IntakeZone->SetBoxExtent(FVector(30.f, 70.f, 35.f));
	IntakeZone->SetRelativeLocation(FVector(132.f, 0.f, 35.f));
	IntakeZone->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	IntakeZone->SetCollisionResponseToAllChannels(ECR_Ignore);
	IntakeZone->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);

	Cell0 = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Cell0"));
	Cell1 = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Cell1"));
	Cell2 = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Cell2"));
	UStaticMeshComponent* CellsArr[3] = { Cell0, Cell1, Cell2 };
	for (int32 i = 0; i < 3; ++i)
	{
		CellsArr[i]->SetupAttachment(Body);
		CellsArr[i]->SetRelativeLocation(FVector(0.f, (i - 1) * 32.f, 66.f));
		CellsArr[i]->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
	CellGrab0 = MakeGrabBox(this, TEXT("CellGrab0"), Cell0, FVector(16.f, 14.f, 22.f));
	CellGrab1 = MakeGrabBox(this, TEXT("CellGrab1"), Cell1, FVector(16.f, 14.f, 22.f));
	CellGrab2 = MakeGrabBox(this, TEXT("CellGrab2"), Cell2, FVector(16.f, 14.f, 22.f));

	BinLever = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BinLever"));
	BinLever->SetupAttachment(Body);
	BinLever->SetRelativeLocation(FVector(-128.f, 40.f, 50.f));
	BinLever->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	BinLeverGrab = MakeGrabBox(this, TEXT("BinLeverGrab"), BinLever, FVector(22.f, 22.f, 24.f));

	BinDoor = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BinDoor"));
	BinDoor->SetupAttachment(Body);
	BinDoor->SetRelativeLocation(FVector(-118.f, 0.f, 30.f));
	BinDoor->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	EyeLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("EyeLight"));
	EyeLight->SetupAttachment(Body);
	EyeLight->SetRelativeLocation(FVector(135.f, 0.f, 62.f));
	EyeLight->SetIntensity(3000.f);
	EyeLight->SetAttenuationRadius(600.f);
	EyeLight->SetCastShadows(false);

	StunFX = CreateDefaultSubobject<UCrankFXEmitterComponent>(TEXT("StunFX"));
	StunFX->SetupAttachment(Body);
	StunFX->SetRelativeLocation(FVector(0.f, 0.f, 150.f));
	StunFX->Effect = ECrankFX::StunStars;
	StunFX->EffectScale = 2.5f;
	StunFX->Rate = 1.f;

	SuctionFX = CreateDefaultSubobject<UCrankFXEmitterComponent>(TEXT("SuctionFX"));
	SuctionFX->SetupAttachment(Body);
	SuctionFX->SetRelativeLocation(FVector(135.f, 0.f, 30.f));
	SuctionFX->Effect = ECrankFX::Suction;
	SuctionFX->Rate = 18.f;

	SmokeFX = CreateDefaultSubobject<UCrankFXEmitterComponent>(TEXT("SmokeFX"));
	SmokeFX->SetupAttachment(Body);
	SmokeFX->SetRelativeLocation(FVector(-60.f, 0.f, 90.f));
	SmokeFX->Effect = ECrankFX::BossSmoke;
	SmokeFX->Rate = 1.5f;
	SmokeFX->EffectScale = 0.6f;
}

void ABossDustEater::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ABossDustEater, BossState);
	DOREPLIFETIME(ABossDustEater, Phase);
	DOREPLIFETIME(ABossDustEater, Cells);
	DOREPLIFETIME(ABossDustEater, CellMask);
	DOREPLIFETIME(ABossDustEater, StateStartTime);
	DOREPLIFETIME(ABossDustEater, StunDuration);
	DOREPLIFETIME(ABossDustEater, ServerLocation);
	DOREPLIFETIME(ABossDustEater, ServerYaw);
	DOREPLIFETIME(ABossDustEater, ServerSpeed);
	DOREPLIFETIME(ABossDustEater, Trapped);
	DOREPLIFETIME(ABossDustEater, LeverProgress);
	DOREPLIFETIME(ABossDustEater, CellProgress);
}

void ABossDustEater::BeginPlay()
{
	Super::BeginPlay();

	VisualLocation = GetActorLocation();
	VisualYaw = GetActorRotation().Yaw;
	ServerLocation = GetActorLocation();
	ServerYaw = VisualYaw;

	const int32 EyeSlot = Body->GetMaterialIndex(TEXT("Eyes"));
	if (EyeSlot != INDEX_NONE)
	{
		EyeMID = Body->CreateDynamicMaterialInstance(EyeSlot);
	}

	MotorAudio = CrankSound::CreateLoop(Body, TEXT("SFX_VacuumLoop"), 0.7f);
	SuctionAudio = CrankSound::CreateLoop(Body, TEXT("SFX_SuctionLoop"), 0.8f);

	if (HasAuthority())
	{
		if (PatrolPoints.Num() == 0)
		{
			const FVector C = GetActorLocation();
			PatrolPoints = { C + FVector(700, 700, 0), C + FVector(-700, 700, 0), C + FVector(-700, -700, 0), C + FVector(700, -700, 0) };
		}
		FActorSpawnParameters Params;
		Params.Owner = this;
		WaterTrail = GetWorld()->SpawnActor<AWaterTrail>(WaterTrailClass ? WaterTrailClass.Get() : AWaterTrail::StaticClass(), FTransform::Identity, Params);
	}
	ApplyVisualState();
}

float ABossDustEater::GetStateTime() const
{
	return BossNow(GetWorld()) - StateStartTime;
}

float ABossDustEater::GetStunRemaining() const
{
	if (BossState != EBossState::Stunned && BossState != EBossState::MiniStunned)
	{
		return 0.f;
	}
	return FMath::Max(0.f, StunDuration - GetStateTime());
}

void ABossDustEater::SetBossState(EBossState NewState)
{
	BossState = NewState;
	StateStartTime = BossNow(GetWorld());
	ForceNetUpdate();
	OnRep_BossState();
}

void ABossDustEater::Activate()
{
	if (HasAuthority() && BossState == EBossState::Dormant)
	{
		SetBossState(EBossState::Patrol);
		MulticastBossEvent(BossEvent::Wake, GetActorLocation());
	}
}

bool ABossDustEater::IsInsideArena(const FVector& Location) const
{
	return !Arena || Arena->ContainsPoint(Location, BodyRadius);
}

// ------------------------------------------------------------------------------------------
// Tick

void ABossDustEater::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (HasAuthority())
	{
		TickAI(DeltaSeconds);
		TickContacts(DeltaSeconds);
		TickTrapped(DeltaSeconds);

		for (int32 i = 0; i < 3; ++i)
		{
			if (CellHolders[i] == 0 && CellProgress[i] > 0.f)
			{
				CellProgress[i] = FMath::Max(0.f, CellProgress[i] - DeltaSeconds);
			}
		}
		if (LeverHolders == 0 && LeverProgress > 0.f)
		{
			LeverProgress = FMath::Max(0.f, LeverProgress - DeltaSeconds);
		}

		ServerLocation = GetActorLocation();
		ServerYaw = GetActorRotation().Yaw;
		ServerSpeed = CurrentSpeed;
	}
	else if (bHasServerMove)
	{
		// Smooth + extrapolate the replicated root.
		const FVector Predicted = FVector(ServerLocation) + FRotator(0.f, ServerYaw, 0.f).Vector() * ServerSpeed * 0.05f;
		VisualLocation = FMath::VInterpTo(VisualLocation, Predicted, DeltaSeconds, 14.f);
		VisualYaw = FMath::FixedTurn(VisualYaw, ServerYaw, 360.f * DeltaSeconds * 2.f);
		SetActorLocationAndRotation(VisualLocation, FRotator(0.f, VisualYaw, 0.f));
	}

	UpdateVisuals(DeltaSeconds);
}

void ABossDustEater::OnRep_ServerMove()
{
	if (!bHasServerMove)
	{
		VisualLocation = ServerLocation;
		VisualYaw = ServerYaw;
	}
	bHasServerMove = true;
}

AWindupCharacter* ABossDustEater::FindTarget() const
{
	AWindupCharacter* Best = nullptr;
	float BestDist = Phase >= 3 ? TNumericLimits<float>::Max() : DetectRadius;
	for (TActorIterator<AWindupCharacter> It(GetWorld()); It; ++It)
	{
		AWindupCharacter* Doll = *It;
		const ECrankBodyState S = Doll->GetBodyState();
		if (S != ECrankBodyState::Normal && S != ECrankBodyState::GettingUp && S != ECrankBodyState::Ragdoll && S != ECrankBodyState::Flying)
		{
			continue;
		}
		if (!IsInsideArena(Doll->GetActorLocation()))
		{
			continue;
		}
		const float Dist = FVector::Dist2D(Doll->GetActorLocation(), GetActorLocation());
		if (Dist < BestDist)
		{
			BestDist = Dist;
			Best = Doll;
		}
	}
	return Best;
}

void ABossDustEater::TickAI(float DeltaSeconds)
{
	switch (BossState)
	{
	case EBossState::Dormant:
	case EBossState::Defeated:
		CurrentSpeed = 0.f;
		break;

	case EBossState::Patrol:
	{
		if (Phase >= 3)
		{
			DockTimer += DeltaSeconds;
		}
		if (AWindupCharacter* Target = FindTarget())
		{
			ChaseTarget = Target;
			LostTargetTime = 0.f;
			SetBossState(EBossState::Chase);
			break;
		}
		if (PatrolPoints.Num() > 0)
		{
			const FVector P = PatrolPoints[PatrolIndex % PatrolPoints.Num()];
			if (FVector::Dist2D(P, GetActorLocation()) < 160.f)
			{
				PatrolIndex = (PatrolIndex + 1) % PatrolPoints.Num();
			}
			TickMovement(DeltaSeconds, P, 0.6f, false);
		}
		if (Phase >= 3 && DockTimer >= CrankBalance::BossDockInterval && Dock)
		{
			SetBossState(EBossState::ReturnToDock);
		}
		break;
	}

	case EBossState::Chase:
	{
		if (Phase >= 3)
		{
			DockTimer += DeltaSeconds;
		}
		AWindupCharacter* Target = ChaseTarget.Get();
		const bool bValid = Target && IsInsideArena(Target->GetActorLocation())
			&& Target->GetBodyState() != ECrankBodyState::Trapped && Target->GetBodyState() != ECrankBodyState::Scattered
			&& Target->GetBodyState() != ECrankBodyState::Frozen && Target->GetBodyState() != ECrankBodyState::Carried;
		if (!bValid)
		{
			Target = FindTarget();
			ChaseTarget = Target;
		}
		if (!Target)
		{
			SetBossState(EBossState::Patrol);
			break;
		}
		const float Dist = FVector::Dist2D(Target->GetActorLocation(), GetActorLocation());
		LostTargetTime = (Phase < 3 && Dist > DetectRadius * 1.8f) ? LostTargetTime + DeltaSeconds : 0.f;
		if (LostTargetTime > 3.f)
		{
			SetBossState(EBossState::Patrol);
			break;
		}
		TickMovement(DeltaSeconds, Target->GetActorLocation() + Target->GetVelocity() * 0.25f, 1.f, true);
		if (Phase >= 3 && DockTimer >= CrankBalance::BossDockInterval && Dock && BossState == EBossState::Chase)
		{
			SetBossState(EBossState::ReturnToDock);
		}
		break;
	}

	case EBossState::ReturnToDock:
	{
		if (!Dock)
		{
			SetBossState(EBossState::Chase);
			break;
		}
		const FVector P = Dock->GetDockPoint();
		TickMovement(DeltaSeconds, P, 1.f, false);
		if (FVector::Dist2D(P, GetActorLocation()) < 80.f)
		{
			if (Dock->IsDisabled())
			{
				DockTimer = 0.f;
				SetBossState(EBossState::Chase);
			}
			else
			{
				CurrentSpeed = 0.f;
				Dock->SetCharging(true);
				SetBossState(EBossState::Docking);
			}
		}
		else if (GetStateTime() > 12.f)
		{
			DockTimer = 0.f;
			SetBossState(EBossState::Chase);
		}
		break;
	}

	case EBossState::Docking:
	{
		const float FacingYaw = Dock ? Dock->GetDockFacing().Rotation().Yaw : GetActorRotation().Yaw;
		SetActorRotation(FRotator(0.f, FMath::FixedTurn(GetActorRotation().Yaw, FacingYaw, 180.f * DeltaSeconds), 0.f));
		if (!Dock || Dock->IsDisabled())
		{
			if (Dock)
			{
				Dock->SetCharging(false);
			}
			DockTimer = 0.f;
			SetBossState(EBossState::Chase);
			break;
		}
		if (GetStateTime() >= CrankBalance::BossDockStayTime)
		{
			Dock->SetCharging(false);
			for (int32 i = 0; i < 3; ++i)
			{
				if ((CellMask & (1 << i)) == 0)
				{
					CellMask |= (1 << i);
					break;
				}
			}
			Cells = FMath::Min(CrankBalance::BossCells, Cells + 1);
			Phase = 2;
			DockTimer = 0.f;
			MulticastBossEvent(BossEvent::Recharged, GetActorLocation());
			SetBossState(EBossState::Chase);
		}
		break;
	}

	case EBossState::Stunned:
	case EBossState::MiniStunned:
		CurrentSpeed = FMath::FInterpConstantTo(CurrentSpeed, 0.f, DeltaSeconds, 2000.f);
		if (GetStateTime() >= StunDuration)
		{
			ShakeOffRiders();
			for (float& P : CellProgress)
			{
				P = 0.f;
			}
			SetBossState(ChaseTarget.IsValid() ? EBossState::Chase : EBossState::Patrol);
		}
		break;

	case EBossState::PhaseTransition:
		AddActorWorldRotation(FRotator(0.f, 420.f * DeltaSeconds, 0.f));
		if (GetStateTime() >= CrankBalance::BossPhaseTransition)
		{
			FinishPhaseTransition();
		}
		break;
	}
}

void ABossDustEater::TickMovement(float DeltaSeconds, const FVector& Target, float SpeedScale, bool bAllowMiniStun)
{
	UWorld* World = GetWorld();
	const float MaxSpeed = CrankBalance::BossSpeed(Phase) * SpeedScale;
	const FVector Loc = GetActorLocation();
	FVector To = Target - Loc;
	To.Z = 0.f;

	const float Yaw = GetActorRotation().Yaw;
	const float DesiredYaw = To.IsNearlyZero() ? Yaw : To.Rotation().Yaw;
	const float TurnRate = Phase <= 1 ? 110.f : (Phase == 2 ? 140.f : 170.f);
	const float NewYaw = FMath::FixedTurn(Yaw, DesiredYaw, TurnRate * DeltaSeconds);
	const float AngleOff = FMath::Abs(FMath::FindDeltaAngleDegrees(NewYaw, DesiredYaw));
	float SpeedTarget = MaxSpeed * FMath::Clamp(1.f - AngleOff / 120.f, 0.25f, 1.f);
	if (To.Size() < 40.f)
	{
		SpeedTarget = 0.f;
	}
	CurrentSpeed = FMath::FInterpConstantTo(CurrentSpeed, SpeedTarget, DeltaSeconds, 700.f);

	const FQuat Rot = FRotator(0.f, NewYaw, 0.f).Quaternion();
	FVector Move = Rot.GetForwardVector() * CurrentSpeed * DeltaSeconds;
	FVector NewLoc = Loc;

	FCollisionObjectQueryParams ObjParams;
	ObjParams.AddObjectTypesToQuery(ECC_WorldStatic);
	ObjParams.AddObjectTypesToQuery(ECC_BossBlocker);
	FCollisionQueryParams Params(SCENE_QUERY_STAT(BossMove), false, this);
	const FVector Center = Loc + FVector(0.f, 0.f, 50.f);
	const FCollisionShape Shape = FCollisionShape::MakeBox(FVector(BodyRadius * 0.82f, BodyRadius * 0.82f, 22.f));

	FHitResult Hit;
	if (!Move.IsNearlyZero() && World->SweepSingleByObjectType(Hit, Center, Center + Move, Rot, ObjParams, Shape, Params))
	{
		if (Hit.bStartPenetrating)
		{
			NewLoc += FVector(Hit.Normal.X, Hit.Normal.Y, 0.f) * (Hit.PenetrationDepth + 1.f);
			Move = FVector::ZeroVector;
		}
		else
		{
			const UPrimitiveComponent* HitComp = Hit.GetComponent();
			const bool bLeg = HitComp && HitComp->ComponentHasTag(TEXT("TableLeg"));
			if (bAllowMiniStun && bLeg && CurrentSpeed > MaxSpeed * 0.7f)
			{
				MulticastBossEvent(BossEvent::MiniStun, Hit.ImpactPoint);
				Stun(false);
				SetActorRotation(Rot);
				return;
			}
			const FVector Allowed = Move * Hit.Time;
			FVector Slide = FVector::VectorPlaneProject(Move * (1.f - Hit.Time), Hit.Normal);
			Slide.Z = 0.f;
			Move = Allowed + Slide * 0.8f;
			CurrentSpeed *= 0.94f;
		}
	}

	NewLoc += Move;
	NewLoc.Z = Loc.Z;
	if (!IsInsideArena(NewLoc))
	{
		NewLoc = Loc;
		CurrentSpeed *= 0.5f;
	}
	SetActorLocationAndRotation(NewLoc, Rot, false, nullptr, ETeleportType::None);

	// Phase 2+: mop mode leaves slippery water behind.
	if (Phase >= 2 && WaterTrail)
	{
		WaterDistance += Move.Size();
		if (WaterDistance > 70.f)
		{
			WaterDistance = 0.f;
			WaterTrail->AddPoint(NewLoc - Rot.GetForwardVector() * (BodyRadius - 10.f));
		}
	}
}

void ABossDustEater::TickContacts(float DeltaSeconds)
{
	for (auto It = ContactCooldown.CreateIterator(); It; ++It)
	{
		It.Value() -= DeltaSeconds;
		if (It.Value() <= 0.f || !It.Key().IsValid())
		{
			It.RemoveCurrent();
		}
	}

	const bool bMoving = BossState == EBossState::Patrol || BossState == EBossState::Chase || BossState == EBossState::ReturnToDock;
	const bool bIntakeOn = bMoving || BossState == EBossState::Docking;
	if (!bIntakeOn)
	{
		return;
	}

	TArray<AActor*> Overlaps;
	IntakeZone->GetOverlappingActors(Overlaps, AWindupCharacter::StaticClass());
	for (AActor* Actor : Overlaps)
	{
		AWindupCharacter* Doll = Cast<AWindupCharacter>(Actor);
		const ECrankBodyState S = Doll->GetBodyState();
		if ((S == ECrankBodyState::Normal || S == ECrankBodyState::Ragdoll || S == ECrankBodyState::GettingUp) && !ContactCooldown.Contains(Doll))
		{
			Doll->EnterDustBin(this);
			Trapped.AddUnique(Doll);
			ContactCooldown.Add(Doll, 1.5f);
			MulticastBossEvent(BossEvent::Suck, IntakeZone->GetComponentLocation());
		}
	}

	if (!bMoving)
	{
		return;
	}

	const float BrushScale = Phase >= 2 ? 1.45f : 1.f;
	BrushZoneL->SetSphereRadius(55.f * BrushScale);
	BrushZoneR->SetSphereRadius(55.f * BrushScale);
	for (USphereComponent* Zone : { BrushZoneL.Get(), BrushZoneR.Get() })
	{
		Overlaps.Reset();
		Zone->GetOverlappingActors(Overlaps, AWindupCharacter::StaticClass());
		for (AActor* Actor : Overlaps)
		{
			AWindupCharacter* Doll = Cast<AWindupCharacter>(Actor);
			if (Doll->GetBodyState() != ECrankBodyState::Normal || ContactCooldown.Contains(Doll))
			{
				continue;
			}
			const FVector Away = (Doll->GetActorLocation() - GetActorLocation()).GetSafeNormal2D();
			ContactCooldown.Add(Doll, 1.5f);
			Doll->StartRagdoll(Away * 480.f + FVector(0.f, 0.f, 320.f), 1.f);
			MulticastBossEvent(BossEvent::Brush, Zone->GetComponentLocation());
		}
	}
}

void ABossDustEater::TickTrapped(float DeltaSeconds)
{
	for (int32 i = Trapped.Num() - 1; i >= 0; --i)
	{
		AWindupCharacter* Doll = Trapped[i];
		if (!IsValid(Doll) || Doll->GetBodyState() != ECrankBodyState::Trapped)
		{
			Trapped.RemoveAt(i);
			continue;
		}
		Doll->GetTorque()->AddTorque(-CrankBalance::BossTrapDrain * DeltaSeconds);
		if (Doll->GetTorque()->IsDischarged())
		{
			EjectTrapped(true, Doll);
		}
	}
}

void ABossDustEater::EjectTrapped(bool bAuto, AWindupCharacter* Only)
{
	const FVector Back = -GetActorForwardVector();
	for (int32 i = Trapped.Num() - 1; i >= 0; --i)
	{
		AWindupCharacter* Doll = Trapped[i];
		if (Only && Doll != Only)
		{
			continue;
		}
		Trapped.RemoveAt(i);
		if (!IsValid(Doll))
		{
			continue;
		}
		const FVector EjectLoc = GetActorLocation() + Back * (BodyRadius + 70.f) + FVector(0.f, 0.f, 60.f);
		const FVector EjectVel = Back * (bAuto ? 300.f : 650.f) + FVector(0.f, 0.f, bAuto ? 200.f : 420.f);
		Doll->ExitDustBin(EjectLoc, EjectVel, bAuto);
		ContactCooldown.Add(Doll, 2.5f);
		MulticastBossEvent(BossEvent::Eject, EjectLoc);
	}
}

void ABossDustEater::ShakeOffRiders()
{
	const float Top = GetActorLocation().Z + 60.f;
	for (TActorIterator<AWindupCharacter> It(GetWorld()); It; ++It)
	{
		AWindupCharacter* Doll = *It;
		if (Doll->GetBodyState() != ECrankBodyState::Normal)
		{
			continue;
		}
		const FVector Offset = Doll->GetActorLocation() - GetActorLocation();
		if (Offset.Size2D() < BodyRadius + 30.f && Doll->GetActorLocation().Z > Top)
		{
			Doll->GetLaunch()->Launch(Offset.GetSafeNormal2D() * 380.f + FVector(0.f, 0.f, 380.f), nullptr, false, false);
		}
	}
}

// ------------------------------------------------------------------------------------------
// Stun / cells / phases

void ABossDustEater::HandleDollImpact(AWindupCharacter* Doll, const FVector& Velocity, const FHitResult& Hit)
{
	if (!HasAuthority() || BossState == EBossState::Stunned || BossState == EBossState::PhaseTransition || BossState == EBossState::Defeated)
	{
		return;
	}
	if (BossState == EBossState::Dormant)
	{
		Activate();
	}

	const FVector ToHit = (FVector(Hit.ImpactPoint) - GetActorLocation()).GetSafeNormal2D();
	const float Front = FVector::DotProduct(GetActorForwardVector(), ToHit);
	const float Speed = Velocity.Size();
	if (Front > 0.3f && Speed >= CrankBalance::BossStunImpactSpeed)
	{
		MulticastBossEvent(BossEvent::BigBonk, Hit.ImpactPoint);
		Stun(true);
	}
	else
	{
		MulticastBossEvent(BossEvent::SmallBonk, Hit.ImpactPoint);
		ChaseTarget = Doll;
		if (BossState == EBossState::Patrol)
		{
			SetBossState(EBossState::Chase);
		}
	}
}

void ABossDustEater::Stun(bool bFull)
{
	CurrentSpeed = 0.f;
	StunDuration = bFull ? CrankBalance::BossStunTime(Phase) : CrankBalance::BossMiniStunTime;
	for (float& P : CellProgress)
	{
		P = 0.f;
	}
	if (Dock)
	{
		Dock->SetCharging(false);
	}
	SetBossState(bFull ? EBossState::Stunned : EBossState::MiniStunned);
}

void ABossDustEater::PullCell(int32 Index)
{
	if ((CellMask & (1 << Index)) == 0)
	{
		return;
	}
	CellMask &= ~(1 << Index);
	Cells = FMath::Max(0, Cells - 1);
	CellProgress[Index] = 0.f;

	UStaticMeshComponent* CellComps[3] = { Cell0, Cell1, Cell2 };
	const FVector CellLoc = CellComps[Index]->GetComponentLocation();
	if (SpentCellClass)
	{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		if (ACarryableActor* Spent = GetWorld()->SpawnActor<ACarryableActor>(SpentCellClass, FTransform(CellLoc + FVector(0, 0, 40.f)), Params))
		{
			if (Spent->GetMesh() && Spent->GetMesh()->IsSimulatingPhysics())
			{
				Spent->GetMesh()->SetPhysicsLinearVelocity(FMath::VRandCone(FVector::UpVector, 0.6f) * 520.f);
			}
		}
	}
	MulticastBossEvent(BossEvent::CellPulled, CellLoc);

	if (Cells <= 0)
	{
		Defeat();
	}
	else
	{
		SetBossState(EBossState::PhaseTransition);
	}
}

void ABossDustEater::FinishPhaseTransition()
{
	Phase = FMath::Clamp(4 - Cells, 1, 3);
	DockTimer = 0.f;
	ShakeOffRiders();
	MulticastBossEvent(BossEvent::PhaseUp, GetActorLocation());
	SetBossState(ChaseTarget.IsValid() ? EBossState::Chase : EBossState::Patrol);
}

void ABossDustEater::Defeat()
{
	CurrentSpeed = 0.f;
	SetBossState(EBossState::Defeated);
	EjectTrapped(false);
	if (Dock)
	{
		Dock->SetCharging(false);
	}

	const FVector Back = -GetActorForwardVector();
	UClass* SpringClass = MainSpringClass ? MainSpringClass.Get() : ACarryablePart::StaticClass();
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
	const FVector SpawnLoc = GetActorLocation() + Back * (BodyRadius + 110.f) + FVector(0.f, 0.f, 70.f);
	if (ACarryableActor* Spring = GetWorld()->SpawnActor<ACarryableActor>(SpringClass, FTransform(GetActorRotation(), SpawnLoc), Params))
	{
		if (Spring->GetMesh() && Spring->GetMesh()->IsSimulatingPhysics())
		{
			Spring->GetMesh()->SetPhysicsLinearVelocity(Back * 380.f + FVector(0.f, 0.f, 200.f));
		}
	}

	MulticastBossEvent(BossEvent::Defeated, GetActorLocation());
	if (Arena)
	{
		Arena->OnBossDefeated(this);
	}
}

void ABossDustEater::DebugCommand(FName Command, float Value)
{
	if (!HasAuthority())
	{
		return;
	}
	if (Command == TEXT("BossWake"))
	{
		Activate();
	}
	else if (Command == TEXT("BossStun"))
	{
		Activate();
		Stun(true);
	}
	else if (Command == TEXT("BossPhase"))
	{
		Activate();
		Phase = FMath::Clamp(FMath::RoundToInt(Value), 1, 3);
		Cells = 4 - Phase;
		CellMask = Cells >= 3 ? 0x7 : (Cells == 2 ? 0x3 : 0x1);
		DockTimer = 0.f;
		OnRep_Cells();
	}
	else if (Command == TEXT("BossKill"))
	{
		Activate();
		Defeat();
	}
	else if (Command == TEXT("BossDock"))
	{
		DockTimer = 999.f;
	}
}

// ------------------------------------------------------------------------------------------
// Suction (phase 3)

FVector ABossDustEater::GetSuctionAccel(const AWindupCharacter* Doll) const
{
	if (!Doll || Phase < 3)
	{
		return FVector::ZeroVector;
	}
	if (BossState != EBossState::Patrol && BossState != EBossState::Chase && BossState != EBossState::ReturnToDock)
	{
		return FVector::ZeroVector;
	}
	const FVector Intake = IntakeZone->GetComponentLocation();
	FVector To = Intake - Doll->GetActorLocation();
	To.Z = 0.f;
	const float Dist = To.Size();
	if (Dist > CrankBalance::BossSuctionRadius || Dist < 1.f)
	{
		return FVector::ZeroVector;
	}
	const FVector Dir = To / Dist;
	if (FVector::DotProduct(GetActorForwardVector(), -Dir) < 0.1f)
	{
		return FVector::ZeroVector;	// only in front of the intake
	}
	return Dir * 1500.f * (1.f - 0.4f * Dist / CrankBalance::BossSuctionRadius);
}

// ------------------------------------------------------------------------------------------
// Grabbing cells / lever

bool ABossDustEater::CanBeGrabbed(const AWindupCharacter* By, ECrankHand Hand, const UPrimitiveComponent* Comp) const
{
	const UBoxComponent* CellGrabs[3] = { CellGrab0, CellGrab1, CellGrab2 };
	for (int32 i = 0; i < 3; ++i)
	{
		if (Comp == CellGrabs[i])
		{
			return BossState == EBossState::Stunned && (CellMask & (1 << i)) != 0;
		}
	}
	if (Comp == BinLeverGrab)
	{
		return Trapped.Num() > 0 && !Trapped.Contains(By);
	}
	return false;
}

void ABossDustEater::OnGrabbed(AWindupCharacter* By, ECrankHand Hand, UPrimitiveComponent* Comp)
{
	const UBoxComponent* CellGrabs[3] = { CellGrab0, CellGrab1, CellGrab2 };
	for (int32 i = 0; i < 3; ++i)
	{
		if (Comp == CellGrabs[i])
		{
			CellHolders[i]++;
			CrankSound::PlayAt(this, TEXT("SFX_Grab"), Comp->GetComponentLocation(), 0.8f, 0.7f);
			return;
		}
	}
	if (Comp == BinLeverGrab)
	{
		LeverHolders++;
	}
}

void ABossDustEater::OnReleased(AWindupCharacter* By, ECrankHand Hand, UPrimitiveComponent* Comp, const FVector& ThrowVelocity)
{
	const UBoxComponent* CellGrabs[3] = { CellGrab0, CellGrab1, CellGrab2 };
	for (int32 i = 0; i < 3; ++i)
	{
		if (Comp == CellGrabs[i])
		{
			CellHolders[i] = FMath::Max(0, CellHolders[i] - 1);
			return;
		}
	}
	if (Comp == BinLeverGrab)
	{
		LeverHolders = FMath::Max(0, LeverHolders - 1);
	}
}

bool ABossDustEater::TickHeld(AWindupCharacter* By, ECrankHand Hand, UPrimitiveComponent* Comp, float DeltaTime)
{
	const UBoxComponent* CellGrabs[3] = { CellGrab0, CellGrab1, CellGrab2 };
	for (int32 i = 0; i < 3; ++i)
	{
		if (Comp == CellGrabs[i])
		{
			if (BossState != EBossState::Stunned || (CellMask & (1 << i)) == 0)
			{
				return false;
			}
			if (FVector::Dist(By->GetActorLocation(), Comp->GetComponentLocation()) > 170.f)
			{
				return false;
			}
			CellProgress[i] += DeltaTime / CrankBalance::BossCellPullTime / FMath::Max(1, CellHolders[i]);
			if (CellProgress[i] >= 1.f)
			{
				PullCell(i);
				return false;
			}
			return true;
		}
	}
	if (Comp == BinLeverGrab)
	{
		if (Trapped.Num() == 0 || FVector::Dist2D(By->GetActorLocation(), Comp->GetComponentLocation()) > 160.f)
		{
			return false;
		}
		LeverProgress += DeltaTime / CrankBalance::BossLeverHoldTime / FMath::Max(1, LeverHolders);
		if (LeverProgress >= 1.f)
		{
			LeverProgress = 0.f;
			CrankSound::PlayAt(this, TEXT("SFX_Lever"), Comp->GetComponentLocation(), 1.f);
			EjectTrapped(false);
			return false;
		}
		return true;
	}
	return false;
}

float ABossDustEater::GetHoldProgress(const UPrimitiveComponent* Comp) const
{
	const UBoxComponent* CellGrabs[3] = { CellGrab0, CellGrab1, CellGrab2 };
	for (int32 i = 0; i < 3; ++i)
	{
		if (Comp == CellGrabs[i])
		{
			return CellProgress[i];
		}
	}
	return Comp == BinLeverGrab ? LeverProgress : -1.f;
}

FText ABossDustEater::GetGrabLabel(const UPrimitiveComponent* Comp) const
{
	if (Comp == BinLeverGrab)
	{
		return NSLOCTEXT("Crank", "BinLever", "먼지통 레버 (친구 구출)");
	}
	return NSLOCTEXT("Crank", "BossCell", "배터리 셀 뽑기");
}

// ------------------------------------------------------------------------------------------
// Visuals

void ABossDustEater::OnRep_BossState()
{
	ApplyVisualState();
}

void ABossDustEater::OnRep_Cells()
{
	ApplyVisualState();
}

void ABossDustEater::ApplyVisualState()
{
	FLinearColor Eye = FLinearColor(0.2f, 1.f, 0.3f);
	switch (BossState)
	{
	case EBossState::Dormant:			Eye = FLinearColor(0.05f, 0.25f, 0.08f); break;
	case EBossState::Patrol:			Eye = FLinearColor(0.2f, 1.f, 0.3f); break;
	case EBossState::Chase:				Eye = FLinearColor(1.f, 0.08f, 0.05f); break;
	case EBossState::Stunned:			Eye = FLinearColor(1.f, 0.85f, 0.1f); break;
	case EBossState::MiniStunned:		Eye = FLinearColor(1.f, 0.5f, 0.1f); break;
	case EBossState::PhaseTransition:	Eye = FLinearColor(1.f, 1.f, 1.f); break;
	case EBossState::ReturnToDock:
	case EBossState::Docking:			Eye = FLinearColor(0.2f, 0.5f, 1.f); break;
	case EBossState::Defeated:			Eye = FLinearColor(0.02f, 0.02f, 0.02f); break;
	}
	EyeLight->SetLightColor(Eye);
	EyeLight->SetIntensity(BossState == EBossState::Defeated ? 0.f : 3000.f);
	if (EyeMID)
	{
		EyeMID->SetVectorParameterValue(TEXT("EyeColor"), Eye * 8.f);
	}

	const bool bStunned = BossState == EBossState::Stunned || BossState == EBossState::MiniStunned;
	StunFX->SetEmitting(bStunned);
	SmokeFX->SetEmitting(BossState == EBossState::Defeated || (Phase >= 3 && BossState != EBossState::Dormant));

	UStaticMeshComponent* CellComps[3] = { Cell0, Cell1, Cell2 };
	for (int32 i = 0; i < 3; ++i)
	{
		CellComps[i]->SetVisibility((CellMask & (1 << i)) != 0);
	}

	if (BossState == EBossState::Stunned)
	{
		CrankSound::PlayAt(this, TEXT("SFX_Hatch"), GetActorLocation(), 1.f);
	}
	if (BossState == EBossState::Defeated)
	{
		BinDoor->SetRelativeRotation(FRotator(0.f, 0.f, 0.f));
	}
}

void ABossDustEater::UpdateVisuals(float DeltaSeconds)
{
	const bool bHatchOpen = BossState == EBossState::Stunned || BossState == EBossState::Defeated;
	HatchAlpha = FMath::FInterpConstantTo(HatchAlpha, bHatchOpen ? 1.f : 0.f, DeltaSeconds, 3.f);
	HatchPivot->SetRelativeRotation(FRotator(HatchAlpha * 105.f, 0.f, 0.f));

	const bool bRampDown = BossState == EBossState::Stunned;
	RampAlpha = FMath::FInterpConstantTo(RampAlpha, bRampDown ? 1.f : 0.f, DeltaSeconds, 2.5f);
	RampPivot->SetRelativeRotation(FRotator(FMath::Lerp(95.f, -24.f, RampAlpha), 180.f, 0.f));
	const ECollisionEnabled::Type RampCollision = RampAlpha > 0.95f ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision;
	if (Ramp->GetCollisionEnabled() != RampCollision)
	{
		Ramp->SetCollisionEnabled(RampCollision);
	}

	const bool bActiveMotor = IsActive() && BossState != EBossState::Stunned && BossState != EBossState::MiniStunned;
	BrushSpin += DeltaSeconds * (bActiveMotor ? 900.f : 60.f);
	BrushL->SetRelativeRotation(FRotator(0.f, BrushSpin, 0.f));
	BrushR->SetRelativeRotation(FRotator(0.f, -BrushSpin, 0.f));
	const float BrushScale = Phase >= 2 ? 1.35f : 1.f;
	BrushL->SetRelativeScale3D(FVector(BrushScale));
	BrushR->SetRelativeScale3D(FVector(BrushScale));

	const bool bSuction = Phase >= 3 && (BossState == EBossState::Patrol || BossState == EBossState::Chase || BossState == EBossState::ReturnToDock);
	SuctionFX->SetEmitting(bSuction);

	if (BossState == EBossState::Stunned || BossState == EBossState::PhaseTransition)
	{
		EyeBlink += DeltaSeconds;
		EyeLight->SetIntensity(FMath::Fmod(EyeBlink, 0.4f) < 0.2f ? 3500.f : 300.f);
	}

	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	const float Speed = HasAuthority() ? CurrentSpeed : ServerSpeed;
	if (MotorAudio)
	{
		if (bActiveMotor && !MotorAudio->IsPlaying())
		{
			MotorAudio->Play();
		}
		else if (!bActiveMotor && MotorAudio->IsPlaying())
		{
			MotorAudio->FadeOut(0.5f, 0.f);
		}
		MotorAudio->SetPitchMultiplier(0.8f + Speed / 900.f + (Phase - 1) * 0.08f);
	}
	if (SuctionAudio)
	{
		if (bSuction && !SuctionAudio->IsPlaying())
		{
			SuctionAudio->FadeIn(0.4f);
		}
		else if (!bSuction && SuctionAudio->IsPlaying())
		{
			SuctionAudio->FadeOut(0.4f, 0.f);
		}
	}
}

void ABossDustEater::MulticastBossEvent_Implementation(uint8 EventId, FVector_NetQuantize Location)
{
	switch (EventId)
	{
	case BossEvent::SmallBonk:
		UCrankFXSubsystem::Spawn(this, ECrankFX::Sparks, Location, FRotator(0.f, GetActorRotation().Yaw, 0.f), 0.8f);
		CrankSound::PlayAt(this, TEXT("SFX_BonkMetal"), Location, 0.8f, 1.2f);
		break;
	case BossEvent::BigBonk:
		UCrankFXSubsystem::Spawn(this, ECrankFX::Sparks, Location, FRotator(0.f, GetActorRotation().Yaw, 0.f), 2.f);
		UCrankFXSubsystem::Spawn(this, ECrankFX::Puff, Location, FRotator::ZeroRotator, 2.f);
		CrankSound::PlayAt(this, TEXT("SFX_BonkMetal"), Location, 1.f, 0.8f);
		CrankSound::PlayAt(this, TEXT("SFX_BossStun"), GetActorLocation(), 1.f);
		break;
	case BossEvent::Suck:
		CrankSound::PlayAt(this, TEXT("SFX_Suck"), Location, 1.f);
		UCrankFXSubsystem::Spawn(this, ECrankFX::DustBurst, Location, FRotator::ZeroRotator, 0.6f);
		break;
	case BossEvent::Brush:
		CrankSound::PlayAt(this, TEXT("SFX_BrushHit"), Location, 1.f);
		UCrankFXSubsystem::Spawn(this, ECrankFX::Puff, Location, FRotator::ZeroRotator, 1.2f);
		break;
	case BossEvent::Eject:
		CrankSound::PlayAt(this, TEXT("SFX_Eject"), Location, 1.f);
		UCrankFXSubsystem::Spawn(this, ECrankFX::DustBurst, Location, FRotator::ZeroRotator, 1.5f);
		break;
	case BossEvent::CellPulled:
		CrankSound::PlayAt(this, TEXT("SFX_CellPull"), Location, 1.f);
		UCrankFXSubsystem::Spawn(this, ECrankFX::CellZap, Location, FRotator::ZeroRotator, 1.5f);
		break;
	case BossEvent::PhaseUp:
		CrankSound::PlayAt(this, TEXT("SFX_Alarm"), Location, 1.f);
		UCrankFXSubsystem::Spawn(this, ECrankFX::BossSmoke, Location + FVector(0, 0, 80.f), FRotator::ZeroRotator, 1.2f);
		break;
	case BossEvent::Recharged:
		CrankSound::PlayAt(this, TEXT("SFX_Dock"), Location, 1.f, 0.8f);
		UCrankFXSubsystem::Spawn(this, ECrankFX::CellZap, Location + FVector(0, 0, 80.f), FRotator::ZeroRotator, 2.f);
		break;
	case BossEvent::Defeated:
		CrankSound::PlayAt(this, TEXT("SFX_BossDown"), Location, 1.f);
		UCrankFXSubsystem::Spawn(this, ECrankFX::BossSmoke, Location + FVector(0, 0, 80.f), FRotator::ZeroRotator, 1.6f);
		UCrankFXSubsystem::Spawn(this, ECrankFX::Pop, Location + FVector(0, 0, 80.f), FRotator::ZeroRotator, 1.5f);
		break;
	case BossEvent::MiniStun:
		CrankSound::PlayAt(this, TEXT("SFX_BonkMetal"), Location, 1.f, 0.6f);
		UCrankFXSubsystem::Spawn(this, ECrankFX::Sparks, Location, FRotator(0.f, GetActorRotation().Yaw + 180.f, 0.f), 1.5f);
		break;
	case BossEvent::Wake:
		CrankSound::PlayAt(this, TEXT("SFX_BossWake"), Location, 1.f);
		break;
	}
}
