// Game instance: remembers why the last network session failed (shown by the main menu) and lists this PC's
// addresses so a host can tell the friend where to join.

#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "Engine/EngineBaseTypes.h"
#include "CrankGameInstance.generated.h"

class UNetDriver;

UCLASS()
class CRANK_API UCrankGameInstance : public UGameInstance
{
	GENERATED_BODY()

public:
	virtual void Init() override;
	virtual void Shutdown() override;

	/** The last connect / disconnect error (empty if none), cleared once read. */
	FText ConsumeNetError();

	/** This PC's IPv4 addresses (LAN / VPN adapters, no loopback). */
	static TArray<FString> GetLocalAddresses();

	/** One line in Saved/Logs/CrankNet.log (kept in shipping builds too, which have no engine log). */
	static void NetLog(const FString& Line);

	/** Address typed into the join box last time (GameUserSettings.ini). */
	static FString LoadLastJoinAddress();
	static void SaveLastJoinAddress(const FString& Address);

protected:
	void HandleNetworkFailure(UWorld* World, UNetDriver* NetDriver, ENetworkFailure::Type FailureType, const FString& ErrorString);
	void HandleTravelFailure(UWorld* World, ETravelFailure::Type FailureType, const FString& ErrorString);

	FText LastNetError;
	FDelegateHandle NetworkFailureHandle;
	FDelegateHandle TravelFailureHandle;
};
