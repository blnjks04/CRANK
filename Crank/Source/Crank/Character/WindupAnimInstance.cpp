#include "Character/WindupAnimInstance.h"
#include "Character/WindupCharacter.h"
#include "Character/TorqueComponent.h"
#include "Character/WindInteractionComponent.h"
#include "Character/GrabComponent.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimationPoseData.h"
#include "BonePose.h"
#include "GameFramework/CharacterMovementComponent.h"

namespace
{
	const FName NAME_Spine(TEXT("spine"));
	const FName NAME_Head(TEXT("head"));
	const FName NAME_UpperArmL(TEXT("upperarm_l"));
	const FName NAME_LowerArmL(TEXT("lowerarm_l"));
	const FName NAME_HandL(TEXT("hand_l"));
	const FName NAME_UpperArmR(TEXT("upperarm_r"));
	const FName NAME_LowerArmR(TEXT("lowerarm_r"));
	const FName NAME_HandR(TEXT("hand_r"));

	float SmoothStep(float X)
	{
		X = FMath::Clamp(X, 0.f, 1.f);
		return X * X * (3.f - 2.f * X);
	}

	bool IsUprightState(EDollAnimState State)
	{
		return State == EDollAnimState::Locomotion || State == EDollAnimState::Air || State == EDollAnimState::Wound || State == EDollAnimState::Emote;
	}
}

// ------------------------------------------------------------------------------------------
// Anim instance (game thread)

FAnimInstanceProxy* UWindupAnimInstance::CreateAnimInstanceProxy()
{
	return new FWindupAnimProxy(this);
}

void UWindupAnimInstance::DestroyAnimInstanceProxy(FAnimInstanceProxy* InProxy)
{
	delete InProxy;
}

const FPoseSnapshot* UWindupAnimInstance::GetSnapshot() const
{
	const AWindupCharacter* Doll = Cast<AWindupCharacter>(GetOwningActor());
	return Doll ? &Doll->GetGetUpSnapshot() : nullptr;
}

void UWindupAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);

	LocalTime += DeltaSeconds;
	AWindupCharacter* Doll = Cast<AWindupCharacter>(GetOwningActor());
	if (!Doll)
	{
		return;
	}

	FDollAnimParams P;
	P.Time = LocalTime;
	P.Speed = Doll->GetVelocity().Size2D();
	P.Torque = Doll->GetTorque() ? Doll->GetTorque()->GetTorque() : 50.f;
	P.BodyState = Doll->GetBodyState();

	const UWindInteractionComponent* Wind = Doll->GetWind();
	const UGrabComponent* Grab = Doll->GetGrab();
	const UCharacterMovementComponent* Move = Doll->GetCharacterMovement();

	switch (P.BodyState)
	{
	case ECrankBodyState::Ragdoll:	P.State = EDollAnimState::Ragdoll; break;
	case ECrankBodyState::Flying:	P.State = EDollAnimState::Fly; break;
	case ECrankBodyState::Splat:	P.State = EDollAnimState::Splat; break;
	case ECrankBodyState::Carried:	P.State = EDollAnimState::Carried; break;
	case ECrankBodyState::Frozen:	P.State = EDollAnimState::Frozen; break;
	default:
		if (Doll->IsHelpless())
		{
			P.State = EDollAnimState::Dizzy;
		}
		else if (Wind && Wind->IsWinding())
		{
			P.State = Wind->IsPulling() ? EDollAnimState::Pull : EDollAnimState::Wind;
		}
		else if (Wind && Wind->IsBeingWound())
		{
			P.State = EDollAnimState::Wound;
		}
		else if (Doll->IsCrawling())
		{
			P.State = EDollAnimState::Crawl;
		}
		else if (Move && Move->IsFalling())
		{
			P.State = EDollAnimState::Air;
		}
		else if (Doll->GetActiveEmote() > 0)
		{
			P.State = EDollAnimState::Emote;
			P.EmoteIndex = Doll->GetActiveEmote() - 1;
			P.EmoteTime = Doll->GetEmoteTime();
		}
		else
		{
			P.State = EDollAnimState::Locomotion;
		}
		break;
	}

	if (Grab && P.State != EDollAnimState::Wind && P.State != EDollAnimState::Pull && P.State != EDollAnimState::Crawl)
	{
		const FCrankHandState& L = Grab->GetHand(ECrankHand::Left);
		const FCrankHandState& R = Grab->GetHand(ECrankHand::Right);
		const bool bCarry = Grab->IsCarrying();
		P.Carry = bCarry ? 1.f : 0.f;
		P.ReachL = !bCarry && (Grab->WantsGrab(ECrankHand::Left) || L.IsHolding()) ? 1.f : 0.f;
		P.ReachR = !bCarry && (Grab->WantsGrab(ECrankHand::Right) || R.IsHolding()) ? 1.f : 0.f;
	}

	P.GetUpAlpha = Doll->GetGetUpAlpha();
	P.bHasSnapshot = Doll->GetGetUpSnapshot().bIsValid && P.GetUpAlpha < 1.f;

	const FDollAnimSet& Set = Doll->GetAnimSet();
	FDollAnimAssets A;
	A.Idle = Set.Idle; A.Walk = Set.Walk; A.Run = Set.Run; A.Sprint = Set.Sprint; A.Crawl = Set.Crawl;
	A.Fall = Set.Fall; A.Fly = Set.Fly; A.Wind = Set.Wind; A.Pull = Set.Pull; A.Wound = Set.Wound;
	A.Carry = Set.Carry; A.Carried = Set.Carried; A.ReachL = Set.ReachL; A.ReachR = Set.ReachR;
	A.Splat = Set.Splat; A.Dizzy = Set.Dizzy; A.Frozen = Set.Frozen;
	for (const TObjectPtr<UAnimSequence>& Emote : Set.Emotes)
	{
		A.Emotes.Add(Emote);
	}

	GameParams = P;
	GameAssets = A;

	// Push straight into the proxy (game thread, before the parallel update).
	FWindupAnimProxy& Proxy = GetProxyOnGameThread<FWindupAnimProxy>();
	Proxy.PullFromInstance(this);
}

// ------------------------------------------------------------------------------------------
// Proxy

void FWindupAnimProxy::PreUpdate(UAnimInstance* InAnimInstance, float DeltaSeconds)
{
	FAnimInstanceProxy::PreUpdate(InAnimInstance, DeltaSeconds);
	PullFromInstance(InAnimInstance);
}

void FWindupAnimProxy::PullFromInstance(const UAnimInstance* InAnimInstance)
{
	const UWindupAnimInstance* Inst = Cast<UWindupAnimInstance>(InAnimInstance);
	if (!Inst)
	{
		return;
	}
	Params = Inst->GetParams();
	Assets = Inst->GetAssets();
	if (Params.bHasSnapshot)
	{
		if (const FPoseSnapshot* Snap = Inst->GetSnapshot())
		{
			Snapshot = *Snap;
		}
	}
}

void FWindupAnimProxy::Update(float DeltaSeconds)
{
	const float Dt = FMath::Min(DeltaSeconds, 0.1f);

	// Cross-fade between base states.
	const float BlendRate = Params.State == EDollAnimState::Ragdoll ? 100.f : 7.f;
	for (int32 i = 0; i < (int32)EDollAnimState::Count; ++i)
	{
		const float Target = (i == (int32)Params.State) ? 1.f : 0.f;
		StateWeights[i] = FMath::FInterpConstantTo(StateWeights[i], Target, Dt, BlendRate);
		if (StateWeights[i] > 0.f)
		{
			StateTimes[i] += Dt;
		}
		else
		{
			StateTimes[i] = 0.f;
		}
	}

	// Locomotion phase: keep walk / run / sprint cycles in sync.
	IdleTime += Dt;
	const float S = Params.Speed;
	if (S > 5.f)
	{
		float Rate = 0.f;
		auto CycleRate = [](UAnimSequence* Seq, float NaturalSpeed, float Speed)
		{
			const float Len = Seq ? FMath::Max(0.1f, Seq->GetPlayLength()) : 0.8f;
			return FMath::Clamp(Speed / NaturalSpeed, 0.5f, 1.6f) / Len;
		};
		if (S <= 150.f)
		{
			Rate = CycleRate(Assets.Walk, 150.f, S);
		}
		else if (S <= 300.f)
		{
			const float A = (S - 150.f) / 150.f;
			Rate = FMath::Lerp(CycleRate(Assets.Walk, 150.f, S), CycleRate(Assets.Run, 300.f, S), A);
		}
		else
		{
			const float A = FMath::Clamp((S - 300.f) / 150.f, 0.f, 1.f);
			Rate = FMath::Lerp(CycleRate(Assets.Run, 300.f, S), CycleRate(Assets.Sprint, 450.f, S), A);
		}
		LocoPhase = FMath::Fmod(LocoPhase + Rate * Dt, 1.f);
	}

	ReachLAlpha = FMath::FInterpConstantTo(ReachLAlpha, Params.ReachL, Dt, 8.f);
	ReachRAlpha = FMath::FInterpConstantTo(ReachRAlpha, Params.ReachR, Dt, 8.f);
	CarryAlpha = FMath::FInterpConstantTo(CarryAlpha, Params.Carry, Dt, 6.f);
	ShakeSeed += Dt;
}

FCompactPoseBoneIndex FWindupAnimProxy::BoneIndex(FName Name) const
{
	const FBoneContainer& Bones = GetRequiredBones();
	const int32 MeshIndex = Bones.GetPoseBoneIndexForBoneName(Name);
	if (MeshIndex == INDEX_NONE)
	{
		return FCompactPoseBoneIndex(INDEX_NONE);
	}
	return Bones.MakeCompactPoseIndex(FMeshPoseBoneIndex(MeshIndex));
}

void FWindupAnimProxy::CacheBoneIndices()
{
	const FBoneContainer& Bones = GetRequiredBones();
	if (CachedBoneSerial == Bones.GetSerialNumber() && LeftArm.Num() > 0)
	{
		return;
	}
	CachedBoneSerial = Bones.GetSerialNumber();

	LeftArm.Reset();
	RightArm.Reset();
	for (const FName& Name : { NAME_UpperArmL, NAME_LowerArmL, NAME_HandL })
	{
		const FCompactPoseBoneIndex Index = BoneIndex(Name);
		if (Index != INDEX_NONE)
		{
			LeftArm.Add(Index);
		}
	}
	for (const FName& Name : { NAME_UpperArmR, NAME_LowerArmR, NAME_HandR })
	{
		const FCompactPoseBoneIndex Index = BoneIndex(Name);
		if (Index != INDEX_NONE)
		{
			RightArm.Add(Index);
		}
	}
	SpineIndex = BoneIndex(NAME_Spine);
	HeadIndex = BoneIndex(NAME_Head);
	UpperArmL = BoneIndex(NAME_UpperArmL);
	UpperArmR = BoneIndex(NAME_UpperArmR);
}

void FWindupAnimProxy::SamplePose(UAnimSequence* Seq, float Time, bool bLoop, FPoseContext& Out) const
{
	if (!Seq)
	{
		Seq = Assets.Idle;
	}
	if (!Seq)
	{
		Out.ResetToRefPose();
		return;
	}
	const float Length = FMath::Max(0.01f, Seq->GetPlayLength());
	const float SampleTime = bLoop ? FMath::Fmod(FMath::Max(0.f, Time), Length) : FMath::Clamp(Time, 0.f, Length);
	FAnimationPoseData PoseData(Out);
	Seq->GetAnimationPose(PoseData, FAnimExtractContext(static_cast<double>(SampleTime), false, FDeltaTimeRecord(), bLoop));
}

void FWindupAnimProxy::EvaluateLocomotion(FPoseContext& Out)
{
	const float S = Params.Speed;
	const float IdleWeight = FMath::Clamp(1.f - S / 60.f, 0.f, 1.f);

	auto SampleCycle = [this](UAnimSequence* Seq, FPoseContext& Pose)
	{
		UAnimSequence* Use = Seq ? Seq : Assets.Idle;
		const float Len = Use ? Use->GetPlayLength() : 1.f;
		SamplePose(Use, LocoPhase * Len, true, Pose);
	};

	// Moving pose (walk -> run -> sprint).
	FPoseContext Moving(this);
	if (S <= 150.f)
	{
		SampleCycle(Assets.Walk, Moving);
	}
	else if (S <= 300.f)
	{
		SampleCycle(Assets.Walk, Moving);
		FPoseContext RunPose(this);
		SampleCycle(Assets.Run, RunPose);
		BlendInto(Moving, RunPose, (S - 150.f) / 150.f);
	}
	else
	{
		SampleCycle(Assets.Run, Moving);
		FPoseContext SprintPose(this);
		SampleCycle(Assets.Sprint, SprintPose);
		BlendInto(Moving, SprintPose, FMath::Clamp((S - 300.f) / 150.f, 0.f, 1.f));
	}

	if (IdleWeight <= 0.f)
	{
		Out = Moving;
		return;
	}
	SamplePose(Assets.Idle, IdleTime, true, Out);
	BlendInto(Out, Moving, 1.f - IdleWeight);
}

void FWindupAnimProxy::EvaluateState(EDollAnimState State, FPoseContext& Out)
{
	const float T = StateTimes[(int32)State];
	switch (State)
	{
	case EDollAnimState::Locomotion:	EvaluateLocomotion(Out); break;
	case EDollAnimState::Crawl:
	{
		// Crawl cycle speed follows actual crawl speed.
		UAnimSequence* Seq = Assets.Crawl;
		const float Len = Seq ? Seq->GetPlayLength() : 1.f;
		const float Phase = FMath::Fmod(Params.Time * FMath::Clamp(Params.Speed / 40.f, 0.f, 1.5f), 1000.f);
		SamplePose(Seq, FMath::Fmod(Phase, Len), true, Out);
		break;
	}
	case EDollAnimState::Air:		SamplePose(Assets.Fall, T, true, Out); break;
	case EDollAnimState::Fly:		SamplePose(Assets.Fly, T, true, Out); break;
	case EDollAnimState::Wind:		SamplePose(Assets.Wind, T, true, Out); break;
	case EDollAnimState::Pull:		SamplePose(Assets.Pull ? Assets.Pull : Assets.Wind, T, false, Out); break;
	case EDollAnimState::Wound:		SamplePose(Assets.Wound, T, true, Out); break;
	case EDollAnimState::Carried:	SamplePose(Assets.Carried, T, true, Out); break;
	case EDollAnimState::Splat:		SamplePose(Assets.Splat, T, false, Out); break;
	case EDollAnimState::Dizzy:		SamplePose(Assets.Dizzy, T, true, Out); break;
	case EDollAnimState::Frozen:	SamplePose(Assets.Frozen ? Assets.Frozen : Assets.Idle, 0.f, false, Out); break;
	case EDollAnimState::Emote:
	{
		UAnimSequence* Seq = Assets.Emotes.IsValidIndex(Params.EmoteIndex) ? Assets.Emotes[Params.EmoteIndex] : nullptr;
		SamplePose(Seq, Params.EmoteTime, true, Out);
		break;
	}
	default:
		SamplePose(Assets.Idle, IdleTime, true, Out);
		break;
	}
}

void FWindupAnimProxy::BlendInto(FPoseContext& Accum, const FPoseContext& Other, float Alpha) const
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

void FWindupAnimProxy::BlendBones(FPoseContext& Base, const FPoseContext& Layer, const TArray<FCompactPoseBoneIndex>& Bones, float Alpha) const
{
	Alpha = FMath::Clamp(Alpha, 0.f, 1.f);
	for (const FCompactPoseBoneIndex Index : Bones)
	{
		FTransform Result;
		Result.Blend(Base.Pose[Index], Layer.Pose[Index], Alpha);
		Base.Pose[Index] = Result;
	}
}

void FWindupAnimProxy::RotateBoneCS(FCompactPose& Pose, FCompactPoseBoneIndex Bone, const FQuat& DeltaCS) const
{
	if (Bone == INDEX_NONE)
	{
		return;
	}
	// Parent component-space transform, composed root first.
	TArray<FCompactPoseBoneIndex, TInlineAllocator<8>> Chain;
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

void FWindupAnimProxy::ApplyProcedural(FPoseContext& Out) const
{
	const float T = Params.Torque;
	const FVector RightCS(0.f, 1.f, 0.f);
	const FVector UpCS(0.f, 0.f, 1.f);
	const FVector FwdCS(1.f, 0.f, 0.f);

	float LeanUpright = 0.f;
	for (int32 i = 0; i < (int32)EDollAnimState::Count; ++i)
	{
		if (IsUprightState((EDollAnimState)i))
		{
			LeanUpright += StateWeights[i];
		}
	}
	LeanUpright = FMath::Clamp(LeanUpright, 0.f, 1.f);

	// 70~100 T: body tilts forward while running. 1~29 T: shoulders sag, head droops.
	float Lean = 0.f;
	if (T >= 70.f)
	{
		Lean = 12.f * FMath::Clamp((Params.Speed - 200.f) / 250.f, 0.f, 1.f);
	}
	const float Slump = (T > 0.f && T < 30.f) ? FMath::Lerp(10.f, 3.f, T / 30.f) : 0.f;

	if (LeanUpright > 0.f && SpineIndex != INDEX_NONE)
	{
		RotateBoneCS(Out.Pose, SpineIndex, FQuat(RightCS, FMath::DegreesToRadians((Lean + Slump * 0.6f) * LeanUpright)));
	}
	if (LeanUpright > 0.f && HeadIndex != INDEX_NONE && Slump > 0.f)
	{
		RotateBoneCS(Out.Pose, HeadIndex, FQuat(RightCS, FMath::DegreesToRadians(Slump * LeanUpright)));
	}

	// 101~120 T: the whole body rattles.
	if (T > 100.f && SpineIndex != INDEX_NONE)
	{
		const float Amp = FMath::Clamp((T - 100.f) / 20.f, 0.f, 1.f) * 4.5f;
		const float A = FMath::Sin(ShakeSeed * 57.f) * Amp;
		const float B = FMath::Sin(ShakeSeed * 43.f + 1.3f) * Amp;
		RotateBoneCS(Out.Pose, SpineIndex, FQuat(FwdCS, FMath::DegreesToRadians(A)));
		if (HeadIndex != INDEX_NONE)
		{
			RotateBoneCS(Out.Pose, HeadIndex, FQuat(UpCS, FMath::DegreesToRadians(B * 1.5f)));
		}
	}
}

void FWindupAnimProxy::ApplyGetUpBlend(FPoseContext& Out) const
{
	if (!Params.bHasSnapshot || Params.GetUpAlpha >= 1.f || !Snapshot.bIsValid)
	{
		return;
	}
	const float Alpha = SmoothStep(Params.GetUpAlpha);
	const FBoneContainer& Bones = Out.Pose.GetBoneContainer();
	for (const FCompactPoseBoneIndex Index : Out.Pose.ForEachBoneIndex())
	{
		const int32 MeshIndex = Bones.MakeMeshPoseIndex(Index).GetInt();
		if (!Snapshot.LocalTransforms.IsValidIndex(MeshIndex))
		{
			continue;
		}
		FTransform Result;
		Result.Blend(Snapshot.LocalTransforms[MeshIndex], Out.Pose[Index], Alpha);
		Out.Pose[Index] = Result;
	}
}

bool FWindupAnimProxy::Evaluate(FPoseContext& Output)
{
	CacheBoneIndices();

	float Total = 0.f;
	bool bFirst = true;
	for (int32 i = 0; i < (int32)EDollAnimState::Count; ++i)
	{
		const float W = StateWeights[i];
		if (W <= 0.01f)
		{
			continue;
		}
		if (bFirst)
		{
			EvaluateState((EDollAnimState)i, Output);
			Total = W;
			bFirst = false;
		}
		else
		{
			FPoseContext Temp(Output);
			EvaluateState((EDollAnimState)i, Temp);
			Total += W;
			BlendInto(Output, Temp, W / Total);
		}
	}
	if (bFirst)
	{
		EvaluateState(Params.State, Output);
	}

	// Arm layers: carry (both arms), reach (per arm).
	if (CarryAlpha > 0.01f && Assets.Carry)
	{
		FPoseContext Layer(Output);
		SamplePose(Assets.Carry, IdleTime, true, Layer);
		BlendBones(Output, Layer, LeftArm, CarryAlpha);
		BlendBones(Output, Layer, RightArm, CarryAlpha);
	}
	if (ReachLAlpha > 0.01f && Assets.ReachL)
	{
		FPoseContext Layer(Output);
		SamplePose(Assets.ReachL, IdleTime, true, Layer);
		BlendBones(Output, Layer, LeftArm, ReachLAlpha);
	}
	if (ReachRAlpha > 0.01f && Assets.ReachR)
	{
		FPoseContext Layer(Output);
		SamplePose(Assets.ReachR, IdleTime, true, Layer);
		BlendBones(Output, Layer, RightArm, ReachRAlpha);
	}

	ApplyProcedural(Output);
	ApplyGetUpBlend(Output);
	Output.Pose.NormalizeRotations();
	return true;
}
