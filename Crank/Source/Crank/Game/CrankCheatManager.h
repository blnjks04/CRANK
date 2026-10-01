// Development cheats (console): Crank.Torque 100, Crank.Goto B, Crank.Dummy, Crank.BossPhase 3 ...

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CheatManager.h"
#include "CrankCheatManager.generated.h"

UCLASS()
class CRANK_API UCrankCheatManager : public UCheatManager
{
	GENERATED_BODY()

public:
	UFUNCTION(Exec) void CrankTorque(float Value);
	UFUNCTION(Exec) void CrankTorqueAll(float Value);
	UFUNCTION(Exec) void CrankGoto(FName Zone);
	UFUNCTION(Exec) void CrankDummy(float Torque = 0.f);
	UFUNCTION(Exec) void CrankRupture();
	UFUNCTION(Exec) void CrankRagdoll();
	UFUNCTION(Exec) void CrankLaunch(float Speed = 1800.f);
	/** Run straight ahead for N seconds (movement / tripping tests). */
	UFUNCTION(Exec) void CrankRun(float Seconds = 2.f);
	/** Server launches player N's doll (25 deg up along that player's view yaw). */
	UFUNCTION(Exec) void CrankLaunchPlayer(int32 PlayerIndex, float Speed = 1800.f);
	UFUNCTION(Exec) void CrankTime(float SecondsLeft);
	UFUNCTION(Exec) void CrankParts();
	UFUNCTION(Exec) void CrankBossWake();
	UFUNCTION(Exec) void CrankBossStun();
	UFUNCTION(Exec) void CrankBossPhase(int32 Phase);
	UFUNCTION(Exec) void CrankBossKill();
	UFUNCTION(Exec) void CrankBossDock();
	UFUNCTION(Exec) void CrankGiantWake();
	UFUNCTION(Exec) void CrankGiantDown();
	UFUNCTION(Exec) void CrankGiantPhase(int32 Phase);
	UFUNCTION(Exec) void CrankGiantHit();
	UFUNCTION(Exec) void CrankGiantKill();
	/** 0 clap, 1 hop, 2 throw */
	UFUNCTION(Exec) void CrankGiantAttack(int32 Which);

private:
	void Send(FName Command, float Value = 0.f, FName Arg = NAME_None);
};
