#include "Boss/CymbalProjectile.h"
#include "Boss/BossGiantMonkey.h"
#include "Character/WindupCharacter.h"
#include "FX/CrankFX.h"
#include "FX/CrankSound.h"
#include "Components/StaticMeshComponent.h"
#include "Components/AudioComponent.h"
#include "GameFramework/GameStateBase.h"
#include "Net/UnrealNetwork.h"
#include "EngineUtils.h"
#include "Engine/World.h"

ACymbalProjectile::ACymbalProjectile()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	SetReplicatingMovement(false);
	bAlwaysRelevant = true;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	Disc = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Disc"));
	Disc->SetupAttachment(Root);
	Disc->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Disc->SetCastShadow(true);
}

void ACymbalProjectile::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION(ACymbalProjectile, From, COND_InitialOnly);
	DOREPLIFETIME_CONDITION(ACymbalProjectile, To, COND_InitialOnly);
	DOREPLIFETIME_CONDITION(ACymbalProjectile, CurveSign, COND_InitialOnly);
	DOREPLIFETIME_CONDITION(ACymbalProjectile, OutTime, COND_InitialOnly);
	DOREPLIFETIME_CONDITION(ACymbalProjectile, LaunchTime, COND_InitialOnly);
}

float ACymbalProjectile::ServerNow() const
{
	const UWorld* World = GetWorld();
	if (const AGameStateBase* GS = World ? World->GetGameState() : nullptr)
	{
		return GS->GetServerWorldTimeSeconds();
	}
	return World ? World->GetTimeSeconds() : 0.f;
}

void ACymbalProjectile::InitFlight(const FVector& InFrom, const FVector& InTo, float InCurveSign, float InOutTime)
{
	From = InFrom;
	To = InTo;
	CurveSign = InCurveSign >= 0.f ? 1.f : -1.f;
	OutTime = FMath::Max(0.3f, InOutTime);
	LaunchTime = ServerNow();
}

void ACymbalProjectile::BeginPlay()
{
	Super::BeginPlay();
	WhooshAudio = CrankSound::CreateLoop(Root, TEXT("SFX_CymbalSpin"), 1.1f);
	if (WhooshAudio)
	{
		WhooshAudio->Play();
	}
	SetActorLocation(EvalPath(ServerNow() - LaunchTime, GetReturnPoint()));
}

FVector ACymbalProjectile::GetReturnPoint() const
{
	// The thrower may have walked or hopped meanwhile: fly back to its hand (CurveSign > 0 = left hand).
	if (const ABossGiantMonkey* Giant = Cast<ABossGiantMonkey>(GetOwner()))
	{
		return Giant->GetCymbalLocation(CurveSign > 0.f);
	}
	return From;
}

FVector ACymbalProjectile::EvalPath(float T, const FVector& ReturnTo) const
{
	const FVector A = From;
	const FVector B = To;
	FVector Flat = B - A;
	Flat.Z = 0.f;
	const float Dist = Flat.Size();
	const FVector Side = FVector::CrossProduct(FVector::UpVector, Flat.GetSafeNormal()) * CurveSign;
	const bool bBack = T > OutTime;
	const float U = FMath::Clamp(bBack ? (T - OutTime) / OutTime : T / OutTime, 0.f, 1.f);
	// Out along one side, back along the other: a lazy figure of an "O".
	const FVector P0 = bBack ? B : A;
	const FVector P2 = bBack ? ReturnTo : B;
	const FVector Ctrl = (P0 + P2) * 0.5f + Side * (bBack ? -1.f : 1.f) * Dist * 0.45f + FVector(0.f, 0.f, 60.f);
	const float V = 1.f - U;
	return P0 * (V * V) + Ctrl * (2.f * V * U) + P2 * (U * U);
}

void ACymbalProjectile::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const float T = ServerNow() - LaunchTime;
	const FVector Prev = GetActorLocation();
	const FVector Pos = EvalPath(T, GetReturnPoint());
	SpinAngle = FMath::Fmod(SpinAngle + SpinRate * DeltaSeconds, 360.f);
	SetActorLocationAndRotation(Pos, FRotator(8.f, SpinAngle, 0.f));

	if (!HasAuthority() || bDone)
	{
		return;
	}

	const FVector Vel = (Pos - Prev) / FMath::Max(DeltaSeconds, 0.001f);
	for (TActorIterator<AWindupCharacter> It(GetWorld()); It; ++It)
	{
		AWindupCharacter* Doll = *It;
		if (AlreadyHit.Contains(Doll))
		{
			continue;
		}
		const ECrankBodyState State = Doll->GetBodyState();
		if (State == ECrankBodyState::Scattered || State == ECrankBodyState::Frozen || State == ECrankBodyState::Trapped)
		{
			continue;
		}
		if (FVector::Dist(Doll->GetActorLocation(), Pos) > HitRadius)
		{
			continue;
		}
		AlreadyHit.Add(Doll);
		const FVector Push = Vel.GetSafeNormal2D() * 650.f + FVector(0.f, 0.f, 380.f);
		Doll->StartRagdoll(Push, 1.2f);
		CrankSound::PlayAt(this, TEXT("SFX_BonkMetal"), Pos, 1.2f, 1.2f);
		UCrankFXSubsystem::Spawn(this, ECrankFX::Sparks, Pos, FRotator::ZeroRotator, 2.5f);
	}

	if (T >= OutTime * 2.f)
	{
		bDone = true;
		if (ABossGiantMonkey* Giant = Cast<ABossGiantMonkey>(GetOwner()))
		{
			Giant->OnCymbalReturned();
		}
		SetLifeSpan(0.05f);
	}
}
