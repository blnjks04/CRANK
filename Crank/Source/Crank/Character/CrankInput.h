// Enhanced Input actions + mapping context created at runtime (design doc chapter 8 controls).

#pragma once

#include "CoreMinimal.h"

class UInputAction;
class UInputMappingContext;

struct FCrankInputActions
{
	UInputAction* Move = nullptr;
	UInputAction* LookMouse = nullptr;
	UInputAction* LookStick = nullptr;
	UInputAction* Jump = nullptr;
	UInputAction* GrabLeft = nullptr;
	UInputAction* GrabRight = nullptr;
	UInputAction* HoldWind = nullptr;
	UInputAction* SelfLaunch = nullptr;
	UInputAction* Emote[4] = { nullptr, nullptr, nullptr, nullptr };
	UInputAction* Menu = nullptr;
	UInputAction* Restart = nullptr;
	UInputAction* Help = nullptr;
	UInputMappingContext* Context = nullptr;
};

namespace CrankInput
{
	CRANK_API const FCrankInputActions& Get();
}
