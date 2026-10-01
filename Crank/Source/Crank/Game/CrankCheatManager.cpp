#include "Game/CrankCheatManager.h"
#include "Game/WindupPlayerController.h"

void UCrankCheatManager::Send(FName Command, float Value, FName Arg)
{
	if (AWindupPlayerController* PC = Cast<AWindupPlayerController>(GetOuterAPlayerController()))
	{
		PC->ServerCheat(Command, Value, Arg);
	}
}

void UCrankCheatManager::CrankTorque(float Value)		{ Send(TEXT("Torque"), Value); }
void UCrankCheatManager::CrankTorqueAll(float Value)	{ Send(TEXT("TorqueAll"), Value); }
void UCrankCheatManager::CrankGoto(FName Zone)			{ Send(TEXT("Goto"), 0.f, Zone); }
void UCrankCheatManager::CrankDummy(float Torque)		{ Send(TEXT("Dummy"), Torque); }
void UCrankCheatManager::CrankRupture()					{ Send(TEXT("Rupture")); }
void UCrankCheatManager::CrankRagdoll()					{ Send(TEXT("Ragdoll")); }
void UCrankCheatManager::CrankLaunch(float Speed)		{ Send(TEXT("Launch"), Speed); }
void UCrankCheatManager::CrankRun(float Seconds)		{ Send(TEXT("Run"), Seconds); }
void UCrankCheatManager::CrankLaunchPlayer(int32 PlayerIndex, float Speed)	{ Send(TEXT("LaunchPlayer"), Speed, FName(*FString::FromInt(PlayerIndex))); }
void UCrankCheatManager::CrankTime(float SecondsLeft)	{ Send(TEXT("Time"), SecondsLeft); }
void UCrankCheatManager::CrankParts()					{ Send(TEXT("Parts")); }
void UCrankCheatManager::CrankBossWake()				{ Send(TEXT("BossWake")); }
void UCrankCheatManager::CrankBossStun()				{ Send(TEXT("BossStun")); }
void UCrankCheatManager::CrankBossPhase(int32 Phase)	{ Send(TEXT("BossPhase"), (float)Phase); }
void UCrankCheatManager::CrankBossKill()				{ Send(TEXT("BossKill")); }
void UCrankCheatManager::CrankBossDock()				{ Send(TEXT("BossDock")); }
void UCrankCheatManager::CrankGiantWake()				{ Send(TEXT("GiantWake")); }
void UCrankCheatManager::CrankGiantDown()				{ Send(TEXT("GiantDown")); }
void UCrankCheatManager::CrankGiantPhase(int32 Phase)	{ Send(TEXT("GiantPhase"), (float)Phase); }
void UCrankCheatManager::CrankGiantHit()				{ Send(TEXT("GiantHit")); }
void UCrankCheatManager::CrankGiantKill()				{ Send(TEXT("GiantKill")); }
void UCrankCheatManager::CrankGiantAttack(int32 Which)	{ Send(TEXT("GiantAttack"), (float)Which); }
