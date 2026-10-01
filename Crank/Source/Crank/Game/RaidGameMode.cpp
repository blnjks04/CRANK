#include "Game/RaidGameMode.h"
#include "Game/RaidGameState.h"
#include "Game/CrankGameInstance.h"
#include "Game/RaidPlayerState.h"
#include "Game/WindupPlayerController.h"
#include "Game/DollAIController.h"
#include "UI/RaidHUD.h"
#include "Character/WindupCharacter.h"
#include "Character/TorqueComponent.h"
#include "Character/ScatterReviveComponent.h"
#include "Gameplay/CrankCheckpoint.h"
#include "Gameplay/DeliveryZone.h"
#include "Boss/BossGiantMonkey.h"
#include "CrankBalance.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "GameFramework/PlayerStart.h"
#include "Kismet/GameplayStatics.h"

ARaidGameMode::ARaidGameMode()
{
	PrimaryActorTick.bCanEverTick = true;
	DefaultPawnClass = AWindupCharacter::StaticClass();
	PlayerControllerClass = AWindupPlayerController::StaticClass();
	GameStateClass = ARaidGameState::StaticClass();
	PlayerStateClass = ARaidPlayerState::StaticClass();
	HUDClass = ARaidHUD::StaticClass();
	DummyClass = AWindupCharacter::StaticClass();
	RaidDuration = CrankBalance::RaidDuration;
}

void ARaidGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
	Super::InitGame(MapName, Options, ErrorMessage);
	if (UGameplayStatics::HasOption(Options, TEXT("dummy")))
	{
		bSpawnDummyWhenAlone = true;
	}
	if (UGameplayStatics::HasOption(Options, TEXT("raidtime")))
	{
		RaidDuration = FCString::Atof(*UGameplayStatics::ParseOption(Options, TEXT("raidtime")));
	}
#if WITH_EDITOR
	// Solo play-in-editor sessions get an AI partner (multi-client PIE sessions are left alone).
	if (GetWorld()->IsPlayInEditor() && !UGameplayStatics::HasOption(Options, TEXT("nodummy")))
	{
		bAutoDummyInPIE = true;
	}
#endif
}

ARaidGameState* ARaidGameMode::GetRaidState() const
{
	return GetGameState<ARaidGameState>();
}

void ARaidGameMode::BeginPlay()
{
	Super::BeginPlay();

	if (ARaidGameState* GS = GetRaidState())
	{
		TActorIterator<ABossGiantMonkey> Giant(GetWorld());
		GS->bRequiresGoldenKey = (bool)Giant;
	}

	ActiveCheckpoint = FindCheckpoint(TEXT("Start"));
	if (!ActiveCheckpoint.IsValid())
	{
		ACrankCheckpoint* Lowest = nullptr;
		for (TActorIterator<ACrankCheckpoint> It(GetWorld()); It; ++It)
		{
			if (!Lowest || It->Order < Lowest->Order)
			{
				Lowest = *It;
			}
		}
		ActiveCheckpoint = Lowest;
	}

	if (ARaidGameState* GS = GetRaidState())
	{
		GS->RaidStartTime = 0.f;
		GS->RaidEndTime = RaidDuration;
		if (ActiveCheckpoint.IsValid())
		{
			GS->ZoneName = ActiveCheckpoint->ZoneName;
			GS->ZoneOrder = ActiveCheckpoint->Order;
		}
	}
}

ACrankCheckpoint* ARaidGameMode::FindCheckpoint(FName ZoneId) const
{
	for (TActorIterator<ACrankCheckpoint> It(GetWorld()); It; ++It)
	{
		if (It->ZoneId == ZoneId)
		{
			return *It;
		}
	}
	return nullptr;
}

void ARaidGameMode::PostLogin(APlayerController* NewPlayer)
{
	if (ARaidPlayerState* PS = NewPlayer ? NewPlayer->GetPlayerState<ARaidPlayerState>() : nullptr)
	{
		PS->PlayerIndex = NextPlayerIndex++;
	}
	Super::PostLogin(NewPlayer);

	if (GetNumPlayers() >= 2)
	{
		// A friend joined a solo-started session: the practice dummy steps aside.
		for (TActorIterator<AWindupCharacter> It(GetWorld()); It; ++It)
		{
			if (!It->IsPlayerControlled() && It->GetController() && !bAutoDummyInPIE)
			{
				AController* AI = It->GetController();
				It->Destroy();
				AI->Destroy();
			}
		}
	}
	UCrankGameInstance::NetLog(FString::Printf(TEXT("player joined: %s (%d players, net mode %d)"), *GetNameSafe(NewPlayer), GetNumPlayers(), (int32)GetNetMode()));
	if (!bRaidStarted && ShouldStartRaid())
	{
		StartRaid();
	}
	if (bAutoDummyInPIE && !bSpawnDummyWhenAlone && !GetWorldTimerManager().IsTimerActive(AutoDummyTimer))
	{
		GetWorldTimerManager().SetTimer(AutoDummyTimer, this, &ARaidGameMode::CheckAutoDummy, 2.5f, false);
	}
}

void ARaidGameMode::Logout(AController* Exiting)
{
	Super::Logout(Exiting);
}

AActor* ARaidGameMode::ChoosePlayerStart_Implementation(AController* Player)
{
	if (ActiveCheckpoint.IsValid())
	{
		return ActiveCheckpoint.Get();
	}
	return Super::ChoosePlayerStart_Implementation(Player);
}

void ARaidGameMode::RestartPlayer(AController* NewPlayer)
{
	if (!NewPlayer || NewPlayer->IsPendingKillPending())
	{
		return;
	}

	const ARaidPlayerState* PS = NewPlayer->GetPlayerState<ARaidPlayerState>();
	const int32 Index = PS ? PS->PlayerIndex : 0;

	FTransform SpawnTM;
	if (ActiveCheckpoint.IsValid())
	{
		SpawnTM = ActiveCheckpoint->GetSpawnTransform(Index);
	}
	else if (AActor* Start = ChoosePlayerStart(NewPlayer))
	{
		SpawnTM = FTransform(FRotator(0.f, Start->GetActorRotation().Yaw, 0.f), Start->GetActorLocation());
	}

	APawn* Pawn = SpawnDefaultPawnAtTransform(NewPlayer, SpawnTM);
	if (!Pawn)
	{
		return;
	}
	NewPlayer->SetPawn(Pawn);
	FinishRestartPlayer(NewPlayer, SpawnTM.Rotator());

	if (AWindupCharacter* Doll = Cast<AWindupCharacter>(Pawn))
	{
		Doll->SetPlayerColorIndex(Index);
		Doll->GetTorque()->SetTorque(Index == 0 ? FirstPlayerTorque : OtherPlayerTorque);
	}

	if (bSpawnDummyWhenAlone && GetNumPlayers() == 1)
	{
		SpawnDummy(SpawnTM.GetLocation() + SpawnTM.GetRotation().GetRightVector() * 140.f, 0.f);
	}
}

bool ARaidGameMode::ShouldStartRaid()
{
	return GetNetMode() != NM_ListenServer || GetNumPlayers() >= 2 || bSpawnDummyWhenAlone || bAutoDummyInPIE;
}

void ARaidGameMode::StartWithoutPartner()
{
	if (bRaidStarted)
	{
		return;
	}
	bSpawnDummyWhenAlone = true;
	TArray<AWindupCharacter*> Dolls;
	GetDolls(Dolls);
	if (Dolls.Num() == 1)
	{
		SpawnDummy(Dolls[0]->GetActorLocation() + Dolls[0]->GetActorRightVector() * 140.f, 0.f);
	}
	StartRaid();
}

void ARaidGameMode::CheckAutoDummy()
{
	if (GetNumPlayers() != 1)
	{
		return;
	}
	AWindupCharacter* PlayerDoll = nullptr;
	for (TActorIterator<AWindupCharacter> It(GetWorld()); It; ++It)
	{
		if (!It->IsPlayerControlled())
		{
			return;	// a partner already exists
		}
		PlayerDoll = *It;
	}
	if (PlayerDoll)
	{
		SpawnDummy(PlayerDoll->GetActorLocation() + PlayerDoll->GetActorRightVector() * 140.f, 0.f);
		if (ARaidGameState* GS = GetRaidState())
		{
			GS->MulticastToast(NSLOCTEXT("Crank", "ToastAutoDummy", "혼자 플레이 중: 연습용 AI 인형이 합류했습니다 (태엽을 감아 깨워 주세요)"), FLinearColor(0.7f, 0.9f, 1.f));
		}
	}
}

AWindupCharacter* ARaidGameMode::SpawnDummy(const FVector& Location, float Torque)
{
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
	UClass* Class = DummyClass ? DummyClass.Get() : AWindupCharacter::StaticClass();
	AWindupCharacter* Dummy = GetWorld()->SpawnActor<AWindupCharacter>(Class, Location, FRotator::ZeroRotator, Params);
	if (!Dummy)
	{
		return nullptr;
	}
	Dummy->AIControllerClass = ADollAIController::StaticClass();
	Dummy->SpawnDefaultController();
	Dummy->SetPlayerColorIndex(2);
	Dummy->GetTorque()->SetTorque(Torque);
	return Dummy;
}

void ARaidGameMode::StartRaid()
{
	ARaidGameState* GS = GetRaidState();
	if (!GS || bRaidStarted)
	{
		return;
	}
	bRaidStarted = true;
	UCrankGameInstance::NetLog(FString::Printf(TEXT("raid started with %d players"), GetNumPlayers()));
	GS->RaidPhase = ERaidPhase::InProgress;
	GS->RaidStartTime = GS->GetServerWorldTimeSeconds();
	GS->RaidEndTime = GS->RaidStartTime + RaidDuration;
	GS->MulticastToast(NSLOCTEXT("Crank", "ToastStart", "시계공의 부엌 — 자정까지 메인 스프링을 되찾아라!"), FLinearColor(1.f, 0.85f, 0.4f));
}

void ARaidGameMode::GetDolls(TArray<AWindupCharacter*>& OutDolls) const
{
	OutDolls.Reset();
	for (TActorIterator<AWindupCharacter> It(GetWorld()); It; ++It)
	{
		if (It->IsPlayerControlled())
		{
			OutDolls.Add(*It);
		}
	}
}

void ARaidGameMode::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	ARaidGameState* GS = GetRaidState();
	if (!GS || GS->RaidPhase != ERaidPhase::InProgress)
	{
		return;
	}

	const float Remaining = GS->GetRemainingTime();
	if (Remaining <= 0.f)
	{
		EndRaid(false, ERaidFailReason::Midnight);
		return;
	}

	// Clock warnings at 5 min and 1 min.
	static const float Warnings[] = { 300.f, 60.f };
	for (const float W : Warnings)
	{
		if (Remaining <= W && Remaining + DeltaSeconds > W)
		{
			GS->MulticastToast(FText::Format(NSLOCTEXT("Crank", "ToastClock", "괘종시계: 자정까지 {0}분!"), FText::AsNumber(FMath::RoundToInt(W / 60.f))), FLinearColor(1.f, 0.4f, 0.3f));
		}
	}

	TickEmergencyWind(DeltaSeconds);
}

void ARaidGameMode::TickEmergencyWind(float DeltaSeconds)
{
	ARaidGameState* GS = GetRaidState();
	if (!GS || GS->bEmergencyUsed)
	{
		return;
	}
	TArray<AWindupCharacter*> Dolls;
	GetDolls(Dolls);
	if (Dolls.Num() == 0)
	{
		return;
	}
	for (AWindupCharacter* Doll : Dolls)
	{
		if (Doll->GetBodyState() == ECrankBodyState::Scattered || Doll->GetTorque()->GetTimeDischarged() < CrankBalance::EmergencyDelay)
		{
			return;
		}
	}
	GS->bEmergencyUsed = true;
	for (AWindupCharacter* Doll : Dolls)
	{
		Doll->GetTorque()->AddTorque(CrankBalance::EmergencyTorque);
	}
	GS->MulticastToast(NSLOCTEXT("Crank", "ToastEmergency", "비상 태엽 작동! 둘 다 +15 T (레이드당 1회)"), FLinearColor(0.5f, 1.f, 0.6f));
}

void ARaidGameMode::EndRaid(bool bCleared, ERaidFailReason Reason)
{
	ARaidGameState* GS = GetRaidState();
	if (!GS || GS->RaidPhase != ERaidPhase::InProgress)
	{
		return;
	}
	GS->RaidPhase = bCleared ? ERaidPhase::Cleared : ERaidPhase::Failed;
	GS->FailReason = Reason;
	GS->ResultTime = GS->GetServerWorldTimeSeconds();

	if (!bCleared)
	{
		TArray<AWindupCharacter*> Dolls;
		GetDolls(Dolls);
		for (AWindupCharacter* Doll : Dolls)
		{
			Doll->Freeze();
		}
	}
}

void ARaidGameMode::OnPartDelivered(ECrankPartType Type, ADeliveryZone* Zone)
{
	ARaidGameState* GS = GetRaidState();
	if (!GS)
	{
		return;
	}
	if (Type == ECrankPartType::MainSpring || Type == ECrankPartType::GoldenKey)
	{
		if (Type == ECrankPartType::MainSpring)
		{
			GS->bMainSpringDelivered = true;
		}
		else
		{
			GS->bGoldenKeyDelivered = true;
		}
		if (GS->IsGoalComplete())
		{
			GS->MulticastToast(NSLOCTEXT("Crank", "ToastMainSpring", "작업대에 태엽이 감긴다! 저택에 다시 시간이 흐른다!"), FLinearColor(1.f, 0.9f, 0.3f));
			EndRaid(true, ERaidFailReason::None);
		}
		else if (Type == ECrankPartType::MainSpring)
		{
			GS->MulticastToast(NSLOCTEXT("Crank", "ToastSpringWaitKey", "메인 스프링 장착! 이제 짝짝이 대왕의 황금 태엽 열쇠만 꽂으면 된다"), FLinearColor(1.f, 0.9f, 0.3f));
		}
		else
		{
			GS->MulticastToast(NSLOCTEXT("Crank", "ToastKeyWaitSpring", "황금 태엽 열쇠 장착! 먼지먹개가 지키는 메인 스프링만 남았다"), FLinearColor(1.f, 0.9f, 0.3f));
		}
		return;
	}
	const uint8 Bit = ARaidGameState::PartBit(Type);
	if (Bit && (GS->CollectedMask & Bit) == 0)
	{
		GS->CollectedMask |= Bit;
		FText Name;
		switch (Type)
		{
		case ECrankPartType::Gear:			Name = NSLOCTEXT("Crank", "Gear", "톱니"); break;
		case ECrankPartType::SpringCoil:	Name = NSLOCTEXT("Crank", "Coil", "스프링 코일"); break;
		default:							Name = NSLOCTEXT("Crank", "Bearing", "보석 베어링"); break;
		}
		GS->MulticastToast(FText::Format(NSLOCTEXT("Crank", "ToastPart", "부품 회수: {0} ({1}/3)"), Name, FText::AsNumber(GS->NumCollected())), FLinearColor(0.6f, 0.9f, 1.f));
	}
}

void ARaidGameMode::OnDollScattered(AWindupCharacter* Doll)
{
	ARaidGameState* GS = GetRaidState();
	TArray<AWindupCharacter*> Dolls;
	GetDolls(Dolls);
	int32 Scattered = 0;
	for (AWindupCharacter* D : Dolls)
	{
		Scattered += D->GetBodyState() == ECrankBodyState::Scattered ? 1 : 0;
	}
	bool bHasDummy = false;
	for (TActorIterator<AWindupCharacter> It(GetWorld()); It; ++It)
	{
		bHasDummy |= !It->IsPlayerControlled();
	}
	if (Dolls.Num() == 1 && bHasDummy)
	{
		// Solo practice: the AI dummy cannot reassemble anyone, so put the player back together after a moment.
		FTimerHandle Handle;
		TWeakObjectPtr<AWindupCharacter> WeakDoll = Doll;
		GetWorldTimerManager().SetTimer(Handle, FTimerDelegate::CreateWeakLambda(this, [WeakDoll]()
		{
			if (AWindupCharacter* D = WeakDoll.Get())
			{
				D->GetScatter()->Revive();
			}
		}), 6.f, false);
		if (GS)
		{
			GS->MulticastToast(NSLOCTEXT("Crank", "ToastScatterSolo", "펑! 과감기 파열 — 연습 모드라 6초 뒤 자동으로 조립됩니다"), FLinearColor(1.f, 0.5f, 0.3f));
		}
		return;
	}
	if (GS)
	{
		GS->MulticastToast(NSLOCTEXT("Crank", "ToastScatter", "펑! 과감기 파열 — 머리와 태엽을 몸통에 가져다 대세요"), FLinearColor(1.f, 0.5f, 0.3f));
	}
	if (Dolls.Num() > 0 && Scattered >= Dolls.Num())
	{
		EndRaid(false, ERaidFailReason::BothScattered);
	}
}

void ARaidGameMode::OnDollRevived(AWindupCharacter* Doll)
{
	if (ARaidGameState* GS = GetRaidState())
	{
		GS->MulticastToast(NSLOCTEXT("Crank", "ToastRevive", "조립 완료! 다시 움직인다 (30 T)"), FLinearColor(0.6f, 1.f, 0.6f));
	}
}

void ARaidGameMode::OnGiantDefeated()
{
	if (ARaidGameState* GS = GetRaidState())
	{
		GS->bGiantDefeated = true;
		GS->MulticastToast(NSLOCTEXT("Crank", "ToastGiant", "짝짝이 대왕 격파! 떨어진 황금 태엽 열쇠를 작업대로!"), FLinearColor(1.f, 0.9f, 0.3f));
	}
}

void ARaidGameMode::OnBossDefeated()
{
	if (ARaidGameState* GS = GetRaidState())
	{
		GS->bBossDefeated = true;
		GS->MulticastToast(NSLOCTEXT("Crank", "ToastBoss", "먼지먹개 MK-II 격파! 메인 스프링을 함께 들고 식탁보 숏컷으로!"), FLinearColor(1.f, 0.9f, 0.3f));
	}
}

void ARaidGameMode::OnCheckpointReached(ACrankCheckpoint* Checkpoint, AWindupCharacter* Doll)
{
	if (!Checkpoint)
	{
		return;
	}
	const ACrankCheckpoint* Current = ActiveCheckpoint.Get();
	if (Current && Checkpoint->Order <= Current->Order)
	{
		return;
	}
	ActiveCheckpoint = Checkpoint;
	if (ARaidGameState* GS = GetRaidState())
	{
		GS->ZoneName = Checkpoint->ZoneName;
		GS->ZoneOrder = Checkpoint->Order;
		GS->MulticastToast(Checkpoint->ZoneName, FLinearColor(1.f, 1.f, 1.f));
	}
}

void ARaidGameMode::RespawnDoll(AWindupCharacter* Doll)
{
	if (!Doll)
	{
		return;
	}
	ACrankCheckpoint* Checkpoint = ActiveCheckpoint.Get();
	if (!Checkpoint)
	{
		return;
	}
	if (Doll->GetBodyState() == ECrankBodyState::Scattered)
	{
		return;
	}
	Doll->EndAllInteractions();
	const ARaidPlayerState* PS = Doll->GetPlayerState<ARaidPlayerState>();
	const FTransform TM = Checkpoint->GetSpawnTransform(PS ? PS->PlayerIndex : 0);
	Doll->TeleportTo(TM.GetLocation(), TM.Rotator());
	Doll->SetBodyState(ECrankBodyState::Normal);
}

void ARaidGameMode::RestartRaid()
{
	GetWorld()->ServerTravel(TEXT("?restart"), false);
}
