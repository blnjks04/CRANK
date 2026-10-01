// Main menu map: no pawn, menu widget, optional "MenuCamera" tagged camera for the 3D backdrop.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/PlayerController.h"
#include "MenuGameMode.generated.h"

class UCrankMainMenuWidget;

UCLASS()
class CRANK_API AMenuPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	AMenuPlayerController();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

protected:
	UPROPERTY(Transient)
	TObjectPtr<UCrankMainMenuWidget> Menu;

	/** Wind-up keys of the backdrop dolls (tag "MenuKey", "MenuKeyFast" spins faster). */
	UPROPERTY(Transient)
	TArray<TObjectPtr<AActor>> SpinningKeys;
};

UCLASS()
class CRANK_API AMenuGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AMenuGameMode();
};
