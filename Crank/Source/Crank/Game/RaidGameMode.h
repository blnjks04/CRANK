// Raid rules (design doc 3.6 emergency wind, 3.7 both ruptured = fail, 4.1 timer, 4.3 grades).

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "CrankTypes.h"
#include "RaidGameMode.generated.h"

class AWindupCharacter;
class ACrankCheckpoint;
class ADeliveryZone;
class ARaidGameState;

UCLASS()
class CRANK_API ARaidGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ARaidGameMode();

	virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void PostLogin(APlayerController* NewPlayer) override;
	virtual AActor* ChoosePlayerStart_Implementation(AController* Player) override;
	virtual void RestartPlayer(AController* NewPlayer) override;
	virtual void Logout(AController* Exiting) override;

	// Events from gameplay
	void OnPartDelivered(ECrankPartType Type, ADeliveryZone* Zone);
	void OnDollScattered(AWindupCharacter* Doll);
	void OnDollRevived(AWindupCharacter* Doll);
	void OnBossDefeated();
	void OnGiantDefeated();
	void OnCheckpointReached(ACrankCheckpoint* Checkpoint, AWindupCharacter* Doll);
	void RespawnDoll(AWindupCharacter* Doll);

	/** Server travel back to the start of the raid. */
	void RestartRaid();

	/** Host waiting for a friend chose to play now: an AI dummy joins and the clock starts. */
	void StartWithoutPartner();

	/** Spawn an AI controlled dummy doll (M1 test target / solo partner). */
	AWindupCharacter* SpawnDummy(const FVector& Location, float Torque);

	/** A hosted (listen) game keeps the clock at 11:30 until the second player arrives. */
	bool ShouldStartRaid();

	ACrankCheckpoint* FindCheckpoint(FName ZoneId) const;
	ACrankCheckpoint* GetActiveCheckpoint() const { return ActiveCheckpoint.Get(); }

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Raid")
	float RaidDuration = 25.f * 60.f;

	/** Torque of the first player at the start (the other starts empty). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Raid")
	float FirstPlayerTorque = 40.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Raid")
	float OtherPlayerTorque = 0.f;

	/** Class used for the AI dummy (cheat / solo testing). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Raid")
	TSubclassOf<AWindupCharacter> DummyClass;

	/** Spawn an AI partner automatically when a single player starts (useful for solo tests). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Raid")
	bool bSpawnDummyWhenAlone = false;

protected:
	void StartRaid();
	void EndRaid(bool bCleared, ERaidFailReason Reason);
	void TickEmergencyWind(float DeltaSeconds);
	void GetDolls(TArray<AWindupCharacter*>& OutDolls) const;
	ARaidGameState* GetRaidState() const;

	void CheckAutoDummy();

	TWeakObjectPtr<ACrankCheckpoint> ActiveCheckpoint;
	FTimerHandle AutoDummyTimer;
	bool bAutoDummyInPIE = false;
	int32 NextPlayerIndex = 0;
	float EmergencyTimer = 0.f;
	bool bRaidStarted = false;
};
