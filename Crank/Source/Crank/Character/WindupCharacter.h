// The windup doll (design doc 9.1 AWindupCharacter).
// ACharacter + semi ragdoll (physical animation on the arms) + full ragdoll on falls.
// Body state is server authoritative and replicated; ragdoll limbs simulate locally, only the
// root (capsule following the pelvis) is synchronised (9.3).

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Animation/PoseSnapshot.h"
#include "CrankTypes.h"
#include "Gameplay/Grabbable.h"
#include "InputActionValue.h"
#include "WindupCharacter.generated.h"

class USpringArmComponent;
class UCameraComponent;
class UTorqueComponent;
class UWindInteractionComponent;
class ULaunchComponent;
class UGrabComponent;
class UScatterReviveComponent;
class UPhysicalAnimationComponent;
class UStaticMeshComponent;
class UCrankFXEmitterComponent;
class UMaterialInstanceDynamic;
class UAnimSequence;
class ABodyPartPickup;
class ASurfaceZone;
class UAudioComponent;

USTRUCT()
struct FBodyStateRep
{
	GENERATED_BODY()

	UPROPERTY()
	ECrankBodyState State = ECrankBodyState::Normal;

	/** Ragdoll impulse / launch velocity / splat normal, depending on the state. */
	UPROPERTY()
	FVector_NetQuantize10 Param = FVector::ZeroVector;

	/** Incremented on every change so repeated states still replicate. */
	UPROPERTY()
	uint8 Seq = 0;
};

USTRUCT()
struct FEmoteRep
{
	GENERATED_BODY()

	UPROPERTY()
	uint8 EmoteId = 0;	// 0 = none, 1..4

	UPROPERTY()
	uint8 Seq = 0;
};

/** Animation set assigned in the character blueprint. */
USTRUCT(BlueprintType)
struct FDollAnimSet
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly) TObjectPtr<UAnimSequence> Idle;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) TObjectPtr<UAnimSequence> Walk;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) TObjectPtr<UAnimSequence> Run;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) TObjectPtr<UAnimSequence> Sprint;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) TObjectPtr<UAnimSequence> Crawl;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) TObjectPtr<UAnimSequence> Fall;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) TObjectPtr<UAnimSequence> Fly;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) TObjectPtr<UAnimSequence> Wind;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) TObjectPtr<UAnimSequence> Pull;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) TObjectPtr<UAnimSequence> Wound;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) TObjectPtr<UAnimSequence> Carry;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) TObjectPtr<UAnimSequence> Carried;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) TObjectPtr<UAnimSequence> ReachL;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) TObjectPtr<UAnimSequence> ReachR;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) TObjectPtr<UAnimSequence> Splat;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) TObjectPtr<UAnimSequence> Dizzy;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) TObjectPtr<UAnimSequence> Frozen;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) TArray<TObjectPtr<UAnimSequence>> Emotes;
};

UCLASS(Blueprintable)
class CRANK_API AWindupCharacter : public ACharacter, public IGrabbable
{
	GENERATED_BODY()

public:
	AWindupCharacter(const FObjectInitializer& ObjectInitializer);

	// AActor / ACharacter
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
	virtual void PossessedBy(AController* NewController) override;
	virtual void OnRep_PlayerState() override;
	virtual bool CanJumpInternal_Implementation() const override;
	virtual void OnJumped_Implementation() override;
	virtual void Landed(const FHitResult& Hit) override;
	virtual void NotifyHit(UPrimitiveComponent* MyComp, AActor* Other, UPrimitiveComponent* OtherComp, bool bSelfMoved, FVector HitLocation, FVector HitNormal, FVector NormalImpulse, const FHitResult& Hit) override;
	virtual void FellOutOfWorld(const UDamageType& DmgType) override;
	virtual void OnConstruction(const FTransform& Transform) override;

	// IGrabbable (partner body: one hand tugs, two hands piggyback)
	virtual bool CanBeGrabbed(const AWindupCharacter* By, ECrankHand Hand, const UPrimitiveComponent* Comp) const override;
	virtual int32 GetGrabPriority(const UPrimitiveComponent* Comp) const override { return 2; }
	virtual EGrabStyle GetGrabStyle(const UPrimitiveComponent* Comp) const override { return EGrabStyle::Friend; }
	virtual FVector GetGrabPoint(const UPrimitiveComponent* Comp, const FVector& HandLocation) const override;
	virtual void OnReleased(AWindupCharacter* By, ECrankHand Hand, UPrimitiveComponent* Comp, const FVector& ThrowVelocity) override;
	virtual FText GetGrabLabel(const UPrimitiveComponent* Comp) const override;

	// Components
	UTorqueComponent* GetTorque() const { return Torque; }
	UWindInteractionComponent* GetWind() const { return Wind; }
	ULaunchComponent* GetLaunch() const { return LaunchComp; }
	UGrabComponent* GetGrab() const { return Grab; }
	UScatterReviveComponent* GetScatter() const { return Scatter; }
	UCameraComponent* GetFollowCamera() const { return FollowCamera; }

	// ---- Body state
	ECrankBodyState GetBodyState() const { return BodyRep.State; }
	float GetBodyStateTime() const;

	/** Server: change body state. Param meaning depends on the state (see FBodyStateRep). */
	void SetBodyState(ECrankBodyState NewState, const FVector& Param = FVector::ZeroVector);

	/** Server: full ragdoll for at least MinTime seconds. */
	void StartRagdoll(const FVector& Impulse, float MinTime = 1.f);

	/** Server: back onto the last floor this doll stood on (fell through the world / out of bounds). */
	void RecoverToSafeGround();

	/** Debug (CrankRun cheat): keep running straight ahead for a while, as if forward were held. */
	void DebugRun(float Seconds) { DebugRunUntil = GetWorld() ? GetWorld()->GetTimeSeconds() + Seconds : 0.f; }

	/** Server: revive after assembly. */
	void ReviveAt(const FVector& Location, bool bHeadInverted);

	/** Server: freeze at raid end. */
	void Freeze();

	bool IsCrawling() const;
	bool CanUseArms() const;
	bool CanMoveByInput() const;
	bool CanSelfLaunch() const;
	bool CanBeWound() const;
	bool CanContinueBeingWound() const;
	bool IsHelpless() const;
	bool IsDusty() const;
	bool IsViewInverted() const;
	bool IsInColdZone() const { return ColdMultiplier > 1.f; }
	FVector GetSplatNormal() const { return FVector(BodyRep.Param); }

	// ---- Torque coupling
	float GetDrainMultiplier() const;
	void RefreshMovementSpeed();
	void OnTorqueUpdated(float OldTorque, float NewTorque);

	// ---- Winding hooks (server, called by the wind component)
	void OnStartBeingWound(AWindupCharacter* Winder);
	void OnStopBeingWound();
	void OnStartWinding(AWindupCharacter* Target);
	void OnStopWinding();
	void NotifyKeyTurned();

	UFUNCTION(NetMulticast, Unreliable)
	void MulticastKeyFreeSpin();

	UFUNCTION(NetMulticast, Unreliable)
	void MulticastImpactFX(FVector_NetQuantize Location, FVector_NetQuantizeNormal Normal, float Speed);

	/** Local cosmetic: add spin to my key (degrees). */
	void AddKeyVisualSpin(float Degrees) { KeyVisualTarget += Degrees; }
	FVector GetKeyWorldLocation() const;

	// ---- Friend carry (piggyback)
	void UpdateFriendGrab();
	void BeginCarriedBy(AWindupCharacter* Carrier);
	void EndCarried(const FVector& ThrowVelocity);
	AWindupCharacter* GetCarriedBy() const { return CarriedBy; }
	AWindupCharacter* GetCarriedFriend() const;

	/** Server: drop everything, stop winding, leave carry. */
	void EndAllInteractions();

	// ---- Status effects (server)
	void ApplyDusty(float Duration);
	void ApplyHelpless(float Duration);

	/** Server: trap inside / release from the boss dust bin. */
	void EnterDustBin(AActor* Boss);
	void ExitDustBin(const FVector& EjectLocation, const FVector& EjectVelocity, bool bAutoEjected);

	// ---- Surfaces
	void RegisterSurfaceZone(ASurfaceZone* Zone, bool bEnter);
	float GetColdMultiplier() const { return ColdMultiplier; }

	// ---- Player look
	int32 GetPlayerColorIndex() const { return PlayerColorIndex; }
	FLinearColor GetPlayerColor() const;
	void SetPlayerColorIndex(int32 Index);

	// ---- Emotes
	void RequestEmote(uint8 EmoteId);
	uint8 GetActiveEmote() const;
	float GetEmoteTime() const;

	// ---- Animation support
	const FPoseSnapshot& GetGetUpSnapshot() const { return GetUpSnapshot; }
	float GetGetUpAlpha() const;
	float GetSquashAlpha() const;
	const FDollAnimSet& GetAnimSet() const { return AnimSet; }
	float GetKeyVisualAngle() const { return KeyVisualAngle; }
	FVector2D GetLocalMoveInput() const { return MoveInput; }

	// ---- Blueprint configured assets
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Doll|Assets")
	FDollAnimSet AnimSet;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Doll|Assets")
	TSubclassOf<ABodyPartPickup> BodyPartClass;

	/** Material slot on the doll mesh that receives the player paint color. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Doll|Assets")
	int32 PaintMaterialIndex = 0;

	/** Physics bodies driven by physical animation (semi ragdoll). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Doll|Physics")
	TArray<FName> SemiRagdollBones;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Doll|Physics")
	FName PelvisBone = TEXT("pelvis");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Doll|Physics")
	FName KeySocket = TEXT("key");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Doll|Physics")
	bool bEnableSemiRagdoll = true;

	/** Approach speed into a wall that knocks the doll over, per band (sprint / overwound). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Doll|Physics")
	float SprintTripSpeed = 420.f;

protected:
	// Input
	void Input_Move(const FInputActionValue& Value);
	void Input_MoveCompleted(const FInputActionValue& Value);
	void Input_LookMouse(const FInputActionValue& Value);
	void Input_LookStick(const FInputActionValue& Value);
	void Input_LookStickCompleted(const FInputActionValue& Value);
	void Input_JumpStarted(const FInputActionValue& Value);
	void Input_JumpCompleted(const FInputActionValue& Value);
	void Input_GrabLeftStarted(const FInputActionValue& Value);
	void Input_GrabLeftCompleted(const FInputActionValue& Value);
	void Input_GrabRightStarted(const FInputActionValue& Value);
	void Input_GrabRightCompleted(const FInputActionValue& Value);
	void Input_HoldWindStarted(const FInputActionValue& Value);
	void Input_HoldWindCompleted(const FInputActionValue& Value);
	void Input_SelfLaunch(const FInputActionValue& Value);
	void Input_Emote1(const FInputActionValue& Value) { RequestEmote(1); }
	void Input_Emote2(const FInputActionValue& Value) { RequestEmote(2); }
	void Input_Emote3(const FInputActionValue& Value) { RequestEmote(3); }
	void Input_Emote4(const FInputActionValue& Value) { RequestEmote(4); }

	UFUNCTION(Server, Reliable)
	void ServerEmote(uint8 EmoteId);

	UFUNCTION(Server, Reliable)
	void ServerStruggle();

	UFUNCTION()
	void OnRep_BodyRep(const FBodyStateRep& OldRep);

	UFUNCTION()
	void OnRep_PlayerColor();

	UFUNCTION()
	void OnRep_CarriedBy();

	UFUNCTION()
	void OnRep_KeyTurns();

	/** Apply the visual / physical setup of a body state on this machine. */
	void ApplyBodyState(ECrankBodyState OldState, ECrankBodyState NewState);
	void EnterRagdollLocal(const FVector& Impulse);
	void ExitRagdollLocal(bool bSnapshot);
	void ApplySemiRagdoll();
	void UpdateRagdollServer(float DeltaSeconds);
	void UpdateRagdollClient(float DeltaSeconds);
	void UpdateCosmetics(float DeltaSeconds);
	void UpdateSurface();
	void UpdateCrawlCrouch();
	void ReconcileHeldInputs();
	void UpdateCameraFeedback(float DeltaSeconds);
	void UpdateStatusServer(float DeltaSeconds);
	void ApplyPaint();
	void TickSplat(float DeltaSeconds);
	FVector ComputeStandLocationFromPelvis(FRotator& OutRotation) const;

	/** Velocity minus whatever pushes into the floor / wall the capsule is touching (a ragdoll starting with it tunnels). */
	FVector RemoveVelocityIntoSurfaces(const FVector& Velocity) const;
	/** Is there any floor under this point (false = fell through the world)? */
	bool HasFloorBelow(const FVector& Point, float Depth) const;
	/** Does something block the way in Dir above the step height (a wall / crate, not a lip the feet climb)? */
	bool IsBlockedAboveStep(const FVector& Dir) const;
	/** Server: remember where the doll last stood on solid ground. */
	void UpdateSafeLocation(float DeltaSeconds);

	// Components
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	TObjectPtr<USpringArmComponent> CameraBoom;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	TObjectPtr<UCameraComponent> FollowCamera;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Doll")
	TObjectPtr<UTorqueComponent> Torque;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Doll")
	TObjectPtr<UWindInteractionComponent> Wind;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Doll")
	TObjectPtr<ULaunchComponent> LaunchComp;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Doll")
	TObjectPtr<UGrabComponent> Grab;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Doll")
	TObjectPtr<UScatterReviveComponent> Scatter;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Doll")
	TObjectPtr<UPhysicalAnimationComponent> PhysicalAnimation;

	/** The windup key on the back (static mesh, spun in code). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Doll")
	TObjectPtr<UStaticMeshComponent> KeyMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Doll|FX")
	TObjectPtr<UCrankFXEmitterComponent> SteamFX;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Doll|FX")
	TObjectPtr<UCrankFXEmitterComponent> DustFX;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Doll|FX")
	TObjectPtr<UCrankFXEmitterComponent> StarsFX;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Doll|FX")
	TObjectPtr<UCrankFXEmitterComponent> FrostFX;

	// Replicated state
	UPROPERTY(ReplicatedUsing = OnRep_BodyRep)
	FBodyStateRep BodyRep;

	UPROPERTY(ReplicatedUsing = OnRep_PlayerColor)
	int32 PlayerColorIndex = 0;

	UPROPERTY(Replicated)
	FEmoteRep EmoteRep;

	UPROPERTY(ReplicatedUsing = OnRep_CarriedBy)
	TObjectPtr<AWindupCharacter> CarriedBy;

	UPROPERTY(ReplicatedUsing = OnRep_KeyTurns)
	uint16 KeyTurns = 0;

	/** Server world times. */
	UPROPERTY(Replicated)
	float DustyUntil = 0.f;

	UPROPERTY(Replicated)
	float HelplessUntil = 0.f;

	UPROPERTY(Replicated)
	float InvertedUntil = 0.f;

	// Local bookkeeping
	float BodyStateLocalTime = 0.f;
	float RagdollMinTime = 1.f;
	float LastTripTime = -100.f;
	// A sprint collision only trips the doll if the next frame shows it really stopped (not stepped over / slid past).
	float PendingTripTime = -1.f;
	float DebugRunUntil = 0.f;
	float PendingTripSpeed = 0.f;
	FVector PendingTripNormal = FVector::ZeroVector;
	FVector PendingTripPoint = FVector::ZeroVector;
	FVector PreMoveVelocity = FVector::ZeroVector;	// server: velocity of the last finished move (hits come after the next one)
	float EmoteLocalStart = 0.f;
	uint8 LastEmoteSeq = 0;
	FVector2D MoveInput = FVector2D::ZeroVector;
	float MoveInputTime = 0.f;
	float StruggleTimer = 0.f;
	float TugCooldown = 0.f;
	FPoseSnapshot GetUpSnapshot;
	float GetUpStartTime = -100.f;
	FVector LastSafeFloor = FVector::ZeroVector;	// floor point under the capsule, last time it stood on ground
	bool bHasSafeFloor = false;
	float SafeFloorTimer = 0.f;
	bool bSkipGetUpBlend = false;
	float KeyVisualAngle = 0.f;
	float KeyVisualTarget = 0.f;
	float KeyFreeSpinUntil = 0.f;
	float TickSoundTimer = 0.f;
	float FootstepDistance = 0.f;
	float CreakTimer = 0.f;
	float WarnTimer = 0.f;
	float ColdMultiplier = 1.f;
	float SurfaceFriction = 8.f;
	float SurfaceSpeedBoost = 1.f;
	float CameraRoll = 0.f;
	float DefaultArmLength = 380.f;
	FVector KeyBackAxisLocal = FVector(-1.f, 0.f, 0.f);
	bool bKeyAxisCached = false;
	int32 SemiRagdollSignature = -1;
	FTransform MeshDefaultRelative;
	TWeakObjectPtr<AActor> TrappedIn;

	UPROPERTY(Transient)
	TArray<TObjectPtr<ASurfaceZone>> ActiveZones;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> PaintMID;
};
