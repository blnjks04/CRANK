#include "CrankAssets.h"
#include "CrankTypes.h"
#include "Engine/StaticMesh.h"
#include "Engine/SkeletalMesh.h"
#include "Materials/MaterialInterface.h"
#include "Sound/SoundBase.h"
#include "Animation/AnimSequence.h"
#include "Engine/Font.h"
#include "Engine/Engine.h"
#include "UObject/Package.h"
#include "Misc/PackageName.h"

namespace
{
	// Remember failed lookups so we do not hit the disk every frame for missing optional assets.
	TMap<FString, TWeakObjectPtr<UObject>>& Cache()
	{
		static TMap<FString, TWeakObjectPtr<UObject>> GCache;
		return GCache;
	}

	TSet<FString>& Missing()
	{
		static TSet<FString> GMissing;
		return GMissing;
	}
}

UObject* CrankAssets::LoadByPath(const TCHAR* Path, UClass* Class)
{
	if (!Path || !*Path)
	{
		return nullptr;
	}

	const FString Key(Path);
	if (TWeakObjectPtr<UObject>* Found = Cache().Find(Key))
	{
		if (UObject* Obj = Found->Get())
		{
			return Obj;
		}
	}
	if (Missing().Contains(Key))
	{
		return nullptr;
	}

	// Avoid noisy load warnings for optional content: check that the package exists first.
	FString PackageName = Key;
	int32 DotIndex;
	if (PackageName.FindChar(TEXT('.'), DotIndex))
	{
		PackageName.LeftInline(DotIndex);
	}
	if (!PackageName.StartsWith(TEXT("/Engine/")) && !FPackageName::DoesPackageExist(PackageName))
	{
		Missing().Add(Key);
		return nullptr;
	}

	UObject* Obj = StaticLoadObject(Class, nullptr, Path, nullptr, LOAD_NoWarn | LOAD_Quiet);
	if (Obj)
	{
		Cache().Add(Key, Obj);
	}
	else
	{
		Missing().Add(Key);
	}
	return Obj;
}

UStaticMesh* CrankAssets::Mesh(const TCHAR* Path, const TCHAR* Fallback)
{
	if (UStaticMesh* Mesh = Cast<UStaticMesh>(LoadByPath(Path, UStaticMesh::StaticClass())))
	{
		return Mesh;
	}
	return Fallback ? Cast<UStaticMesh>(LoadByPath(Fallback, UStaticMesh::StaticClass())) : nullptr;
}

USkeletalMesh* CrankAssets::SkelMesh(const TCHAR* Path)
{
	return Cast<USkeletalMesh>(LoadByPath(Path, USkeletalMesh::StaticClass()));
}

UMaterialInterface* CrankAssets::Material(const TCHAR* Path, bool bFallbackToBasic)
{
	if (UMaterialInterface* Mat = Cast<UMaterialInterface>(LoadByPath(Path, UMaterialInterface::StaticClass())))
	{
		return Mat;
	}
	return bFallbackToBasic ? Cast<UMaterialInterface>(LoadByPath(CrankPaths::BasicMaterial, UMaterialInterface::StaticClass())) : nullptr;
}

USoundBase* CrankAssets::Sound(const FString& Name)
{
	const FString Path = FString::Printf(TEXT("/Game/Crank/Audio/%s.%s"), *Name, *Name);
	return Cast<USoundBase>(LoadByPath(*Path, USoundBase::StaticClass()));
}

UAnimSequence* CrankAssets::DollAnim(const FString& Name)
{
	const FString Asset = FString::Printf(TEXT("A_Doll_%s"), *Name);
	const FString Path = FString::Printf(TEXT("%s%s.%s"), CrankPaths::DollAnimFolder, *Asset, *Asset);
	return Cast<UAnimSequence>(LoadByPath(*Path, UAnimSequence::StaticClass()));
}

UStaticMesh* CrankAssets::PropMesh(const FString& Name)
{
	const FString Path = FString::Printf(TEXT("/Game/Crank/Props/%s.%s"), *Name, *Name);
	return Cast<UStaticMesh>(LoadByPath(*Path, UStaticMesh::StaticClass()));
}

UMaterialInterface* CrankAssets::PropMaterial(const FString& Name)
{
	const FString Path = FString::Printf(TEXT("/Game/Crank/Materials/%s.%s"), *Name, *Name);
	return Cast<UMaterialInterface>(LoadByPath(*Path, UMaterialInterface::StaticClass()));
}

UFont* CrankAssets::UIFont()
{
	if (UFont* Font = Cast<UFont>(LoadByPath(CrankPaths::UIFont, UFont::StaticClass())))
	{
		return Font;
	}
	return GEngine ? GEngine->GetLargeFont() : nullptr;
}
