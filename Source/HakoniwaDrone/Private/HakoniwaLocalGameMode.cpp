#include "HakoniwaLocalGameMode.h"

#include "Components/SceneComponent.h"

AHakoniwaLocalViewPawn::AHakoniwaLocalViewPawn()
{
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	BaseEyeHeight = 0.0f;   // 視点は PlayerStart の位置そのもの
}

AHakoniwaLocalGameMode::AHakoniwaLocalGameMode()
{
	DefaultPawnClass = AHakoniwaLocalViewPawn::StaticClass();
}
