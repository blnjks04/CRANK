// Canvas HUD (design doc 5.1: diegetic first; HUD only shows the partner's torque as a small icon).
// Also: raid clock, parts, boss cells, context prompts, winding ring, slingshot trajectory, results.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "RaidHUD.generated.h"

class UFont;
class AWindupCharacter;
class ARaidGameState;

UCLASS()
class CRANK_API ARaidHUD : public AHUD
{
	GENERATED_BODY()

public:
	virtual void DrawHUD() override;

protected:
	// Primitives
	void Text(const FString& Str, float X, float Y, float Size, const FLinearColor& Color, bool bCenter = false, bool bOutline = true);
	FVector2D MeasureText(const FString& Str, float Size);
	void Panel(float X, float Y, float W, float H, const FLinearColor& Color);
	void Disc(const FVector2D& Center, float Radius, const FLinearColor& Color, int32 Sides = 32);
	void Ring(const FVector2D& Center, float Radius, float Thickness, float Fraction, const FLinearColor& Color, float StartAngleDeg = -90.f);
	void Line(const FVector2D& A, const FVector2D& B, float Thickness, const FLinearColor& Color);
	void Star(const FVector2D& Center, float Radius, const FLinearColor& Color);

	// Sections
	void DrawClock(const ARaidGameState* GS);
	void DrawParts(const ARaidGameState* GS);
	void DrawBoss();
	void DrawGiant();
	void DrawPartnerIcons(const AWindupCharacter* Me);
	void DrawWinding(const AWindupCharacter* Me);
	void DrawAim(const AWindupCharacter* Me);
	void DrawPrompts(const AWindupCharacter* Me);
	void DrawToasts(ARaidGameState* GS);
	void DrawResult(const ARaidGameState* GS);
	void DrawHelp();
	void DrawMenu();
	void DrawVignettes(const AWindupCharacter* Me);
	void DrawWaiting(const ARaidGameState* GS);

	float S = 1.f;	// UI scale (1080p = 1)
	FLinearColor TorqueColor(float Torque) const;

	UPROPERTY(Transient)
	TObjectPtr<UFont> Font;

	uint8 LastPhase = 0;
	TArray<FString> HostAddresses;
	bool bAddressesQueried = false;
	float ResultShownTime = 0.f;
};
