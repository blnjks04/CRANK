#include "Character/CrankInput.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "InputModifiers.h"
#include "InputCoreTypes.h"
#include "UObject/Package.h"

namespace
{
	UInputAction* MakeAction(const TCHAR* Name, EInputActionValueType Type)
	{
		UInputAction* Action = NewObject<UInputAction>(GetTransientPackage(), Name);
		Action->ValueType = Type;
		Action->AddToRoot();
		return Action;
	}

	template <typename TModifier>
	TModifier* AddModifier(FEnhancedActionKeyMapping& Mapping, UObject* Outer)
	{
		TModifier* Modifier = NewObject<TModifier>(Outer);
		Mapping.Modifiers.Add(Modifier);
		return Modifier;
	}

	FCrankInputActions Build()
	{
		FCrankInputActions A;
		A.Move = MakeAction(TEXT("IA_Crank_Move"), EInputActionValueType::Axis2D);
		A.LookMouse = MakeAction(TEXT("IA_Crank_LookMouse"), EInputActionValueType::Axis2D);
		A.LookStick = MakeAction(TEXT("IA_Crank_LookStick"), EInputActionValueType::Axis2D);
		A.Jump = MakeAction(TEXT("IA_Crank_Jump"), EInputActionValueType::Boolean);
		A.GrabLeft = MakeAction(TEXT("IA_Crank_GrabLeft"), EInputActionValueType::Boolean);
		A.GrabRight = MakeAction(TEXT("IA_Crank_GrabRight"), EInputActionValueType::Boolean);
		A.HoldWind = MakeAction(TEXT("IA_Crank_HoldWind"), EInputActionValueType::Boolean);
		A.SelfLaunch = MakeAction(TEXT("IA_Crank_SelfLaunch"), EInputActionValueType::Boolean);
		for (int32 i = 0; i < 4; ++i)
		{
			A.Emote[i] = MakeAction(*FString::Printf(TEXT("IA_Crank_Emote%d"), i + 1), EInputActionValueType::Boolean);
		}
		A.Menu = MakeAction(TEXT("IA_Crank_Menu"), EInputActionValueType::Boolean);
		A.Restart = MakeAction(TEXT("IA_Crank_Restart"), EInputActionValueType::Boolean);
		A.Help = MakeAction(TEXT("IA_Crank_Help"), EInputActionValueType::Boolean);

		UInputMappingContext* Ctx = NewObject<UInputMappingContext>(GetTransientPackage(), TEXT("IMC_Crank"));
		Ctx->AddToRoot();
		A.Context = Ctx;

		// Move: WASD + left stick
		{
			FEnhancedActionKeyMapping& W = Ctx->MapKey(A.Move, EKeys::W);
			AddModifier<UInputModifierSwizzleAxis>(W, Ctx);

			FEnhancedActionKeyMapping& S = Ctx->MapKey(A.Move, EKeys::S);
			AddModifier<UInputModifierSwizzleAxis>(S, Ctx);
			AddModifier<UInputModifierNegate>(S, Ctx);

			FEnhancedActionKeyMapping& Akey = Ctx->MapKey(A.Move, EKeys::A);
			AddModifier<UInputModifierNegate>(Akey, Ctx);

			Ctx->MapKey(A.Move, EKeys::D);

			FEnhancedActionKeyMapping& Stick = Ctx->MapKey(A.Move, EKeys::Gamepad_Left2D);
			UInputModifierDeadZone* Dz = AddModifier<UInputModifierDeadZone>(Stick, Ctx);
			Dz->LowerThreshold = 0.2f;
		}

		// Look
		Ctx->MapKey(A.LookMouse, EKeys::Mouse2D);
		{
			FEnhancedActionKeyMapping& Stick = Ctx->MapKey(A.LookStick, EKeys::Gamepad_Right2D);
			UInputModifierDeadZone* Dz = AddModifier<UInputModifierDeadZone>(Stick, Ctx);
			Dz->LowerThreshold = 0.15f;
		}

		Ctx->MapKey(A.Jump, EKeys::SpaceBar);
		Ctx->MapKey(A.Jump, EKeys::Gamepad_FaceButton_Bottom);

		Ctx->MapKey(A.GrabLeft, EKeys::LeftMouseButton);
		Ctx->MapKey(A.GrabLeft, EKeys::Gamepad_LeftTrigger);
		Ctx->MapKey(A.GrabRight, EKeys::RightMouseButton);
		Ctx->MapKey(A.GrabRight, EKeys::Gamepad_RightTrigger);

		Ctx->MapKey(A.HoldWind, EKeys::E);
		Ctx->MapKey(A.HoldWind, EKeys::Gamepad_FaceButton_Left);

		Ctx->MapKey(A.SelfLaunch, EKeys::F);
		Ctx->MapKey(A.SelfLaunch, EKeys::Gamepad_FaceButton_Top);

		const FKey EmoteKeys[4] = { EKeys::One, EKeys::Two, EKeys::Three, EKeys::Four };
		const FKey EmotePad[4] = { EKeys::Gamepad_DPad_Up, EKeys::Gamepad_DPad_Right, EKeys::Gamepad_DPad_Down, EKeys::Gamepad_DPad_Left };
		for (int32 i = 0; i < 4; ++i)
		{
			Ctx->MapKey(A.Emote[i], EmoteKeys[i]);
			Ctx->MapKey(A.Emote[i], EmotePad[i]);
		}

		Ctx->MapKey(A.Menu, EKeys::Escape);
		Ctx->MapKey(A.Menu, EKeys::M);	// Esc ends play-in-editor sessions
		Ctx->MapKey(A.Menu, EKeys::Gamepad_Special_Right);
		Ctx->MapKey(A.Restart, EKeys::R);
		Ctx->MapKey(A.Restart, EKeys::Gamepad_Special_Left);
		Ctx->MapKey(A.Help, EKeys::F1);
		Ctx->MapKey(A.Help, EKeys::Tab);
		return A;
	}
}

const FCrankInputActions& CrankInput::Get()
{
	static FCrankInputActions Actions = Build();
	return Actions;
}
