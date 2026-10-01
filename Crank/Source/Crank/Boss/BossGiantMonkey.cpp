#include "Boss/BossGiantMonkey.h"
#include "Boss/CymbalProjectile.h"
#include "Boss/GiantArena.h"
#include "Character/WindupCharacter.h"
#include "Gameplay/CarryableActor.h"
#include "Game/RaidGameMode.h"
#include "Game/RaidGameState.h"
#include "CrankBalance.h"
#include "FX/CrankFX.h"
#include "FX/CrankSound.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Boss/GiantAnimInstance.h"
#include "AnimationRuntime.h"
#include "Components/StaticMeshComponent.h"
#include "Components/BoxComponent.h"
#include "Components/SphereComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/AudioComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/GameStateBase.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Net/UnrealNetwork.h"
#include "EngineUtils.h"
#include "Engine/World.h"

namespace GiantEvent
{
	enum : uint8
	{
		Wake, ClapRing, StompRing, PhaseRing, KeyHit, Deflect, Bonk, BodyBonk, WindDown, Rewind, Throw, PhaseUp, Defeated, KeyPop,
		DamageBody, DamageKey,
	};

	// Ring shapes shared by the server (damage) and every client (visual).
	struct FRingSpec { float MaxRadius; float Speed; float Strength; };
	inline FRingSpec Spec(uint8 Event)
	{
		switch (Event)
		{
		case StompRing:	return { 1000.f, 1300.f, 0.7f };
		case PhaseRing:	return { 2400.f, 1300.f, 1.1f };
		default:		return { CrankBalance::GiantRingMax, CrankBalance::GiantRingSpeed, 1.f };
		}
	}
}

ABossGiantMonkey::ABossGiantMonkey()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PrePhysics;
	bReplicates = true;
	SetReplicatingMovement(false);
	bAlwaysRelevant = true;
	SetNetUpdateFrequency(30.f);

	BodyCollision = CreateDefaultSubobject<UCapsuleComponent>(TEXT("BodyCollision"));
	BodyCollision->InitCapsuleSize(230.f, 450.f);
	BodyCollision->SetCollisionProfileName(TEXT("BlockAllDynamic"));
	BodyCollision->SetCanEverAffectNavigation(false);
	SetRootComponent(BodyCollision);

	Mesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(BodyCollision);
	Mesh->SetRelativeLocation(FVector(0.f, 0.f, -450.f));
	Mesh->SetRelativeRotation(FRotator(0.f, -90.f, 0.f));	// rig faces +Y; actor forward is +X
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->SetAnimationMode(EAnimationMode::AnimationBlueprint);
	Mesh->SetAnimInstanceClass(UGiantAnimInstance::StaticClass());
	// The key / head hit volumes ride bones, so the server needs a fresh pose even when nobody looks.
	Mesh->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
	Mesh->bEnableUpdateRateOptimizations = false;

	// Lying on its back after the defeat clip: a long capsule behind the feet (actor -X), resting on the floor.
	FallenCollision = CreateDefaultSubobject<UCapsuleComponent>(TEXT("FallenCollision"));
	FallenCollision->SetupAttachment(BodyCollision);
	FallenCollision->InitCapsuleSize(200.f, 470.f);
	FallenCollision->SetRelativeLocation(FVector(-400.f, 0.f, -450.f + 210.f));
	FallenCollision->SetRelativeRotation(FRotator(90.f, 0.f, 0.f));
	FallenCollision->SetCollisionProfileName(TEXT("BlockAllDynamic"));
	FallenCollision->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	FallenCollision->SetCanEverAffectNavigation(false);

	// The weak point: the big wind-up key sticking out of the back (component space of the rig).
	KeyPivot = CreateDefaultSubobject<USceneComponent>(TEXT("KeyPivot"));
	KeyPivot->SetupAttachment(Mesh);
	KeyPivot->SetRelativeLocation(FVector(13.f, -215.f, 450.f));
	KeyPivot->SetRelativeRotation(FRotator(0.f, -90.f, 0.f));

	KeyMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("KeyMesh"));
	KeyMesh->SetupAttachment(KeyPivot);
	KeyMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	KeyHitBox = CreateDefaultSubobject<UBoxComponent>(TEXT("KeyHitBox"));
	KeyHitBox->SetupAttachment(KeyPivot);
	KeyHitBox->SetRelativeLocation(FVector(220.f, 0.f, 0.f));
	KeyHitBox->SetBoxExtent(FVector(240.f, 125.f, 140.f));
	KeyHitBox->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	KeyHitBox->SetCollisionObjectType(ECC_WorldDynamic);
	KeyHitBox->SetCollisionResponseToAllChannels(ECR_Ignore);
	KeyHitBox->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
	KeyHitBox->SetCanEverAffectNavigation(false);

	KeyLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("KeyLight"));
	KeyLight->SetupAttachment(KeyPivot);
	KeyLight->SetRelativeLocation(FVector(380.f, 0.f, 0.f));
	KeyLight->SetIntensityUnits(ELightUnits::Lumens);
	KeyLight->SetIntensity(0.f);
	KeyLight->SetAttenuationRadius(900.f);
	KeyLight->SetLightColor(FLinearColor(1.f, 0.78f, 0.3f));
	KeyLight->SetCastShadows(false);

	HeadHitSphere = CreateDefaultSubobject<USphereComponent>(TEXT("HeadHitSphere"));
	HeadHitSphere->SetupAttachment(Mesh);
	HeadHitSphere->SetRelativeLocation(FVector(21.f, -40.f, 690.f));
	HeadHitSphere->InitSphereRadius(185.f);
	HeadHitSphere->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	HeadHitSphere->SetCollisionObjectType(ECC_WorldDynamic);
	HeadHitSphere->SetCollisionResponseToAllChannels(ECR_Ignore);
	HeadHitSphere->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
	HeadHitSphere->SetCanEverAffectNavigation(false);

	StarsFX = CreateDefaultSubobject<UCrankFXEmitterComponent>(TEXT("StarsFX"));
	StarsFX->SetupAttachment(Mesh);
	StarsFX->SetRelativeLocation(FVector(21.f, -40.f, 930.f));
	StarsFX->Effect = ECrankFX::StunStars;
	StarsFX->Rate = 5.f;
	StarsFX->EffectScale = 4.f;
	StarsFX->bEmitting = false;

	SteamFX = CreateDefaultSubobject<UCrankFXEmitterComponent>(TEXT("SteamFX"));
	SteamFX->SetupAttachment(KeyPivot);
	SteamFX->Effect = ECrankFX::Steam;
	SteamFX->Rate = 18.f;
	SteamFX->EffectScale = 3.f;
	SteamFX->bEmitting = false;
}

void ABossGiantMonkey::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ABossGiantMonkey, GiantState);
	DOREPLIFETIME(ABossGiantMonkey, StateStartTime);
	DOREPLIFETIME(ABossGiantMonkey, StateDuration);
	DOREPLIFETIME(ABossGiantMonkey, Phase);
	DOREPLIFETIME(ABossGiantMonkey, HP);
	DOREPLIFETIME(ABossGiantMonkey, SubCount);
	DOREPLIFETIME(ABossGiantMonkey, HopFrom);
	DOREPLIFETIME(ABossGiantMonkey, HopTo);
	DOREPLIFETIME(ABossGiantMonkey, ServerLocation);
	DOREPLIFETIME(ABossGiantMonkey, ServerYaw);
	DOREPLIFETIME(ABossGiantMonkey, LastKeyHitTime);
	DOREPLIFETIME(ABossGiantMonkey, LastHitTime);
	DOREPLIFETIME(ABossGiantMonkey, bCymbalOut);
	DOREPLIFETIME(ABossGiantMonkey, bThrowLeft);
	DOREPLIFETIME(ABossGiantMonkey, WalkSpeed);
	DOREPLIFETIME(ABossGiantMonkey, LookTarget);
}

float ABossGiantMonkey::BossNow() const
{
	const UWorld* World = GetWorld();
	if (const AGameStateBase* GS = World ? World->GetGameState() : nullptr)
	{
		return GS->GetServerWorldTimeSeconds();
	}
	return World ? World->GetTimeSeconds() : 0.f;
}

float ABossGiantMonkey::GetStateTime() const
{
	return BossNow() - StateStartTime;
}

FVector ABossGiantMonkey::GetFloorLocation() const
{
	return GetActorLocation() - FVector(0.f, 0.f, HalfHeight);
}

FVector ABossGiantMonkey::GetKeyWorldLocation() const
{
	return KeyPivot->GetComponentTransform().TransformPosition(FVector(260.f, 0.f, 0.f));
}

FVector ABossGiantMonkey::GetCymbalLocation(bool bLeft) const
{
	const FName Cymbal = bLeft ? BoneCymbalL : BoneCymbalR;
	return Mesh->GetSocketLocation(Mesh->GetBoneIndex(Cymbal) != INDEX_NONE ? Cymbal : (bLeft ? BoneHandL : BoneHandR));
}

void ABossGiantMonkey::BeginPlay()
{
	Super::BeginPlay();

	// Ride the bones so the key / head targets follow the clips (slump, fall...): keep their bind-pose placement.
	auto RideBone = [this](USceneComponent* Comp, FName Bone)
	{
		const USkeletalMesh* Asset = Mesh->GetSkeletalMeshAsset();
		const int32 Index = Asset ? Asset->GetRefSkeleton().FindBoneIndex(Bone) : INDEX_NONE;
		if (!Comp || Index == INDEX_NONE)
		{
			UE_LOG(LogCrank, Warning, TEXT("%s: rig bone %s missing"), *GetName(), *Bone.ToString());
			return;
		}
		const FTransform BoneCS = FAnimationRuntime::GetComponentSpaceTransformRefPose(Asset->GetRefSkeleton(), Index);
		const FTransform CompCS = Comp->GetRelativeTransform();
		Comp->AttachToComponent(Mesh, FAttachmentTransformRules::KeepRelativeTransform, Bone);
		Comp->SetRelativeTransform(CompCS.GetRelativeTransform(BoneCS));
	};
	RideBone(KeyPivot, BoneSpineHigh);
	RideBone(HeadHitSphere, BoneHead);
	RideBone(StarsFX, BoneHead);

	if (KeyMesh->GetStaticMesh())
	{
		KeyMID = KeyMesh->CreateDynamicMaterialInstance(0);
	}
	if (Mesh->GetSkeletalMeshAsset())
	{
		BodyMID = Mesh->CreateDynamicMaterialInstance(0);
	}

	// Shockwave ring pool.
	if (RingMesh)
	{
		RingMeshRadius = FMath::Max(1.f, RingMesh->GetBounds().BoxExtent.X);
		for (int32 i = 0; i < 4; ++i)
		{
			UStaticMeshComponent* Ring = NewObject<UStaticMeshComponent>(this, *FString::Printf(TEXT("Ring%d"), i));
			Ring->SetStaticMesh(RingMesh);
			Ring->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			Ring->SetCastShadow(false);
			Ring->SetUsingAbsoluteLocation(true);
			Ring->SetUsingAbsoluteRotation(true);
			Ring->SetUsingAbsoluteScale(true);
			Ring->SetupAttachment(BodyCollision);
			Ring->RegisterComponent();
			Ring->SetVisibility(false);
			if (RingMaterial)
			{
				Ring->SetMaterial(0, RingMaterial);
			}
			RingMIDs.Add(Ring->CreateDynamicMaterialInstance(0));
			RingMeshes.Add(Ring);
		}
	}

	TickAudio = CrankSound::CreateLoop(BodyCollision, TEXT("SFX_GiantTick"), 0.9f);

	if (HasAuthority())
	{
		HP = CrankBalance::GiantMaxHP;
		ServerLocation = GetActorLocation();
		ServerYaw = GetActorRotation().Yaw;
		StateStartTime = BossNow();
	}
	VisualYaw = GetActorRotation().Yaw;
	VisualLocation = GetActorLocation();
}

void ABossGiantMonkey::Activate()
{
	if (HasAuthority() && GiantState == EGiantState::Dormant)
	{
		SetGiantState(EGiantState::Idle, 2.2f);
		MulticastGiantEvent(GiantEvent::Wake, GetActorLocation(), 0.f);
		if (ARaidGameState* GS = GetWorld()->GetGameState<ARaidGameState>())
		{
			GS->MulticastToast(NSLOCTEXT("Crank", "ToastGiantWake", "짝짝이 대왕이 깨어났다! 친구를 발사해 부딪히면 피해 — 등 뒤 태엽 열쇠는 큰 피해"), FLinearColor(1.f, 0.75f, 0.35f));
		}
	}
}

void ABossGiantMonkey::SetGiantState(EGiantState NewState, float Duration)
{
	GiantState = NewState;
	StateStartTime = BossNow();
	StateDuration = Duration;
	WalkSpeed = 0.f;
	ForceNetUpdate();
	OnRep_GiantState();
}

void ABossGiantMonkey::OnRep_GiantState()
{
	LocalStateEnter = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
	if (TickAudio)
	{
		const bool bTicking = IsActive() && GiantState != EGiantState::WoundDown;
		if (bTicking && !TickAudio->IsPlaying())
		{
			TickAudio->Play();
		}
		else if (!bTicking && TickAudio->IsPlaying())
		{
			TickAudio->Stop();
		}
	}
}

// ------------------------------------------------------------------------------------------
// Tick

void ABossGiantMonkey::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	UpdateCollision();
	if (HasAuthority())
	{
		TickServer(DeltaSeconds);
		TickRings(DeltaSeconds);
		VisualYaw = ServerYaw;
		VisualLocation = EvalLocation();
	}
	else
	{
		VisualYaw = FMath::FixedTurn(VisualYaw, ServerYaw, 300.f * DeltaSeconds);
		// Hops are reproduced exactly; walking / landing corrections are smoothed.
		const FVector Goal = EvalLocation();
		VisualLocation = GiantState == EGiantState::Hop ? Goal : FMath::VInterpTo(VisualLocation, Goal, DeltaSeconds, 10.f);
	}
	SetActorLocationAndRotation(VisualLocation, FRotator(0.f, VisualYaw, 0.f));

	UpdateKey(DeltaSeconds);
	if (GetNetMode() != NM_DedicatedServer)
	{
		UpdateRingVisuals(DeltaSeconds);
		UpdateFeedback(DeltaSeconds);
	}
}

FVector ABossGiantMonkey::EvalLocation() const
{
	if (GiantState == EGiantState::Hop)
	{
		const float T = FMath::Clamp(GetStateTime() / CrankBalance::GiantHopTime, 0.f, 1.f);
		return FMath::Lerp(FVector(HopFrom), FVector(HopTo), T) + FVector(0.f, 0.f, 4.f * CrankBalance::GiantHopHeight * T * (1.f - T));
	}
	return ServerLocation;
}

void ABossGiantMonkey::TickServer(float DeltaSeconds)
{
	const float T = GetStateTime();
	switch (GiantState)
	{
	case EGiantState::Dormant:
		return;

	case EGiantState::Idle:
	{
		AWindupCharacter* Victim = Target.Get();
		if (!Victim || FMath::Fmod(T, 1.f) < DeltaSeconds)
		{
			Victim = FindTarget();
			Target = Victim;
			LookTarget = Victim;
		}
		WalkSpeed = 0.f;
		if (Victim)
		{
			TurnToward(Victim->GetActorLocation(), DeltaSeconds);
			// Waddle closer to a far target between attacks.
			FVector To = Victim->GetActorLocation() - GetActorLocation();
			To.Z = 0.f;
			const FVector Fwd = GetActorForwardVector();
			const float Speed = WalkSpeedBase + 25.f * (Phase - 1);
			const FVector Wanted = FVector(ServerLocation) + Fwd * Speed * DeltaSeconds;
			if (To.Size() > 1500.f && FVector::DotProduct(Fwd, To.GetSafeNormal()) > 0.6f && !(Arena && Arena->IsInSafeSpot(Wanted)))
			{
				const FVector Next = ClampToArena(Wanted);
				WalkSpeed = (FVector(Next) - FVector(ServerLocation)).Size2D() / FMath::Max(DeltaSeconds, 0.001f);
				ServerLocation = FVector(Next.X, Next.Y, ServerLocation.Z);
			}
		}
		if (T >= StateDuration)
		{
			WalkSpeed = 0.f;
			ChooseNextAttack();
		}
		break;
	}

	case EGiantState::ClapWindup:
		if (const AWindupCharacter* Victim = Target.Get())
		{
			TurnToward(Victim->GetActorLocation(), DeltaSeconds * 0.5f);
		}
		if (T >= StateDuration)
		{
			ClapDoneAt = 0.12f;
			SetGiantState(EGiantState::Clap, CrankBalance::GiantClapTime);
		}
		break;

	case EGiantState::Clap:
		if (ClapDoneAt >= 0.f && T >= ClapDoneAt)
		{
			ClapDoneAt = -1.f;
			const FVector Origin = GetFloorLocation() + GetActorForwardVector() * 120.f;
			const GiantEvent::FRingSpec Spec = GiantEvent::Spec(GiantEvent::ClapRing);
			StartRing(Origin, Spec.MaxRadius, Spec.Speed, Spec.Strength);
			MulticastGiantEvent(GiantEvent::ClapRing, Origin, 0.f);
		}
		if (T >= StateDuration)
		{
			++SubCount;
			if (SubCount < CrankBalance::GiantClapsPerAttack(Phase))
			{
				SetGiantState(EGiantState::ClapWindup, CrankBalance::GiantClapChainWindup);
			}
			else
			{
				++AttackCount;
				SetGiantState(EGiantState::Idle, CrankBalance::GiantIdleTime(Phase));
			}
		}
		break;

	case EGiantState::HopWindup:
		if (const AWindupCharacter* Victim = Target.Get())
		{
			TurnToward(Victim->GetActorLocation(), DeltaSeconds);
		}
		if (T >= StateDuration)
		{
			FVector Goal = GetActorLocation();
			if (const AWindupCharacter* Victim = Target.Get())
			{
				FVector ToVictim = Victim->GetActorLocation() - GetActorLocation();
				ToVictim.Z = 0.f;
				const float Dist = FMath::Clamp(ToVictim.Size() - 150.f, 0.f, CrankBalance::GiantHopRange);
				Goal += ToVictim.GetSafeNormal() * Dist;
			}
			Goal.Z = GetActorLocation().Z;
			HopFrom = GetActorLocation();
			HopTo = ClampToArena(Goal);
			bLanded = false;
			SetGiantState(EGiantState::Hop, CrankBalance::GiantHopTime);
		}
		break;

	case EGiantState::Hop:
		if (!bLanded && T >= StateDuration)
		{
			bLanded = true;
			ServerLocation = HopTo;
			SetActorLocation(ServerLocation);
			const FVector Floor = GetFloorLocation();
			Stomp(Floor, CrankBalance::GiantStompRadius);
			const GiantEvent::FRingSpec Spec = GiantEvent::Spec(GiantEvent::StompRing);
			StartRing(Floor, Spec.MaxRadius, Spec.Speed, Spec.Strength);
			MulticastGiantEvent(GiantEvent::StompRing, Floor, 0.f);
			++SubCount;
			if (SubCount < CrankBalance::GiantHopsPerAttack(Phase))
			{
				SetGiantState(EGiantState::HopWindup, CrankBalance::GiantHopChainWindup);
			}
			else
			{
				++AttackCount;
				SetGiantState(EGiantState::Idle, CrankBalance::GiantIdleTime(Phase) + 0.3f);
			}
		}
		break;

	case EGiantState::ThrowWindup:
		if (const AWindupCharacter* Victim = Target.Get())
		{
			TurnToward(Victim->GetActorLocation(), DeltaSeconds);
		}
		if (T >= StateDuration)
		{
			ThrowCymbal();
			SetGiantState(EGiantState::Throw, 0.5f);
		}
		break;

	case EGiantState::Throw:
		if (T >= StateDuration)
		{
			++AttackCount;
			SetGiantState(EGiantState::Idle, CrankBalance::GiantIdleTime(Phase));
		}
		break;

	case EGiantState::WoundDown:
		if (T >= StateDuration)
		{
			SetGiantState(EGiantState::Rewind, CrankBalance::GiantRewindTime);
			MulticastGiantEvent(GiantEvent::Rewind, GetKeyWorldLocation(), 0.f);
		}
		break;

	case EGiantState::Rewind:
		if (T >= StateDuration)
		{
			AttackCount = 0;
			SetGiantState(EGiantState::Idle, CrankBalance::GiantIdleTime(Phase));
		}
		break;

	case EGiantState::Stagger:
		if (T >= StateDuration)
		{
			SetGiantState(EGiantState::Idle, 0.6f);
		}
		break;

	case EGiantState::PhaseTransition:
		if (!bPhaseStompDone && T >= 1.3f)
		{
			bPhaseStompDone = true;
			const FVector Floor = GetFloorLocation();
			Stomp(Floor, CrankBalance::GiantStompRadius + 200.f);
			const GiantEvent::FRingSpec Spec = GiantEvent::Spec(GiantEvent::PhaseRing);
			StartRing(Floor, Spec.MaxRadius, Spec.Speed, Spec.Strength);
			MulticastGiantEvent(GiantEvent::PhaseRing, Floor, 0.f);
		}
		if (T >= StateDuration)
		{
			Phase = FMath::Clamp(FMath::Max(Phase + 1, CrankBalance::GiantPhaseForHP(HP)), 1, 3);
			AttackCount = 0;
			SetGiantState(EGiantState::Idle, 0.8f);
		}
		break;

	case EGiantState::Defeated:
		if (!bRewardSpawned && T >= 0.9f)
		{
			bRewardSpawned = true;
			SpawnReward();
		}
		break;
	}
}

// ------------------------------------------------------------------------------------------
// Attack choice / targeting

AWindupCharacter* ABossGiantMonkey::FindTarget() const
{
	AWindupCharacter* Best = nullptr;
	float BestDist = TNumericLimits<float>::Max();
	for (TActorIterator<AWindupCharacter> It(GetWorld()); It; ++It)
	{
		AWindupCharacter* Doll = *It;
		const ECrankBodyState State = Doll->GetBodyState();
		if (State == ECrankBodyState::Scattered || State == ECrankBodyState::Frozen)
		{
			continue;
		}
		if (Arena && !Arena->ContainsPoint(Doll->GetActorLocation()))
		{
			continue;
		}
		float Dist = FVector::Dist2D(Doll->GetActorLocation(), GetActorLocation()) * (Doll->IsPlayerControlled() ? 1.f : 1.4f);
		if (Arena && Arena->IsInSafeSpot(Doll->GetActorLocation()))
		{
			Dist *= 2.5f;	// dolls hiding at a respawn point are a last resort
		}
		if (Dist < BestDist)
		{
			BestDist = Dist;
			Best = Doll;
		}
	}
	return Best;
}

void ABossGiantMonkey::TurnToward(const FVector& Location, float DeltaSeconds)
{
	const FVector To = Location - GetActorLocation();
	if (To.SizeSquared2D() < 1.f)
	{
		return;
	}
	ServerYaw = FMath::FixedTurn(ServerYaw, To.Rotation().Yaw, TurnRate * (1.f + 0.15f * (Phase - 1)) * DeltaSeconds);
}

void ABossGiantMonkey::ChooseNextAttack()
{
	if (AttackCount >= CrankBalance::GiantAttacksPerCycle(Phase))
	{
		SetGiantState(EGiantState::WoundDown, CrankBalance::GiantWoundDownTime(Phase));
		MulticastGiantEvent(GiantEvent::WindDown, GetKeyWorldLocation(), 0.f);
		return;
	}

	AWindupCharacter* Victim = FindTarget();
	Target = Victim;
	LookTarget = Victim;
	if (!Victim)
	{
		SetGiantState(EGiantState::Idle, 1.5f);	// nobody inside: wait
		return;
	}

	const float Dist = FVector::Dist2D(Victim->GetActorLocation(), GetActorLocation());
	float WClap = bCymbalOut ? 0.f : 1.f;
	float WHop = Dist > 800.f ? 1.f : 0.35f;
	float WThrow = (Phase >= 2 && !bCymbalOut && CymbalClass) ? 0.8f : 0.f;
	auto Damp = [this](EGiantState Attack, float& Weight)
	{
		if (LastAttack == Attack && SameAttackStreak >= 2)
		{
			Weight = 0.f;
		}
	};
	Damp(EGiantState::ClapWindup, WClap);
	Damp(EGiantState::HopWindup, WHop);
	Damp(EGiantState::ThrowWindup, WThrow);
	if (WClap + WHop + WThrow <= 0.f)
	{
		WHop = 1.f;
	}

	const float Roll = FMath::FRand() * (WClap + WHop + WThrow);
	EGiantState Next = EGiantState::HopWindup;
	float Duration = CrankBalance::GiantHopWindup;
	if (Roll < WClap)
	{
		Next = EGiantState::ClapWindup;
		Duration = CrankBalance::GiantClapWindup(Phase);
	}
	else if (Roll < WClap + WThrow)
	{
		Next = EGiantState::ThrowWindup;
		Duration = CrankBalance::GiantThrowWindup;
	}
	SameAttackStreak = Next == LastAttack ? SameAttackStreak + 1 : 1;
	LastAttack = Next;
	SubCount = 0;
	SetGiantState(Next, Duration);
}

FVector ABossGiantMonkey::ClampToArena(const FVector& Location) const
{
	return Arena ? Arena->ClampPoint(Location, 420.f) : Location;
}

// ------------------------------------------------------------------------------------------
// Shockwaves / knockback (server)

void ABossGiantMonkey::StartRing(const FVector& Origin, float MaxRadius, float Speed, float Strength)
{
	FServerRing& Ring = Rings.AddDefaulted_GetRef();
	Ring.Origin = Origin;
	Ring.Start = BossNow();
	Ring.MaxRadius = MaxRadius;
	Ring.Speed = Speed;
	Ring.Strength = Strength;
}

void ABossGiantMonkey::TickRings(float DeltaSeconds)
{
	const float Now = BossNow();
	for (int32 i = Rings.Num() - 1; i >= 0; --i)
	{
		FServerRing& Ring = Rings[i];
		const float Radius = (Now - Ring.Start) * Ring.Speed;
		if (Radius > Ring.MaxRadius)
		{
			Rings.RemoveAt(i);
			continue;
		}
		for (TActorIterator<AWindupCharacter> It(GetWorld()); It; ++It)
		{
			AWindupCharacter* Doll = *It;
			if (Ring.AlreadyHit.Contains(Doll))
			{
				continue;
			}
			const ECrankBodyState State = Doll->GetBodyState();
			if (State != ECrankBodyState::Normal && State != ECrankBodyState::GettingUp && State != ECrankBodyState::Carried)
			{
				continue;
			}
			const FVector P = Doll->GetActorLocation();
			const float Dist = FVector::Dist2D(P, Ring.Origin);
			if (FMath::Abs(Dist - Radius) > CrankBalance::GiantRingBand * 0.5f + 25.f)
			{
				continue;
			}
			// Jumped over it?
			const float Feet = P.Z - Doll->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
			const UCharacterMovementComponent* Move = Doll->GetCharacterMovement();
			if (Feet - Ring.Origin.Z > CrankBalance::GiantRingGroundHeight || (Move && Move->IsFalling() && Feet - Ring.Origin.Z > 25.f))
			{
				continue;
			}
			if (Knock(Doll, Ring.Origin, Ring.Strength))
			{
				Ring.AlreadyHit.Add(Doll);
			}
		}
	}
}

void ABossGiantMonkey::Stomp(const FVector& Origin, float Radius)
{
	for (TActorIterator<AWindupCharacter> It(GetWorld()); It; ++It)
	{
		AWindupCharacter* Doll = *It;
		const ECrankBodyState State = Doll->GetBodyState();
		if (State == ECrankBodyState::Scattered || State == ECrankBodyState::Frozen || State == ECrankBodyState::Trapped)
		{
			continue;
		}
		const FVector P = Doll->GetActorLocation();
		const float Dist = FVector::Dist2D(P, Origin);
		if (Dist > Radius || P.Z - Origin.Z > 260.f)
		{
			continue;
		}
		Knock(Doll, Origin, 1.35f - 0.5f * Dist / Radius);
	}
}

bool ABossGiantMonkey::Knock(AWindupCharacter* Doll, const FVector& From, float Strength)
{
	const float Now = BossNow();
	if (const float* Last = LastKnockTime.Find(Doll); Last && Now - *Last < CrankBalance::GiantKnockGrace)
	{
		return false;
	}
	LastKnockTime.Add(Doll, Now);
	FVector Dir = Doll->GetActorLocation() - From;
	Dir.Z = 0.f;
	Dir = Dir.IsNearlyZero() ? -GetActorForwardVector() : Dir.GetSafeNormal();
	Doll->StartRagdoll(Dir * CrankBalance::GiantKnockSpeed * Strength + FVector(0.f, 0.f, CrankBalance::GiantKnockLift * Strength), 1.2f);
	return true;
}

// ------------------------------------------------------------------------------------------
// Hits from launched dolls (server)

void ABossGiantMonkey::HandleDollImpact(AWindupCharacter* Doll, const FVector& Velocity, const FHitResult& Hit)
{
	if (!HasAuthority() || !Doll || GiantState == EGiantState::Defeated || HP <= 0)
	{
		return;
	}
	if (GiantState == EGiantState::Dormant)
	{
		Activate();	// a doll in the face is a rude awakening (and it still hurts)
	}
	// One hit per flight: bounces off the same doll within a moment count once.
	const float Now = BossNow();
	if (const float* Last = LastDollHitTime.Find(Doll); Last && Now - *Last < CrankBalance::GiantHitCooldown)
	{
		return;
	}
	LastDollHitTime.Add(Doll, Now);

	const UPrimitiveComponent* Comp = Hit.GetComponent();
	const FVector Point = Hit.ImpactPoint;

	if (Comp == KeyHitBox)
	{
		const bool bExposed = IsKeyExposed();
		ApplyDamage(bExposed ? CrankBalance::GiantKeyWoundDamage : CrankBalance::GiantKeyDamage, true, Point);
		return;
	}

	// The head sphere sits mostly inside the body capsule: the upper front of the capsule is "the face" too.
	const FVector Local = GetActorTransform().InverseTransformPosition(Point);
	const bool bFace = Comp == HeadHitSphere || (Comp == BodyCollision && Local.Z > HalfHeight * 0.2f && Local.X > -60.f);
	if (bFace)
	{
		const bool bWindup = GiantState == EGiantState::ClapWindup || GiantState == EGiantState::HopWindup || GiantState == EGiantState::ThrowWindup;
		MulticastGiantEvent(GiantEvent::Bonk, Point, bWindup ? 1.f : 0.f);
		ApplyDamage(CrankBalance::GiantFaceDamage, false, Point);
		if (bWindup && GiantState != EGiantState::Defeated && GiantState != EGiantState::PhaseTransition)
		{
			// A doll in the face cancels the attack and still counts toward the wind-down.
			++AttackCount;
			SetGiantState(EGiantState::Stagger, CrankBalance::GiantStaggerTime);
		}
		return;
	}

	MulticastGiantEvent(GiantEvent::BodyBonk, Point, 0.f);
	ApplyDamage(CrankBalance::GiantBodyDamage, false, Point);
}

void ABossGiantMonkey::ApplyDamage(int32 Amount, bool bKey, const FVector& Point)
{
	if (HP <= 0 || Amount <= 0)
	{
		return;
	}
	const float Now = BossNow();
	HP = FMath::Max(0, HP - Amount);
	LastHitTime = Now;
	if (bKey)
	{
		LastKeyHitTime = Now;
		MulticastGiantEvent(GiantEvent::KeyHit, GetKeyWorldLocation(), (float)Amount);
	}
	MulticastGiantEvent(bKey ? GiantEvent::DamageKey : GiantEvent::DamageBody, Point, (float)Amount);
	ForceNetUpdate();

	ARaidGameState* GS = GetWorld()->GetGameState<ARaidGameState>();
	if (HP <= 0)
	{
		Defeat();
		return;
	}
	if (bKey && GS)
	{
		GS->MulticastToast(FText::Format(NSLOCTEXT("Crank", "ToastGiantKeyHit", "태엽 열쇠 직격! -{0} (남은 체력 {1})"),
			FText::AsNumber(Amount), FText::AsNumber(HP)), FLinearColor(1.f, 0.85f, 0.3f));
	}
	// 100 -> 66 -> 33: each third makes it angrier.
	if (CrankBalance::GiantPhaseForHP(HP) > Phase && GiantState != EGiantState::PhaseTransition)
	{
		bPhaseStompDone = false;
		SetGiantState(EGiantState::PhaseTransition, CrankBalance::GiantPhaseTransition);
		MulticastGiantEvent(GiantEvent::PhaseUp, GetActorLocation(), (float)(Phase + 1));
		if (GS)
		{
			GS->MulticastToast(FText::Format(NSLOCTEXT("Crank", "ToastGiantPhase", "짝짝이 대왕이 화났다! 페이즈 {0}"), FText::AsNumber(Phase + 1)), FLinearColor(1.f, 0.4f, 0.3f));
		}
	}
}

void ABossGiantMonkey::Defeat()
{
	Rings.Reset();
	bRewardSpawned = false;
	SetGiantState(EGiantState::Defeated, 999.f);
	MulticastGiantEvent(GiantEvent::Defeated, GetActorLocation(), 0.f);
	if (ARaidGameMode* GM = GetWorld()->GetAuthGameMode<ARaidGameMode>())
	{
		GM->OnGiantDefeated();
	}
	if (Arena)
	{
		Arena->OnGiantDefeated(this);
	}
}

void ABossGiantMonkey::SpawnReward()
{
	// The giant lands on its back (and on its key): the golden key springs out over its feet, toward the players.
	const FVector Fwd = GetActorForwardVector();
	const FVector SpawnLoc = GetFloorLocation() + Fwd * 280.f + FVector(0.f, 0.f, 420.f);
	MulticastGiantEvent(GiantEvent::KeyPop, SpawnLoc, 0.f);
	if (!RewardClass)
	{
		return;
	}
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
	if (ACarryableActor* Reward = GetWorld()->SpawnActor<ACarryableActor>(RewardClass, FTransform(GetActorRotation(), SpawnLoc), Params))
	{
		if (UStaticMeshComponent* RewardMesh = Reward->GetMesh(); RewardMesh && RewardMesh->IsSimulatingPhysics())
		{
			RewardMesh->SetPhysicsLinearVelocity(Fwd * 320.f + FVector(0.f, 0.f, 380.f));
			RewardMesh->SetPhysicsAngularVelocityInDegrees(FVector(0.f, 360.f, 120.f));
		}
	}
}

void ABossGiantMonkey::UpdateCollision()
{
	const bool bFallen = GiantState == EGiantState::Defeated && GetStateTime() >= 0.75f;
	if (bFallen == bFallenCollision)
	{
		return;
	}
	bFallenCollision = bFallen;
	const ECollisionEnabled::Type Standing = bFallen ? ECollisionEnabled::NoCollision : ECollisionEnabled::QueryAndPhysics;
	BodyCollision->SetCollisionEnabled(Standing);
	KeyHitBox->SetCollisionEnabled(Standing);
	HeadHitSphere->SetCollisionEnabled(Standing);
	FallenCollision->SetCollisionEnabled(bFallen ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
}

void ABossGiantMonkey::ThrowCymbal()
{
	MulticastGiantEvent(GiantEvent::Throw, GetActorLocation(), 0.f);
	if (!CymbalClass)
	{
		return;
	}
	bThrowLeft = FMath::RandBool();
	const FVector From = GetCymbalLocation(bThrowLeft);
	FVector Aim = GetActorLocation() + GetActorForwardVector() * 1500.f;
	if (const AWindupCharacter* Victim = Target.Get())
	{
		Aim = Victim->GetActorLocation() + Victim->GetVelocity() * 0.5f;
	}
	Aim.Z = GetFloorLocation().Z + 70.f;

	const FTransform SpawnTM(FRotator::ZeroRotator, From);
	ACymbalProjectile* Cymbal = GetWorld()->SpawnActorDeferred<ACymbalProjectile>(CymbalClass, SpawnTM, this, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (Cymbal)
	{
		Cymbal->InitFlight(From, Aim, bThrowLeft ? 1.f : -1.f, CrankBalance::GiantCymbalOutTime);
		Cymbal->FinishSpawning(SpawnTM);
		++CymbalsOut;
		bCymbalOut = true;
	}
}

void ABossGiantMonkey::OnCymbalReturned()
{
	CymbalsOut = FMath::Max(0, CymbalsOut - 1);
	bCymbalOut = CymbalsOut > 0;
}

void ABossGiantMonkey::DebugCommand(FName Command, float Value)
{
	if (!HasAuthority())
	{
		return;
	}
	if (Command == TEXT("GiantWake"))
	{
		Activate();
	}
	else if (Command == TEXT("GiantDown"))
	{
		Activate();
		SetGiantState(EGiantState::WoundDown, CrankBalance::GiantWoundDownTime(Phase));
		MulticastGiantEvent(GiantEvent::WindDown, GetKeyWorldLocation(), 0.f);
	}
	else if (Command == TEXT("GiantPhase"))
	{
		Activate();
		Phase = FMath::Clamp(FMath::RoundToInt(Value), 1, 3);
		HP = Phase == 1 ? CrankBalance::GiantMaxHP : (Phase == 2 ? 66 : 33);
		AttackCount = 0;
		SetGiantState(EGiantState::Idle, 0.5f);
	}
	else if (Command == TEXT("GiantHit"))
	{
		Activate();
		if (!IsKeyExposed())
		{
			SetGiantState(EGiantState::WoundDown, CrankBalance::GiantWoundDownTime(Phase));
		}
		ApplyDamage(CrankBalance::GiantKeyWoundDamage, true, GetKeyWorldLocation());
	}
	else if (Command == TEXT("GiantKill"))
	{
		Activate();
		HP = 0;
		Defeat();
	}
	else if (Command == TEXT("GiantAttack"))
	{
		Activate();
		int32 Which = FMath::RoundToInt(Value);
		if (Which == 2 && bCymbalOut)
		{
			Which = 0;	// one cymbal at a time
		}
		Target = FindTarget();
		LookTarget = Target.Get();
		SubCount = 0;
		SetGiantState(Which == 1 ? EGiantState::HopWindup : (Which == 2 ? EGiantState::ThrowWindup : EGiantState::ClapWindup),
			Which == 1 ? CrankBalance::GiantHopWindup : (Which == 2 ? CrankBalance::GiantThrowWindup : CrankBalance::GiantClapWindup(Phase)));
	}
}

// ------------------------------------------------------------------------------------------
// Events (every machine)

void ABossGiantMonkey::MulticastGiantEvent_Implementation(uint8 EventId, FVector_NetQuantize Location, float Param)
{
	const FVector Loc = Location;
	auto AddRing = [this, Loc](uint8 Event)
	{
		const GiantEvent::FRingSpec Spec = GiantEvent::Spec(Event);
		FRingVisual& Ring = RingVisuals.AddDefaulted_GetRef();
		Ring.Origin = Loc;
		Ring.Start = GetWorld()->GetTimeSeconds();
		Ring.MaxRadius = Spec.MaxRadius;
		Ring.Speed = Spec.Speed;
	};

	switch (EventId)
	{
	case GiantEvent::Wake:
		CrankSound::PlayAt(this, TEXT("SFX_MonkeyScreech"), Loc, 1.4f);
		break;
	case GiantEvent::ClapRing:
		AddRing(EventId);
		CrankSound::PlayAt(this, TEXT("SFX_CymbalCrash"), Loc + FVector(0.f, 0.f, 600.f), 1.6f, FMath::FRandRange(0.95f, 1.05f));
		UCrankFXSubsystem::Spawn(this, ECrankFX::Sparks, GetActorLocation() + GetActorForwardVector() * 260.f + FVector(0.f, 0.f, 150.f), FRotator::ZeroRotator, 4.f, FLinearColor(1.f, 0.85f, 0.4f));
		break;
	case GiantEvent::StompRing:
	case GiantEvent::PhaseRing:
		AddRing(EventId);
		CrankSound::PlayAt(this, TEXT("SFX_GiantStomp"), Loc, 1.6f, EventId == GiantEvent::PhaseRing ? 0.8f : 1.f);
		UCrankFXSubsystem::Spawn(this, ECrankFX::DustBurst, Loc + FVector(0.f, 0.f, 20.f), FRotator::ZeroRotator, 5.f);
		break;
	case GiantEvent::KeyHit:
		CrankSound::PlayAt(this, TEXT("SFX_KeyHit"), Loc, 1.5f);
		UCrankFXSubsystem::Spawn(this, ECrankFX::Sparks, Loc, FRotator::ZeroRotator, 5.f, FLinearColor(1.f, 0.9f, 0.4f));
		UCrankFXSubsystem::Spawn(this, ECrankFX::Confetti, Loc, FRotator::ZeroRotator, 3.f);
		break;
	case GiantEvent::Deflect:
		CrankSound::PlayAt(this, TEXT("SFX_Deflect"), Loc, 1.2f);
		UCrankFXSubsystem::Spawn(this, ECrankFX::Sparks, Loc, FRotator::ZeroRotator, 2.f);
		break;
	case GiantEvent::Bonk:
		CrankSound::PlayAt(this, TEXT("SFX_BonkMetal"), Loc, 1.4f, Param > 0.f ? 0.8f : 1.1f);
		UCrankFXSubsystem::Spawn(this, ECrankFX::StunStars, Loc, FRotator::ZeroRotator, 3.f);
		break;
	case GiantEvent::BodyBonk:
		CrankSound::PlayAt(this, TEXT("SFX_Bonk"), Loc, 1.f, 0.7f);
		UCrankFXSubsystem::Spawn(this, ECrankFX::Puff, Loc, FRotator::ZeroRotator, 2.f);
		break;
	case GiantEvent::WindDown:
		CrankSound::PlayAt(this, TEXT("SFX_GiantWindDown"), Loc, 1.4f);
		break;
	case GiantEvent::Rewind:
		CrankSound::PlayAt(this, TEXT("SFX_GiantRewind"), Loc, 1.4f);
		break;
	case GiantEvent::Throw:
		CrankSound::PlayAt(this, TEXT("SFX_CymbalWhoosh"), Loc + FVector(0.f, 0.f, 500.f), 1.3f);
		break;
	case GiantEvent::PhaseUp:
		CrankSound::PlayAt(this, TEXT("SFX_MonkeyScreech"), Loc, 1.5f, 0.85f + 0.08f * Param);
		break;
	case GiantEvent::Defeated:
		CrankSound::PlayAt(this, TEXT("SFX_GiantFall"), Loc, 1.6f);
		CrankSound::PlayAt(this, TEXT("SFX_BossDown"), Loc, 1.2f);
		break;
	case GiantEvent::DamageBody:
	case GiantEvent::DamageKey:
	{
		FDamagePopup& Popup = DamagePopups.AddDefaulted_GetRef();
		Popup.Location = Loc;
		Popup.Amount = FMath::RoundToInt(Param);
		Popup.Time = GetWorld()->GetTimeSeconds();
		Popup.bKey = EventId == GiantEvent::DamageKey;
		if (DamagePopups.Num() > 12)
		{
			DamagePopups.RemoveAt(0);
		}
		break;
	}
	case GiantEvent::KeyPop:
		CrankSound::PlayAt(this, TEXT("SFX_Pop"), Loc, 1.5f, 0.7f);
		UCrankFXSubsystem::Spawn(this, ECrankFX::Confetti, Loc, FRotator::ZeroRotator, 6.f);
		UCrankFXSubsystem::Spawn(this, ECrankFX::Pop, Loc, FRotator::ZeroRotator, 4.f);
		break;
	default:
		break;
	}
}

// ------------------------------------------------------------------------------------------
// Key (every machine): the body itself is animated by UGiantAnimInstance

void ABossGiantMonkey::UpdateKey(float DeltaSeconds)
{
	float SpinRate = 540.f;
	switch (GiantState)
	{
	case EGiantState::Dormant:
	case EGiantState::Defeated:			SpinRate = 0.f; break;
	case EGiantState::WoundDown:		SpinRate = 35.f; break;
	case EGiantState::Rewind:			SpinRate = 1600.f; break;
	case EGiantState::PhaseTransition:	SpinRate = 1200.f; break;
	default: break;
	}
	KeyMesh->AddLocalRotation(FRotator(0.f, 0.f, -SpinRate * DeltaSeconds));
	KeyMesh->SetVisibility(!(GiantState == EGiantState::Defeated && GetStateTime() >= 0.9f));
}

void ABossGiantMonkey::UpdateRingVisuals(float DeltaSeconds)
{
	const float Now = GetWorld()->GetTimeSeconds();
	for (int32 i = RingVisuals.Num() - 1; i >= 0; --i)
	{
		const FRingVisual& Ring = RingVisuals[i];
		if ((Now - Ring.Start) * Ring.Speed > Ring.MaxRadius)
		{
			RingVisuals.RemoveAt(i);
		}
	}
	const bool bPuff = Now - LastRingPuffTime > 0.07f;
	if (bPuff)
	{
		LastRingPuffTime = Now;
	}
	for (int32 i = 0; i < RingMeshes.Num(); ++i)
	{
		UStaticMeshComponent* RingComp = RingMeshes[i];
		if (!RingVisuals.IsValidIndex(i))
		{
			RingComp->SetVisibility(false);
			continue;
		}
		const FRingVisual& Ring = RingVisuals[i];
		const float Radius = FMath::Max(10.f, (Now - Ring.Start) * Ring.Speed);
		const float Life = FMath::Clamp(Radius / Ring.MaxRadius, 0.f, 1.f);
		RingComp->SetVisibility(true);
		RingComp->SetWorldLocation(Ring.Origin + FVector(0.f, 0.f, 6.f));
		const float S = Radius / RingMeshRadius;
		RingComp->SetWorldScale3D(FVector(S, S, 1.5f));
		if (RingMIDs.IsValidIndex(i) && RingMIDs[i])
		{
			RingMIDs[i]->SetVectorParameterValue(TEXT("Color"), FLinearColor(1.f, 0.85f, 0.45f, 0.85f * (1.f - Life * 0.7f)));
		}
		if (bPuff)
		{
			for (int32 k = 0; k < 5; ++k)
			{
				const float Ang = FMath::FRandRange(0.f, 2.f * PI);
				const FVector P = Ring.Origin + FVector(FMath::Cos(Ang) * Radius, FMath::Sin(Ang) * Radius, 25.f);
				UCrankFXSubsystem::Spawn(this, ECrankFX::Puff, P, FRotator::ZeroRotator, 1.4f, FLinearColor(0.9f, 0.82f, 0.7f));
			}
		}
	}
}

void ABossGiantMonkey::UpdateFeedback(float DeltaSeconds)
{
	const float Now = BossNow();
	const float T = GetStateTime();
	const bool bExposed = IsKeyExposed();
	const float Pulse = 0.5f + 0.5f * FMath::Sin(GetWorld()->GetTimeSeconds() * 6.f);
	const float Flash = FMath::Clamp(1.f - (Now - LastKeyHitTime) / 0.35f, 0.f, 1.f);

	float Glow = bExposed ? 2.f + 4.f * Pulse : 0.f;
	Glow = FMath::Max(Glow, Flash * 25.f);
	if (KeyMID)
	{
		KeyMID->SetScalarParameterValue(TEXT("Glow"), Glow);
	}
	KeyLight->SetIntensity(bExposed ? 9000.f + 9000.f * Pulse + Flash * 40000.f : Flash * 40000.f);

	// Every hit flashes the whole toy for a moment.
	if (BodyMID)
	{
		const float BodyFlash = FMath::Clamp(1.f - (Now - LastHitTime) / 0.22f, 0.f, 1.f);
		BodyMID->SetScalarParameterValue(TEXT("Glow"), BodyFlash * 1.6f);
	}
	const float LocalNow = GetWorld()->GetTimeSeconds();
	DamagePopups.RemoveAll([LocalNow](const FDamagePopup& Popup) { return LocalNow - Popup.Time > 1.3f; });

	StarsFX->SetEmitting(GiantState == EGiantState::Stagger || (GiantState == EGiantState::WoundDown && T > 0.6f));
	SteamFX->SetEmitting(GiantState == EGiantState::Rewind || GiantState == EGiantState::PhaseTransition);
}
