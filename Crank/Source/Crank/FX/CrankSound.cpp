#include "FX/CrankSound.h"
#include "CrankAssets.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "Sound/SoundAttenuation.h"
#include "Components/AudioComponent.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "UObject/Package.h"

USoundAttenuation* CrankSound::DefaultAttenuation()
{
	static TWeakObjectPtr<USoundAttenuation> GAttenuation;
	if (!GAttenuation.IsValid())
	{
		USoundAttenuation* Att = NewObject<USoundAttenuation>(GetTransientPackage(), TEXT("CrankDefaultAttenuation"));
		Att->AddToRoot();
		Att->Attenuation.bAttenuate = true;
		Att->Attenuation.bSpatialize = true;
		Att->Attenuation.AttenuationShape = EAttenuationShape::Sphere;
		Att->Attenuation.AttenuationShapeExtents = FVector(250.f, 0.f, 0.f);
		Att->Attenuation.FalloffDistance = 3200.f;
		Att->Attenuation.DistanceAlgorithm = EAttenuationDistanceModel::NaturalSound;
		GAttenuation = Att;
	}
	return GAttenuation.Get();
}

void CrankSound::PlayAt(const UObject* WorldContext, const FString& Name, const FVector& Location, float Volume, float Pitch)
{
	UWorld* World = WorldContext ? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull) : nullptr;
	if (!World || World->GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	if (USoundBase* Sound = CrankAssets::Sound(Name))
	{
		UGameplayStatics::PlaySoundAtLocation(World, Sound, Location, FRotator::ZeroRotator, Volume, Pitch, 0.f, DefaultAttenuation());
	}
}

void CrankSound::Play2D(const UObject* WorldContext, const FString& Name, float Volume, float Pitch)
{
	UWorld* World = WorldContext ? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull) : nullptr;
	if (!World || World->GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	if (USoundBase* Sound = CrankAssets::Sound(Name))
	{
		UGameplayStatics::PlaySound2D(World, Sound, Volume, Pitch);
	}
}

UAudioComponent* CrankSound::CreateLoop(USceneComponent* AttachTo, const FString& Name, float Volume)
{
	if (!AttachTo)
	{
		return nullptr;
	}
	USoundBase* Sound = CrankAssets::Sound(Name);
	if (!Sound)
	{
		return nullptr;
	}
	UAudioComponent* Comp = NewObject<UAudioComponent>(AttachTo->GetOwner());
	Comp->SetSound(Sound);
	Comp->bAutoActivate = false;
	Comp->bAutoDestroy = false;
	Comp->SetVolumeMultiplier(Volume);
	Comp->AttenuationSettings = DefaultAttenuation();
	Comp->SetupAttachment(AttachTo);
	Comp->RegisterComponent();
	return Comp;
}
