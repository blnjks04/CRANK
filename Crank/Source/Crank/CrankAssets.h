// Central registry of content paths used from C++.
// Everything is loaded lazily by path so that code keeps working (with engine fallbacks)
// even if an art asset is missing.

#pragma once

#include "CoreMinimal.h"

class UStaticMesh;
class USkeletalMesh;
class UMaterialInterface;
class USoundBase;
class UAnimSequence;
class UFont;
class UTexture2D;

namespace CrankPaths
{
	// Character
	inline const TCHAR* DollMesh			= TEXT("/Game/Crank/Characters/Doll/SK_Doll.SK_Doll");
	inline const TCHAR* DollKeyMesh			= TEXT("/Game/Crank/Characters/Doll/SM_Doll_Key.SM_Doll_Key");
	inline const TCHAR* DollPartHead		= TEXT("/Game/Crank/Characters/Doll/SM_DollPart_Head.SM_DollPart_Head");
	inline const TCHAR* DollPartTorso		= TEXT("/Game/Crank/Characters/Doll/SM_DollPart_Torso.SM_DollPart_Torso");
	inline const TCHAR* DollPartArm			= TEXT("/Game/Crank/Characters/Doll/SM_DollPart_Arm.SM_DollPart_Arm");
	inline const TCHAR* DollPartLeg			= TEXT("/Game/Crank/Characters/Doll/SM_DollPart_Leg.SM_DollPart_Leg");
	inline const TCHAR* DollAnimFolder		= TEXT("/Game/Crank/Characters/Doll/Anims/");

	// Materials
	inline const TCHAR* MatDollPaint		= TEXT("/Game/Crank/Materials/M_DollPaint.M_DollPaint");
	inline const TCHAR* MatVertexColor		= TEXT("/Game/Crank/Materials/M_VC_Matte.M_VC_Matte");
	inline const TCHAR* MatFXPuff			= TEXT("/Game/Crank/Materials/M_FX_Puff.M_FX_Puff");
	inline const TCHAR* MatFXUnlit			= TEXT("/Game/Crank/Materials/M_FX_Unlit.M_FX_Unlit");
	inline const TCHAR* MatFXLit			= TEXT("/Game/Crank/Materials/M_FX_Lit.M_FX_Lit");
	inline const TCHAR* MatSolid			= TEXT("/Game/Crank/Materials/M_Solid.M_Solid");
	inline const TCHAR* MatWater			= TEXT("/Game/Crank/Materials/M_Water.M_Water");
	inline const TCHAR* MatFrost			= TEXT("/Game/Crank/Materials/M_Frost.M_Frost");

	// FX meshes
	inline const TCHAR* FXPuff				= TEXT("/Game/Crank/FX/SM_FX_Puff.SM_FX_Puff");
	inline const TCHAR* FXStar				= TEXT("/Game/Crank/FX/SM_FX_Star.SM_FX_Star");
	inline const TCHAR* FXBolt				= TEXT("/Game/Crank/FX/SM_FX_Bolt.SM_FX_Bolt");
	inline const TCHAR* FXDrop				= TEXT("/Game/Crank/FX/SM_FX_Drop.SM_FX_Drop");
	inline const TCHAR* FXNote				= TEXT("/Game/Crank/FX/SM_FX_Note.SM_FX_Note");
	inline const TCHAR* FXSpring			= TEXT("/Game/Crank/FX/SM_FX_Spring.SM_FX_Spring");
	inline const TCHAR* FXDot				= TEXT("/Game/Crank/FX/SM_FX_Dot.SM_FX_Dot");
	inline const TCHAR* FXRing				= TEXT("/Game/Crank/FX/SM_FX_Ring.SM_FX_Ring");

	// Engine fallbacks
	inline const TCHAR* Cube				= TEXT("/Engine/BasicShapes/Cube.Cube");
	inline const TCHAR* Sphere				= TEXT("/Engine/BasicShapes/Sphere.Sphere");
	inline const TCHAR* Cylinder			= TEXT("/Engine/BasicShapes/Cylinder.Cylinder");
	inline const TCHAR* Cone				= TEXT("/Engine/BasicShapes/Cone.Cone");
	inline const TCHAR* Plane				= TEXT("/Engine/BasicShapes/Plane.Plane");
	inline const TCHAR* BasicMaterial		= TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial");

	// UI
	inline const TCHAR* UIFont				= TEXT("/Game/Crank/UI/F_Crank.F_Crank");
}

namespace CrankAssets
{
	CRANK_API UObject* LoadByPath(const TCHAR* Path, UClass* Class);

	CRANK_API UStaticMesh* Mesh(const TCHAR* Path, const TCHAR* Fallback = nullptr);
	CRANK_API USkeletalMesh* SkelMesh(const TCHAR* Path);
	CRANK_API UMaterialInterface* Material(const TCHAR* Path, bool bFallbackToBasic = true);
	CRANK_API USoundBase* Sound(const FString& Name);	// name inside /Game/Crank/Audio/
	CRANK_API UAnimSequence* DollAnim(const FString& Name);	// name inside the doll anim folder (without A_Doll_)
	CRANK_API UStaticMesh* PropMesh(const FString& Name);	// /Game/Crank/Props/<Name>
	CRANK_API UMaterialInterface* PropMaterial(const FString& Name); // /Game/Crank/Materials/<Name>
	CRANK_API UFont* UIFont();
}
