#include "Boss/GiantAnimInstance.h"
#include "Boss/BossGiantMonkey.h"
#include "Character/WindupCharacter.h"
#include "Animation/AnimSequence.h"
#include "Components/SkeletalMeshComponent.h"
#include "BonePose.h"

FAnimInstanceProxy* UGiantAnimInstance::CreateAnimInstanceProxy()
{
	return new FGiantAnimProxy(this);
}

void UGiantAnimInstance::DestroyAnimInstanceProxy(FAnimInstanceProxy* InProxy)
{
	delete static_cast<FGiantAnimProxy*>(InProxy);
}

void UGiantAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);

	const ABossGiantMonkey* Boss = Cast<ABossGiantMonkey>(GetOwningActor());
	if (!Boss)
	{
		return;
	}
	const FGiantAnimSet& Set = Boss->GetAnimSet();
	const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
	LoopClock += DeltaSeconds;

	const float Yaw = Boss->GetActorRotation().Yaw;
	if (bHasYaw && DeltaSeconds > KINDA_SMALL_NUMBER)
	{
		const float Rate = FMath::Abs(FMath::FindDeltaAngleDegrees(LastYaw, Yaw)) / DeltaSeconds;
		TurnRate = FMath::FInterpTo(TurnRate, Rate, DeltaSeconds, 8.f);
	}
	LastYaw = Yaw;
	bHasYaw = true;

	const EGiantState State = Boss->GetGiantState();
	const float T = FMath::Max(0.f, Boss->GetStateTime());
	const float D = FMath::Max(0.05f, Boss->GetStateDuration());
	if (LastState == (uint8)EGiantState::Hop && State != EGiantState::Hop)
	{
		LandLocalTime = Now;
	}
	LastState = (uint8)State;

	UAnimSequence* Seq = nullptr;
	float Time = 0.f;
	bool bLoop = false;
	auto Fit = [&](UAnimSequence* S)	// stretch the clip over the state duration
	{
		Seq = S;
		Time = FMath::Clamp(T / D, 0.f, 1.f) * (S ? S->GetPlayLength() : 1.f);
		bLoop = false;
	};
	auto Raw = [&](UAnimSequence* S, float At)
	{
		Seq = S;
		Time = At;
		bLoop = false;
	};
	auto Loop = [&](UAnimSequence* S)
	{
		Seq = S;
		Time = LoopClock;
		bLoop = true;
	};

	switch (State)
	{
	case EGiantState::Dormant:
		Loop(Set.Dormant);
		break;
	case EGiantState::Idle:
	{
		const float LandLen = Set.Land ? Set.Land->GetPlayLength() : 0.f;
		if (Now - LandLocalTime < LandLen)
		{
			Raw(Set.Land, Now - LandLocalTime);
		}
		else if (Boss->GetWalkSpeed() > 20.f || TurnRate > 25.f)
		{
			Loop(Set.Walk);	// waddling forward, or stepping around while turning in place
		}
		else
		{
			Loop(Set.Idle);
		}
		break;
	}
	case EGiantState::ClapWindup:		Fit(Set.ClapWindup); break;
	case EGiantState::Clap:				Raw(Set.Clap, T); break;	// cymbals meet at 0.13 s, like the server ring
	case EGiantState::HopWindup:		Fit(Set.HopWindup); break;
	case EGiantState::Hop:				Fit(Set.HopAir); break;
	case EGiantState::ThrowWindup:		Fit(Boss->IsThrowLeft() ? Set.ThrowWindupL : Set.ThrowWindupR); break;
	case EGiantState::Throw:			Fit(Boss->IsThrowLeft() ? Set.ThrowL : Set.ThrowR); break;
	case EGiantState::WoundDown:
	{
		const float Enter = Set.WindDownEnter ? Set.WindDownEnter->GetPlayLength() : 0.f;
		if (T < Enter)
		{
			Raw(Set.WindDownEnter, T);
		}
		else
		{
			Seq = Set.WoundDown;
			Time = T - Enter;
			bLoop = true;
		}
		break;
	}
	case EGiantState::Rewind:			Fit(Set.Rewind); break;
	case EGiantState::Stagger:			Fit(Set.Stagger); break;
	case EGiantState::PhaseTransition:	Fit(Set.PhaseRoar); break;	// slam keyed at half time = server stomp
	case EGiantState::Defeated:			Raw(Set.Defeat, T); break;
	}
	if (!Seq)
	{
		Seq = Set.Idle;
		Time = LoopClock;
		bLoop = true;
	}

	// Cross-fade whenever the clip changes.
	if (Seq != LastSeq)
	{
		PrevSeq = LastSeq;
		PrevTime = LastSeqTime;
		bPrevLoop = bLastLoop;
		SwitchLocalTime = Now;
	}
	else if (PrevSeq)
	{
		PrevTime += DeltaSeconds;
	}
	LastSeq = Seq;
	LastSeqTime = Time;
	bLastLoop = bLoop;
	const float BlendTime = (State == EGiantState::Clap || State == EGiantState::Throw) ? 0.06f : 0.2f;

	FGiantAnimParams P;
	P.Current = { Seq, Time, bLoop };
	P.Previous = { PrevSeq, PrevTime, bPrevLoop };
	P.BlendAlpha = PrevSeq ? FMath::Clamp((Now - SwitchLocalTime) / BlendTime, 0.f, 1.f) : 1.f;

	// Key hit flinch, synced on server time so every machine flinches together.
	const float SinceHit = Boss->BossNow() - Boss->GetLastHitTime();
	const float HitLen = Set.HitReact ? Set.HitReact->GetPlayLength() : 0.f;
	if (Set.HitReact && SinceHit >= 0.f && SinceHit < HitLen && State != EGiantState::Defeated)
	{
		P.Hit = { Set.HitReact, SinceHit, false };
		P.HitAlpha = FMath::Clamp(FMath::Min(SinceHit / 0.06f, (HitLen - SinceHit) / 0.2f), 0.f, 1.f);
	}

	// Head follows the target.
	float WantYaw = 0.f;
	const bool bLooks = State != EGiantState::WoundDown && State != EGiantState::Defeated && State != EGiantState::Dormant && State != EGiantState::Stagger;
	if (const AWindupCharacter* Look = Boss->GetLookTarget(); Look && bLooks)
	{
		const FVector Local = Boss->GetActorTransform().InverseTransformPosition(Look->GetActorLocation());
		WantYaw = FMath::Clamp(FMath::RadiansToDegrees(FMath::Atan2(Local.Y, Local.X)), -35.f, 35.f);
	}
	P.HeadYaw = FMath::FInterpTo(GameParams.HeadYaw, WantYaw, DeltaSeconds, 4.f);
	P.HiddenHand = Boss->IsCymbalOut() ? (Boss->IsThrowLeft() ? 0 : 1) : -1;
	P.HeadBone = Boss->BoneHead;
	// Hide only the flying cymbal (older rigs without cymbal bones lose the whole hand).
	const USkeletalMeshComponent* SkelMesh = GetSkelMeshComponent();
	const bool bCymbalBones = SkelMesh && SkelMesh->GetBoneIndex(Boss->BoneCymbalL) != INDEX_NONE;
	P.HandL = bCymbalBones ? Boss->BoneCymbalL : Boss->BoneHandL;
	P.HandR = bCymbalBones ? Boss->BoneCymbalR : Boss->BoneHandR;
	GameParams = P;

	// Push straight into the proxy (game thread, before the parallel evaluation).
	GetProxyOnGameThread<FGiantAnimProxy>().PullFromInstance(this);
}

// ------------------------------------------------------------------------------------------
// Proxy

void FGiantAnimProxy::PreUpdate(UAnimInstance* InAnimInstance, float DeltaSeconds)
{
	FAnimInstanceProxy::PreUpdate(InAnimInstance, DeltaSeconds);
	PullFromInstance(InAnimInstance);
}

void FGiantAnimProxy::PullFromInstance(const UAnimInstance* InAnimInstance)
{
	if (const UGiantAnimInstance* Inst = Cast<UGiantAnimInstance>(InAnimInstance))
	{
		Params = Inst->GetParams();
	}
}

void FGiantAnimProxy::Sample(const FGiantClipPlay& Clip, FPoseContext& Out) const
{
	if (!Clip.Seq)
	{
		Out.ResetToRefPose();
		return;
	}
	const float Length = FMath::Max(0.01f, Clip.Seq->GetPlayLength());
	const float SampleTime = Clip.bLoop ? FMath::Fmod(FMath::Max(0.f, Clip.Time), Length) : FMath::Clamp(Clip.Time, 0.f, Length);
	FAnimationPoseData PoseData(Out);
	Clip.Seq->GetAnimationPose(PoseData, FAnimExtractContext(static_cast<double>(SampleTime), false, FDeltaTimeRecord(), Clip.bLoop));
}

void FGiantAnimProxy::Blend(FPoseContext& Accum, const FPoseContext& Other, float Alpha) const
{
	Alpha = FMath::Clamp(Alpha, 0.f, 1.f);
	if (Alpha <= 0.f)
	{
		return;
	}
	if (Alpha >= 1.f)
	{
		Accum.Pose.CopyBonesFrom(Other.Pose);
		return;
	}
	for (const FCompactPoseBoneIndex Index : Accum.Pose.ForEachBoneIndex())
	{
		FTransform Result;
		Result.Blend(Accum.Pose[Index], Other.Pose[Index], Alpha);
		Accum.Pose[Index] = Result;
	}
}

FCompactPoseBoneIndex FGiantAnimProxy::FindBone(const FBoneContainer& Bones, FName Name) const
{
	const int32 MeshIndex = Bones.GetPoseBoneIndexForBoneName(Name);
	if (MeshIndex == INDEX_NONE)
	{
		return FCompactPoseBoneIndex(INDEX_NONE);
	}
	return Bones.MakeCompactPoseIndex(FMeshPoseBoneIndex(MeshIndex));
}

void FGiantAnimProxy::RotateBoneCS(FCompactPose& Pose, FCompactPoseBoneIndex Bone, const FQuat& DeltaCS) const
{
	if (Bone == INDEX_NONE)
	{
		return;
	}
	TArray<FCompactPoseBoneIndex, TInlineAllocator<12>> Chain;
	FCompactPoseBoneIndex Parent = Pose.GetParentBoneIndex(Bone);
	while (Parent != INDEX_NONE)
	{
		Chain.Add(Parent);
		Parent = Pose.GetParentBoneIndex(Parent);
	}
	FTransform ParentCS = FTransform::Identity;
	for (int32 i = Chain.Num() - 1; i >= 0; --i)
	{
		ParentCS = Pose[Chain[i]] * ParentCS;
	}
	const FTransform BoneCS = Pose[Bone] * ParentCS;
	FTransform NewCS = BoneCS;
	NewCS.SetRotation((DeltaCS * BoneCS.GetRotation()).GetNormalized());
	Pose[Bone] = NewCS.GetRelativeTransform(ParentCS);
}

bool FGiantAnimProxy::Evaluate(FPoseContext& Output)
{
	Sample(Params.Current, Output);

	if (Params.Previous.Seq && Params.BlendAlpha < 1.f)
	{
		FPoseContext Prev(Output);
		Sample(Params.Previous, Prev);
		Blend(Prev, Output, Params.BlendAlpha);
		Output.Pose.CopyBonesFrom(Prev.Pose);
	}
	if (Params.Hit.Seq && Params.HitAlpha > 0.f)
	{
		FPoseContext Hit(Output);
		Sample(Params.Hit, Hit);
		Blend(Output, Hit, Params.HitAlpha);
	}

	FCompactPose& Pose = Output.Pose;
	const FBoneContainer& Bones = Pose.GetBoneContainer();
	if (FMath::Abs(Params.HeadYaw) > 0.1f)
	{
		RotateBoneCS(Pose, FindBone(Bones, Params.HeadBone), FQuat(FVector::UpVector, FMath::DegreesToRadians(Params.HeadYaw)));
	}
	if (Params.HiddenHand >= 0)
	{
		const FCompactPoseBoneIndex Hand = FindBone(Bones, Params.HiddenHand == 0 ? Params.HandL : Params.HandR);
		if (Hand != INDEX_NONE)
		{
			Pose[Hand].SetScale3D(FVector(0.01f));
		}
	}
	return true;
}
