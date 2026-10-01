// Local input setup (Enhanced Input), in-raid menu / help, restart request, debug cheats.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "InputActionValue.h"
#include "WindupPlayerController.generated.h"

UCLASS()
class CRANK_API AWindupPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	AWindupPlayerController();

	virtual void BeginPlay() override;
	virtual void SetupInputComponent() override;
	virtual void PlayerTick(float DeltaTime) override;

	bool IsMenuOpen() const { return bMenuOpen; }
	bool IsHelpOpen() const { return bHelpOpen; }

	UFUNCTION(Server, Reliable)
	void ServerRequestRestart();

	/** Debug helpers routed to the server (used by UCrankCheatManager). */
	UFUNCTION(Server, Reliable)
	void ServerCheat(FName Command, float Value, FName Arg);

protected:
	void Input_Menu(const FInputActionValue& Value);
	void Input_Restart(const FInputActionValue& Value);
	void Input_Help(const FInputActionValue& Value);
	void QuitToMenu();

	bool bMenuOpen = false;
	bool bHelpOpen = false;
	float HelpAutoHideTime = 0.f;
};
