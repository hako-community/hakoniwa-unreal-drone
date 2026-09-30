#include "DroneControlLocalInput.h"

#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"

UDroneControlLocalInput::UDroneControlLocalInput()
{
	PrimaryComponentTick.bCanEverTick = false;
}

APlayerController* UDroneControlLocalInput::GetPlayerController() const
{
	const UWorld* World = GetWorld();
	return World != nullptr ? World->GetFirstPlayerController() : nullptr;
}

bool UDroneControlLocalInput::IsReady_Implementation()
{
	return GetPlayerController() != nullptr;
}

float UDroneControlLocalInput::Axis(const FKey& Key) const
{
	const APlayerController* PC = GetPlayerController();
	if (PC == nullptr) return 0.0f;
	const float V = PC->GetInputAnalogKeyState(Key);
	return FMath::Abs(V) < StickDeadzone ? 0.0f : FMath::Clamp(V, -1.0f, 1.0f);
}

float UDroneControlLocalInput::KeyPair(const FKey& Plus, const FKey& Minus) const
{
	const APlayerController* PC = GetPlayerController();
	if (!bEnableKeyboard || PC == nullptr) return 0.0f;
	return (PC->IsInputKeyDown(Plus) ? 1.0f : 0.0f) - (PC->IsInputKeyDown(Minus) ? 1.0f : 0.0f);
}

bool UDroneControlLocalInput::JustPressed(const FKey& Pad, const FKey& Key) const
{
	const APlayerController* PC = GetPlayerController();
	if (PC == nullptr) return false;
	return PC->WasInputKeyJustPressed(Pad) || (bEnableKeyboard && Key.IsValid() && PC->WasInputKeyJustPressed(Key));
}

bool UDroneControlLocalInput::JustReleased(const FKey& Pad, const FKey& Key) const
{
	const APlayerController* PC = GetPlayerController();
	if (PC == nullptr) return false;
	return PC->WasInputKeyJustReleased(Pad) || (bEnableKeyboard && Key.IsValid() && PC->WasInputKeyJustReleased(Key));
}

// ★ PDU と同じ約束: 左 X = 上下（上が +）・左 Y = ヨー（右が +）。Unreal のスティックは上・右が +。
FVector2D UDroneControlLocalInput::GetLeftStickInput_Implementation()
{
	const float UpDown = FMath::Clamp(Axis(EKeys::Gamepad_LeftY) + KeyPair(EKeys::W, EKeys::S), -1.0f, 1.0f);
	const float TurnLR = FMath::Clamp(Axis(EKeys::Gamepad_LeftX) + KeyPair(EKeys::D, EKeys::A), -1.0f, 1.0f);
	return FVector2D(UpDown, TurnLR);
}

// ★ PDU と同じ約束: 右 X = 前後（前が +）・右 Y = 左右（右が +）
FVector2D UDroneControlLocalInput::GetRightStickInput_Implementation()
{
	const float MoveFB = FMath::Clamp(Axis(EKeys::Gamepad_RightY) + KeyPair(EKeys::Up, EKeys::Down), -1.0f, 1.0f);
	const float MoveLR = FMath::Clamp(Axis(EKeys::Gamepad_RightX) + KeyPair(EKeys::Right, EKeys::Left), -1.0f, 1.0f);
	return FVector2D(MoveFB, MoveLR);
}

bool UDroneControlLocalInput::IsAButtonPressed_Implementation() { return JustPressed(EKeys::Gamepad_FaceButton_Bottom, EKeys::SpaceBar); }
bool UDroneControlLocalInput::IsAButtonReleased_Implementation() { return JustReleased(EKeys::Gamepad_FaceButton_Bottom, EKeys::SpaceBar); }
bool UDroneControlLocalInput::IsBButtonPressed_Implementation() { return JustPressed(EKeys::Gamepad_FaceButton_Right, EKeys::B); }
bool UDroneControlLocalInput::IsBButtonReleased_Implementation() { return JustReleased(EKeys::Gamepad_FaceButton_Right, EKeys::B); }
bool UDroneControlLocalInput::IsXButtonPressed_Implementation() { return JustPressed(EKeys::Gamepad_FaceButton_Left, EKeys::M); }
bool UDroneControlLocalInput::IsXButtonReleased_Implementation() { return JustReleased(EKeys::Gamepad_FaceButton_Left, EKeys::M); }
bool UDroneControlLocalInput::IsYButtonPressed_Implementation() { return JustPressed(EKeys::Gamepad_FaceButton_Top, EKeys::C); }
bool UDroneControlLocalInput::IsYButtonReleased_Implementation() { return JustReleased(EKeys::Gamepad_FaceButton_Top, EKeys::C); }
bool UDroneControlLocalInput::IsUpButtonPressed_Implementation() { return JustPressed(EKeys::Gamepad_DPad_Up, EKeys::PageUp); }
bool UDroneControlLocalInput::IsUpButtonReleased_Implementation() { return JustReleased(EKeys::Gamepad_DPad_Up, EKeys::PageUp); }
bool UDroneControlLocalInput::IsDownButtonPressed_Implementation() { return JustPressed(EKeys::Gamepad_DPad_Down, EKeys::PageDown); }
bool UDroneControlLocalInput::IsDownButtonReleased_Implementation() { return JustReleased(EKeys::Gamepad_DPad_Down, EKeys::PageDown); }
