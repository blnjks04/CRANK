#include "FX/CrankFX.h"
#include "CrankAssets.h"
#include "CrankTypes.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "GameFramework/Actor.h"

namespace
{
	constexpr int32 PoolCapacity = 384;

	FVector RandCone(const FVector& Dir, float HalfAngleDeg)
	{
		return FMath::VRandCone(Dir.GetSafeNormal(), FMath::DegreesToRadians(HalfAngleDeg));
	}

	FVector RandHemisphereUp()
	{
		FVector V = FMath::VRand();
		V.Z = FMath::Abs(V.Z);
		return V;
	}

	FLinearColor WithAlpha(const FLinearColor& C, float A)
	{
		return FLinearColor(C.R, C.G, C.B, A);
	}

	float Rand(float Min, float Max) { return FMath::FRandRange(Min, Max); }
}

// --------------------------------------------------------------------------------------------
// Subsystem

bool UCrankFXSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

TStatId UCrankFXSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UCrankFXSubsystem, STATGROUP_Tickables);
}

void UCrankFXSubsystem::Deinitialize()
{
	Pools.Empty();
	PoolComponents.Empty();
	Host = nullptr;
	Super::Deinitialize();
}

AActor* UCrankFXSubsystem::GetHost()
{
	if (!Host)
	{
		UWorld* World = GetWorld();
		FActorSpawnParameters Params;
		Params.ObjectFlags |= RF_Transient;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Host = World->SpawnActor<AActor>(AActor::StaticClass(), FTransform::Identity, Params);
		if (Host)
		{
			USceneComponent* Root = NewObject<USceneComponent>(Host, TEXT("Root"));
			Host->SetRootComponent(Root);
			Root->RegisterComponent();
#if WITH_EDITOR
			Host->SetActorLabel(TEXT("CrankFXHost"));
#endif
		}
	}
	return Host;
}

FCrankParticlePool* UCrankFXSubsystem::GetPool(const TCHAR* MeshPath, const TCHAR* MaterialPath)
{
	const FString Key = FString(MeshPath) + TEXT("|") + FString(MaterialPath);
	if (FCrankParticlePool* Existing = Pools.Find(Key))
	{
		return Existing;
	}

	AActor* HostActor = GetHost();
	if (!HostActor)
	{
		return nullptr;
	}

	// Fallback meshes keep effects visible even before the art pass.
	const TCHAR* Fallback = CrankPaths::Sphere;
	if (FCString::Strcmp(MeshPath, CrankPaths::FXBolt) == 0 || FCString::Strcmp(MeshPath, CrankPaths::FXNote) == 0)
	{
		Fallback = CrankPaths::Cube;
	}
	else if (FCString::Strcmp(MeshPath, CrankPaths::FXSpring) == 0)
	{
		Fallback = CrankPaths::Cylinder;
	}
	UStaticMesh* Mesh = CrankAssets::Mesh(MeshPath, Fallback);
	UMaterialInterface* Material = CrankAssets::Material(MaterialPath, true);
	if (!Mesh)
	{
		return nullptr;
	}

	UInstancedStaticMeshComponent* ISM = NewObject<UInstancedStaticMeshComponent>(HostActor);
	ISM->SetStaticMesh(Mesh);
	if (Material)
	{
		for (int32 i = 0; i < FMath::Max(1, Mesh->GetStaticMaterials().Num()); ++i)
		{
			ISM->SetMaterial(i, Material);
		}
	}
	ISM->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ISM->SetCastShadow(false);
	ISM->SetMobility(EComponentMobility::Movable);
	ISM->NumCustomDataFloats = 4;
	ISM->SetupAttachment(HostActor->GetRootComponent());
	ISM->RegisterComponent();
	PoolComponents.Add(ISM);

	FCrankParticlePool& Pool = Pools.Add(Key);
	Pool.ISM = ISM;
	Pool.Capacity = PoolCapacity;
	Pool.Particles.SetNum(PoolCapacity);
	Pool.Transforms.Init(FTransform(FQuat::Identity, FVector(0, 0, -100000.f), FVector::ZeroVector), PoolCapacity);
	Pool.CustomData.Init(0.f, PoolCapacity * 4);
	Pool.FreeSlots.Reserve(PoolCapacity);
	for (int32 i = PoolCapacity - 1; i >= 0; --i)
	{
		Pool.FreeSlots.Add(i);
		Pool.Particles[i].Life = -1.f;	// dead marker
	}
	ISM->AddInstances(Pool.Transforms, false, true, false);
	return &Pool;
}

void UCrankFXSubsystem::AddParticle(const TCHAR* MeshPath, const TCHAR* MaterialPath, const FCrankParticle& Particle)
{
	FCrankParticlePool* Pool = GetPool(MeshPath, MaterialPath);
	if (!Pool || Pool->FreeSlots.Num() == 0)
	{
		return;
	}
	const int32 Slot = Pool->FreeSlots.Pop(EAllowShrinking::No);
	Pool->Particles[Slot] = Particle;
	Pool->Particles[Slot].Age = 0.f;
	Pool->AliveCount++;
}

void UCrankFXSubsystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	const float Dt = FMath::Min(DeltaTime, 0.05f);
	for (TPair<FString, FCrankParticlePool>& Pair : Pools)
	{
		FCrankParticlePool& Pool = Pair.Value;
		if (!Pool.ISM)
		{
			continue;
		}
		if (Pool.AliveCount == 0 && Pool.ISM->GetVisibleFlag() == false)
		{
			continue;
		}

		bool bAnyAlive = false;
		for (int32 i = 0; i < Pool.Capacity; ++i)
		{
			FCrankParticle& P = Pool.Particles[i];
			if (P.Life < 0.f)
			{
				Pool.Transforms[i].SetScale3D(FVector::ZeroVector);
				continue;
			}

			P.Age += Dt;
			if (P.Age >= P.Life)
			{
				P.Life = -1.f;
				Pool.FreeSlots.Add(i);
				Pool.AliveCount--;
				Pool.Transforms[i].SetScale3D(FVector::ZeroVector);
				continue;
			}
			bAnyAlive = true;

			if (USceneComponent* Follow = P.Follow.Get())
			{
				const float Angle = P.OrbitPhase + P.OrbitSpeed * P.Age;
				const FVector Orbit(FMath::Cos(Angle) * P.OrbitRadius, FMath::Sin(Angle) * P.OrbitRadius, 0.f);
				P.Pos = Follow->GetComponentLocation() + P.FollowOffset + Orbit;
			}
			else
			{
				P.Vel.Z -= P.Gravity * Dt;
				P.Vel *= FMath::Max(0.f, 1.f - P.Drag * Dt);
				P.Pos += P.Vel * Dt;
			}

			const float AngSpeed = P.AngVel.Size();
			if (AngSpeed > KINDA_SMALL_NUMBER)
			{
				P.Rot = FQuat(P.AngVel / AngSpeed, AngSpeed * Dt) * P.Rot;
			}

			const float T = P.Age / P.Life;
			const float PopIn = FMath::Clamp(P.Age / 0.06f, 0.f, 1.f);
			const float Size = FMath::Lerp(P.Size0, P.Size1, 1.f - FMath::Square(1.f - T)) * PopIn;
			const FLinearColor Color = FMath::Lerp(P.Color0, P.Color1, T);

			Pool.Transforms[i] = FTransform(P.Rot, P.Pos, P.Stretch * Size);
			Pool.CustomData[i * 4 + 0] = Color.R;
			Pool.CustomData[i * 4 + 1] = Color.G;
			Pool.CustomData[i * 4 + 2] = Color.B;
			Pool.CustomData[i * 4 + 3] = Color.A;
		}

		Pool.ISM->BatchUpdateInstancesTransforms(0, Pool.Transforms, true, false, true);
		Pool.ISM->SetCustomData(0, Pool.Capacity - 1, Pool.CustomData, false);
		Pool.ISM->MarkRenderStateDirty();
		Pool.ISM->SetVisibility(bAnyAlive);
	}
}

void UCrankFXSubsystem::Spawn(const UObject* WorldContext, ECrankFX Type, const FVector& Location, const FRotator& Rotation, float Scale, FLinearColor Tint, USceneComponent* Follow)
{
	UWorld* World = WorldContext ? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull) : nullptr;
	if (!World || World->GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	if (UCrankFXSubsystem* FX = World->GetSubsystem<UCrankFXSubsystem>())
	{
		FX->SpawnPreset(Type, Location, Rotation, Scale, Tint, Follow);
	}
}

void UCrankFXSubsystem::SpawnPreset(ECrankFX Type, const FVector& Loc, const FRotator& Rot, float S, FLinearColor Tint, USceneComponent* Follow)
{
	const FVector Fwd = Rot.Vector();
	const FLinearColor Dust(0.62f, 0.56f, 0.48f, 1.f);

	auto Puff = [&](const FVector& Pos, const FVector& Vel, float Life, float Size0, float Size1, const FLinearColor& C, float A0, float Gravity, float Drag)
	{
		FCrankParticle P;
		P.Pos = Pos; P.Vel = Vel; P.Life = Life;
		P.Size0 = Size0; P.Size1 = Size1;
		P.Color0 = WithAlpha(C, A0); P.Color1 = WithAlpha(C, 0.f);
		P.Gravity = Gravity; P.Drag = Drag;
		P.Rot = FQuat(FVector::UpVector, Rand(0.f, PI * 2.f));
		P.AngVel = FVector(0, 0, Rand(-1.5f, 1.5f));
		AddParticle(CrankPaths::FXPuff, CrankPaths::MatFXPuff, P);
	};

	auto Bit = [&](const TCHAR* Mesh, const TCHAR* Mat, const FVector& Pos, const FVector& Vel, float Life, float Size, const FLinearColor& C, float Gravity)
	{
		FCrankParticle P;
		P.Pos = Pos; P.Vel = Vel; P.Life = Life;
		P.Size0 = Size; P.Size1 = Size * 0.6f;
		P.Color0 = C; P.Color1 = C;
		P.Gravity = Gravity; P.Drag = 0.4f;
		P.Rot = FQuat(FMath::VRand(), Rand(0.f, PI));
		P.AngVel = FMath::VRand() * Rand(6.f, 16.f);
		AddParticle(Mesh, Mat, P);
	};

	auto Spark = [&](const FVector& Pos, const FVector& Vel, float Life, float Size, const FLinearColor& C, float Gravity)
	{
		FCrankParticle P;
		P.Pos = Pos; P.Vel = Vel; P.Life = Life;
		P.Size0 = Size; P.Size1 = Size * 0.3f;
		P.Color0 = C; P.Color1 = WithAlpha(C * 0.5f, 1.f);
		P.Gravity = Gravity; P.Drag = 1.f;
		P.Rot = FRotationMatrix::MakeFromX(Vel.GetSafeNormal()).ToQuat();
		P.Stretch = FVector(4.f, 1.f, 1.f);
		AddParticle(CrankPaths::FXDot, CrankPaths::MatFXUnlit, P);
	};

	switch (Type)
	{
	case ECrankFX::Puff:
		for (int32 i = 0; i < 6; ++i)
		{
			Puff(Loc + FMath::VRand() * 8.f * S, RandHemisphereUp() * Rand(60.f, 140.f) * S, Rand(0.5f, 0.9f), 0.10f * S, 0.28f * S, Tint * Dust, 0.6f, -20.f, 2.f);
		}
		break;

	case ECrankFX::Steam:
		for (int32 i = 0; i < 2; ++i)
		{
			Puff(Loc, RandCone(Fwd, 20.f) * Rand(150.f, 260.f) * S, Rand(0.5f, 0.9f), 0.05f * S, 0.22f * S, FLinearColor(0.95f, 0.97f, 1.f), 0.55f, -80.f, 3.f);
		}
		break;

	case ECrankFX::DustBurst:
		for (int32 i = 0; i < 18; ++i)
		{
			Puff(Loc + FMath::VRand() * 20.f * S, RandHemisphereUp() * Rand(150.f, 380.f) * S, Rand(0.8f, 1.4f), 0.18f * S, 0.5f * S, Tint * FLinearColor(0.5f, 0.43f, 0.35f), 0.85f, -30.f, 3.f);
		}
		break;

	case ECrankFX::DustTrail:
		for (int32 i = 0; i < 2; ++i)
		{
			Puff(Loc + FVector(Rand(-6, 6), Rand(-6, 6), 0), RandHemisphereUp() * Rand(20.f, 45.f), Rand(0.5f, 0.8f), 0.05f * S, 0.14f * S, FLinearColor(0.5f, 0.43f, 0.35f), 0.55f, -10.f, 2.f);
		}
		break;

	case ECrankFX::Pop:
	{
		FCrankParticle Ring;
		Ring.Pos = Loc; Ring.Life = 0.35f; Ring.Size0 = 0.2f * S; Ring.Size1 = 2.6f * S;
		Ring.Color0 = FLinearColor(1.f, 0.95f, 0.8f, 0.9f); Ring.Color1 = FLinearColor(1.f, 0.9f, 0.7f, 0.f);
		AddParticle(CrankPaths::FXRing, CrankPaths::MatFXPuff, Ring);
		for (int32 i = 0; i < 14; ++i)
		{
			Puff(Loc + FMath::VRand() * 15.f, FMath::VRand() * Rand(200.f, 420.f) * S, Rand(0.6f, 1.1f), 0.2f * S, 0.55f * S, FLinearColor(0.92f, 0.9f, 0.86f), 0.85f, -40.f, 4.f);
		}
		for (int32 i = 0; i < 10; ++i)
		{
			Bit(CrankPaths::FXBolt, CrankPaths::MatFXLit, Loc, RandCone(FVector::UpVector, 70.f) * Rand(300.f, 700.f) * S, Rand(1.2f, 2.0f), 0.07f * S, FLinearColor(0.7f, 0.7f, 0.72f), 980.f);
		}
		for (int32 i = 0; i < 6; ++i)
		{
			Bit(CrankPaths::FXSpring, CrankPaths::MatFXLit, Loc, RandCone(FVector::UpVector, 60.f) * Rand(350.f, 750.f) * S, Rand(1.2f, 2.0f), 0.09f * S, FLinearColor(0.85f, 0.65f, 0.2f), 980.f);
		}
		for (int32 i = 0; i < 14; ++i)
		{
			Spark(Loc, FMath::VRand() * Rand(400.f, 900.f), Rand(0.25f, 0.45f), 0.06f, FLinearColor(8.f, 4.f, 0.8f, 1.f), 600.f);
		}
		break;
	}

	case ECrankFX::Sparks:
		for (int32 i = 0; i < 10; ++i)
		{
			Spark(Loc, RandCone(Fwd, 60.f) * Rand(250.f, 600.f) * S, Rand(0.2f, 0.4f), 0.05f * S, FLinearColor(8.f, 4.5f, 1.f, 1.f), 900.f);
		}
		break;

	case ECrankFX::StunStars:
		for (int32 i = 0; i < 3; ++i)
		{
			FCrankParticle P;
			P.Pos = Loc;
			P.Life = 1.0f;
			P.Size0 = 0.12f * S; P.Size1 = 0.12f * S;
			P.Color0 = FLinearColor(6.f, 4.8f, 0.6f, 1.f); P.Color1 = P.Color0;
			P.Follow = Follow;
			P.FollowOffset = Follow ? (Loc - Follow->GetComponentLocation()) : FVector::ZeroVector;
			P.OrbitRadius = 28.f * S;
			P.OrbitSpeed = 4.f;
			P.OrbitPhase = i * (2.f * PI / 3.f);
			P.AngVel = FVector(0, 0, 6.f);
			AddParticle(CrankPaths::FXStar, CrankPaths::MatFXUnlit, P);
		}
		break;

	case ECrankFX::Splash:
		for (int32 i = 0; i < 10; ++i)
		{
			FCrankParticle P;
			P.Pos = Loc; P.Vel = RandCone(FVector::UpVector, 45.f) * Rand(150.f, 350.f) * S; P.Life = Rand(0.4f, 0.7f);
			P.Size0 = Rand(0.04f, 0.07f) * S; P.Size1 = P.Size0 * 0.5f;
			P.Color0 = FLinearColor(0.55f, 0.8f, 1.f, 0.8f); P.Color1 = FLinearColor(0.55f, 0.8f, 1.f, 0.2f);
			P.Gravity = 980.f;
			AddParticle(CrankPaths::FXDrop, CrankPaths::MatFXPuff, P);
		}
		break;

	case ECrankFX::WindClick:
		for (int32 i = 0; i < 3; ++i)
		{
			Spark(Loc, FMath::VRand() * Rand(60.f, 130.f), 0.2f, 0.025f, FLinearColor(4.f, 4.f, 3.f, 1.f), 0.f);
		}
		break;

	case ECrankFX::RhythmGood:
	{
		FCrankParticle Ring;
		Ring.Pos = Loc; Ring.Life = 0.3f; Ring.Size0 = 0.12f * S; Ring.Size1 = 0.55f * S;
		Ring.Rot = Rot.Quaternion() * FQuat(FVector::RightVector, PI * 0.5f);
		Ring.Color0 = FLinearColor(0.6f, 4.f, 1.f, 1.f); Ring.Color1 = FLinearColor(0.3f, 2.f, 0.5f, 0.f);
		AddParticle(CrankPaths::FXRing, CrankPaths::MatFXUnlit, Ring);
		for (int32 i = 0; i < 4; ++i)
		{
			FCrankParticle P;
			P.Pos = Loc; P.Vel = FMath::VRand() * Rand(80.f, 140.f); P.Life = 0.35f;
			P.Size0 = 0.05f * S; P.Size1 = 0.02f * S;
			P.Color0 = FLinearColor(0.6f, 5.f, 1.2f, 1.f); P.Color1 = P.Color0;
			P.Drag = 3.f; P.AngVel = FVector(0, 0, 10.f);
			AddParticle(CrankPaths::FXStar, CrankPaths::MatFXUnlit, P);
		}
		break;
	}

	case ECrankFX::Notes:
	{
		FCrankParticle P;
		P.Pos = Loc + FVector(Rand(-20, 20), Rand(-20, 20), 0);
		P.Vel = FVector(Rand(-15, 15), Rand(-15, 15), Rand(45.f, 75.f));
		P.Life = Rand(1.4f, 2.0f);
		P.Size0 = Rand(0.10f, 0.14f) * S; P.Size1 = P.Size0;
		P.Color0 = Tint * FLinearColor(4.f, 3.2f, 0.8f, 1.f); P.Color1 = WithAlpha(P.Color0, 0.f);
		P.Rot = FQuat(FVector::UpVector, Rand(-0.4f, 0.4f));
		P.AngVel = FVector(0, 0, Rand(-1.f, 1.f));
		AddParticle(CrankPaths::FXNote, CrankPaths::MatFXUnlit, P);
		break;
	}

	case ECrankFX::Confetti:
	case ECrankFX::Deliver:
	{
		static const FLinearColor Colors[] = {
			FLinearColor(0.9f, 0.15f, 0.1f), FLinearColor(0.1f, 0.4f, 0.95f), FLinearColor(0.98f, 0.8f, 0.1f),
			FLinearColor(0.15f, 0.75f, 0.25f), FLinearColor(0.85f, 0.3f, 0.85f) };
		const int32 Count = Type == ECrankFX::Deliver ? 36 : 50;
		for (int32 i = 0; i < Count; ++i)
		{
			FCrankParticle P;
			P.Pos = Loc; P.Vel = RandCone(FVector::UpVector, 50.f) * Rand(300.f, 650.f) * S; P.Life = Rand(1.5f, 2.6f);
			P.Size0 = 0.06f * S; P.Size1 = 0.05f * S;
			P.Stretch = FVector(1.f, 0.6f, 0.12f);
			P.Color0 = Colors[FMath::RandRange(0, 4)]; P.Color1 = P.Color0;
			P.Gravity = 380.f; P.Drag = 1.6f;
			P.Rot = FQuat(FMath::VRand(), Rand(0.f, PI));
			P.AngVel = FMath::VRand() * Rand(5.f, 12.f);
			AddParticle(CrankPaths::FXBolt, CrankPaths::MatFXLit, P);
		}
		if (Type == ECrankFX::Deliver)
		{
			for (int32 i = 0; i < 12; ++i)
			{
				FCrankParticle P;
				P.Pos = Loc; P.Vel = FMath::VRand() * Rand(100.f, 260.f); P.Life = Rand(0.6f, 1.0f);
				P.Size0 = 0.08f; P.Size1 = 0.02f;
				P.Color0 = FLinearColor(6.f, 5.f, 1.f, 1.f); P.Color1 = P.Color0;
				P.Drag = 2.f; P.AngVel = FVector(0, 0, 8.f);
				AddParticle(CrankPaths::FXStar, CrankPaths::MatFXUnlit, P);
			}
		}
		break;
	}

	case ECrankFX::ColdMist:
		for (int32 i = 0; i < 2; ++i)
		{
			FVector V = FMath::VRand(); V.Z = 0.f;
			Puff(Loc, V.GetSafeNormal() * Rand(25.f, 60.f) * S, Rand(2.0f, 3.2f), 0.25f * S, 0.65f * S, FLinearColor(0.78f, 0.9f, 1.f), 0.2f, 8.f, 0.4f);
		}
		break;

	case ECrankFX::Slip:
		for (int32 i = 0; i < 8; ++i)
		{
			FVector V = FMath::VRand(); V.Z = FMath::Abs(V.Z) * 0.3f;
			Puff(Loc, V.GetSafeNormal() * Rand(120.f, 220.f), Rand(0.4f, 0.7f), 0.08f, 0.2f, Dust, 0.6f, 0.f, 4.f);
		}
		for (int32 i = 0; i < 8; ++i)
		{
			Spark(Loc + FVector(0, 0, 50.f), FMath::VRand() * Rand(200.f, 400.f), 0.3f, 0.04f, FLinearColor(6.f, 5.f, 2.f, 1.f), 300.f);
		}
		break;

	case ECrankFX::Land:
		for (int32 i = 0; i < 8; ++i)
		{
			const float A = i * (2.f * PI / 8.f) + Rand(-0.2f, 0.2f);
			const FVector Dir(FMath::Cos(A), FMath::Sin(A), 0.1f);
			Puff(Loc, Dir * Rand(120.f, 220.f) * S, Rand(0.4f, 0.6f), 0.08f * S, 0.2f * S, Tint * Dust, 0.6f, 0.f, 4.f);
		}
		break;

	case ECrankFX::Footstep:
		Puff(Loc, RandHemisphereUp() * Rand(15.f, 35.f), Rand(0.35f, 0.5f), 0.04f * S, 0.09f * S, Tint * Dust, 0.4f, 0.f, 3.f);
		break;

	case ECrankFX::BossSmoke:
		for (int32 i = 0; i < 16; ++i)
		{
			Puff(Loc + FMath::VRand() * 40.f * S, RandCone(FVector::UpVector, 60.f) * Rand(80.f, 200.f) * S, Rand(1.2f, 2.0f), 0.3f * S, 0.8f * S, FLinearColor(0.18f, 0.18f, 0.2f), 0.75f, -80.f, 1.5f);
		}
		break;

	case ECrankFX::CellZap:
		for (int32 i = 0; i < 12; ++i)
		{
			Spark(Loc, FMath::VRand() * Rand(200.f, 500.f), Rand(0.2f, 0.35f), 0.04f, FLinearColor(2.f, 4.f, 9.f, 1.f), 200.f);
		}
		break;

	case ECrankFX::Suction:
	{
		// Streaks converging toward Loc along -Fwd... spawn on a cone in front and fly toward the intake.
		for (int32 i = 0; i < 3; ++i)
		{
			const FVector Offset = RandCone(Fwd, 50.f) * Rand(200.f, 380.f) * S + FVector(0, 0, Rand(5.f, 60.f));
			FCrankParticle P;
			P.Pos = Loc + Offset;
			P.Vel = -Offset.GetSafeNormal() * Rand(500.f, 700.f);
			P.Life = FMath::Clamp(Offset.Size() / 650.f, 0.2f, 0.7f);
			P.Size0 = 0.05f; P.Size1 = 0.02f;
			P.Color0 = FLinearColor(0.8f, 0.82f, 0.9f, 0.35f); P.Color1 = FLinearColor(0.8f, 0.82f, 0.9f, 0.f);
			P.Rot = FRotationMatrix::MakeFromX(P.Vel.GetSafeNormal()).ToQuat();
			P.Stretch = FVector(5.f, 1.f, 1.f);
			AddParticle(CrankPaths::FXPuff, CrankPaths::MatFXPuff, P);
		}
		break;
	}
	}
}

// --------------------------------------------------------------------------------------------
// Emitter component

UCrankFXEmitterComponent::UCrankFXEmitterComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
}

void UCrankFXEmitterComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!bEmitting || Rate <= 0.f || GetNetMode() == NM_DedicatedServer)
	{
		Accumulator = 0.f;
		return;
	}

	Accumulator += DeltaTime * Rate;
	if (Accumulator < 1.f)
	{
		return;
	}

	UCrankFXSubsystem* FX = GetWorld() ? GetWorld()->GetSubsystem<UCrankFXSubsystem>() : nullptr;
	if (!FX)
	{
		return;
	}

	while (Accumulator >= 1.f)
	{
		Accumulator -= 1.f;
		FVector Offset(FMath::FRandRange(-SpawnExtent.X, SpawnExtent.X), FMath::FRandRange(-SpawnExtent.Y, SpawnExtent.Y), FMath::FRandRange(-SpawnExtent.Z, SpawnExtent.Z));
		const FVector Loc = GetComponentTransform().TransformPosition(Offset);
		FX->SpawnPreset(Effect, Loc, GetComponentRotation(), EffectScale, Tint, Effect == ECrankFX::StunStars ? this : nullptr);
	}
}
