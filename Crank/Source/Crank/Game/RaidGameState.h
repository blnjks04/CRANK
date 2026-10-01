// Raid timer, collected parts, clear / fail result (design doc 4.1 / 4.3 / 9.1).

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "CrankTypes.h"
#include "RaidGameState.generated.h"

USTRUCT()
struct FRaidToast
{
	GENERATED_BODY()

	UPROPERTY()
	FText Text;

	UPROPERTY()
	FLinearColor Color = FLinearColor::White;

	float LocalTime = 0.f;
};

UCLASS()
class CRANK_API ARaidGameState : public AGameStateBase
{
	GENERATED_BODY()

public:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Raid")
	ERaidPhase RaidPhase = ERaidPhase::Waiting;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Raid")
	float RaidStartTime = 0.f;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Raid")
	float RaidEndTime = 0.f;

	/** Bit 0 gear, 1 spring coil, 2 jewel bearing. */
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Raid")
	uint8 CollectedMask = 0;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Raid")
	bool bMainSpringDelivered = false;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Raid")
	bool bBossDefeated = false;

	/** Zone F: the giant's golden key must be delivered too when the level has a giant. */
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Raid")
	bool bRequiresGoldenKey = false;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Raid")
	bool bGoldenKeyDelivered = false;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Raid")
	bool bGiantDefeated = false;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Raid")
	bool bEmergencyUsed = false;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Raid")
	ERaidFailReason FailReason = ERaidFailReason::None;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Raid")
	float ResultTime = 0.f;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Raid")
	FText ZoneName;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Raid")
	int32 ZoneOrder = 0;

	/** Seconds left until midnight. */
	float GetRemainingTime() const;
	float GetElapsedTime() const;
	int32 NumCollected() const;
	bool IsGoalComplete() const { return bMainSpringDelivered && (bGoldenKeyDelivered || !bRequiresGoldenKey); }
	int32 GetStars() const { return IsGoalComplete() ? 1 + NumCollected() : 0; }

	static uint8 PartBit(ECrankPartType Type);

	/** Server -> everyone: short HUD message. */
	UFUNCTION(NetMulticast, Reliable)
	void MulticastToast(const FText& Text, FLinearColor Color);

	const TArray<FRaidToast>& GetToasts() const { return Toasts; }
	void PruneToasts(float Now);

protected:
	TArray<FRaidToast> Toasts;
};
