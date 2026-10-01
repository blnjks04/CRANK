// Shared enums, log category and small helpers for Windup Crew.

#pragma once

#include "CoreMinimal.h"
#include "CrankTypes.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(LogCrank, Log, All);

/** Custom collision channels (see DefaultEngine.ini). */
#define ECC_Boss			ECC_GameTraceChannel1
#define ECC_BossBlocker		ECC_GameTraceChannel2

/** Torque band (design doc 3.2). */
UENUM(BlueprintType)
enum class ETorqueBand : uint8
{
	Discharged	UMETA(DisplayName = "방전"),
	Toddle		UMETA(DisplayName = "아장"),
	Run			UMETA(DisplayName = "달리기"),
	Sprint		UMETA(DisplayName = "질주"),
	Overwound	UMETA(DisplayName = "과감기"),
};

/** Whole-body state of a windup doll. Relationships (winding, carrying) are tracked separately. */
UENUM(BlueprintType)
enum class ECrankBodyState : uint8
{
	Normal,		// standing / walking / crawling (crawl = Normal at 0 T)
	Ragdoll,	// full ragdoll (fall, slip, knockback, landing)
	GettingUp,	// blending from ragdoll back to animation
	Flying,		// launched by slingshot / self release
	Splat,		// flattened against a wall after a launch
	Carried,	// piggy-backed by the partner
	Scattered,	// ruptured into parts, waiting for assembly
	Trapped,	// inside the boss dust bin
	Frozen,		// raid ended
};

UENUM(BlueprintType)
enum class ECrankHand : uint8
{
	Left,
	Right,
};

/** Optional / required raid parts. */
UENUM(BlueprintType)
enum class ECrankPartType : uint8
{
	None,
	Gear,			// B - light
	SpringCoil,		// C - heavy
	JewelBearing,	// D - light
	MainSpring,		// E - mandatory, two-person carry
	Dishcloth,		// arena tool
	BatteryCell,	// spent boss cell (cosmetic)
	GoldenKey,		// F - dropped by the giant boss, mandatory when a giant is in the level
};

/** Weight class of a carryable. */
UENUM(BlueprintType)
enum class ECarryWeight : uint8
{
	Light,
	Heavy,
	TwoPerson,	// main spring
	Cloth,		// dishcloth: very slow alone
};

/** Parts a ruptured doll scatters into (design doc 3.7). */
UENUM(BlueprintType)
enum class EBodyPartType : uint8
{
	Head,
	Torso,
	Key,
	Arm,
	Leg,
};

UENUM(BlueprintType)
enum class ERaidPhase : uint8
{
	Waiting,
	InProgress,
	Cleared,
	Failed,
};

UENUM(BlueprintType)
enum class ERaidFailReason : uint8
{
	None,
	Midnight,
	BothScattered,
};

UENUM(BlueprintType)
enum class EBossState : uint8
{
	Dormant,
	Patrol,
	Chase,
	Stunned,
	MiniStunned,
	PhaseTransition,
	ReturnToDock,
	Docking,
	Defeated,
};

/** Giant cymbal monkey "짝짝이 대왕" (zone F). */
UENUM(BlueprintType)
enum class EGiantState : uint8
{
	Dormant,
	Idle,
	ClapWindup,
	Clap,
	HopWindup,
	Hop,
	ThrowWindup,
	Throw,
	WoundDown,		// spring ran out: the key on its back is exposed
	Rewind,
	Stagger,		// bonked on the head during a wind-up
	PhaseTransition,
	Defeated,
};

/** What the local player is currently able to do - drives HUD prompts. */
UENUM(BlueprintType)
enum class ECrankPrompt : uint8
{
	None,
	GrabKey,		// behind partner: hold LMB to grab key
	Winding,		// circle the mouse / hold E
	CanSling,		// partner >= 100 T: hold S to aim
	Aiming,			// release mouse to fire
	SelfLaunch,		// I am >= 100 T: press F
	Grab,			// something grabbable in reach
	Carrying,		// release to drop / move+release to throw
	Lever,			// keep holding
	Crawl,			// discharged
	Trapped,
	Scattered,
	Assemble,		// carrying head/key to partner torso
};

namespace CrankUtil
{
	/** Torque value -> band. */
	inline ETorqueBand BandForTorque(float Torque)
	{
		if (Torque <= 0.f)		return ETorqueBand::Discharged;
		if (Torque < 30.f)		return ETorqueBand::Toddle;
		if (Torque < 70.f)		return ETorqueBand::Run;
		if (Torque <= 100.f)	return ETorqueBand::Sprint;
		return ETorqueBand::Overwound;
	}

	/** Player colors (character 1 type, 2 colors). */
	inline FLinearColor PlayerColor(int32 Index)
	{
		switch (Index % 4)
		{
		case 0:  return FLinearColor(0.80f, 0.12f, 0.08f);	// tin red
		case 1:  return FLinearColor(0.10f, 0.32f, 0.85f);	// tin blue
		case 2:  return FLinearColor(0.12f, 0.62f, 0.22f);
		default: return FLinearColor(0.95f, 0.65f, 0.10f);
		}
	}
}
