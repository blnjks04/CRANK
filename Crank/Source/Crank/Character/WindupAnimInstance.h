// Native animation for the windup doll. No anim graph: a custom FAnimInstanceProxy samples the
// doll's animation set (assigned in BP_WindupDoll), blends locomotion / states / arm layers on a
// worker thread and adds procedural tin-toy motion (lean, slump, overwind shake, get-up blend).

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimInstanceProxy.h"
#include "Animation/PoseSnapshot.h"
#include "CrankTypes.h"
#include "WindupAnimInstance.generated.h"

class UAnimSequence;

/** Discrete base layer the proxy blends between. */
enum class EDollAnimState : uint8
{
	Locomotion,
	Crawl,
	Air,
	Fly,
	Wind,
	Pull,
	Wound,
	Carried,
	Splat,
	Dizzy,
	Frozen,
	Emote,
	Ragdoll,
	Count
};

struct FDollAnimParams
{
	float Speed = 0.f;
	float Torque = 50.f;
	ECrankBodyState BodyState = ECrankBodyState::Normal;
	EDollAnimState State = EDollAnimState::Locomotion;
	int32 EmoteIndex = -1;
	float EmoteTime = 0.f;
	float ReachL = 0.f;
	float ReachR = 0.f;
	float Carry = 0.f;
	float GetUpAlpha = 1.f;
	float Time = 0.f;
	bool bHasSnapshot = false;
};

struct FDollAnimAssets
{
	UAnimSequence* Idle = nullptr;
	UAnimSequence* Walk = nullptr;
	UAnimSequence* Run = nullptr;
	UAnimSequence* Sprint = nullptr;
	UAnimSequence* Crawl = nullptr;
	UAnimSequence* Fall = nullptr;
	UAnimSequence* Fly = nullptr;
	UAnimSequence* Wind = nullptr;
	UAnimSequence* Pull = nullptr;
	UAnimSequence* Wound = nullptr;
	UAnimSequence* Carry = nullptr;
	UAnimSequence* Carried = nullptr;
	UAnimSequence* ReachL = nullptr;
	UAnimSequence* ReachR = nullptr;
	UAnimSequence* Splat = nullptr;
	UAnimSequence* Dizzy = nullptr;
	UAnimSequence* Frozen = nullptr;
	TArray<UAnimSequence*> Emotes;
};

struct FWindupAnimProxy : public FAnimInstanceProxy
{
	FWindupAnimProxy() = default;
	explicit FWindupAnimProxy(UAnimInstance* Instance) : FAnimInstanceProxy(Instance) {}

	virtual void PreUpdate(UAnimInstance* InAnimInstance, float DeltaSeconds) override;
	/** Copy the game-thread parameters (called again after NativeUpdateAnimation computed them). */
	void PullFromInstance(const UAnimInstance* InAnimInstance);
	virtual void Update(float DeltaSeconds) override;
	virtual bool Evaluate(FPoseContext& Output) override;

private:
	void SamplePose(UAnimSequence* Seq, float Time, bool bLoop, FPoseContext& Out) const;
	void EvaluateState(EDollAnimState State, FPoseContext& Out);
	void EvaluateLocomotion(FPoseContext& Out);
	void BlendInto(FPoseContext& Accum, const FPoseContext& Other, float Alpha) const;
	void BlendBones(FPoseContext& Base, const FPoseContext& Layer, const TArray<FCompactPoseBoneIndex>& Bones, float Alpha) const;
	void ApplyProcedural(FPoseContext& Out) const;
	void ApplyGetUpBlend(FPoseContext& Out) const;
	void RotateBoneCS(FCompactPose& Pose, FCompactPoseBoneIndex Bone, const FQuat& DeltaCS) const;
	FCompactPoseBoneIndex BoneIndex(FName Name) const;
	void CacheBoneIndices();

	FDollAnimParams Params;
	FDollAnimAssets Assets;
	FPoseSnapshot Snapshot;

	float StateWeights[(int32)EDollAnimState::Count] = {};
	float StateTimes[(int32)EDollAnimState::Count] = {};
	float LocoPhase = 0.f;
	float IdleTime = 0.f;
	float ReachLAlpha = 0.f;
	float ReachRAlpha = 0.f;
	float CarryAlpha = 0.f;
	float ShakeSeed = 0.f;

	uint16 CachedBoneSerial = 0;
	TArray<FCompactPoseBoneIndex> LeftArm;
	TArray<FCompactPoseBoneIndex> RightArm;
	FCompactPoseBoneIndex SpineIndex = FCompactPoseBoneIndex(INDEX_NONE);
	FCompactPoseBoneIndex HeadIndex = FCompactPoseBoneIndex(INDEX_NONE);
	FCompactPoseBoneIndex UpperArmL = FCompactPoseBoneIndex(INDEX_NONE);
	FCompactPoseBoneIndex UpperArmR = FCompactPoseBoneIndex(INDEX_NONE);
};

UCLASS(Transient, NotBlueprintable)
class CRANK_API UWindupAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;

	const FDollAnimParams& GetParams() const { return GameParams; }
	const FDollAnimAssets& GetAssets() const { return GameAssets; }
	const FPoseSnapshot* GetSnapshot() const;

protected:
	virtual FAnimInstanceProxy* CreateAnimInstanceProxy() override;
	virtual void DestroyAnimInstanceProxy(FAnimInstanceProxy* InProxy) override;

	FDollAnimParams GameParams;
	FDollAnimAssets GameAssets;
	float LocalTime = 0.f;
};
