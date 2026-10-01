// "짝짝이 대왕" - a giant wind-up cymbal monkey (zone F final boss, model generated with Tripo AI).
// Launched dolls always hurt it (100 HP); the big wind-up key on its back hurts a lot, most of all while its spring
// has run down.
// Attacks: cymbal clap shockwaves (jump over the ring), hop stomps, a boomerang cymbal throw (phase 2+).
// Server-authoritative state machine; every machine picks the keyframed clip (Blender-authored, see
// ArtSource/Blender/giant_anims.py) from the replicated state + start time (UGiantAnimInstance), so the 9 m rig
// stays in sync without replicating bones.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CrankTypes.h"
#include "BossGiantMonkey.generated.h"

class UCapsuleComponent;
class USkeletalMeshComponent;
class UAnimSequence;
class UStaticMeshComponent;
class UBoxComponent;
class USphereComponent;
class UPointLightComponent;
class UAudioComponent;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class UStaticMesh;
class UCrankFXEmitterComponent;
class AWindupCharacter;
class ACarryableActor;
class ACymbalProjectile;
class AGiantArena;

/** Keyframed clips of the giant (assigned in BP_Boss_GiantMonkey). All in place: the actor moves itself. */
USTRUCT(BlueprintType)
struct FGiantAnimSet
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly) TObjectPtr<UAnimSequence> Idle;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) TObjectPtr<UAnimSequence> Walk;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) TObjectPtr<UAnimSequence> Dormant;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) TObjectPtr<UAnimSequence> ClapWindup;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) TObjectPtr<UAnimSequence> Clap;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) TObjectPtr<UAnimSequence> HopWindup;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) TObjectPtr<UAnimSequence> HopAir;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) TObjectPtr<UAnimSequence> Land;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) TObjectPtr<UAnimSequence> ThrowWindupL;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) TObjectPtr<UAnimSequence> ThrowL;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) TObjectPtr<UAnimSequence> ThrowWindupR;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) TObjectPtr<UAnimSequence> ThrowR;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) TObjectPtr<UAnimSequence> WindDownEnter;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) TObjectPtr<UAnimSequence> WoundDown;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) TObjectPtr<UAnimSequence> Rewind;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) TObjectPtr<UAnimSequence> HitReact;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) TObjectPtr<UAnimSequence> Stagger;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) TObjectPtr<UAnimSequence> PhaseRoar;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) TObjectPtr<UAnimSequence> Defeat;
};

UCLASS(Blueprintable)
class CRANK_API ABossGiantMonkey : public AActor
{
	GENERATED_BODY()

public:
	ABossGiantMonkey();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	/** Server: a launched doll slammed into the boss. */
	void HandleDollImpact(AWindupCharacter* Doll, const FVector& Velocity, const FHitResult& Hit);

	/** Server: start the fight (arena entered / cheat). */
	void Activate();

	/** Server: the thrown cymbal came back. */
	void OnCymbalReturned();

	/** Server: GiantWake, GiantDown, GiantPhase n, GiantHit, GiantKill. */
	void DebugCommand(FName Command, float Value);

	EGiantState GetGiantState() const { return GiantState; }
	int32 GetPhase() const { return Phase; }
	int32 GetHP() const { return HP; }
	bool IsActive() const { return GiantState != EGiantState::Dormant && GiantState != EGiantState::Defeated; }
	bool IsKeyExposed() const { return GiantState == EGiantState::WoundDown; }
	float GetStateTime() const;
	float GetStateDuration() const { return StateDuration; }
	FVector GetKeyWorldLocation() const;
	float BossNow() const;

	/** Where the cymbal of that hand sits (the CymbalL / CymbalR bone, the hand when the rig has none). */
	FVector GetCymbalLocation(bool bLeft) const;

	// ---- Animation inputs (UGiantAnimInstance)
	const FGiantAnimSet& GetAnimSet() const { return AnimSet; }
	bool IsThrowLeft() const { return bThrowLeft; }
	bool IsCymbalOut() const { return bCymbalOut; }
	float GetLastKeyHitTime() const { return LastKeyHitTime; }
	float GetLastHitTime() const { return LastHitTime; }

	/** Damage numbers floating up from recent hits (cosmetic, every machine). */
	struct FDamagePopup
	{
		FVector Location = FVector::ZeroVector;
		int32 Amount = 0;
		float Time = 0.f;	// local world time
		bool bKey = false;
	};
	const TArray<FDamagePopup>& GetDamagePopups() const { return DamagePopups; }
	float GetWalkSpeed() const { return WalkSpeed; }
	int32 GetSubCount() const { return SubCount; }
	const AWindupCharacter* GetLookTarget() const { return LookTarget; }

	// ---- Wiring
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Giant")
	TObjectPtr<AGiantArena> Arena;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Giant")
	TSubclassOf<ACymbalProjectile> CymbalClass;

	/** Dropped when defeated (the golden key). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Giant")
	TSubclassOf<ACarryableActor> RewardClass;

	/** Expanding shockwave ring visual (flat torus) and its material (needs a "Color" vector parameter). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Giant")
	TObjectPtr<UStaticMesh> RingMesh;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Giant")
	TObjectPtr<UMaterialInterface> RingMaterial;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Giant|Anim")
	FGiantAnimSet AnimSet;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Giant")
	float TurnRate = 55.f;

	/** Waddle speed toward a far target between attacks (phase 1; +25 per phase). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Giant")
	float WalkSpeedBase = 150.f;

	/** Half height of the body capsule (actor origin = capsule center). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Giant")
	float HalfHeight = 450.f;

	// ---- Rig (Tripo / Mixamo bone names; the mesh faces +Y in component space)
	UPROPERTY(EditDefaultsOnly, Category = "Giant|Rig") FName BoneSpineHigh = TEXT("Spine2");
	UPROPERTY(EditDefaultsOnly, Category = "Giant|Rig") FName BoneHead = TEXT("Head");
	UPROPERTY(EditDefaultsOnly, Category = "Giant|Rig") FName BoneHandL = TEXT("LeftHand");
	UPROPERTY(EditDefaultsOnly, Category = "Giant|Rig") FName BoneHandR = TEXT("RightHand");
	/** Bones carrying only the cymbal discs (scaled away while that cymbal flies). */
	UPROPERTY(EditDefaultsOnly, Category = "Giant|Rig") FName BoneCymbalL = TEXT("CymbalL");
	UPROPERTY(EditDefaultsOnly, Category = "Giant|Rig") FName BoneCymbalR = TEXT("CymbalR");

protected:
	// ---- Server
	void SetGiantState(EGiantState NewState, float Duration);
	void TickServer(float DeltaSeconds);
	void TickRings(float DeltaSeconds);
	void ChooseNextAttack();
	AWindupCharacter* FindTarget() const;
	void TurnToward(const FVector& Location, float DeltaSeconds);
	void StartRing(const FVector& Origin, float MaxRadius, float Speed, float Strength);
	void Stomp(const FVector& Origin, float Radius);
	/** False while that doll is still in its post-knock grace. */
	bool Knock(AWindupCharacter* Doll, const FVector& From, float Strength);
	/** Server: HP loss from a launched doll (or a cheat); phase changes / defeat follow from the new HP. */
	void ApplyDamage(int32 Amount, bool bKey, const FVector& Point);
	void ThrowCymbal();
	void Defeat();
	FVector ClampToArena(const FVector& Location) const;
	FVector GetFloorLocation() const;
	void SpawnReward();

	UFUNCTION()
	void OnRep_GiantState();

	UFUNCTION(NetMulticast, Reliable)
	void MulticastGiantEvent(uint8 EventId, FVector_NetQuantize Location, float Param);

	// ---- Every machine
	void UpdateKey(float DeltaSeconds);
	void UpdateRingVisuals(float DeltaSeconds);
	void UpdateFeedback(float DeltaSeconds);
	/** Standing body vs. the lying body once it has fallen. */
	void UpdateCollision();
	FVector EvalLocation() const;

	// ---- Components
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Giant")
	TObjectPtr<UCapsuleComponent> BodyCollision;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Giant")
	TObjectPtr<USkeletalMeshComponent> Mesh;

	/** Collision of the defeated giant lying on its back (enabled once it has hit the floor). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Giant")
	TObjectPtr<UCapsuleComponent> FallenCollision;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Giant")
	TObjectPtr<USceneComponent> KeyPivot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Giant")
	TObjectPtr<UStaticMeshComponent> KeyMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Giant")
	TObjectPtr<UBoxComponent> KeyHitBox;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Giant")
	TObjectPtr<UPointLightComponent> KeyLight;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Giant")
	TObjectPtr<USphereComponent> HeadHitSphere;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Giant")
	TObjectPtr<UCrankFXEmitterComponent> StarsFX;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Giant")
	TObjectPtr<UCrankFXEmitterComponent> SteamFX;

	/** Pool of expanding shockwave rings (cosmetic, every machine; created at BeginPlay from RingMesh). */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> RingMeshes;

	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> TickAudio;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> KeyMID;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> BodyMID;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInstanceDynamic>> RingMIDs;

	// ---- Replicated state
	UPROPERTY(ReplicatedUsing = OnRep_GiantState)
	EGiantState GiantState = EGiantState::Dormant;

	UPROPERTY(Replicated)
	float StateStartTime = 0.f;	// server world time

	UPROPERTY(Replicated)
	float StateDuration = 0.f;

	UPROPERTY(Replicated)
	int32 Phase = 1;

	UPROPERTY(Replicated)
	int32 HP = 100;

	/** Claps / hops already done inside the current attack. */
	UPROPERTY(Replicated)
	int32 SubCount = 0;

	UPROPERTY(Replicated)
	FVector_NetQuantize HopFrom;

	UPROPERTY(Replicated)
	FVector_NetQuantize HopTo;

	UPROPERTY(Replicated)
	FVector_NetQuantize ServerLocation;

	UPROPERTY(Replicated)
	float ServerYaw = 0.f;

	UPROPERTY(Replicated)
	float LastKeyHitTime = -100.f;

	/** Any damaging hit (drives the flinch layer and the body flash). */
	UPROPERTY(Replicated)
	float LastHitTime = -100.f;

	UPROPERTY(Replicated)
	bool bCymbalOut = false;

	/** Current waddle speed (0 = standing), drives the walk clip. */
	UPROPERTY(Replicated)
	float WalkSpeed = 0.f;

	UPROPERTY(Replicated)
	bool bThrowLeft = false;

	UPROPERTY(Replicated)
	TObjectPtr<AWindupCharacter> LookTarget;

	// ---- Server bookkeeping
	struct FServerRing
	{
		FVector Origin = FVector::ZeroVector;
		float Start = 0.f;
		float MaxRadius = 0.f;
		float Speed = 0.f;
		float Strength = 1.f;
		TArray<TWeakObjectPtr<AWindupCharacter>> AlreadyHit;
	};
	TArray<FServerRing> Rings;
	TWeakObjectPtr<AWindupCharacter> Target;
	TMap<TWeakObjectPtr<AWindupCharacter>, float> LastKnockTime;
	TMap<TWeakObjectPtr<AWindupCharacter>, float> LastDollHitTime;
	int32 CymbalsOut = 0;
	int32 AttackCount = 0;
	EGiantState LastAttack = EGiantState::Idle;
	int32 SameAttackStreak = 0;
	float ClapDoneAt = -1.f;	// clap impact time inside the Clap state
	bool bLanded = false;
	bool bRewardSpawned = false;
	bool bPhaseStompDone = false;

	// ---- Local visuals
	struct FRingVisual
	{
		FVector Origin = FVector::ZeroVector;
		float Start = 0.f;	// local world time
		float MaxRadius = 0.f;
		float Speed = 0.f;
	};
	TArray<FRingVisual> RingVisuals;
	float VisualYaw = 0.f;
	FVector VisualLocation = FVector::ZeroVector;
	float LocalStateEnter = 0.f;
	float LastRingPuffTime = 0.f;
	float RingMeshRadius = 45.f;
	bool bFallenCollision = false;
	TArray<FDamagePopup> DamagePopups;
};
