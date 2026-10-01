// Balance sheet (design doc chapter 7). All values are initial tuning values.

#pragma once

#include "CoreMinimal.h"

namespace CrankBalance
{
	// 7.1 Torque / winding
	constexpr float TorqueMax				= 100.f;
	constexpr float TorqueOverMax			= 120.f;
	constexpr float WindPerTurn				= 8.f;
	constexpr float WindPerTurnRhythm		= 10.f;
	constexpr float RhythmMin				= 0.3f;		// seconds per turn
	constexpr float RhythmMax				= 0.5f;
	constexpr float SlipInterval			= 0.2f;		// faster than this per turn = slip roll
	constexpr float SlipChance				= 0.3f;
	constexpr float SlipFallTime			= 1.0f;
	constexpr float OverwindPerTurn			= 5.f;
	constexpr float RupturePerTorque		= 0.03f;	// (T-100)*3% per turn
	constexpr float HoldWindPerSecond		= 20.f;
	constexpr float HoldOverwindInterval	= 0.5f;		// hold winding above 100 = virtual turn every 0.5s
	constexpr float WindGrabRange			= 100.f;	// 1 m behind the partner
	constexpr float MinServerTurnInterval	= 0.08f;	// server side rate cap

	// 7.2 Movement
	constexpr float SpeedCrawl		= 45.f;		// balance pass 2: every band ~12% faster
	constexpr float SpeedWalk		= 170.f;
	constexpr float SpeedRun		= 335.f;
	constexpr float SpeedSprint		= 500.f;
	constexpr float DrainWalk		= 0.6f;		// balance pass 2: torque drains ~40% slower
	constexpr float DrainRun		= 1.2f;
	constexpr float DrainSprint		= 2.4f;
	constexpr float JumpMinTorque	= 30.f;
	constexpr float JumpCost		= 4.f;
	constexpr float GetUpCost		= 2.f;
	constexpr float GetUpTime		= 1.2f;

	constexpr float MulCarryFriend		= 1.6f;
	constexpr float MulHeavySolo		= 1.3f;
	constexpr float MulMainSpringSolo	= 2.0f;
	constexpr float MulMainSpringDuo	= 1.2f;
	constexpr float MulCold				= 1.5f;

	// 7.3 Launch
	constexpr float LaunchMinTorque		= 100.f;
	constexpr float SlingPullTime		= 1.0f;
	constexpr float SlingSpeedMin		= 1400.f;
	constexpr float SlingSpeedMax		= 2200.f;
	constexpr float SelfLaunchSpeed		= 1600.f;
	constexpr float OverwindBonusPerT	= 0.025f;	// +2.5% per torque over 100
	constexpr float OverwindBonusMax	= 0.5f;
	constexpr float LaunchTorqueCost	= 30.f;		// a launch spends 30 (100 -> 70) instead of dropping to 20
	constexpr float LandRagdollTime		= 1.0f;

	// 7.4 Survival / revive
	constexpr float ScatterRadius		= 300.f;
	constexpr float ReviveTorque		= 30.f;
	constexpr float InvertedViewTime	= 10.f;
	constexpr float StationRate			= 5.f;
	constexpr float StationMax			= 60.f;
	constexpr float EmergencyDelay		= 15.f;
	constexpr float EmergencyTorque		= 15.f;
	constexpr float RaidDuration		= 30.f * 60.f;

	// 7.5 Boss
	constexpr int32 BossCells				= 3;
	constexpr float BossStunImpactSpeed		= 1500.f;
	constexpr float BossMiniStunTime		= 2.0f;
	constexpr float BossCellPullTime		= 1.5f;
	constexpr float BossTrapDrain			= 3.f;
	constexpr float BossLeverHoldTime		= 1.0f;
	constexpr float BossEjectHelplessTime	= 3.f;
	constexpr float BossDustyTime			= 30.f;
	constexpr float BossClothDisableTime	= 15.f;
	constexpr float BossPhaseTransition		= 2.f;
	constexpr float BossWaterLifetime		= 8.f;
	constexpr float BossWaterFriction		= 0.2f;
	constexpr float BossSuctionRadius		= 400.f;
	constexpr float BossSuctionSpeed		= 200.f;
	constexpr float BossDockInterval		= 15.f;
	constexpr float BossDockStayTime		= 3.f;

	// 7.6 Giant boss "짝짝이 대왕" (zone F): launched dolls always hurt it, the wind-up key on its back hurts a lot
	constexpr int32 GiantMaxHP				= 100;
	constexpr int32 GiantBodyDamage			= 5;		// a launched doll slamming into the body
	constexpr int32 GiantFaceDamage			= 7;		// ...into the face (also cancels a wind-up)
	constexpr int32 GiantKeyDamage			= 18;		// ...into the key while it fights
	constexpr int32 GiantKeyWoundDamage		= 30;		// ...into the key while its spring has run down
	constexpr float GiantHitCooldown		= 0.4f;		// per doll (bounces count once)
	inline int32 GiantPhaseForHP(int32 HP)	{ return HP > 66 ? 1 : (HP > 33 ? 2 : 3); }
	constexpr float GiantRingSpeed			= 1050.f;
	constexpr float GiantRingMax			= 2100.f;
	constexpr float GiantRingBand			= 150.f;
	constexpr float GiantRingGroundHeight	= 70.f;		// higher than this above the floor = jumped over
	constexpr float GiantKnockSpeed			= 430.f;	// horizontal knock-back (x ring / stomp strength)
	constexpr float GiantKnockLift			= 340.f;
	constexpr float GiantHopRange			= 1000.f;
	constexpr float GiantHopHeight			= 380.f;
	constexpr float GiantHopTime			= 1.1f;
	constexpr float GiantHopWindup			= 1.0f;
	constexpr float GiantHopChainWindup		= 0.8f;
	constexpr float GiantStompRadius		= 480.f;
	constexpr float GiantClapTime			= 0.7f;
	constexpr float GiantClapChainWindup	= 0.9f;
	constexpr float GiantThrowWindup		= 1.1f;
	constexpr float GiantKnockGrace			= 2.6f;		// a knocked doll can't be knocked again for this long (ragdoll + get up + escape)
	constexpr float GiantStaggerTime		= 2.0f;
	constexpr float GiantRewindTime			= 1.6f;
	constexpr float GiantPhaseTransition	= 2.6f;
	constexpr float GiantCymbalOutTime		= 1.5f;		// boomerang out, the same back
	inline float GiantWoundDownTime(int32 Phase)	{ return Phase <= 1 ? 11.f : (Phase == 2 ? 10.f : 9.f); }
	inline int32 GiantAttacksPerCycle(int32 Phase)	{ return Phase <= 2 ? 3 : 4; }
	inline float GiantIdleTime(int32 Phase)			{ return Phase <= 1 ? 2.6f : (Phase == 2 ? 2.2f : 1.8f); }
	inline float GiantClapWindup(int32 Phase)		{ return Phase <= 1 ? 1.9f : (Phase == 2 ? 1.7f : 1.5f); }
	inline int32 GiantClapsPerAttack(int32 Phase)	{ return Phase <= 1 ? 1 : 2; }
	inline int32 GiantHopsPerAttack(int32 Phase)	{ return Phase <= 1 ? 1 : 2; }

	inline float BossSpeed(int32 Phase)		{ return Phase <= 1 ? 350.f : (Phase == 2 ? 450.f : 550.f); }
	inline float BossStunTime(int32 Phase)	{ return Phase <= 1 ? 5.f : (Phase == 2 ? 4.f : 3.5f); }
}
