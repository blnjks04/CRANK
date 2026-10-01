// Winding a partner's key (design doc 3.3 / 3.4 / 9.2).
// The winder's client measures circular mouse / stick input and reports whole turns; the server
// applies torque, rhythm bonus, slips, overwinding and validates the turn rate.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CrankTypes.h"
#include "WindInteractionComponent.generated.h"

class AWindupCharacter;

UENUM(BlueprintType)
enum class EWindTurnResult : uint8
{
	Normal,
	Rhythm,
	Overwind,
	Slip,
	Capped,
};

UCLASS(ClassGroup = (Crank), meta = (BlueprintSpawnableComponent))
class CRANK_API UWindInteractionComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UWindInteractionComponent();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	// ---- State queries (any machine)
	bool IsWinding() const { return WindTarget != nullptr; }
	bool IsBeingWound() const { return WoundBy != nullptr; }
	AWindupCharacter* GetWindTarget() const { return WindTarget; }
	AWindupCharacter* GetWoundBy() const { return WoundBy; }
	bool IsPulling() const;
	bool IsHoldWinding() const { return WindTarget != nullptr && bHoldWinding; }
	/** Local pull charge 0..1 (winder's client) */
	float GetLocalPullAlpha() const;
	/** Local turn progress 0..1 for HUD ring (winder's client). */
	float GetTurnProgress() const { return FMath::Clamp(FMath::Abs(AccumAngle) / 360.f, 0.f, 1.f); }
	float GetLastTurnInterval() const { return LastTurnInterval; }
	EWindTurnResult GetLastTurnResult() const { return LastTurnResult; }
	float GetLastTurnTime() const { return LastTurnLocalTime; }
	/** Is the partner currently launchable by slingshot? */
	bool CanSling() const;
	/** Should mouse/stick look input go to winding instead of the camera? */
	bool ConsumesLookInput() const;
	/** Is the locally aimed launch direction available (winder's client while pulling)? */
	bool GetLocalAim(FVector& OutOrigin, FVector& OutVelocity) const;

	// ---- Server
	/** Called by the grab component when a hand closes. Returns true if winding started. */
	bool TryBeginWinding(ECrankHand Hand);
	void StopWinding();
	void OnServerGrabReleased(ECrankHand Hand);
	/** Find a partner whose key is within reach of this doll (server + client prompt). */
	AWindupCharacter* FindWindablePartner() const;
	/** Server: AI / scripted steady winding (same as holding E). */
	void SetHoldWindServer(bool bHold);

	// ---- Owning client input
	void AddMouseDelta(const FVector2D& Delta);
	void AddStickInput(const FVector2D& Stick);
	void SetHoldWindInput(bool bPressed);
	void SetPullInput(bool bPressed);
	void OnLocalGrabReleased(ECrankHand Hand);
	void SetBreakFreeInput(float MoveMagnitude, float DeltaTime);

protected:
	UFUNCTION(Server, Reliable)
	void ServerWindTurn(float Interval);

	UFUNCTION(Server, Reliable)
	void ServerSetHoldWind(bool bHold);

	UFUNCTION(Server, Reliable)
	void ServerSetPulling(bool bPull);

	UFUNCTION(Server, Reliable)
	void ServerFireSling(FVector_NetQuantizeNormal AimDir);

	UFUNCTION(Server, Reliable)
	void ServerBreakFree();

	UFUNCTION(NetMulticast, Unreliable)
	void MulticastTurnFeedback(EWindTurnResult Result);

	UFUNCTION(Client, Reliable)
	void ClientWindingStarted(FVector_NetQuantize10 SnapLocation, FRotator SnapRotation, bool bSnap);

	UFUNCTION()
	void OnRep_WindTarget(AWindupCharacter* OldTarget);

	void ApplyTurn(float Interval, bool bVirtual);
	void LocalTurnCompleted();
	void DoSlip();
	void ResetLocalWinding();
	float ComputeSlingSpeed(float PullTime) const;
	FVector ComputeLocalAimDir() const;

	AWindupCharacter* GetDoll() const;

	/** Partner whose key I hold (on the winder). */
	UPROPERTY(ReplicatedUsing = OnRep_WindTarget)
	TObjectPtr<AWindupCharacter> WindTarget;

	/** Partner holding my key (on the target). */
	UPROPERTY(Replicated)
	TObjectPtr<AWindupCharacter> WoundBy;

	UPROPERTY(Replicated)
	bool bHoldWinding = false;

	UPROPERTY(Replicated)
	bool bPulling = false;

	ECrankHand WindingHand = ECrankHand::Left;

	// Server
	float LastServerTurnTime = -100.f;
	float PullStartServerTime = 0.f;
	float HoldOverwindAccumulator = 0.f;
	float WindStartServerTime = 0.f;

	// Local (winder's client)
	float AccumAngle = 0.f;
	FVector2D PendingMouseDelta = FVector2D::ZeroVector;
	FVector2D LastMouseDir = FVector2D::ZeroVector;
	bool bHasLastMouseDir = false;
	float LastStickAngle = 0.f;
	bool bHasLastStick = false;
	float LastTurnLocalTime = -100.f;
	float LastTurnInterval = 0.f;
	EWindTurnResult LastTurnResult = EWindTurnResult::Normal;
	bool bLocalPulling = false;
	float LocalPullStart = 0.f;
	bool bLocalHoldWind = false;
	float BreakFreeTimer = 0.f;
};
