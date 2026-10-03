#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/Pawn.h"
#include "HakoniwaLocalGameMode.generated.h"

/**
 * 視点だけの Pawn（移動の部品を持たないので入力で動かない）。
 * ★ ルートの部品が無いと PlayerStart に置かれず原点に出るので、USceneComponent を持たせる。
 */
UCLASS()
class HAKONIWADRONE_API AHakoniwaLocalViewPawn : public APawn
{
	GENERATED_BODY()

public:
	AHakoniwaLocalViewPawn();
};

/**
 * ★ 2026-10-03（related/devai_dll/hakodrone_unreal_drone_migration_20260930.md・U4）:
 *   オフラインのレベル（AvatarLocal）用のゲームモード。
 *   既定のゲームモードは空を飛ぶ DefaultPawn を作り、ゲームパッド・WASD でカメラが動いてしまう
 *   （ドローンの操作と同じ入力を取り合う）。動かない AHakoniwaLocalViewPawn にして、視点を PlayerStart に固定する。
 */
UCLASS()
class HAKONIWADRONE_API AHakoniwaLocalGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AHakoniwaLocalGameMode();
};
