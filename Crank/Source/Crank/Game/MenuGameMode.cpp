#include "Game/MenuGameMode.h"
#include "UI/CrankMainMenuWidget.h"
#include "FX/CrankSound.h"
#include "Camera/CameraActor.h"
#include "Components/AudioComponent.h"
#include "Kismet/GameplayStatics.h"
#include "EngineUtils.h"

AMenuGameMode::AMenuGameMode()
{
	DefaultPawnClass = nullptr;
	PlayerControllerClass = AMenuPlayerController::StaticClass();
}

AMenuPlayerController::AMenuPlayerController()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bTickEvenWhenPaused = true;
}

void AMenuPlayerController::BeginPlay()
{
	Super::BeginPlay();
	if (!IsLocalController())
	{
		return;
	}

	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		if (It->ActorHasTag(TEXT("MenuKey")))
		{
			SpinningKeys.Add(*It);
		}
	}

	for (TActorIterator<ACameraActor> It(GetWorld()); It; ++It)
	{
		if (It->ActorHasTag(TEXT("MenuCamera")))
		{
			SetViewTarget(*It);
			break;
		}
	}

	Menu = CreateWidget<UCrankMainMenuWidget>(this, UCrankMainMenuWidget::StaticClass());
	if (Menu)
	{
		Menu->SetIsFocusable(true);
		Menu->AddToViewport();
		FInputModeUIOnly Mode;
		Mode.SetWidgetToFocus(Menu->TakeWidget());
		SetInputMode(Mode);
	}
	bShowMouseCursor = true;
	CrankSound::Play2D(this, TEXT("SFX_MusicBox"), 0.4f);
}

void AMenuPlayerController::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// The wound doll's key turns in ratchet steps, the winder's own key slowly unwinds.
	const float Time = GetWorld()->GetTimeSeconds();
	for (AActor* Key : SpinningKeys)
	{
		if (!IsValid(Key))
		{
			continue;
		}
		const bool bFast = Key->ActorHasTag(TEXT("MenuKeyFast"));
		const float Rate = bFast ? 260.f * (0.35f + 0.65f * FMath::Square(FMath::Sin(Time * 3.2f))) : -70.f;
		Key->AddActorLocalRotation(FRotator(0.f, 0.f, Rate * DeltaSeconds));
	}
}
