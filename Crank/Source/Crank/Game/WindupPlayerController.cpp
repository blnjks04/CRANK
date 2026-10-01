#include "Game/WindupPlayerController.h"
#include "Game/RaidGameMode.h"
#include "Game/RaidGameState.h"
#include "Game/CrankCheatManager.h"
#include "Game/CrankGameInstance.h"
#include "Character/WindupCharacter.h"
#include "Character/TorqueComponent.h"
#include "Character/LaunchComponent.h"
#include "Character/ScatterReviveComponent.h"
#include "Character/CrankInput.h"
#include "Boss/BossDustEater.h"
#include "Boss/BossGiantMonkey.h"
#include "Gameplay/CrankCheckpoint.h"
#include "EnhancedInputSubsystems.h"
#include "EnhancedInputComponent.h"
#include "Kismet/GameplayStatics.h"
#include "EngineUtils.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "Misc/CommandLine.h"
#include "TimerManager.h"

AWindupPlayerController::AWindupPlayerController()
{
	CheatClass = UCrankCheatManager::StaticClass();
}

void AWindupPlayerController::BeginPlay()
{
	Super::BeginPlay();

	if (IsLocalController())
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
		{
			Subsystem->AddMappingContext(CrankInput::Get().Context, 0);
		}
		SetInputMode(FInputModeGameOnly());
		bShowMouseCursor = false;
		bHelpOpen = true;
		HelpAutoHideTime = GetWorld()->GetTimeSeconds() + 12.f;
		if (GetNetMode() == NM_Client)
		{
			UCrankGameInstance::NetLog(FString::Printf(TEXT("connected to host, map %s"), *GetWorld()->GetMapName()));
		}

#if !UE_BUILD_SHIPPING
		// Packaged smoke tests: -CrankAutoShot=<seconds> grabs a screenshot without any input.
		float AutoShot = 0.f;
		if (FParse::Value(FCommandLine::Get(), TEXT("CrankAutoShot="), AutoShot) && AutoShot > 0.f)
		{
			FTimerHandle Handle;
			GetWorldTimerManager().SetTimer(Handle, FTimerDelegate::CreateWeakLambda(this, [this]() { ConsoleCommand(TEXT("HighResShot 1280x720")); }), AutoShot, false);
		}
#endif
	}
}

void AWindupPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();
	if (UEnhancedInputComponent* Input = Cast<UEnhancedInputComponent>(InputComponent))
	{
		const FCrankInputActions& A = CrankInput::Get();
		Input->BindAction(A.Menu, ETriggerEvent::Started, this, &AWindupPlayerController::Input_Menu);
		Input->BindAction(A.Restart, ETriggerEvent::Started, this, &AWindupPlayerController::Input_Restart);
		Input->BindAction(A.Help, ETriggerEvent::Started, this, &AWindupPlayerController::Input_Help);
	}
}

void AWindupPlayerController::PlayerTick(float DeltaTime)
{
	Super::PlayerTick(DeltaTime);

	if (bHelpOpen && HelpAutoHideTime > 0.f && GetWorld()->GetTimeSeconds() > HelpAutoHideTime)
	{
		bHelpOpen = false;
		HelpAutoHideTime = 0.f;
	}

	if (bMenuOpen && WasInputKeyJustPressed(EKeys::Q))
	{
		QuitToMenu();
	}
}

void AWindupPlayerController::Input_Menu(const FInputActionValue& Value)
{
	bMenuOpen = !bMenuOpen;
}

void AWindupPlayerController::Input_Help(const FInputActionValue& Value)
{
	bHelpOpen = !bHelpOpen;
	HelpAutoHideTime = 0.f;
}

void AWindupPlayerController::Input_Restart(const FInputActionValue& Value)
{
	const ARaidGameState* GS = GetWorld()->GetGameState<ARaidGameState>();
	// Result screen: play again. Host waiting for a friend: start now with the AI dummy.
	if (GS && GS->RaidPhase != ERaidPhase::InProgress)
	{
		ServerRequestRestart();
	}
}

void AWindupPlayerController::ServerRequestRestart_Implementation()
{
	if (ARaidGameMode* GM = GetWorld()->GetAuthGameMode<ARaidGameMode>())
	{
		const ARaidGameState* GS = GetWorld()->GetGameState<ARaidGameState>();
		if (GS && GS->RaidPhase == ERaidPhase::Waiting)
		{
			GM->StartWithoutPartner();
		}
		else if (GS && GS->RaidPhase != ERaidPhase::InProgress)
		{
			GM->RestartRaid();
		}
	}
}

void AWindupPlayerController::QuitToMenu()
{
	UGameplayStatics::OpenLevel(this, TEXT("/Game/Crank/Maps/Lvl_MainMenu"));
}

void AWindupPlayerController::ServerCheat_Implementation(FName Command, float Value, FName Arg)
{
#if !UE_BUILD_SHIPPING
	UWorld* World = GetWorld();
	ARaidGameMode* GM = World->GetAuthGameMode<ARaidGameMode>();
	ARaidGameState* GS = World->GetGameState<ARaidGameState>();
	AWindupCharacter* Doll = GetPawn<AWindupCharacter>();
	UE_LOG(LogCrank, Log, TEXT("ServerCheat %s %.1f %s from %s"), *Command.ToString(), Value, *Arg.ToString(), *GetName());

	if (Command == TEXT("Torque") && Doll)
	{
		Doll->GetTorque()->SetTorque(Value);
	}
	else if (Command == TEXT("TorqueAll"))
	{
		for (TActorIterator<AWindupCharacter> It(World); It; ++It)
		{
			It->GetTorque()->SetTorque(Value);
		}
	}
	else if (Command == TEXT("Goto") && GM)
	{
		if (ACrankCheckpoint* CP = GM->FindCheckpoint(Arg))
		{
			int32 Index = 0;
			for (TActorIterator<AWindupCharacter> It(World); It; ++It)
			{
				const FTransform TM = CP->GetSpawnTransform(Index++);
				It->EndAllInteractions();
				It->TeleportTo(TM.GetLocation(), TM.Rotator());
				It->SetBodyState(ECrankBodyState::Normal);
			}
			GM->OnCheckpointReached(CP, Doll);
		}
	}
	else if (Command == TEXT("Dummy") && GM && Doll)
	{
		GM->SpawnDummy(Doll->GetActorLocation() + Doll->GetActorForwardVector() * 160.f, Value);
	}
	else if (Command == TEXT("Rupture") && Doll)
	{
		Doll->GetScatter()->Rupture(nullptr);
	}
	else if (Command == TEXT("Ragdoll") && Doll)
	{
		Doll->StartRagdoll(Doll->GetActorForwardVector() * 200.f + FVector(0, 0, 200.f), 1.f);
	}
	else if (Command == TEXT("Launch") && Doll)
	{
		const FRotator Rot(25.f, GetControlRotation().Yaw, 0.f);
		Doll->GetLaunch()->Launch(Rot.Vector() * (Value > 0.f ? Value : 1800.f), Doll, true, false);
	}
	else if (Command == TEXT("Run") && Doll)
	{
		Doll->DebugRun(Value > 0.f ? Value : 2.f);
	}
	else if (Command == TEXT("LaunchPlayer"))
	{
		if (APlayerController* Other = UGameplayStatics::GetPlayerController(World, FCString::Atoi(*Arg.ToString())))
		{
			if (AWindupCharacter* OtherDoll = Other->GetPawn<AWindupCharacter>())
			{
				const FRotator Rot(25.f, Other->GetControlRotation().Yaw, 0.f);
				OtherDoll->GetLaunch()->Launch(Rot.Vector() * (Value > 0.f ? Value : 1800.f), OtherDoll, true, false);
			}
		}
	}
	else if (Command == TEXT("Time") && GS)
	{
		GS->RaidEndTime = GS->GetServerWorldTimeSeconds() + Value;
	}
	else if (Command == TEXT("Parts") && GS)
	{
		GS->CollectedMask = 7;
	}
	else if (Command.ToString().StartsWith(TEXT("Giant")))
	{
		for (TActorIterator<ABossGiantMonkey> It(World); It; ++It)
		{
			It->DebugCommand(Command, Value);
		}
	}
	else if (Command.ToString().StartsWith(TEXT("Boss")))
	{
		for (TActorIterator<ABossDustEater> It(World); It; ++It)
		{
			It->DebugCommand(Command, Value);
		}
	}
#endif
}
