#include "Character/WindInteractionComponent.h"
#include "Character/WindupCharacter.h"
#include "Character/TorqueComponent.h"
#include "Character/LaunchComponent.h"
#include "Character/ScatterReviveComponent.h"
#include "Character/GrabComponent.h"
#include "CrankBalance.h"
#include "FX/CrankFX.h"
#include "FX/CrankSound.h"
#include "Net/UnrealNetwork.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"

UWindInteractionComponent::UWindInteractionComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	SetIsReplicatedByDefault(true);
}

void UWindInteractionComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UWindInteractionComponent, WindTarget);
	DOREPLIFETIME(UWindInteractionComponent, WoundBy);
	DOREPLIFETIME(UWindInteractionComponent, bHoldWinding);
	DOREPLIFETIME(UWindInteractionComponent, bPulling);
}

AWindupCharacter* UWindInteractionComponent::GetDoll() const
{
	return Cast<AWindupCharacter>(GetOwner());
}

bool UWindInteractionComponent::IsPulling() const
{
	const AWindupCharacter* Doll = GetDoll();
	if (Doll && Doll->IsLocallyControlled())
	{
		return bLocalPulling;
	}
	return bPulling;
}

float UWindInteractionComponent::GetLocalPullAlpha() const
{
	if (!bLocalPulling || !GetWorld())
	{
		return 0.f;
	}
	return FMath::Clamp((GetWorld()->GetTimeSeconds() - LocalPullStart) / CrankBalance::SlingPullTime, 0.f, 1.f);
}

bool UWindInteractionComponent::CanSling() const
{
	return WindTarget && WindTarget->GetTorque()->CanLaunch();
}

bool UWindInteractionComponent::ConsumesLookInput() const
{
	return IsWinding() && !IsPulling();
}

// ------------------------------------------------------------------------------------------
// Partner search / start / stop (server)

AWindupCharacter* UWindInteractionComponent::FindWindablePartner() const
{
	const AWindupCharacter* Doll = GetDoll();
	UWorld* World = GetWorld();
	if (!Doll || !World || !Doll->CanUseArms() || IsWinding() || IsBeingWound())
	{
		return nullptr;
	}

	const FVector MyLoc = Doll->GetActorLocation();
	const FVector MyFwd = Doll->GetActorForwardVector();

	AWindupCharacter* Best = nullptr;
	float BestDist = TNumericLimits<float>::Max();
	for (TActorIterator<AWindupCharacter> It(World); It; ++It)
	{
		AWindupCharacter* Other = *It;
		if (Other == Doll || !Other->CanBeWound())
		{
			continue;
		}

		const FVector KeyLoc = Other->GetKeyWorldLocation();
		const float Dist2D = FVector::Dist2D(MyLoc, KeyLoc);
		if (Dist2D > CrankBalance::WindGrabRange || FMath::Abs(KeyLoc.Z - MyLoc.Z) > 90.f)
		{
			continue;
		}

		// I must face the key...
		const FVector ToKey = (KeyLoc - MyLoc).GetSafeNormal2D();
		if (FVector::DotProduct(MyFwd, ToKey) < 0.25f)
		{
			continue;
		}

		// ...and stand behind an upright partner.
		if (!Other->IsCrawling())
		{
			const FVector FromOther = (MyLoc - Other->GetActorLocation()).GetSafeNormal2D();
			if (FVector::DotProduct(Other->GetActorForwardVector(), FromOther) > -0.25f)
			{
				continue;
			}
		}

		if (Dist2D < BestDist)
		{
			BestDist = Dist2D;
			Best = Other;
		}
	}
	return Best;
}

bool UWindInteractionComponent::TryBeginWinding(ECrankHand Hand)
{
	AWindupCharacter* Doll = GetDoll();
	if (!Doll || !Doll->HasAuthority() || IsWinding() || IsBeingWound())
	{
		return false;
	}

	AWindupCharacter* Partner = FindWindablePartner();
	if (!Partner)
	{
		return false;
	}

	WindTarget = Partner;
	Partner->GetWind()->WoundBy = Doll;
	WindingHand = Hand;
	bPulling = false;	// bHoldWinding mirrors the held E key and survives grabbing the key
	HoldOverwindAccumulator = 0.f;
	LastServerTurnTime = -100.f;
	WindStartServerTime = GetWorld()->GetTimeSeconds();

	Partner->OnStartBeingWound(Doll);
	Doll->OnStartWinding(Partner);

	// Stand right behind the partner's key.
	FVector SnapLoc = Doll->GetActorLocation();
	FRotator SnapRot = Doll->GetActorRotation();
	if (!Partner->IsCrawling())
	{
		const FVector PartnerFwd = Partner->GetActorForwardVector().GetSafeNormal2D();
		SnapLoc = Partner->GetActorLocation() - PartnerFwd * 58.f;
		SnapLoc.Z = Doll->GetActorLocation().Z;
		SnapRot = FRotator(0.f, PartnerFwd.Rotation().Yaw, 0.f);
	}
	else
	{
		const FVector ToKey = (Partner->GetKeyWorldLocation() - Doll->GetActorLocation()).GetSafeNormal2D();
		SnapRot = FRotator(0.f, ToKey.Rotation().Yaw, 0.f);
	}
	Doll->SetActorLocationAndRotation(SnapLoc, SnapRot, true);
	if (!Doll->IsLocallyControlled())
	{
		ClientWindingStarted(Doll->GetActorLocation(), SnapRot, true);
	}
	else
	{
		ResetLocalWinding();
	}

	CrankSound::PlayAt(Doll, TEXT("SFX_KeyGrab"), Partner->GetKeyWorldLocation(), 0.8f);
	return true;
}

void UWindInteractionComponent::ClientWindingStarted_Implementation(FVector_NetQuantize10 SnapLocation, FRotator SnapRotation, bool bSnap)
{
	AWindupCharacter* Doll = GetDoll();
	if (Doll && bSnap)
	{
		Doll->SetActorLocationAndRotation(SnapLocation, SnapRotation, false, nullptr, ETeleportType::TeleportPhysics);
	}
	ResetLocalWinding();
}

void UWindInteractionComponent::StopWinding()
{
	AWindupCharacter* Doll = GetDoll();
	if (!Doll || !Doll->HasAuthority())
	{
		return;
	}

	if (AWindupCharacter* Target = WindTarget)
	{
		Target->GetWind()->WoundBy = nullptr;
		Target->OnStopBeingWound();
	}
	WindTarget = nullptr;
	bPulling = false;
	Doll->OnStopWinding();
	if (Doll->IsLocallyControlled())
	{
		ResetLocalWinding();
	}
}

void UWindInteractionComponent::OnServerGrabReleased(ECrankHand Hand)
{
	if (IsWinding() && Hand == WindingHand)
	{
		StopWinding();
	}
}

void UWindInteractionComponent::OnRep_WindTarget(AWindupCharacter* OldTarget)
{
	ResetLocalWinding();
	if (AWindupCharacter* Doll = GetDoll())
	{
		if (WindTarget)
		{
			Doll->OnStartWinding(WindTarget);
		}
		else
		{
			Doll->OnStopWinding();
		}
	}
}

void UWindInteractionComponent::ResetLocalWinding()
{
	AccumAngle = 0.f;
	bHasLastMouseDir = false;
	bHasLastStick = false;
	LastTurnLocalTime = -100.f;
	bLocalPulling = false;
	// bLocalHoldWind is the raw E key state: keep it so the release is never swallowed.
}

// ------------------------------------------------------------------------------------------
// Tick

void UWindInteractionComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	AWindupCharacter* Doll = GetDoll();
	if (!Doll)
	{
		return;
	}

	// Local key spin feedback while hold winding.
	if (WindTarget && bHoldWinding)
	{
		WindTarget->AddKeyVisualSpin(540.f * DeltaTime);
	}

	if (!Doll->HasAuthority())
	{
		return;
	}

	if (AWindupCharacter* Target = WindTarget)
	{
		const bool bTargetOk = IsValid(Target) && Target->CanContinueBeingWound() && Doll->CanUseArms()
			&& FVector::Dist2D(Target->GetKeyWorldLocation(), Doll->GetActorLocation()) < 170.f;
		if (!bTargetOk)
		{
			StopWinding();
			return;
		}

		if (bHoldWinding && !bPulling)
		{
			UTorqueComponent* Torque = Target->GetTorque();
			if (Torque->GetTorque() < CrankBalance::TorqueMax)
			{
				const float Gain = FMath::Min(CrankBalance::HoldWindPerSecond * DeltaTime, CrankBalance::TorqueMax - Torque->GetTorque());
				Torque->AddTorque(Gain);
				HoldOverwindAccumulator = 0.f;
			}
			else
			{
				HoldOverwindAccumulator += DeltaTime;
				if (HoldOverwindAccumulator >= CrankBalance::HoldOverwindInterval)
				{
					HoldOverwindAccumulator = 0.f;
					ApplyTurn(CrankBalance::HoldOverwindInterval, true);
				}
			}
		}

		if (bPulling && !CanSling())
		{
			bPulling = false;
		}
	}
}

// ------------------------------------------------------------------------------------------
// Owning-client input

void UWindInteractionComponent::AddMouseDelta(const FVector2D& Delta)
{
	if (!IsWinding() || bLocalPulling)
	{
		return;
	}

	// Collect small deltas so the direction estimate is not dominated by integer mouse counts.
	PendingMouseDelta += Delta;
	const float Mag = PendingMouseDelta.Size();
	if (Mag < 0.2f)
	{
		return;
	}
	const FVector2D Dir = PendingMouseDelta / Mag;
	PendingMouseDelta = FVector2D::ZeroVector;

	if (bHasLastMouseDir)
	{
		const float Cross = LastMouseDir.X * Dir.Y - LastMouseDir.Y * Dir.X;
		const float Dot = FVector2D::DotProduct(LastMouseDir, Dir);
		float AngleDeg = FMath::RadiansToDegrees(FMath::Atan2(Cross, Dot));
		if (FMath::Abs(AngleDeg) > 110.f)
		{
			AngleDeg = 0.f;	// sudden reversal / noise
		}
		AccumAngle += AngleDeg;
		if (WindTarget)
		{
			WindTarget->AddKeyVisualSpin(FMath::Abs(AngleDeg));
		}
		if (FMath::Abs(AccumAngle) >= 360.f)
		{
			AccumAngle -= 360.f * FMath::Sign(AccumAngle);
			LocalTurnCompleted();
		}
	}
	LastMouseDir = Dir;
	bHasLastMouseDir = true;
}

void UWindInteractionComponent::AddStickInput(const FVector2D& Stick)
{
	if (!IsWinding() || bLocalPulling)
	{
		return;
	}
	if (Stick.Size() < 0.5f)
	{
		bHasLastStick = false;
		return;
	}
	const float Angle = FMath::RadiansToDegrees(FMath::Atan2(Stick.Y, Stick.X));
	if (bHasLastStick)
	{
		const float Delta = FMath::FindDeltaAngleDegrees(LastStickAngle, Angle);
		if (FMath::Abs(Delta) < 90.f)
		{
			AccumAngle += Delta;
			if (WindTarget)
			{
				WindTarget->AddKeyVisualSpin(FMath::Abs(Delta));
			}
			if (FMath::Abs(AccumAngle) >= 360.f)
			{
				AccumAngle -= 360.f * FMath::Sign(AccumAngle);
				LocalTurnCompleted();
			}
		}
	}
	LastStickAngle = Angle;
	bHasLastStick = true;
}

void UWindInteractionComponent::LocalTurnCompleted()
{
	const float Now = GetWorld()->GetTimeSeconds();
	float Interval = Now - LastTurnLocalTime;
	if (LastTurnLocalTime < 0.f || Interval > 3.f)
	{
		Interval = 1.f;	// first turn: neutral
	}
	LastTurnLocalTime = Now;
	LastTurnInterval = Interval;
	LastTurnResult = (Interval >= CrankBalance::RhythmMin && Interval <= CrankBalance::RhythmMax) ? EWindTurnResult::Rhythm : EWindTurnResult::Normal;

	// Immediate local click for responsiveness; the server confirms the result.
	if (AWindupCharacter* Target = WindTarget)
	{
		CrankSound::PlayAt(this, TEXT("SFX_Click"), Target->GetKeyWorldLocation(), 0.9f, LastTurnResult == EWindTurnResult::Rhythm ? 1.12f : 1.f);
		UCrankFXSubsystem::Spawn(this, ECrankFX::WindClick, Target->GetKeyWorldLocation());
	}

	if (GetOwner()->HasAuthority())
	{
		ServerWindTurn_Implementation(Interval);
	}
	else
	{
		ServerWindTurn(Interval);
	}
}

void UWindInteractionComponent::ServerWindTurn_Implementation(float Interval)
{
	if (!IsWinding() || bPulling)
	{
		return;
	}
	const float Now = GetWorld()->GetTimeSeconds();
	if (Now - LastServerTurnTime < CrankBalance::MinServerTurnInterval)
	{
		return;	// rate cap
	}
	// Do not trust a claimed slow (rhythm) interval when the server saw much faster turns.
	const float ServerInterval = Now - LastServerTurnTime;
	LastServerTurnTime = Now;
	ApplyTurn(ServerInterval < Interval * 0.6f ? ServerInterval : Interval, false);
}

void UWindInteractionComponent::ApplyTurn(float Interval, bool bVirtual)
{
	AWindupCharacter* Doll = GetDoll();
	AWindupCharacter* Target = WindTarget;
	if (!Doll || !Target)
	{
		return;
	}

	UTorqueComponent* Torque = Target->GetTorque();
	const float T = Torque->GetTorque();

	if (!bVirtual && Interval < CrankBalance::SlipInterval && FMath::FRand() < CrankBalance::SlipChance)
	{
		DoSlip();
		return;
	}

	EWindTurnResult Result = EWindTurnResult::Normal;
	if (T >= CrankBalance::TorqueMax)
	{
		Torque->AddTorque(CrankBalance::OverwindPerTurn);
		const float NewT = Torque->GetTorque();
		Result = EWindTurnResult::Overwind;
		const float RuptureChance = (NewT - CrankBalance::TorqueMax) * CrankBalance::RupturePerTorque;
		if (NewT >= CrankBalance::TorqueOverMax || FMath::FRand() < RuptureChance)
		{
			MulticastTurnFeedback(EWindTurnResult::Overwind);
			StopWinding();
			Target->GetScatter()->Rupture(Doll);
			return;
		}
	}
	else
	{
		const bool bRhythm = !bVirtual && Interval >= CrankBalance::RhythmMin && Interval <= CrankBalance::RhythmMax;
		const float Gain = bRhythm ? CrankBalance::WindPerTurnRhythm : CrankBalance::WindPerTurn;
		Torque->AddTorque(FMath::Min(Gain, CrankBalance::TorqueMax - T));
		Result = bRhythm ? EWindTurnResult::Rhythm : EWindTurnResult::Normal;
		if (T + Gain >= CrankBalance::TorqueMax && T < CrankBalance::TorqueMax)
		{
			Result = EWindTurnResult::Capped;
		}
	}

	Target->NotifyKeyTurned();
	MulticastTurnFeedback(Result);
}

void UWindInteractionComponent::DoSlip()
{
	AWindupCharacter* Doll = GetDoll();
	AWindupCharacter* Target = WindTarget;
	MulticastTurnFeedback(EWindTurnResult::Slip);
	StopWinding();
	if (Doll)
	{
		Doll->StartRagdoll(-Doll->GetActorForwardVector() * 170.f + FVector(0, 0, 140.f), CrankBalance::SlipFallTime);
	}
	if (Target)
	{
		Target->StartRagdoll(Target->GetActorForwardVector() * 150.f + FVector(0, 0, 110.f), CrankBalance::SlipFallTime);
		Target->MulticastKeyFreeSpin();
	}
}

void UWindInteractionComponent::MulticastTurnFeedback_Implementation(EWindTurnResult Result)
{
	AWindupCharacter* Doll = GetDoll();
	if (!Doll)
	{
		return;
	}
	const FVector KeyLoc = WindTarget ? WindTarget->GetKeyWorldLocation() : Doll->GetActorLocation() + Doll->GetActorForwardVector() * 40.f + FVector(0, 0, 10.f);
	const bool bLocalWinder = Doll->IsLocallyControlled();

	switch (Result)
	{
	case EWindTurnResult::Normal:
		if (!bLocalWinder)
		{
			CrankSound::PlayAt(Doll, TEXT("SFX_Click"), KeyLoc, 0.8f);
		}
		break;
	case EWindTurnResult::Rhythm:
	case EWindTurnResult::Capped:
		if (!bLocalWinder)
		{
			CrankSound::PlayAt(Doll, TEXT("SFX_Click"), KeyLoc, 0.8f, 1.12f);
		}
		CrankSound::PlayAt(Doll, TEXT("SFX_RhythmDing"), KeyLoc, bLocalWinder ? 0.7f : 0.35f, Result == EWindTurnResult::Capped ? 1.3f : 1.f);
		UCrankFXSubsystem::Spawn(Doll, ECrankFX::RhythmGood, KeyLoc, Doll->GetActorRotation());
		break;
	case EWindTurnResult::Overwind:
	{
		const float T = WindTarget ? WindTarget->GetTorque()->GetTorque() : 110.f;
		CrankSound::PlayAt(Doll, TEXT("SFX_Click"), KeyLoc, 0.9f, 0.85f);
		CrankSound::PlayAt(Doll, TEXT("SFX_Creak"), KeyLoc, 0.7f, 1.f + (T - 100.f) * 0.03f);
		UCrankFXSubsystem::Spawn(Doll, ECrankFX::Steam, KeyLoc, FRotator(70.f, Doll->GetActorRotation().Yaw, 0.f), 1.2f);
		break;
	}
	case EWindTurnResult::Slip:
		CrankSound::PlayAt(Doll, TEXT("SFX_Slip"), KeyLoc, 1.f);
		UCrankFXSubsystem::Spawn(Doll, ECrankFX::Slip, Doll->GetActorLocation() - FVector(0, 0, 45.f));
		break;
	}
	if (bLocalWinder)
	{
		LastTurnResult = Result;
	}
}

void UWindInteractionComponent::SetHoldWindInput(bool bPressed)
{
	AWindupCharacter* Doll = GetDoll();
	if (!Doll || bLocalHoldWind == bPressed)
	{
		return;
	}
	bLocalHoldWind = bPressed;

	// E works on its own: it grabs the key (left hand) and keeps winding at a steady rate.
	Doll->GetGrab()->SetHoldSourceInput(bPressed);

	if (Doll->HasAuthority())
	{
		ServerSetHoldWind_Implementation(bPressed);
	}
	else
	{
		ServerSetHoldWind(bPressed);
	}
}

void UWindInteractionComponent::SetHoldWindServer(bool bHold)
{
	ServerSetHoldWind_Implementation(bHold);
}

void UWindInteractionComponent::ServerSetHoldWind_Implementation(bool bHold)
{
	bHoldWinding = bHold;
	HoldOverwindAccumulator = 0.f;
}

void UWindInteractionComponent::SetPullInput(bool bPressed)
{
	if (bPressed == bLocalPulling)
	{
		return;
	}
	if (bPressed && !(IsWinding() && CanSling()))
	{
		return;
	}
	bLocalPulling = bPressed;
	LocalPullStart = GetWorld()->GetTimeSeconds();
	if (GetOwner()->HasAuthority())
	{
		ServerSetPulling_Implementation(bPressed);
	}
	else
	{
		ServerSetPulling(bPressed);
	}
	if (bPressed && WindTarget)
	{
		CrankSound::PlayAt(this, TEXT("SFX_Stretch"), WindTarget->GetKeyWorldLocation(), 0.8f);
	}
}

void UWindInteractionComponent::ServerSetPulling_Implementation(bool bPull)
{
	if (bPull && (!IsWinding() || !CanSling()))
	{
		return;
	}
	bPulling = bPull;
	if (bPull)
	{
		PullStartServerTime = GetWorld()->GetTimeSeconds();
	}
}

FVector UWindInteractionComponent::ComputeLocalAimDir() const
{
	const AWindupCharacter* Doll = GetDoll();
	FRotator Rot = Doll ? Doll->GetActorRotation() : FRotator::ZeroRotator;
	if (Doll)
	{
		if (const AController* Controller = Doll->GetController())
		{
			Rot = Controller->GetControlRotation();
		}
	}
	Rot.Pitch = FMath::Clamp(FRotator::NormalizeAxis(Rot.Pitch) + 14.f, -5.f, 65.f);
	Rot.Roll = 0.f;
	return Rot.Vector();
}

float UWindInteractionComponent::ComputeSlingSpeed(float PullTime) const
{
	const float Alpha = FMath::Clamp(PullTime / CrankBalance::SlingPullTime, 0.f, 1.f);
	const float Bonus = WindTarget ? WindTarget->GetTorque()->GetOverwindBonus() : 0.f;
	return FMath::Lerp(CrankBalance::SlingSpeedMin, CrankBalance::SlingSpeedMax, Alpha) * (1.f + Bonus);
}

bool UWindInteractionComponent::GetLocalAim(FVector& OutOrigin, FVector& OutVelocity) const
{
	if (!IsWinding() || !bLocalPulling || !WindTarget)
	{
		return false;
	}
	OutOrigin = WindTarget->GetActorLocation() + FVector(0, 0, 10.f);
	OutVelocity = ComputeLocalAimDir() * ComputeSlingSpeed(GetWorld()->GetTimeSeconds() - LocalPullStart);
	return true;
}

void UWindInteractionComponent::OnLocalGrabReleased(ECrankHand Hand)
{
	if (IsWinding() && bLocalPulling && CanSling())
	{
		const FVector Dir = ComputeLocalAimDir();
		bLocalPulling = false;
		if (GetOwner()->HasAuthority())
		{
			ServerFireSling_Implementation(Dir);
		}
		else
		{
			ServerFireSling(Dir);
		}
	}
}

void UWindInteractionComponent::ServerFireSling_Implementation(FVector_NetQuantizeNormal AimDir)
{
	AWindupCharacter* Doll = GetDoll();
	if (!Doll || !IsWinding() || !bPulling || !CanSling())
	{
		return;
	}
	const float PullTime = GetWorld()->GetTimeSeconds() - PullStartServerTime;
	const float Speed = ComputeSlingSpeed(PullTime);
	AWindupCharacter* Target = WindTarget;
	StopWinding();

	FVector Dir = FVector(AimDir).GetSafeNormal();
	if (Dir.IsNearlyZero())
	{
		Dir = Target->GetActorForwardVector();
	}
	Target->GetLaunch()->Launch(Dir * Speed, Doll, true);
	CrankSound::PlayAt(Doll, TEXT("SFX_Sling"), Doll->GetActorLocation(), 1.f);
}

void UWindInteractionComponent::SetBreakFreeInput(float MoveMagnitude, float DeltaTime)
{
	if (!IsBeingWound())
	{
		BreakFreeTimer = 0.f;
		return;
	}
	BreakFreeTimer = MoveMagnitude > 0.9f ? BreakFreeTimer + DeltaTime : FMath::Max(0.f, BreakFreeTimer - DeltaTime);
	if (BreakFreeTimer > 0.6f)
	{
		BreakFreeTimer = 0.f;
		if (GetOwner()->HasAuthority())
		{
			ServerBreakFree_Implementation();
		}
		else
		{
			ServerBreakFree();
		}
	}
}

void UWindInteractionComponent::ServerBreakFree_Implementation()
{
	if (AWindupCharacter* Winder = WoundBy)
	{
		Winder->GetWind()->StopWinding();
	}
}
