#include "UI/RaidHUD.h"
#include "Game/RaidGameState.h"
#include "Game/WindupPlayerController.h"
#include "Game/CrankGameInstance.h"
#include "Character/WindupCharacter.h"
#include "Character/TorqueComponent.h"
#include "Character/WindInteractionComponent.h"
#include "Character/GrabComponent.h"
#include "Character/ScatterReviveComponent.h"
#include "Boss/BossDustEater.h"
#include "Boss/BossGiantMonkey.h"
#include "Gameplay/Grabbable.h"
#include "CrankAssets.h"
#include "CrankBalance.h"
#include "FX/CrankSound.h"
#include "Engine/Canvas.h"
#include "Engine/Font.h"
#include "Engine/Engine.h"
#include "CanvasItem.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/SkeletalMeshComponent.h"
#include "Framework/Application/SlateApplication.h"
#include "Fonts/FontMeasure.h"
#include "Rendering/SlateRenderer.h"
#include "RenderUtils.h"

namespace
{
	const FLinearColor ColPanel(0.03f, 0.025f, 0.02f, 0.62f);
	const FLinearColor ColGold(1.f, 0.82f, 0.38f, 1.f);
	const FLinearColor ColCream(1.f, 0.96f, 0.88f, 1.f);
	const FLinearColor ColDim(0.7f, 0.66f, 0.6f, 1.f);
	const FLinearColor ColRed(1.f, 0.32f, 0.25f, 1.f);
	const FLinearColor ColGreen(0.45f, 1.f, 0.5f, 1.f);

	FString FormatTime(float Seconds)
	{
		const int32 Total = FMath::Max(0, FMath::CeilToInt(Seconds));
		return FString::Printf(TEXT("%02d:%02d"), Total / 60, Total % 60);
	}
}

// ------------------------------------------------------------------------------------------
// Primitives

void ARaidHUD::Text(const FString& Str, float X, float Y, float Size, const FLinearColor& Color, bool bCenter, bool bOutline)
{
	if (!Font || Str.IsEmpty())
	{
		return;
	}
	FSlateFontInfo Info(Font, FMath::Max(6, FMath::RoundToInt(Size * S)));
	if (bOutline)
	{
		Info.OutlineSettings.OutlineSize = FMath::Max(1, FMath::RoundToInt(2.f * S));
		Info.OutlineSettings.OutlineColor = FLinearColor(0.f, 0.f, 0.f, 0.85f * Color.A);
	}
	FCanvasTextItem Item(FVector2D(X, Y), FText::FromString(Str), Info, Color);
	Item.bCentreX = bCenter;
	Item.BlendMode = SE_BLEND_Translucent;
	Canvas->DrawItem(Item);
}

FVector2D ARaidHUD::MeasureText(const FString& Str, float Size)
{
	if (!Font || !FSlateApplication::IsInitialized())
	{
		return FVector2D(Str.Len() * Size * 0.6f * S, Size * S);
	}
	const FSlateFontInfo Info(Font, FMath::Max(6, FMath::RoundToInt(Size * S)));
	return FSlateApplication::Get().GetRenderer()->GetFontMeasureService()->Measure(Str, Info);
}

void ARaidHUD::Panel(float X, float Y, float W, float H, const FLinearColor& Color)
{
	FCanvasTileItem Tile(FVector2D(X, Y), FVector2D(W, H), Color);
	Tile.BlendMode = SE_BLEND_Translucent;
	Canvas->DrawItem(Tile);
}

void ARaidHUD::Disc(const FVector2D& Center, float Radius, const FLinearColor& Color, int32 Sides)
{
	FCanvasNGonItem NGon(Center, FVector2D(Radius, Radius), Sides, Color);
	NGon.BlendMode = SE_BLEND_Translucent;
	Canvas->DrawItem(NGon);
}

void ARaidHUD::Line(const FVector2D& A, const FVector2D& B, float Thickness, const FLinearColor& Color)
{
	FCanvasLineItem Item(A, B);
	Item.LineThickness = Thickness;
	Item.SetColor(Color);
	Item.BlendMode = SE_BLEND_Translucent;
	Canvas->DrawItem(Item);
}

void ARaidHUD::Ring(const FVector2D& Center, float Radius, float Thickness, float Fraction, const FLinearColor& Color, float StartAngleDeg)
{
	Fraction = FMath::Clamp(Fraction, 0.f, 1.f);
	if (Fraction <= 0.f)
	{
		return;
	}
	const int32 Segments = FMath::Max(3, FMath::CeilToInt(64 * Fraction));
	const float Start = FMath::DegreesToRadians(StartAngleDeg);
	const float Sweep = 2.f * PI * Fraction;
	const float Inner = Radius - Thickness * 0.5f;
	const float Outer = Radius + Thickness * 0.5f;
	TArray<FCanvasUVTri> Tris;
	Tris.Reserve(Segments * 2);
	for (int32 i = 0; i < Segments; ++i)
	{
		const float A0 = Start + Sweep * i / Segments;
		const float A1 = Start + Sweep * (i + 1) / Segments;
		const FVector2D D0(FMath::Cos(A0), FMath::Sin(A0));
		const FVector2D D1(FMath::Cos(A1), FMath::Sin(A1));
		FCanvasUVTri T1, T2;
		T1.V0_Pos = Center + D0 * Inner; T1.V1_Pos = Center + D0 * Outer; T1.V2_Pos = Center + D1 * Outer;
		T2.V0_Pos = Center + D0 * Inner; T2.V1_Pos = Center + D1 * Outer; T2.V2_Pos = Center + D1 * Inner;
		T1.V0_Color = T1.V1_Color = T1.V2_Color = Color;
		T2.V0_Color = T2.V1_Color = T2.V2_Color = Color;
		Tris.Add(T1);
		Tris.Add(T2);
	}
	FCanvasTriangleItem Item(Tris, GWhiteTexture);
	Item.BlendMode = SE_BLEND_Translucent;
	Canvas->DrawItem(Item);
}

void ARaidHUD::Star(const FVector2D& Center, float Radius, const FLinearColor& Color)
{
	TArray<FCanvasUVTri> Tris;
	for (int32 i = 0; i < 10; ++i)
	{
		const float A0 = -PI * 0.5f + i * PI / 5.f;
		const float A1 = -PI * 0.5f + (i + 1) * PI / 5.f;
		const float R0 = (i % 2 == 0) ? Radius : Radius * 0.45f;
		const float R1 = (i % 2 == 0) ? Radius * 0.45f : Radius;
		FCanvasUVTri T;
		T.V0_Pos = Center;
		T.V1_Pos = Center + FVector2D(FMath::Cos(A0), FMath::Sin(A0)) * R0;
		T.V2_Pos = Center + FVector2D(FMath::Cos(A1), FMath::Sin(A1)) * R1;
		T.V0_Color = T.V1_Color = T.V2_Color = Color;
		Tris.Add(T);
	}
	FCanvasTriangleItem Item(Tris, GWhiteTexture);
	Item.BlendMode = SE_BLEND_Translucent;
	Canvas->DrawItem(Item);
}

FLinearColor ARaidHUD::TorqueColor(float Torque) const
{
	if (Torque > 100.f)	return ColRed;
	if (Torque >= 70.f)	return FLinearColor(0.45f, 0.85f, 1.f);
	if (Torque >= 30.f)	return ColGreen;
	if (Torque > 0.f)	return FLinearColor(1.f, 0.78f, 0.3f);
	return FLinearColor(0.55f, 0.55f, 0.55f);
}

// ------------------------------------------------------------------------------------------

void ARaidHUD::DrawHUD()
{
	Super::DrawHUD();
	if (!Canvas)
	{
		return;
	}
	if (!Font)
	{
		Font = CrankAssets::UIFont();
	}
	S = FMath::Max(0.5f, Canvas->ClipY / 1080.f);

	ARaidGameState* GS = GetWorld()->GetGameState<ARaidGameState>();
	const AWindupCharacter* Me = Cast<AWindupCharacter>(GetOwningPawn());

	DrawVignettes(Me);
	if (GS)
	{
		DrawClock(GS);
		DrawParts(GS);
	}
	DrawBoss();
	DrawGiant();
	if (Me)
	{
		DrawPartnerIcons(Me);
		DrawWinding(Me);
		DrawAim(Me);
		DrawPrompts(Me);
	}
	if (GS)
	{
		DrawToasts(GS);
		DrawResult(GS);
		DrawWaiting(GS);
	}

	if (const AWindupPlayerController* PC = Cast<AWindupPlayerController>(GetOwningPlayerController()))
	{
		if (PC->IsHelpOpen())
		{
			DrawHelp();
		}
		if (PC->IsMenuOpen())
		{
			DrawMenu();
		}
	}
}

void ARaidHUD::DrawVignettes(const AWindupCharacter* Me)
{
	if (!Me)
	{
		return;
	}
	const float W = Canvas->ClipX;
	const float H = Canvas->ClipY;
	auto Border = [this, W, H](const FLinearColor& C, float Thick)
	{
		for (int32 i = 0; i < 6; ++i)
		{
			const float T = Thick * (i + 1) / 6.f;
			const FLinearColor Layer(C.R, C.G, C.B, C.A / 6.f);
			Panel(0, 0, W, T, Layer);
			Panel(0, H - T, W, T, Layer);
			Panel(0, 0, T, H, Layer);
			Panel(W - T, 0, T, H, Layer);
		}
	};
	if (Me->IsInColdZone())
	{
		Border(FLinearColor(0.55f, 0.8f, 1.f, 0.35f), 90.f * S);
	}
	if (Me->IsDusty())
	{
		Border(FLinearColor(0.35f, 0.27f, 0.18f, 0.45f), 120.f * S);
	}
	if (Me->GetTorque()->GetTorque() > 100.f)
	{
		const float Pulse = 0.5f + 0.5f * FMath::Sin(GetWorld()->GetTimeSeconds() * 10.f);
		Border(FLinearColor(1.f, 0.2f, 0.1f, 0.25f * Pulse), 70.f * S);
	}
}

void ARaidHUD::DrawClock(const ARaidGameState* GS)
{
	const float Remaining = GS->GetRemainingTime();
	const float CX = Canvas->ClipX * 0.5f;
	const float Y = 18.f * S;
	const float W = 300.f * S;
	const float H = 58.f * S;
	Panel(CX - W * 0.5f, Y, W, H, ColPanel);

	// Little clock face: the hand sweeps toward midnight.
	const FVector2D Face(CX - W * 0.5f + 32.f * S, Y + H * 0.5f);
	Disc(Face, 21.f * S, FLinearColor(0.95f, 0.9f, 0.78f, 0.95f));
	Ring(Face, 21.f * S, 3.f * S, 1.f, ColGold);
	const float Frac = 1.f - Remaining / FMath::Max(1.f, CrankBalance::RaidDuration);
	const float HandAngle = FMath::DegreesToRadians(-90.f + 360.f * Frac);
	Line(Face, Face + FVector2D(FMath::Cos(HandAngle), FMath::Sin(HandAngle)) * 16.f * S, 2.5f * S, FLinearColor(0.15f, 0.1f, 0.08f));
	Line(Face, Face + FVector2D(0.f, -12.f * S), 2.f * S, FLinearColor(0.6f, 0.1f, 0.08f));

	FLinearColor TimeColor = ColCream;
	if (Remaining < 180.f)
	{
		const float Pulse = Remaining < 60.f ? 0.5f + 0.5f * FMath::Sin(GetWorld()->GetTimeSeconds() * 8.f) : 1.f;
		TimeColor = FMath::Lerp(ColCream, ColRed, Pulse);
	}
	Text(TEXT("자정까지"), CX - 52.f * S, Y + 7.f * S, 15.f, ColDim);
	Text(FormatTime(Remaining), CX - 52.f * S, Y + 22.f * S, 26.f, TimeColor);
	Text(GS->ZoneName.ToString(), CX + 60.f * S, Y + 20.f * S, 16.f, ColGold);
}

void ARaidHUD::DrawWaiting(const ARaidGameState* GS)
{
	if (GS->RaidPhase != ERaidPhase::Waiting || GetNetMode() != NM_ListenServer)
	{
		return;
	}
	if (!bAddressesQueried)
	{
		bAddressesQueried = true;
		HostAddresses = UCrankGameInstance::GetLocalAddresses();
	}
	// Left of center so the auto-shown controls card (top right) stays readable.
	const float W = 940.f * S;
	const float H = 250.f * S;
	const float CX = FMath::Max(Canvas->ClipX * 0.41f, W * 0.5f + 20.f * S);
	const float X = CX - W * 0.5f;
	const float Y = Canvas->ClipY * 0.26f;
	Panel(X, Y, W, H, FLinearColor(0.05f, 0.03f, 0.02f, 0.82f));
	const float Dots = FMath::Fmod(GetWorld()->GetRealTimeSeconds(), 3.f);
	Text(FString::Printf(TEXT("친구를 기다리는 중%s"), Dots < 1.f ? TEXT(".") : (Dots < 2.f ? TEXT("..") : TEXT("..."))), CX, Y + 16.f * S, 34.f, ColGold, true);
	Text(TEXT("친구가 메인 메뉴의 '참가하기'에 아래 주소를 넣으면 레이드가 시작됩니다"), CX, Y + 70.f * S, 18.f, ColCream, true);
	const int32 Port = GetWorld()->URL.Port;
	const FString Addresses = HostAddresses.Num() ? FString::Join(HostAddresses, TEXT("   /   ")) : FString(TEXT("(주소를 찾지 못했습니다)"));
	Text(FString::Printf(TEXT("내 주소:  %s"), *Addresses), CX, Y + 104.f * S, 24.f, FLinearColor(0.75f, 0.95f, 1.f), true);
	Text(FString::Printf(TEXT("포트 %d (UDP) · 같은 공유기면 위 주소 그대로 · 인터넷이면 포트포워딩 또는 Radmin VPN·ZeroTier 주소"), Port), CX, Y + 146.f * S, 15.f, ColDim, true);
	Text(TEXT("[R] 친구 없이 시작 (연습용 AI 인형)      [Esc / M] 메뉴"), CX, Y + 196.f * S, 18.f, ColCream, true);
}

void ARaidHUD::DrawParts(const ARaidGameState* GS)
{
	const float X = 22.f * S;
	const float Y = 18.f * S;
	const int32 NumSlots = GS->bRequiresGoldenKey ? 5 : 4;
	Panel(X, Y, (18.f + NumSlots * 58.f) * S, 118.f * S, ColPanel);
	Text(TEXT("회수한 부품"), X + 12.f * S, Y + 8.f * S, 15.f, ColGold);

	struct FSlot { const TCHAR* Name; bool bHave; };
	const FSlot Slots[5] = {
		{ TEXT("톱니"), (GS->CollectedMask & 1) != 0 },
		{ TEXT("코일"), (GS->CollectedMask & 2) != 0 },
		{ TEXT("베어링"), (GS->CollectedMask & 4) != 0 },
		{ TEXT("메인"), GS->bMainSpringDelivered },
		{ TEXT("열쇠"), GS->bGoldenKeyDelivered },
	};
	for (int32 i = 0; i < NumSlots; ++i)
	{
		const FVector2D C(X + (36.f + i * 58.f) * S, Y + 62.f * S);
		const bool bMain = i >= 3;
		const FLinearColor On = bMain ? ColGold : FLinearColor(0.6f, 0.85f, 1.f);
		Disc(C, 20.f * S, Slots[i].bHave ? FLinearColor(On.R, On.G, On.B, 0.9f) : FLinearColor(0.2f, 0.2f, 0.2f, 0.7f), i == 0 ? 8 : 24);
		Ring(C, 20.f * S, 2.5f * S, 1.f, bMain ? ColGold : ColDim);
		if (i == 1 || i == 3)
		{
			Ring(C, 11.f * S, 2.f * S, 1.f, Slots[i].bHave ? FLinearColor(0.1f, 0.1f, 0.1f) : ColDim);
		}
		Text(Slots[i].Name, C.X, C.Y + 24.f * S, 13.f, Slots[i].bHave ? ColCream : ColDim, true);
	}
}

void ARaidHUD::DrawBoss()
{
	const ABossDustEater* Boss = nullptr;
	for (TActorIterator<ABossDustEater> It(GetWorld()); It; ++It)
	{
		Boss = *It;
		break;
	}
	if (!Boss || !Boss->IsActive())
	{
		return;
	}
	if (const APawn* Me = GetOwningPawn(); Me && FVector::Dist2D(Me->GetActorLocation(), Boss->GetActorLocation()) > 4500.f)
	{
		return;
	}

	const float CX = Canvas->ClipX * 0.5f;
	const float Y = 84.f * S;
	const float W = 360.f * S;
	Panel(CX - W * 0.5f, Y, W, 54.f * S, FLinearColor(0.08f, 0.02f, 0.02f, 0.7f));
	Text(FString::Printf(TEXT("먼지먹개 MK-II  ·  P%d"), Boss->GetPhase()), CX - W * 0.5f + 14.f * S, Y + 15.f * S, 18.f, ColCream);

	for (int32 i = 0; i < 3; ++i)
	{
		const float BX = CX + W * 0.5f - (40.f + i * 34.f) * S;
		const bool bHave = i < Boss->GetCells();
		Panel(BX, Y + 12.f * S, 26.f * S, 30.f * S, FLinearColor(0.f, 0.f, 0.f, 0.6f));
		Panel(BX + 3.f * S, Y + 15.f * S, 20.f * S, 24.f * S, bHave ? FLinearColor(0.35f, 1.f, 0.4f, 0.95f) : FLinearColor(0.25f, 0.25f, 0.25f, 0.7f));
		Panel(BX + 8.f * S, Y + 8.f * S, 10.f * S, 4.f * S, ColDim);
	}

	const float Stun = Boss->GetStunRemaining();
	if (Stun > 0.f)
	{
		const float Frac = Stun / FMath::Max(0.1f, CrankBalance::BossStunTime(Boss->GetPhase()));
		Panel(CX - W * 0.5f, Y + 50.f * S, W * FMath::Clamp(Frac, 0.f, 1.f), 5.f * S, FLinearColor(1.f, 0.85f, 0.2f, 0.9f));
		Text(TEXT("기절! 경사판으로 올라가 배터리 셀을 뽑아라"), CX, Y + 60.f * S, 17.f, FLinearColor(1.f, 0.9f, 0.3f), true);
	}
	else if (Boss->GetBossState() == EBossState::ReturnToDock || Boss->GetBossState() == EBossState::Docking)
	{
		Text(TEXT("충전 독으로 복귀 중! 발사로 기절시키거나 행주로 독을 덮어라"), CX, Y + 60.f * S, 17.f, FLinearColor(0.5f, 0.75f, 1.f), true);
	}
}

void ARaidHUD::DrawGiant()
{
	const ABossGiantMonkey* Giant = nullptr;
	for (TActorIterator<ABossGiantMonkey> It(GetWorld()); It; ++It)
	{
		Giant = *It;
		break;
	}
	if (!Giant || !Giant->IsActive())
	{
		return;
	}
	if (const APawn* Me = GetOwningPawn(); Me && FVector::Dist2D(Me->GetActorLocation(), Giant->GetActorLocation()) > 6500.f)
	{
		return;
	}

	const float CX = Canvas->ClipX * 0.5f;
	const float Y = 84.f * S;
	const float W = 560.f * S;
	Panel(CX - W * 0.5f, Y, W, 54.f * S, FLinearColor(0.1f, 0.03f, 0.01f, 0.72f));
	Text(FString::Printf(TEXT("짝짝이 대왕  ·  P%d"), Giant->GetPhase()), CX - W * 0.5f + 14.f * S, Y + 15.f * S, 18.f, ColCream);

	// HP bar (100) with the phase notches at 66 / 33.
	const float BarX = CX - W * 0.5f + 222.f * S;
	const float BarW = W - 236.f * S;
	const float BarY = Y + 17.f * S;
	const float BarH = 20.f * S;
	const float HPFrac = FMath::Clamp((float)Giant->GetHP() / (float)CrankBalance::GiantMaxHP, 0.f, 1.f);
	const FLinearColor BarColor = Giant->GetPhase() <= 1 ? FLinearColor(1.f, 0.76f, 0.22f) : (Giant->GetPhase() == 2 ? FLinearColor(1.f, 0.5f, 0.15f) : FLinearColor(0.95f, 0.22f, 0.12f));
	const float Flash = FMath::Clamp(1.f - (Giant->BossNow() - Giant->GetLastHitTime()) / 0.25f, 0.f, 1.f);
	Panel(BarX, BarY, BarW, BarH, FLinearColor(0.12f, 0.1f, 0.09f, 0.9f));
	Panel(BarX, BarY, BarW * HPFrac, BarH, FMath::Lerp(BarColor, FLinearColor::White, Flash * 0.7f));
	for (const float Notch : { 0.66f, 0.33f })
	{
		Panel(BarX + BarW * Notch - 1.f * S, BarY - 3.f * S, 2.f * S, BarH + 6.f * S, FLinearColor(0.f, 0.f, 0.f, 0.8f));
	}
	Text(FString::Printf(TEXT("%d / %d"), Giant->GetHP(), CrankBalance::GiantMaxHP), BarX + BarW * 0.5f, BarY + 1.f * S, 14.f, ColCream, true);

	// Damage numbers rising from the hit points.
	if (APlayerController* ViewPC = GetOwningPlayerController())
	{
		const float LocalNow = GetWorld()->GetTimeSeconds();
		for (const ABossGiantMonkey::FDamagePopup& Popup : Giant->GetDamagePopups())
		{
			const float Age = LocalNow - Popup.Time;
			if (Age < 0.f || Age > 1.2f)
			{
				continue;
			}
			FVector2D Screen;
			if (!UGameplayStatics::ProjectWorldToScreen(ViewPC, Popup.Location + FVector(0.f, 0.f, 120.f + Age * 160.f), Screen))
			{
				continue;
			}
			const float Alpha = FMath::Clamp((1.2f - Age) / 0.4f, 0.f, 1.f);
			const FLinearColor Color = Popup.bKey ? FLinearColor(1.f, 0.85f, 0.25f, Alpha) : FLinearColor(1.f, 0.97f, 0.9f, Alpha);
			Text(FString::Printf(TEXT("-%d"), Popup.Amount), Screen.X, Screen.Y, Popup.bKey ? 40.f : 26.f, Color, true);
		}
	}

	FString Hint = TEXT("친구를 발사해 부딪히면 피해 · 등 뒤 태엽 열쇠는 큰 피해");
	FLinearColor HintColor = ColGold;
	const float T = Giant->GetStateTime();
	switch (Giant->GetGiantState())
	{
	case EGiantState::ClapWindup:
	case EGiantState::Clap:
		Hint = TEXT("짝! 바닥 충격파 — 고리가 다가오면 점프로 넘어라 (준비 동작 중 얼굴에 발사하면 끊긴다)");
		HintColor = FLinearColor(1.f, 0.6f, 0.35f);
		break;
	case EGiantState::HopWindup:
	case EGiantState::Hop:
		Hint = TEXT("쿵! 내려찍기 — 착지 지점에서 멀어져라");
		HintColor = FLinearColor(1.f, 0.6f, 0.35f);
		break;
	case EGiantState::ThrowWindup:
	case EGiantState::Throw:
		Hint = TEXT("심벌즈 부메랑! 날아오는 원반을 피하라");
		HintColor = FLinearColor(1.f, 0.6f, 0.35f);
		break;
	case EGiantState::WoundDown:
	{
		const float Left = FMath::Max(0.f, Giant->GetStateDuration() - T);
		Hint = FString::Printf(TEXT("태엽이 풀렸다! 지금 등 뒤 열쇠를 맞히면 치명타 (%.0f초)"), Left);
		HintColor = FLinearColor(1.f, 0.92f, 0.35f);
		const float Frac = Left / FMath::Max(0.1f, Giant->GetStateDuration());
		Panel(CX - W * 0.5f, Y + 50.f * S, W * FMath::Clamp(Frac, 0.f, 1.f), 5.f * S, FLinearColor(1.f, 0.85f, 0.2f, 0.9f));
		break;
	}
	case EGiantState::Rewind:
		Hint = TEXT("다시 태엽을 감는 중...");
		break;
	case EGiantState::Stagger:
		Hint = TEXT("얼굴에 명중! 공격이 끊겼다");
		HintColor = FLinearColor(0.6f, 1.f, 0.6f);
		break;
	case EGiantState::PhaseTransition:
		Hint = TEXT("화났다! 거대한 충격파 주의");
		HintColor = FLinearColor(1.f, 0.4f, 0.3f);
		break;
	default:
		break;
	}
	Text(Hint, CX, Y + 60.f * S, 17.f, HintColor, true);
}

void ARaidHUD::DrawPartnerIcons(const AWindupCharacter* Me)
{
	APlayerController* PC = GetOwningPlayerController();
	if (!PC || !PC->PlayerCameraManager)
	{
		return;
	}
	const FVector CamLoc = PC->PlayerCameraManager->GetCameraLocation();
	const FVector CamFwd = PC->PlayerCameraManager->GetCameraRotation().Vector();

	for (TActorIterator<AWindupCharacter> It(GetWorld()); It; ++It)
	{
		const AWindupCharacter* Other = *It;
		if (Other == Me)
		{
			continue;
		}
		// Float above the head bone so lying / crawling / ragdolling partners keep the icon close.
		FVector Anchor = Other->GetActorLocation() + FVector(0.f, 0.f, 50.f);
		if (const USkeletalMeshComponent* PartnerMesh = Other->GetMesh(); PartnerMesh && PartnerMesh->GetBoneIndex(TEXT("head")) != INDEX_NONE)
		{
			Anchor = PartnerMesh->GetBoneLocation(TEXT("head"));
		}
		const FVector WorldPos = Anchor + FVector(0.f, 0.f, 45.f);
		const FVector Screen3 = Project(WorldPos, false);
		FVector2D P(Screen3.X, Screen3.Y);
		const bool bBehind = FVector::DotProduct(WorldPos - CamLoc, CamFwd) < 0.f;
		const float Margin = 50.f * S;
		const bool bOffscreen = bBehind || P.X < Margin || P.Y < Margin || P.X > Canvas->ClipX - Margin || P.Y > Canvas->ClipY - Margin;
		if (bBehind)
		{
			P = FVector2D(Canvas->ClipX - P.X, Canvas->ClipY - Margin);
		}
		P.X = FMath::Clamp(P.X, Margin, Canvas->ClipX - Margin);
		P.Y = FMath::Clamp(P.Y, Margin + 140.f * S, Canvas->ClipY - Margin);

		const float T = Other->GetTorque()->GetTorque();
		const float R = (bOffscreen ? 20.f : 24.f) * S;
		const FLinearColor Paint = Other->GetPlayerColor();
		Disc(P, R + 4.f * S, FLinearColor(0.f, 0.f, 0.f, 0.55f));
		Ring(P, R, 5.f * S, 1.f, FLinearColor(0.2f, 0.2f, 0.2f, 0.8f));
		Ring(P, R, 5.f * S, FMath::Min(T, 100.f) / 100.f, TorqueColor(T));
		if (T > 100.f)
		{
			const float Pulse = 0.5f + 0.5f * FMath::Sin(GetWorld()->GetTimeSeconds() * 12.f);
			Ring(P, R + 6.f * S, 3.f * S, (T - 100.f) / 20.f, FLinearColor(1.f, 0.2f, 0.1f, 0.5f + 0.5f * Pulse));
		}
		Disc(P, R * 0.55f, FLinearColor(Paint.R, Paint.G, Paint.B, 0.9f));

		FString Status = FString::Printf(TEXT("%d"), FMath::RoundToInt(T));
		switch (Other->GetBodyState())
		{
		case ECrankBodyState::Scattered:	Status = TEXT("파열!"); break;
		case ECrankBodyState::Trapped:		Status = TEXT("갇힘!"); break;
		case ECrankBodyState::Carried:		Status = TEXT("업힘"); break;
		default: break;
		}
		Text(Status, P.X, P.Y - 9.f * S, 15.f, ColCream, true);
		if (Other->IsCrawling())
		{
			Text(TEXT("방전"), P.X, P.Y + R + 2.f * S, 13.f, ColDim, true);
		}
	}
}

void ARaidHUD::DrawWinding(const AWindupCharacter* Me)
{
	const UWindInteractionComponent* Wind = Me->GetWind();
	const FVector2D C(Canvas->ClipX * 0.5f, Canvas->ClipY * 0.5f);

	if (!Wind->IsWinding())
	{
		Disc(C, 3.f * S, FLinearColor(1.f, 1.f, 1.f, 0.7f), 12);
		return;
	}

	const AWindupCharacter* Target = Wind->GetWindTarget();
	const float T = Target ? Target->GetTorque()->GetTorque() : 0.f;
	const float Now = GetWorld()->GetTimeSeconds();

	if (!Wind->IsPulling())
	{
		// Turn progress ring + partner torque.
		const float Radius = 80.f * S;
		Ring(C, Radius, 10.f * S, 1.f, FLinearColor(0.f, 0.f, 0.f, 0.45f));
		Ring(C, Radius, 10.f * S, Wind->GetTurnProgress(), FLinearColor(1.f, 0.85f, 0.4f, 0.9f));
		Ring(C, Radius + 16.f * S, 6.f * S, FMath::Min(T, 100.f) / 100.f, TorqueColor(T));
		if (T > 100.f)
		{
			Ring(C, Radius + 26.f * S, 6.f * S, (T - 100.f) / 20.f, ColRed);
		}
		Text(FString::Printf(TEXT("%d T"), FMath::RoundToInt(T)), C.X, C.Y - 16.f * S, 30.f, TorqueColor(T), true);

		const float SinceTurn = Now - Wind->GetLastTurnTime();
		if (SinceTurn < 0.45f)
		{
			const EWindTurnResult Result = Wind->GetLastTurnResult();
			if (Result == EWindTurnResult::Rhythm || Result == EWindTurnResult::Capped)
			{
				Text(TEXT("리듬!"), C.X, C.Y + 24.f * S, 22.f, ColGreen, true);
			}
			else if (Result == EWindTurnResult::Overwind)
			{
				Text(TEXT("과감기 위험!"), C.X, C.Y + 24.f * S, 22.f, ColRed, true);
			}
		}
		if (Wind->GetLastTurnResult() == EWindTurnResult::Slip && SinceTurn < 1.2f)
		{
			Text(TEXT("미끄러짐!"), C.X, C.Y + 24.f * S, 26.f, ColRed, true);
		}

		// Rhythm guide: a dot that orbits once per 0.4 s.
		const float GuideAngle = FMath::Fmod(Now / 0.4f, 1.f) * 2.f * PI - PI * 0.5f;
		Disc(C + FVector2D(FMath::Cos(GuideAngle), FMath::Sin(GuideAngle)) * (Radius - 22.f * S), 5.f * S, FLinearColor(0.5f, 1.f, 0.6f, 0.6f), 12);
	}
}

void ARaidHUD::DrawAim(const AWindupCharacter* Me)
{
	const UWindInteractionComponent* Wind = Me->GetWind();
	FVector Origin, Velocity;
	if (!Wind->GetLocalAim(Origin, Velocity))
	{
		return;
	}

	FPredictProjectilePathParams Params(8.f, Origin, Velocity, 2.5f, ECC_Visibility);
	Params.ActorsToIgnore.Add(const_cast<AWindupCharacter*>(Me));
	if (AWindupCharacter* Target = Wind->GetWindTarget())
	{
		Params.ActorsToIgnore.Add(Target);
	}
	Params.SimFrequency = 20.f;
	Params.bTraceWithCollision = true;
	FPredictProjectilePathResult Result;
	UGameplayStatics::PredictProjectilePath(GetWorld(), Params, Result);

	const int32 Num = Result.PathData.Num();
	for (int32 i = 0; i < Num; i += 2)
	{
		const FVector Screen3 = Project(Result.PathData[i].Location, false);
		const float Alpha = 1.f - (float)i / FMath::Max(1, Num);
		Disc(FVector2D(Screen3.X, Screen3.Y), (5.f * Alpha + 2.f) * S, FLinearColor(1.f, 0.9f, 0.4f, 0.4f + 0.5f * Alpha), 10);
	}
	if (Result.HitResult.bBlockingHit)
	{
		const FVector Screen3 = Project(Result.HitResult.ImpactPoint, false);
		Ring(FVector2D(Screen3.X, Screen3.Y), 16.f * S, 4.f * S, 1.f, FLinearColor(1.f, 0.4f, 0.2f, 0.9f));
	}

	// Pull meter.
	const float Pull = Wind->GetLocalPullAlpha();
	const float CX = Canvas->ClipX * 0.5f;
	const float Y = Canvas->ClipY * 0.72f;
	Panel(CX - 150.f * S, Y, 300.f * S, 18.f * S, ColPanel);
	Panel(CX - 147.f * S, Y + 3.f * S, 294.f * S * Pull, 12.f * S, FMath::Lerp(FLinearColor(1.f, 0.8f, 0.3f), ColRed, Pull));
	Text(FString::Printf(TEXT("새총 당김 %d%%  ·  %d uu/s"), FMath::RoundToInt(Pull * 100.f), FMath::RoundToInt(Velocity.Size())), CX, Y + 22.f * S, 17.f, ColCream, true);
	Text(TEXT("조준 후 마우스 버튼을 떼면 발사!"), CX, Y + 44.f * S, 16.f, ColGold, true);
}

void ARaidHUD::DrawPrompts(const AWindupCharacter* Me)
{
	const float CX = Canvas->ClipX * 0.5f;
	const float Y = Canvas->ClipY - 120.f * S;
	TArray<FString> Lines;
	FLinearColor Color = ColCream;

	const UWindInteractionComponent* Wind = Me->GetWind();
	const UGrabComponent* Grab = Me->GetGrab();
	const float T = Me->GetTorque()->GetTorque();

	switch (Me->GetBodyState())
	{
	case ECrankBodyState::Scattered:
	{
		const uint8 Missing = Me->GetScatter()->GetMissingMask();
		Lines.Add(TEXT("산산조각! 동료가 머리와 태엽을 몸통에 가져다 대면 부활합니다"));
		Lines.Add(FString::Printf(TEXT("필요: %s%s"), (Missing & 1) ? TEXT("머리 ") : TEXT(""), (Missing & 2) ? TEXT("태엽") : TEXT("")));
		Color = FLinearColor(1.f, 0.6f, 0.4f);
		break;
	}
	case ECrankBodyState::Trapped:
		Lines.Add(FString::Printf(TEXT("먼지통에 갇힘! 토크 -3/s (%d T)"), FMath::RoundToInt(T)));
		Lines.Add(TEXT("동료가 보스 뒤쪽 레버를 1초 당기면 탈출합니다"));
		Color = FLinearColor(1.f, 0.6f, 0.4f);
		break;
	case ECrankBodyState::Carried:
		Lines.Add(TEXT("업혀 있음 — 이동키를 계속 누르면 탈출"));
		break;
	case ECrankBodyState::Normal:
	case ECrankBodyState::GettingUp:
	{
		if (Me->IsHelpless())
		{
			Lines.Add(TEXT("먼지 범벅! 정신이 없다..."));
			break;
		}
		if (Wind->IsBeingWound())
		{
			Lines.Add(TEXT("친구가 태엽을 감는 중... (이동키 길게: 빠져나가기)"));
			if (T >= CrankBalance::LaunchMinTorque)
			{
				Lines.Add(TEXT("[F] 자가 해제 발사  ·  친구가 [S]로 새총 조준 가능"));
			}
			break;
		}
		if (Wind->IsWinding())
		{
			if (Wind->IsPulling())
			{
				break;
			}
			Lines.Add(TEXT("마우스(R스틱)로 원을 그려 감기  ·  [E] 홀드: 일정 속도 감기"));
			if (Wind->CanSling())
			{
				Lines.Add(TEXT("[S] 홀드: 새총 조준 (당길수록 빠름)  ·  더 감으면 과감기 위험!"));
				Color = ColGold;
			}
			break;
		}

		const float Hold = Grab->GetHoldProgress();
		if (Hold >= 0.f)
		{
			const FVector2D C(CX, Canvas->ClipY * 0.5f);
			Ring(C, 40.f * S, 8.f * S, 1.f, FLinearColor(0.f, 0.f, 0.f, 0.5f));
			Ring(C, 40.f * S, 8.f * S, Hold, ColGold);
		}

		bool bHolding = false;
		for (const ECrankHand Hand : { ECrankHand::Left, ECrankHand::Right })
		{
			const FCrankHandState& H = Grab->GetHand(Hand);
			if (const IGrabbable* G = Cast<IGrabbable>(H.Actor))
			{
				Lines.Add(FString::Printf(TEXT("%s: %s"), Hand == ECrankHand::Left ? TEXT("왼손") : TEXT("오른손"), *G->GetGrabLabel(H.Component).ToString()));
				bHolding = true;
			}
		}
		if (bHolding)
		{
			Lines.Add(TEXT("버튼을 떼면 놓기 · 달리면서 떼면 던지기"));
		}
		else if (Wind->FindWindablePartner())
		{
			Lines.Add(TEXT("[좌클릭] 또는 [E] 친구 태엽 잡기  ·  [우클릭→좌클릭] 업기"));
			Color = ColGold;
		}
		else
		{
			AActor* Actor = nullptr;
			UPrimitiveComponent* Comp = nullptr;
			if (Grab->FindBestTarget(ECrankHand::Left, Actor, Comp))
			{
				if (const IGrabbable* G = Cast<IGrabbable>(Actor))
				{
					Lines.Add(FString::Printf(TEXT("[좌/우 클릭] %s"), *G->GetGrabLabel(Comp).ToString()));
				}
				else
				{
					Lines.Add(TEXT("[좌/우 클릭] 붙잡기"));
				}
			}
		}

		if (Me->IsCrawling())
		{
			Lines.Add(TEXT("방전! 기어가서 친구에게 감아달라고 하거나 오르골 위로 — 팔은 계속 쓸 수 있어요"));
			Color = FLinearColor(1.f, 0.8f, 0.5f);
		}
		else if (Me->CanSelfLaunch())
		{
			Lines.Add(TEXT("[F] 자가 해제 발사"));
		}
		break;
	}
	default:
		break;
	}

	for (int32 i = 0; i < Lines.Num(); ++i)
	{
		const FVector2D Size = MeasureText(Lines[i], 19.f);
		const float LY = Y + i * 30.f * S;
		Panel(CX - Size.X * 0.5f - 12.f * S, LY - 4.f * S, Size.X + 24.f * S, 28.f * S, ColPanel);
		Text(Lines[i], CX, LY, 19.f, Color, true);
	}
}

void ARaidHUD::DrawToasts(ARaidGameState* GS)
{
	const float Now = GetWorld()->GetTimeSeconds();
	GS->PruneToasts(Now);
	const TArray<FRaidToast>& Toasts = GS->GetToasts();
	float Y = 160.f * S;
	for (const FRaidToast& Toast : Toasts)
	{
		const float Age = Now - Toast.LocalTime;
		const float Alpha = FMath::Clamp(FMath::Min(Age / 0.2f, (4.5f - Age) / 0.6f), 0.f, 1.f);
		const FString Str = Toast.Text.ToString();
		const FVector2D Size = MeasureText(Str, 22.f);
		Panel(Canvas->ClipX * 0.5f - Size.X * 0.5f - 16.f * S, Y - 5.f * S, Size.X + 32.f * S, 36.f * S, FLinearColor(0.f, 0.f, 0.f, 0.55f * Alpha));
		Text(Str, Canvas->ClipX * 0.5f, Y, 22.f, FLinearColor(Toast.Color.R, Toast.Color.G, Toast.Color.B, Alpha), true);
		Y += 42.f * S;
	}
}

void ARaidHUD::DrawResult(const ARaidGameState* GS)
{
	if (GS->RaidPhase != ERaidPhase::Cleared && GS->RaidPhase != ERaidPhase::Failed)
	{
		LastPhase = (uint8)GS->RaidPhase;
		return;
	}
	const float Now = GetWorld()->GetTimeSeconds();
	if (LastPhase != (uint8)GS->RaidPhase)
	{
		LastPhase = (uint8)GS->RaidPhase;
		ResultShownTime = Now;
		CrankSound::Play2D(this, GS->RaidPhase == ERaidPhase::Cleared ? TEXT("SFX_Win") : TEXT("SFX_Fail"), 0.9f);
		if (GS->FailReason == ERaidFailReason::Midnight)
		{
			CrankSound::Play2D(this, TEXT("SFX_ClockChime"), 1.f);
		}
	}
	const float Alpha = FMath::Clamp((Now - ResultShownTime) / 0.6f, 0.f, 1.f);
	const float W = Canvas->ClipX;
	const float H = Canvas->ClipY;
	Panel(0, 0, W, H, FLinearColor(0.02f, 0.015f, 0.01f, 0.6f * Alpha));

	const float CX = W * 0.5f;
	const float CY = H * 0.38f;
	if (GS->RaidPhase == ERaidPhase::Cleared)
	{
		Text(TEXT("레이드 클리어!"), CX, CY - 70.f * S, 56.f, FLinearColor(1.f, 0.85f, 0.35f, Alpha), true);
		const int32 Stars = GS->GetStars();
		for (int32 i = 0; i < 4; ++i)
		{
			const float Pop = FMath::Clamp((Now - ResultShownTime - 0.5f - i * 0.25f) / 0.25f, 0.f, 1.f);
			const FVector2D P(CX + (i - 1.5f) * 84.f * S, CY + 30.f * S);
			Star(P, 36.f * S * (i < Stars ? Pop : 1.f), i < Stars ? FLinearColor(1.f, 0.82f, 0.2f, Alpha) : FLinearColor(0.25f, 0.25f, 0.25f, 0.7f * Alpha));
		}
		Text(FString::Printf(TEXT("%s + 부품 %d/3  ·  걸린 시간 %s"), GS->bRequiresGoldenKey ? TEXT("메인 스프링 + 황금 열쇠") : TEXT("메인 스프링"),
			GS->NumCollected(), *FormatTime(GS->GetElapsedTime())), CX, CY + 90.f * S, 22.f, FLinearColor(1.f, 1.f, 1.f, Alpha), true);
	}
	else
	{
		const bool bMidnight = GS->FailReason == ERaidFailReason::Midnight;
		Text(bMidnight ? TEXT("뎅... 뎅... 자정이 되었습니다") : TEXT("둘 다 산산조각!"), CX, CY - 60.f * S, 48.f, FLinearColor(1.f, 0.45f, 0.35f, Alpha), true);
		Text(bMidnight ? TEXT("주인이 돌아왔고, 인형들은 평범한 장난감으로 굳어버렸다") : TEXT("아무도 조립해 줄 수 없게 되었다"), CX, CY + 10.f * S, 22.f, FLinearColor(1.f, 1.f, 1.f, Alpha), true);
	}
	Text(TEXT("[R] 다시 하기  ·  [Esc/M] 메뉴"), CX, CY + 150.f * S, 22.f, FLinearColor(1.f, 0.9f, 0.6f, Alpha), true);
}

void ARaidHUD::DrawHelp()
{
	const float W = 560.f * S;
	const float X = Canvas->ClipX - W - 22.f * S;
	const float Y = 150.f * S;
	static const TCHAR* Rows[] = {
		TEXT("WASD  이동 (토크가 높을수록 빠르다)"),
		TEXT("마우스  카메라  ·  Space 점프 (30 T 이상)"),
		TEXT("좌/우 클릭 홀드  왼손/오른손 잡기"),
		TEXT("친구 등 뒤에서 좌클릭 → 마우스로 원 그리기 = 감기"),
		TEXT("E 홀드  일정 속도로 감기 (리듬 보너스 없음)"),
		TEXT("100 T 이상: 감는 중 S 홀드로 조준, 버튼 떼면 새총 발사"),
		TEXT("F  자가 해제 발사 (100 T 이상)"),
		TEXT("친구를 우클릭으로 잡고 좌클릭 = 업기, 달리며 놓으면 던지기"),
		TEXT("1~4  이모트  ·  Tab/F1 도움말  ·  Esc/M 메뉴"),
	};
	const int32 Num = UE_ARRAY_COUNT(Rows);
	Panel(X, Y, W, (46.f + Num * 28.f) * S, ColPanel);
	Text(TEXT("조작법"), X + 16.f * S, Y + 10.f * S, 20.f, ColGold);
	for (int32 i = 0; i < Num; ++i)
	{
		Text(Rows[i], X + 16.f * S, Y + (42.f + i * 28.f) * S, 16.f, ColCream);
	}
}

void ARaidHUD::DrawMenu()
{
	const float W = Canvas->ClipX;
	const float H = Canvas->ClipY;
	Panel(0, 0, W, H, FLinearColor(0.f, 0.f, 0.f, 0.45f));
	Text(TEXT("일시정지"), W * 0.5f, H * 0.4f, 44.f, ColGold, true);
	Text(TEXT("[Esc/M] 계속하기  ·  [Q] 메인 메뉴로  ·  [Tab] 조작법"), W * 0.5f, H * 0.4f + 70.f * S, 22.f, ColCream, true);
}
