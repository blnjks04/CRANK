#include "UI/CrankMainMenuWidget.h"
#include "CrankAssets.h"
#include "FX/CrankSound.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/EditableTextBox.h"
#include "Components/Image.h"
#include "Components/Border.h"
#include "Engine/Font.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "GameFramework/PlayerController.h"
#include "Game/CrankGameInstance.h"
#include "Misc/CommandLine.h"
#include "TimerManager.h"
#include "Engine/World.h"

UTextBlock* UCrankMainMenuWidget::MakeText(const FText& Text, int32 Size, const FLinearColor& Color)
{
	UTextBlock* Block = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	Block->SetText(Text);
	FSlateFontInfo Info(CrankAssets::UIFont(), Size);
	Info.OutlineSettings.OutlineSize = Size >= 30 ? 3 : 1;
	Info.OutlineSettings.OutlineColor = FLinearColor(0.f, 0.f, 0.f, 0.8f);
	Block->SetFont(Info);
	Block->SetColorAndOpacity(FSlateColor(Color));
	Block->SetJustification(ETextJustify::Center);
	return Block;
}

UButton* UCrankMainMenuWidget::AddButton(UVerticalBox* Box, const FText& Label, FName Handler)
{
	UButton* Button = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
	Button->SetBackgroundColor(FLinearColor(0.75f, 0.35f, 0.18f, 1.f));
	Button->AddChild(MakeText(Label, 26, FLinearColor(1.f, 0.96f, 0.88f)));
	FScriptDelegate Delegate;
	Delegate.BindUFunction(this, Handler);
	Button->OnClicked.Add(Delegate);
	if (UVerticalBoxSlot* BoxSlot = Box->AddChildToVerticalBox(Button))
	{
		BoxSlot->SetPadding(FMargin(0.f, 8.f));
		BoxSlot->SetHorizontalAlignment(HAlign_Fill);
	}
	return Button;
}

TSharedRef<SWidget> UCrankMainMenuWidget::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("Root"));
		WidgetTree->RootWidget = Root;

		// Warm dim backdrop so the 3D menu scene still shows through.
		UImage* Shade = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
		Shade->SetColorAndOpacity(FLinearColor(0.02f, 0.01f, 0.01f, 0.35f));
		if (UCanvasPanelSlot* ShadeSlot = Root->AddChildToCanvas(Shade))
		{
			ShadeSlot->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
			ShadeSlot->SetOffsets(FMargin(0.f));
		}

		UVerticalBox* Box = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		if (UCanvasPanelSlot* BoxSlot = Root->AddChildToCanvas(Box))
		{
			BoxSlot->SetAnchors(FAnchors(0.08f, 0.12f, 0.08f, 0.12f));
			BoxSlot->SetAutoSize(true);
		}

		Box->AddChildToVerticalBox(MakeText(NSLOCTEXT("Crank", "Title", "와인드업 크루"), 84, FLinearColor(1.f, 0.82f, 0.36f)));
		Box->AddChildToVerticalBox(MakeText(NSLOCTEXT("Crank", "Subtitle", "시계공의 부엌 — 2인 협동 레이드 (MVP)"), 24, FLinearColor(1.f, 0.95f, 0.85f)));

		UTextBlock* Spacer = MakeText(FText::GetEmpty(), 16, FLinearColor::White);
		Box->AddChildToVerticalBox(Spacer);

		AddButton(Box, NSLOCTEXT("Crank", "Host", "방 만들기 (호스트)"), GET_FUNCTION_NAME_CHECKED(UCrankMainMenuWidget, OnHost));
		const TArray<FString> Mine = UCrankGameInstance::GetLocalAddresses();
		Box->AddChildToVerticalBox(MakeText(FText::Format(NSLOCTEXT("Crank", "MyAddress", "내 주소 (친구에게 알려주기): {0}  ·  포트 7777"),
			FText::FromString(Mine.Num() ? FString::Join(Mine, TEXT(" / ")) : FString(TEXT("-")))), 17, FLinearColor(0.75f, 0.95f, 1.f)));

		AddressBox = WidgetTree->ConstructWidget<UEditableTextBox>(UEditableTextBox::StaticClass());
		const FString LastAddress = UCrankGameInstance::LoadLastJoinAddress();
		AddressBox->SetText(FText::FromString(LastAddress.IsEmpty() ? TEXT("127.0.0.1") : LastAddress));
		AddressBox->SetHintText(NSLOCTEXT("Crank", "IpHint", "호스트 IP 주소"));
		if (UVerticalBoxSlot* IpSlot = Box->AddChildToVerticalBox(AddressBox))
		{
			IpSlot->SetPadding(FMargin(0.f, 8.f, 0.f, 0.f));
		}
		AddButton(Box, NSLOCTEXT("Crank", "Join", "참가하기 (IP)"), GET_FUNCTION_NAME_CHECKED(UCrankMainMenuWidget, OnJoin));
		AddButton(Box, NSLOCTEXT("Crank", "Solo", "솔로 테스트 (AI 더미 인형)"), GET_FUNCTION_NAME_CHECKED(UCrankMainMenuWidget, OnSolo));
		AddButton(Box, NSLOCTEXT("Crank", "Quit", "종료"), GET_FUNCTION_NAME_CHECKED(UCrankMainMenuWidget, OnQuit));

		StatusText = MakeText(NSLOCTEXT("Crank", "MenuHint", "혼자서는 한 발짝도 못 걷는 태엽 인형 둘. 서로의 등을 감아주세요."), 18, FLinearColor(0.85f, 0.8f, 0.72f));
		Box->AddChildToVerticalBox(StatusText);
		if (UCrankGameInstance* GI = Cast<UCrankGameInstance>(GetGameInstance()))
		{
			const FText Error = GI->ConsumeNetError();
			if (!Error.IsEmpty())
			{
				StatusText->SetText(Error);
				StatusText->SetColorAndOpacity(FSlateColor(FLinearColor(1.f, 0.55f, 0.45f)));
			}
		}
	}
	return Super::RebuildWidget();
}

void UCrankMainMenuWidget::NativeConstruct()
{
	Super::NativeConstruct();
	static bool bCommandLineHandled = false;
	if (!bCommandLineHandled)
	{
		bCommandLineHandled = true;
		if (FParse::Param(FCommandLine::Get(), TEXT("CrankHost")) || FCommandLine::Get() && FString(FCommandLine::Get()).Contains(TEXT("-CrankJoin=")))
		{
			FTimerHandle Handle;
			GetWorld()->GetTimerManager().SetTimer(Handle, FTimerDelegate::CreateUObject(this, &UCrankMainMenuWidget::RunCommandLineAction), 1.f, false);
		}
	}
}

void UCrankMainMenuWidget::RunCommandLineAction()
{
	FString Address;
	if (FParse::Value(FCommandLine::Get(), TEXT("CrankJoin="), Address) && !Address.IsEmpty())
	{
		if (AddressBox)
		{
			AddressBox->SetText(FText::FromString(Address));
		}
		OnJoin();
	}
	else if (FParse::Param(FCommandLine::Get(), TEXT("CrankHost")))
	{
		OnHost();
	}
}

void UCrankMainMenuWidget::OnHost()
{
	CrankSound::Play2D(this, TEXT("SFX_UIClick"));
	UCrankGameInstance::NetLog(TEXT("hosting a game (listen server)"));
	UGameplayStatics::OpenLevel(this, TEXT("/Game/Crank/Maps/Lvl_Kitchen"), true, TEXT("listen"));
}

void UCrankMainMenuWidget::OnJoin()
{
	CrankSound::Play2D(this, TEXT("SFX_UIClick"));
	if (APlayerController* PC = GetOwningPlayer())
	{
		const FString Address = AddressBox ? AddressBox->GetText().ToString().TrimStartAndEnd() : TEXT("127.0.0.1");
		UCrankGameInstance::SaveLastJoinAddress(Address);
		UCrankGameInstance::NetLog(FString::Printf(TEXT("joining %s"), *Address));
		if (StatusText)
		{
			StatusText->SetText(FText::Format(NSLOCTEXT("Crank", "Joining", "{0} 에 접속 중..."), FText::FromString(Address)));
		}
		PC->ClientTravel(Address.IsEmpty() ? TEXT("127.0.0.1") : Address, TRAVEL_Absolute);
	}
}

void UCrankMainMenuWidget::OnSolo()
{
	CrankSound::Play2D(this, TEXT("SFX_UIClick"));
	UGameplayStatics::OpenLevel(this, TEXT("/Game/Crank/Maps/Lvl_Kitchen"), true, TEXT("dummy"));
}

void UCrankMainMenuWidget::OnQuit()
{
	UKismetSystemLibrary::QuitGame(this, GetOwningPlayer(), EQuitPreference::Quit, false);
}
