// Lightweight mesh-particle system for the low-poly tin toy look.
// Particles are instanced static meshes (puffs, bolts, springs, stars, notes) simulated on the
// game thread. Purely cosmetic and local: callers trigger effects on every machine that should see them.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Components/SceneComponent.h"
#include "CrankFX.generated.h"

class UInstancedStaticMeshComponent;
class UStaticMesh;
class UMaterialInterface;

UENUM(BlueprintType)
enum class ECrankFX : uint8
{
	Puff,
	Steam,
	DustBurst,
	DustTrail,
	Pop,
	Sparks,
	StunStars,
	Splash,
	WindClick,
	RhythmGood,
	Notes,
	Confetti,
	ColdMist,
	Slip,
	Land,
	BossSmoke,
	CellZap,
	Deliver,
	Footstep,
	Suction,
};

struct FCrankParticle
{
	FVector Pos = FVector::ZeroVector;
	FVector Vel = FVector::ZeroVector;
	FQuat Rot = FQuat::Identity;
	FVector AngVel = FVector::ZeroVector;	// axis * rad/s
	float Age = 0.f;
	float Life = 1.f;
	float Size0 = 1.f;
	float Size1 = 1.f;
	FLinearColor Color0 = FLinearColor::White;
	FLinearColor Color1 = FLinearColor::White;
	float Gravity = 0.f;		// uu/s^2 (positive = down)
	float Drag = 0.f;
	FVector Stretch = FVector::OneVector;

	// optional orbit around a followed component
	TWeakObjectPtr<USceneComponent> Follow;
	FVector FollowOffset = FVector::ZeroVector;
	float OrbitRadius = 0.f;
	float OrbitSpeed = 0.f;
	float OrbitPhase = 0.f;
};

struct FCrankParticlePool
{
	TObjectPtr<UInstancedStaticMeshComponent> ISM = nullptr;
	TArray<FCrankParticle> Particles;	// slot per instance
	TArray<int32> FreeSlots;
	TArray<FTransform> Transforms;
	TArray<float> CustomData;
	int32 Capacity = 0;
	int32 AliveCount = 0;
};

UCLASS()
class CRANK_API UCrankFXSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	/** Spawn a preset effect. No-op on dedicated servers. */
	static void Spawn(const UObject* WorldContext, ECrankFX Type, const FVector& Location, const FRotator& Rotation = FRotator::ZeroRotator, float Scale = 1.f, FLinearColor Tint = FLinearColor::White, USceneComponent* Follow = nullptr);

	void SpawnPreset(ECrankFX Type, const FVector& Location, const FRotator& Rotation, float Scale, FLinearColor Tint, USceneComponent* Follow);

	/** Add a single particle to the pool identified by mesh + material. */
	void AddParticle(const TCHAR* MeshPath, const TCHAR* MaterialPath, const FCrankParticle& Particle);

	// UTickableWorldSubsystem
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;
	virtual void Deinitialize() override;

private:
	FCrankParticlePool* GetPool(const TCHAR* MeshPath, const TCHAR* MaterialPath);
	AActor* GetHost();

	TMap<FString, FCrankParticlePool> Pools;

	UPROPERTY(Transient)
	TObjectPtr<AActor> Host;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UInstancedStaticMeshComponent>> PoolComponents;
};

/** Continuous emitter that can be attached to bones / components (steam, dust trail, notes, cold mist). */
UCLASS(ClassGroup = (Crank), meta = (BlueprintSpawnableComponent))
class CRANK_API UCrankFXEmitterComponent : public USceneComponent
{
	GENERATED_BODY()

public:
	UCrankFXEmitterComponent();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FX")
	ECrankFX Effect = ECrankFX::Puff;

	/** Bursts per second. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FX")
	float Rate = 4.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FX")
	float EffectScale = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FX")
	FLinearColor Tint = FLinearColor::White;

	/** Random spawn offset within this box extent (local space). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FX")
	FVector SpawnExtent = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FX")
	bool bEmitting = false;

	UFUNCTION(BlueprintCallable, Category = "FX")
	void SetEmitting(bool bNewEmitting) { bEmitting = bNewEmitting; }

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	float Accumulator = 0.f;
};
