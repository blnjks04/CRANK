// Editor helpers exposed to Python for the asset pipeline (unreal.CrankEditorLibrary.*).

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CrankEditorLibrary.generated.h"

class USkeletalMesh;
class UPhysicsAsset;
class UBlueprint;

UCLASS()
class UCrankEditorLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** Create (or rebuild) a ragdoll physics asset with a body for every bone and limited joints. */
	UFUNCTION(BlueprintCallable, Category = "Crank Editor")
	static UPhysicsAsset* BuildDollPhysicsAsset(USkeletalMesh* Mesh, const FString& PackagePath, float MinBoneSize = 2.f);

	/** Set a property on a component template (native or inherited) of a blueprint's CDO from text. */
	UFUNCTION(BlueprintCallable, Category = "Crank Editor")
	static bool SetBlueprintComponentProperty(UBlueprint* Blueprint, FName ComponentName, FName PropertyName, const FString& ValueText);

	/** Set a property on a blueprint CDO from text. */
	UFUNCTION(BlueprintCallable, Category = "Crank Editor")
	static bool SetBlueprintDefaultProperty(UBlueprint* Blueprint, FName PropertyName, const FString& ValueText);

	/** Set a property on any object (level actor, component) from text. */
	UFUNCTION(BlueprintCallable, Category = "Crank Editor")
	static bool SetObjectPropertyText(UObject* Object, FName PropertyName, const FString& ValueText);

	/** Read a property as text (debugging the pipeline). */
	UFUNCTION(BlueprintCallable, Category = "Crank Editor")
	static FString GetObjectPropertyText(UObject* Object, FName PropertyName);

	/** Find a component of a placed actor by name. */
	UFUNCTION(BlueprintCallable, Category = "Crank Editor")
	static UActorComponent* FindComponentByName(AActor* Actor, FName ComponentName);
};
