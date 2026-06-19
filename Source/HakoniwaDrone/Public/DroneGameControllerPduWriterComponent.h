#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DroneControlOp.h"
#include "PduManager.h"
#include "hako_msgs/pdu_cpptype_conv_GameControllerOperation.hpp"
#include "DroneGameControllerPduWriterComponent.generated.h"

UCLASS(ClassGroup=(Hakoniwa), meta=(BlueprintSpawnableComponent))
class HAKONIWADRONE_API UDroneGameControllerPduWriterComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UDroneGameControllerPduWriterComponent();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hakoniwa|PDU")
	bool bUseConfiguredRobotName = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hakoniwa|PDU")
	FString RobotName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hakoniwa|PDU")
	FString FallbackRobotName = TEXT("Drone");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hakoniwa|PDU")
	FString PduName = TEXT("hako_cmd_game");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hakoniwa|PDU")
	bool bAutoDeclarePdu = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hakoniwa|PDU")
	bool bWriteGlobalPduForDronePro = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hakoniwa|Input")
	bool bPollControlInputInWriter = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hakoniwa|Input")
	bool bApplyUnityInputMapping = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hakoniwa|Input")
	bool bAutoDisableUnityMappingForPduSource = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hakoniwa|Input")
	float StickStrength = 0.1f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hakoniwa|Input")
	float StickYawStrength = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hakoniwa|Input")
	bool bInvertVerticalInput = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hakoniwa|Input")
	bool bInvertForwardInput = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hakoniwa|Input")
	bool bInvertHorizontalInput = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hakoniwa|Input")
	bool bInvertHeadingInput = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hakoniwa|Input")
	bool bHoldRadioControlPulse = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hakoniwa|Input", meta = (ClampMin = "0.0"))
	float RadioControlPulseHoldSeconds = 0.6f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hakoniwa|Debug")
	bool bLogPduWrites = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hakoniwa|Debug", meta = (ClampMin = "0.0"))
	float InputLogDeadzone = 0.01f;

	UFUNCTION(BlueprintCallable, Category = "Hakoniwa|PDU")
	bool DeclarePdu();

protected:
	virtual void BeginPlay() override;

public:
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	UPROPERTY()
	UPduManager* PduManager = nullptr;

	UPROPERTY()
	TScriptInterface<IDroneControlOp> ControlOp;

	bool bDeclared = false;
	float RadioControlPulseRemaining = 0.0f;
	float InputLogAccumulator = 0.0f;
	int32 LastButton0Value = -1;
	bool ButtonStates[15] = {};

	void FindPduManager();
	void ResolveRobotName();
	void FindControlOp();
	bool WriteGameControllerPdu(float DeltaTime);
	void UpdateButtonPulse(float DeltaTime, bool bAButtonPressed, bool bAButtonReleased, HakoCpp_GameControllerOperation& OutPdu);
};
