// "먼지먹개 MK-II" robot vacuum boss (design doc chapter 6).
// Server-authoritative kinematic pawn with a C++ state machine:
// Dormant -> Patrol <-> Chase -> (Mini)Stunned -> PhaseTransition, P3 ReturnToDock / Docking, Defeated.
// Health = 3 battery cells pulled out by hand while stunned.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "CrankTypes.h"
#include "Gameplay/Grabbable.h"
#include "BossDustEater.generated.h"

class UStaticMeshComponent;
class UBoxComponent;
class USphereComponent;
class UAudioComponent;
class UPointLightComponent;
class UMaterialInstanceDynamic;
class UCrankFXEmitterComponent;
class AWindupCharacter;
class AChargingDock;
class AWaterTrail;
class ACarryableActor;
class ABossArenaManager;

UCLASS(Blueprintable)
class CRANK_API ABossDustEater : public APawn, public IGrabbable
{
	GENERATED_BODY()

public:
	ABossDustEater();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	// IGrabbable: battery cells (while stunned) and the rear dust bin lever.
	virtual bool CanBeGrabbed(const AWindupCharacter* By, ECrankHand Hand, const UPrimitiveComponent* Comp) const override;
	virtual int32 GetGrabPriority(const UPrimitiveComponent* Comp) const override { return 9; }
	virtual EGrabStyle GetGrabStyle(const UPrimitiveComponent* Comp) const override { return EGrabStyle::Anchor; }
	virtual void OnGrabbed(AWindupCharacter* By, ECrankHand Hand, UPrimitiveComponent* Comp) override;
	virtual void OnReleased(AWindupCharacter* By, ECrankHand Hand, UPrimitiveComponent* Comp, const FVector& ThrowVelocity) override;
	virtual bool TickHeld(AWindupCharacter* By, ECrankHand Hand, UPrimitiveComponent* Comp, float DeltaTime) override;
	virtual float GetHoldProgress(const UPrimitiveComponent* Comp) const override;
	virtual FText GetGrabLabel(const UPrimitiveComponent* Comp) const override;

	/** Server: a launched doll hit the boss. */
	void HandleDollImpact(AWindupCharacter* Doll, const FVector& Velocity, const FHitResult& Hit);

	/** Server: start the fight (arena entered). */
	void Activate();

	/** Server: debug cheats (BossStun, BossPhase n, BossKill, BossWake, BossDock). */
	void DebugCommand(FName Command, float Value);

	/** P3 suction pull acceleration for a doll (any machine, zero when out of range). */
	FVector GetSuctionAccel(const AWindupCharacter* Doll) const;

	EBossState GetBossState() const { return BossState; }
	int32 GetPhase() const { return Phase; }
	int32 GetCells() const { return Cells; }
	bool IsActive() const { return BossState != EBossState::Dormant && BossState != EBossState::Defeated; }
	float GetStateTime() const;
	float GetStunRemaining() const;

	// ---- Wiring (set on the placed instance / BP)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss")
	TObjectPtr<AChargingDock> Dock;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss")
	TObjectPtr<ABossArenaManager> Arena;

	/** Patrol loop (world space). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss", meta = (MakeEditWidget = true))
	TArray<FVector> PatrolPoints;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Boss")
	TSubclassOf<AWaterTrail> WaterTrailClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Boss")
	TSubclassOf<ACarryableActor> MainSpringClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Boss")
	TSubclassOf<ACarryableActor> SpentCellClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Boss")
	float DetectRadius = 1100.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Boss")
	float BodyRadius = 125.f;

protected:
	// ---- State machine (server)
	void SetBossState(EBossState NewState);
	void TickAI(float DeltaSeconds);
	void TickMovement(float DeltaSeconds, const FVector& Target, float SpeedScale, bool bAllowMiniStun);
	void TickContacts(float DeltaSeconds);
	void TickTrapped(float DeltaSeconds);
	AWindupCharacter* FindTarget() const;
	void Stun(bool bFull);
	void PullCell(int32 Index);
	void FinishPhaseTransition();
	void Defeat();
	void EjectTrapped(bool bAuto, AWindupCharacter* Only = nullptr);
	void ShakeOffRiders();
	bool IsInsideArena(const FVector& Location) const;

	UFUNCTION()
	void OnRep_BossState();

	UFUNCTION()
	void OnRep_ServerMove();

	UFUNCTION()
	void OnRep_Cells();

	UFUNCTION(NetMulticast, Reliable)
	void MulticastBossEvent(uint8 EventId, FVector_NetQuantize Location);

	void ApplyVisualState();
	void UpdateVisuals(float DeltaSeconds);

	// ---- Components
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Boss")
	TObjectPtr<UStaticMeshComponent> Body;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Boss")
	TObjectPtr<UStaticMeshComponent> Bumper;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Boss")
	TObjectPtr<USceneComponent> HatchPivot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Boss")
	TObjectPtr<UStaticMeshComponent> Hatch;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Boss")
	TObjectPtr<USceneComponent> RampPivot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Boss")
	TObjectPtr<UStaticMeshComponent> Ramp;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Boss")
	TObjectPtr<UStaticMeshComponent> BrushL;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Boss")
	TObjectPtr<UStaticMeshComponent> BrushR;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Boss")
	TObjectPtr<USphereComponent> BrushZoneL;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Boss")
	TObjectPtr<USphereComponent> BrushZoneR;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Boss")
	TObjectPtr<UBoxComponent> IntakeZone;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Boss")
	TObjectPtr<UStaticMeshComponent> Cell0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Boss")
	TObjectPtr<UStaticMeshComponent> Cell1;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Boss")
	TObjectPtr<UStaticMeshComponent> Cell2;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Boss")
	TObjectPtr<UBoxComponent> CellGrab0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Boss")
	TObjectPtr<UBoxComponent> CellGrab1;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Boss")
	TObjectPtr<UBoxComponent> CellGrab2;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Boss")
	TObjectPtr<UStaticMeshComponent> BinLever;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Boss")
	TObjectPtr<UBoxComponent> BinLeverGrab;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Boss")
	TObjectPtr<UStaticMeshComponent> BinDoor;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Boss")
	TObjectPtr<UPointLightComponent> EyeLight;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Boss")
	TObjectPtr<UCrankFXEmitterComponent> StunFX;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Boss")
	TObjectPtr<UCrankFXEmitterComponent> SuctionFX;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Boss")
	TObjectPtr<UCrankFXEmitterComponent> SmokeFX;

	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> MotorAudio;

	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> SuctionAudio;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> EyeMID;

	// ---- Replicated state
	UPROPERTY(ReplicatedUsing = OnRep_BossState)
	EBossState BossState = EBossState::Dormant;

	UPROPERTY(Replicated)
	int32 Phase = 1;

	UPROPERTY(ReplicatedUsing = OnRep_Cells)
	int32 Cells = 3;

	/** Bit per cell slot still inside the boss. */
	UPROPERTY(ReplicatedUsing = OnRep_Cells)
	uint8 CellMask = 0x7;

	UPROPERTY(Replicated)
	float StateStartTime = 0.f;	// server world time

	UPROPERTY(Replicated)
	float StunDuration = 0.f;

	UPROPERTY(ReplicatedUsing = OnRep_ServerMove)
	FVector_NetQuantize ServerLocation;

	UPROPERTY(ReplicatedUsing = OnRep_ServerMove)
	float ServerYaw = 0.f;

	UPROPERTY(Replicated)
	float ServerSpeed = 0.f;

	UPROPERTY(Replicated)
	TArray<TObjectPtr<AWindupCharacter>> Trapped;

	UPROPERTY(Replicated)
	float LeverProgress = 0.f;

	UPROPERTY(Replicated)
	float CellProgress[3] = { 0.f, 0.f, 0.f };

	// ---- Server bookkeeping
	float CurrentSpeed = 0.f;
	int32 PatrolIndex = 0;
	TWeakObjectPtr<AWindupCharacter> ChaseTarget;
	float LostTargetTime = 0.f;
	float DockTimer = 0.f;
	float WaterDistance = 0.f;
	FVector LastWaterPoint = FVector::ZeroVector;
	TMap<TWeakObjectPtr<AWindupCharacter>, float> ContactCooldown;
	int32 LeverHolders = 0;
	int32 CellHolders[3] = { 0, 0, 0 };
	int32 PendingPhaseCells = 3;

	UPROPERTY(Transient)
	TObjectPtr<AWaterTrail> WaterTrail;

	// ---- Client visuals
	float HatchAlpha = 0.f;
	float RampAlpha = 0.f;
	float BrushSpin = 0.f;
	float VisualYaw = 0.f;
	FVector VisualLocation = FVector::ZeroVector;
	bool bHasServerMove = false;
	float EyeBlink = 0.f;
};
