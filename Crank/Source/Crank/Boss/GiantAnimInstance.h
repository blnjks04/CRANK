// Native animation for "짝짝이 대왕". No anim graph: the game thread maps the boss state (replicated state + server
// start time) to one of the keyframed clips, a worker-thread proxy samples it, cross-fades from the previous clip,
// layers the hit flinch, turns the head toward the target and hides the hand whose cymbal is flying.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimInstanceProxy.h"
#include "GiantAnimInstance.generated.h"

class UAnimSequence;

struct FGiantClipPlay
{
	UAnimSequence* Seq = nullptr;
	float Time = 0.f;
	bool bLoop = false;
};

struct FGiantAnimParams
{
	FGiantClipPlay Current;
	FGiantClipPlay Previous;
	float BlendAlpha = 1.f;		// weight of Current over Previous
	FGiantClipPlay Hit;
	float HitAlpha = 0.f;
	float HeadYaw = 0.f;		// degrees, + = toward the actor's right
	int32 HiddenHand = -1;		// 0 = left, 1 = right (its cymbal bone is scaled away)
	FName HeadBone;
	FName HandL;
	FName HandR;
};

struct FGiantAnimProxy : public FAnimInstanceProxy
{
	FGiantAnimProxy() = default;
	explicit FGiantAnimProxy(UAnimInstance* Instance) : FAnimInstanceProxy(Instance) {}

	virtual void PreUpdate(UAnimInstance* InAnimInstance, float DeltaSeconds) override;
	virtual bool Evaluate(FPoseContext& Output) override;

	void PullFromInstance(const UAnimInstance* InAnimInstance);

private:
	void Sample(const FGiantClipPlay& Clip, FPoseContext& Out) const;
	void Blend(FPoseContext& Accum, const FPoseContext& Other, float Alpha) const;
	void RotateBoneCS(FCompactPose& Pose, FCompactPoseBoneIndex Bone, const FQuat& DeltaCS) const;
	FCompactPoseBoneIndex FindBone(const FBoneContainer& Bones, FName Name) const;

	FGiantAnimParams Params;
};

UCLASS(Transient, NotBlueprintable)
class CRANK_API UGiantAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;

	const FGiantAnimParams& GetParams() const { return GameParams; }

protected:
	virtual FAnimInstanceProxy* CreateAnimInstanceProxy() override;
	virtual void DestroyAnimInstanceProxy(FAnimInstanceProxy* InProxy) override;

	FGiantAnimParams GameParams;

	// Cross-fade bookkeeping (game thread, local time). Tracked so a force-deleted / reimported clip is nulled here too.
	UPROPERTY(Transient)
	TObjectPtr<UAnimSequence> LastSeq;
	float LastSeqTime = 0.f;
	bool bLastLoop = false;
	UPROPERTY(Transient)
	TObjectPtr<UAnimSequence> PrevSeq;
	float PrevTime = 0.f;
	bool bPrevLoop = false;
	float SwitchLocalTime = -100.f;
	uint8 LastState = 0;
	float LandLocalTime = -100.f;
	float LoopClock = 0.f;

	// Turning in place steps with the walk cycle instead of sliding the feet.
	float LastYaw = 0.f;
	float TurnRate = 0.f;		// smoothed deg/s
	bool bHasYaw = false;
};
