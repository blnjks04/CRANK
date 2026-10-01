#include "CrankEditorLibrary.h"
#include "PhysicsAssetUtils.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "PhysicsEngine/PhysicsConstraintTemplate.h"
#include "PhysicsEngine/SkeletalBodySetup.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/Blueprint.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Misc/PackageName.h"
#include "GameFramework/Actor.h"
#include "Components/ActorComponent.h"
#include "Engine/SCS_Node.h"
#include "Engine/SimpleConstructionScript.h"

UPhysicsAsset* UCrankEditorLibrary::BuildDollPhysicsAsset(USkeletalMesh* Mesh, const FString& PackagePath, float MinBoneSize)
{
	if (!Mesh)
	{
		return nullptr;
	}

	const FString AssetName = FPackageName::GetLongPackageAssetName(PackagePath);
	UPackage* Package = CreatePackage(*PackagePath);
	UPhysicsAsset* PhysAsset = FindObject<UPhysicsAsset>(Package, *AssetName);
	if (!PhysAsset)
	{
		PhysAsset = NewObject<UPhysicsAsset>(Package, *AssetName, RF_Public | RF_Standalone | RF_Transactional);
		FAssetRegistryModule::AssetCreated(PhysAsset);
	}

	FPhysAssetCreateParams Params;
	Params.MinBoneSize = MinBoneSize;
	Params.MinWeldSize = 0.1f;
	Params.GeomType = EFG_Sphyl;
	Params.VertWeight = EVW_DominantWeight;
	Params.bAutoOrientToBone = true;
	Params.bCreateConstraints = true;
	Params.bWalkPastSmall = false;
	Params.bBodyForAll = true;
	Params.bDisableCollisionsByDefault = true;
	Params.AngularConstraintMode = ACM_Limited;

	FText Error;
	if (!FPhysicsAssetUtils::CreateFromSkeletalMesh(PhysAsset, Mesh, Params, Error, true, false))
	{
		UE_LOG(LogTemp, Error, TEXT("BuildDollPhysicsAsset failed: %s"), *Error.ToString());
		return nullptr;
	}

	// Tin toy joints: stiff hips / knees, loose shoulders.
	for (UPhysicsConstraintTemplate* Constraint : PhysAsset->ConstraintSetup)
	{
		if (!Constraint)
		{
			continue;
		}
		FConstraintInstance& CI = Constraint->DefaultInstance;
		const FString Child = CI.ConstraintBone1.ToString();
		if (Child.StartsWith(TEXT("calf")) || Child.StartsWith(TEXT("lowerarm")))
		{
			CI.SetAngularSwing1Limit(ACM_Limited, 10.f);
			CI.SetAngularSwing2Limit(ACM_Limited, 70.f);
			CI.SetAngularTwistLimit(ACM_Limited, 5.f);
		}
		else if (Child.StartsWith(TEXT("upperarm")))
		{
			CI.SetAngularSwing1Limit(ACM_Limited, 80.f);
			CI.SetAngularSwing2Limit(ACM_Limited, 80.f);
			CI.SetAngularTwistLimit(ACM_Limited, 30.f);
		}
		else if (Child.StartsWith(TEXT("head")))
		{
			CI.SetAngularSwing1Limit(ACM_Limited, 25.f);
			CI.SetAngularSwing2Limit(ACM_Limited, 25.f);
			CI.SetAngularTwistLimit(ACM_Limited, 30.f);
		}
		else
		{
			CI.SetAngularSwing1Limit(ACM_Limited, 35.f);
			CI.SetAngularSwing2Limit(ACM_Limited, 35.f);
			CI.SetAngularTwistLimit(ACM_Limited, 15.f);
		}
	}

	PhysAsset->MarkPackageDirty();
	Mesh->SetPhysicsAsset(PhysAsset);
	Mesh->MarkPackageDirty();

	const FString FileName = FPackageName::LongPackageNameToFilename(PackagePath, FPackageName::GetAssetPackageExtension());
	FSavePackageArgs SaveArgs;
	SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
	UPackage::SavePackage(Package, PhysAsset, *FileName, SaveArgs);
	return PhysAsset;
}

static UActorComponent* FindTemplate(UBlueprint* Blueprint, FName ComponentName)
{
	if (!Blueprint || !Blueprint->GeneratedClass)
	{
		return nullptr;
	}
	// Native / inherited components live on the CDO.
	if (AActor* CDO = Cast<AActor>(Blueprint->GeneratedClass->GetDefaultObject()))
	{
		TInlineComponentArray<UActorComponent*> Components;
		CDO->GetComponents(Components);
		for (UActorComponent* Comp : Components)
		{
			if (Comp && Comp->GetFName() == ComponentName)
			{
				return Comp;
			}
		}
	}
	// Components added in the blueprint itself.
	if (Blueprint->SimpleConstructionScript)
	{
		for (USCS_Node* Node : Blueprint->SimpleConstructionScript->GetAllNodes())
		{
			if (Node && Node->GetVariableName() == ComponentName)
			{
				return Node->ComponentTemplate;
			}
		}
	}
	return nullptr;
}

bool UCrankEditorLibrary::SetObjectPropertyText(UObject* Object, FName PropertyName, const FString& ValueText)
{
	if (!Object)
	{
		return false;
	}
	FProperty* Property = Object->GetClass()->FindPropertyByName(PropertyName);
	if (!Property)
	{
		UE_LOG(LogTemp, Warning, TEXT("SetObjectPropertyText: %s has no property %s"), *Object->GetName(), *PropertyName.ToString());
		return false;
	}
	Object->Modify();
	void* ValuePtr = Property->ContainerPtrToValuePtr<void>(Object);
	const TCHAR* Result = Property->ImportText_Direct(*ValueText, ValuePtr, Object, PPF_None);
	if (!Result)
	{
		UE_LOG(LogTemp, Warning, TEXT("SetObjectPropertyText: could not parse '%s' for %s.%s"), *ValueText, *Object->GetName(), *PropertyName.ToString());
		return false;
	}
	FPropertyChangedEvent Event(Property);
	Object->PostEditChangeProperty(Event);
	Object->MarkPackageDirty();
	return true;
}

FString UCrankEditorLibrary::GetObjectPropertyText(UObject* Object, FName PropertyName)
{
	if (!Object)
	{
		return FString();
	}
	FProperty* Property = Object->GetClass()->FindPropertyByName(PropertyName);
	if (!Property)
	{
		return FString();
	}
	FString Out;
	Property->ExportTextItem_Direct(Out, Property->ContainerPtrToValuePtr<void>(Object), nullptr, Object, PPF_None);
	return Out;
}

bool UCrankEditorLibrary::SetBlueprintComponentProperty(UBlueprint* Blueprint, FName ComponentName, FName PropertyName, const FString& ValueText)
{
	UActorComponent* Template = FindTemplate(Blueprint, ComponentName);
	if (!Template)
	{
		UE_LOG(LogTemp, Warning, TEXT("SetBlueprintComponentProperty: no component %s"), *ComponentName.ToString());
		return false;
	}
	const bool bOk = SetObjectPropertyText(Template, PropertyName, ValueText);
	if (bOk)
	{
		FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
	}
	return bOk;
}

bool UCrankEditorLibrary::SetBlueprintDefaultProperty(UBlueprint* Blueprint, FName PropertyName, const FString& ValueText)
{
	if (!Blueprint || !Blueprint->GeneratedClass)
	{
		return false;
	}
	UObject* CDO = Blueprint->GeneratedClass->GetDefaultObject();
	const bool bOk = SetObjectPropertyText(CDO, PropertyName, ValueText);
	if (bOk)
	{
		FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
	}
	return bOk;
}

UActorComponent* UCrankEditorLibrary::FindComponentByName(AActor* Actor, FName ComponentName)
{
	if (!Actor)
	{
		return nullptr;
	}
	TInlineComponentArray<UActorComponent*> Components;
	Actor->GetComponents(Components);
	for (UActorComponent* Comp : Components)
	{
		if (Comp && Comp->GetFName() == ComponentName)
		{
			return Comp;
		}
	}
	return nullptr;
}
