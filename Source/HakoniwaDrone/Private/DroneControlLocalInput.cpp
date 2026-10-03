#include "DroneControlLocalInput.h"

#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"
#include "Misc/App.h"

#if PLATFORM_WINDOWS
#include "GameInputBaseModule.h"
#endif

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

// ★ GameInput は既定では「前面でないあいだ」全軸 0 の読み取りを送る（0〜1 を −1〜+1 に直す割り当てでは倒し切り）。
//   その 0 が前面に戻ったあとに遅れて届くと機体が勝手に動く（U4 でユーザが確認）。背景でも実際の値を送らせて、0 を出させない。
//   背景にいるあいだの入力は Axis で中立にする。GameInput は起動時に非同期で作られるので、できるまで毎回試す。
void UDroneControlLocalInput::EnsureGameInputBackgroundPolicy() const
{
#if PLATFORM_WINDOWS && GAME_INPUT_SUPPORT
	if (bGameInputPolicySet || !FGameInputBaseModule::IsAvailable()) return;
	if (IGameInput* GameInput = FGameInputBaseModule::GetGameInput())
	{
		GameInput->SetFocusPolicy(GameInputEnableBackgroundInput);
		bGameInputPolicySet = true;
		UE_LOG(LogTemp, Log, TEXT("DroneControlLocalInput: GameInput の背景入力を有効にしました（前面でないときの全軸 0 を防ぐ）"));
	}
#endif
}

float UDroneControlLocalInput::Axis(const FKey& Key) const
{
	const APlayerController* PC = GetPlayerController();
	if (PC == nullptr) return 0.0f;
	EnsureGameInputBackgroundPolicy();
	// ★ アプリが前面にいないあいだは中立にする。GameInput は前面でないとき軸を全部 0 で送り、
	//   スティックの割り当て（0〜1 を −1〜+1 に直す）では 0 が「倒し切り」になって、機体が回りながら飛んで行く（U4 で確認）。
	if (!FApp::HasFocus())
	{
		bHadFocus = false;
		return 0.0f;
	}
	if (!bHadFocus)
	{
		// ★ 前面に戻った瞬間: Unreal には前面でないあいだに届いた値（全部 0 → 倒し切り）が残っていて、
		//   スティックを動かすまで消えない。各軸は、前面に戻ってから最初に読んだ値を「古い値」として覚え、
		//   値が動く（新しい読み取りが届く）まで中立にする。
		bHadFocus = true;
		StaleAxisValues.Reset();
		AxesReadSinceFocus.Reset();
	}
	const FName Name = Key.GetFName();
	const float V = PC->GetInputAnalogKeyState(Key);
	if (!AxesReadSinceFocus.Contains(Name))
	{
		AxesReadSinceFocus.Add(Name);
		if (FMath::Abs(V) >= StickDeadzone)
		{
			StaleAxisValues.Add(Name, V);
		}
	}
	if (const float* Stale = StaleAxisValues.Find(Name))
	{
		if (FMath::Abs(V - *Stale) < 0.01f) return 0.0f;
		StaleAxisValues.Remove(Name);
	}
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
// ★ Unreal はゲームに渡す Gamepad_RightY だけ符号を反転する（SceneViewport.cpp・マウスの Y と同じ扱い。XInput でも同じ）。
//   右スティックの上を + にするため、ここで戻す（左の Gamepad_LeftY は反転されない）。
FVector2D UDroneControlLocalInput::GetRightStickInput_Implementation()
{
	const float MoveFB = FMath::Clamp(-Axis(EKeys::Gamepad_RightY) + KeyPair(EKeys::Up, EKeys::Down), -1.0f, 1.0f);
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
