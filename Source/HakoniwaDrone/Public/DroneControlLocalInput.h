#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DroneControlOp.h"
#include "DroneControlLocalInput.generated.h"

/**
 * ★ 2026-09-30（related/devai_dll/hakodrone_unreal_drone_migration_20260930.md・U3）:
 *   **Unreal のゲームパッド／キーボードを直接読む**入力（箱庭につながずにアプリ単体で飛ばすため）。
 *   それまで IDroneControlOp の実装は UDroneControlPdu（箱庭の PDU「hako_cmd_game」を読む）だけで、
 *   ローカル RC（オフライン）でも入力は外のコントローラのアプリから箱庭経由で来ていた。
 *
 *   値は PDU（hako_msgs/GameControllerOperation・drone-core の rc_utils.py）と同じ約束で返す:
 *     左スティック  X = 上下（上が +）・Y = ヨー（右が +）
 *     右スティック  X = 前後（前が +）・Y = 左右（右が +）
 *   ボタン: A = アーム（FaceButton_Bottom・Space）・B = FaceButton_Right・X = モード切替（FaceButton_Left・M）・
 *           Y = FaceButton_Top・上下 = 十字キー
 *   キーボード（ゲームパッドが無いとき）: W/S 上下・A/D ヨー・↑/↓ 前後・←/→ 左右。
 */
UCLASS(ClassGroup = (Hakoniwa), meta = (BlueprintSpawnableComponent))
class HAKONIWADRONE_API UDroneControlLocalInput : public UActorComponent, public IDroneControlOp
{
	GENERATED_BODY()

public:
	UDroneControlLocalInput();

	/** キーボードでも操作できるようにする（ゲームパッドが無い開発機での確認用） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Drone Local Input")
	bool bEnableKeyboard = true;

	/** スティックの不感帯（Unreal の入力設定の不感帯とは別に掛ける） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Drone Local Input")
	float StickDeadzone = 0.05f;

	virtual void DoInitialize_Implementation(const FString& RobotName) override {}
	virtual void Run_Implementation() override {}
	virtual bool IsReady_Implementation() override;
	virtual void Flush_Implementation() override {}
	virtual FVector2D GetLeftStickInput_Implementation() override;
	virtual FVector2D GetRightStickInput_Implementation() override;
	virtual bool IsAButtonPressed_Implementation() override;
	virtual bool IsAButtonReleased_Implementation() override;
	virtual bool IsBButtonPressed_Implementation() override;
	virtual bool IsBButtonReleased_Implementation() override;
	virtual bool IsXButtonPressed_Implementation() override;
	virtual bool IsXButtonReleased_Implementation() override;
	virtual bool IsYButtonPressed_Implementation() override;
	virtual bool IsYButtonReleased_Implementation() override;
	virtual bool IsUpButtonPressed_Implementation() override;
	virtual bool IsUpButtonReleased_Implementation() override;
	virtual bool IsDownButtonPressed_Implementation() override;
	virtual bool IsDownButtonReleased_Implementation() override;

private:
	class APlayerController* GetPlayerController() const;
	float Axis(const FKey& Key) const;
	float KeyPair(const FKey& Plus, const FKey& Minus) const;
	bool JustPressed(const FKey& Pad, const FKey& Key) const;
	bool JustReleased(const FKey& Pad, const FKey& Key) const;

	// 前面に戻った直後の古い軸の値を捨てるための状態（Axis は const なので mutable）
	mutable bool bHadFocus = true;
	mutable TMap<FName, float> StaleAxisValues;
	mutable TSet<FName> AxesReadSinceFocus;
};
