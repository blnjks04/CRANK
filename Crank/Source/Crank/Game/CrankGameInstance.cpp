#include "Game/CrankGameInstance.h"
#include "CrankTypes.h"
#include "Engine/Engine.h"
#include "Misc/ConfigCacheIni.h"
#include "SocketSubsystem.h"
#include "IPAddress.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/DateTime.h"
#include "HAL/PlatformProcess.h"

void UCrankGameInstance::Init()
{
	Super::Init();
	if (GEngine)
	{
		NetworkFailureHandle = GEngine->OnNetworkFailure().AddUObject(this, &UCrankGameInstance::HandleNetworkFailure);
		TravelFailureHandle = GEngine->OnTravelFailure().AddUObject(this, &UCrankGameInstance::HandleTravelFailure);
	}
}

void UCrankGameInstance::Shutdown()
{
	if (GEngine)
	{
		GEngine->OnNetworkFailure().Remove(NetworkFailureHandle);
		GEngine->OnTravelFailure().Remove(TravelFailureHandle);
	}
	Super::Shutdown();
}

FText UCrankGameInstance::ConsumeNetError()
{
	FText Error = LastNetError;
	LastNetError = FText::GetEmpty();
	return Error;
}

void UCrankGameInstance::HandleNetworkFailure(UWorld* World, UNetDriver* NetDriver, ENetworkFailure::Type FailureType, const FString& ErrorString)
{
	UE_LOG(LogCrank, Warning, TEXT("Network failure %s: %s"), ENetworkFailure::ToString(FailureType), *ErrorString);
	NetLog(FString::Printf(TEXT("network failure %s: %s"), ENetworkFailure::ToString(FailureType), *ErrorString));
	switch (FailureType)
	{
	case ENetworkFailure::PendingConnectionFailure:
	case ENetworkFailure::ConnectionTimeout:
		LastNetError = NSLOCTEXT("Crank", "NetFailConnect", "접속 실패: 호스트에 연결할 수 없습니다. IP 주소, 호스트의 방화벽 허용, UDP 7777 포트를 확인하세요.");
		break;
	case ENetworkFailure::ConnectionLost:
		LastNetError = NSLOCTEXT("Crank", "NetFailLost", "상대와의 연결이 끊어졌습니다.");
		break;
	case ENetworkFailure::NetDriverListenFailure:
		LastNetError = NSLOCTEXT("Crank", "NetFailListen", "방 만들기 실패: 포트 7777을 열 수 없습니다. 다른 게임 창이 열려 있는지 확인하세요.");
		break;
	case ENetworkFailure::OutdatedClient:
	case ENetworkFailure::OutdatedServer:
		LastNetError = NSLOCTEXT("Crank", "NetFailVersion", "접속 실패: 두 사람의 게임 버전이 다릅니다. 같은 배포 파일을 사용하세요.");
		break;
	default:
		LastNetError = FText::Format(NSLOCTEXT("Crank", "NetFailOther", "네트워크 오류: {0}"), FText::FromString(ErrorString));
		break;
	}
}

void UCrankGameInstance::HandleTravelFailure(UWorld* World, ETravelFailure::Type FailureType, const FString& ErrorString)
{
	UE_LOG(LogCrank, Warning, TEXT("Travel failure %s: %s"), ETravelFailure::ToString(FailureType), *ErrorString);
	NetLog(FString::Printf(TEXT("travel failure %s: %s"), ETravelFailure::ToString(FailureType), *ErrorString));
	LastNetError = FText::Format(NSLOCTEXT("Crank", "TravelFail", "접속 실패: {0}"), FText::FromString(ErrorString));
}

TArray<FString> UCrankGameInstance::GetLocalAddresses()
{
	TArray<FString> Out;
	ISocketSubsystem* Sockets = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
	TArray<TSharedPtr<FInternetAddr>> Addresses;
	if (Sockets && Sockets->GetLocalAdapterAddresses(Addresses))
	{
		for (const TSharedPtr<FInternetAddr>& Address : Addresses)
		{
			if (!Address.IsValid() || Address->GetProtocolType() != FNetworkProtocolTypes::IPv4)
			{
				continue;
			}
			const FString Text = Address->ToString(false);
			if (!Text.StartsWith(TEXT("127.")) && !Text.StartsWith(TEXT("169.254.")) && !Text.StartsWith(TEXT("0.")) && !Out.Contains(Text))
			{
				Out.Add(Text);
			}
		}
	}
	return Out;
}

void UCrankGameInstance::NetLog(const FString& Line)
{
	const FString Path = FPaths::ProjectLogDir() / TEXT("CrankNet.log");
	const FString Stamped = FString::Printf(TEXT("[%s pid %u] %s") LINE_TERMINATOR, *FDateTime::Now().ToString(TEXT("%Y-%m-%d %H:%M:%S")), FPlatformProcess::GetCurrentProcessId(), *Line);
	FFileHelper::SaveStringToFile(Stamped, *Path, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM, &IFileManager::Get(), FILEWRITE_Append);
	UE_LOG(LogCrank, Log, TEXT("%s"), *Line);
}

FString UCrankGameInstance::LoadLastJoinAddress()
{
	FString Address;
	if (GConfig)
	{
		GConfig->GetString(TEXT("Crank"), TEXT("LastJoinAddress"), Address, GGameUserSettingsIni);
	}
	return Address;
}

void UCrankGameInstance::SaveLastJoinAddress(const FString& Address)
{
	if (GConfig)
	{
		GConfig->SetString(TEXT("Crank"), TEXT("LastJoinAddress"), *Address, GGameUserSettingsIni);
		GConfig->Flush(false, GGameUserSettingsIni);
	}
}
