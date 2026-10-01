// Tiny sound helper: plays procedurally generated SFX from /Game/Crank/Audio by name.

#pragma once

#include "CoreMinimal.h"

class UAudioComponent;
class USceneComponent;
class USoundAttenuation;

namespace CrankSound
{
	CRANK_API USoundAttenuation* DefaultAttenuation();

	/** One-shot 3D sound. Safe to call on dedicated servers (does nothing). */
	CRANK_API void PlayAt(const UObject* WorldContext, const FString& Name, const FVector& Location, float Volume = 1.f, float Pitch = 1.f);

	/** One-shot UI / 2D sound. */
	CRANK_API void Play2D(const UObject* WorldContext, const FString& Name, float Volume = 1.f, float Pitch = 1.f);

	/** Create (not started) attached looping audio component. */
	CRANK_API UAudioComponent* CreateLoop(USceneComponent* AttachTo, const FString& Name, float Volume = 1.f);
}
