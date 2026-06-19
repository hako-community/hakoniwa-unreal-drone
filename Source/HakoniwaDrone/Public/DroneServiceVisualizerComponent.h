#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DroneControlOp.h"
#include "PduManager.h"
#include "DroneServiceVisualizerComponent.generated.h"

class UDroneLedComponent;
class UDronePropellerComponent;
class UFlightModeLedComponent;

UCLASS(ClassGroup=(Hakoniwa), meta=(BlueprintSpawnableComponent))
class HAKONIWADRONE_API UDroneServiceVisualizerComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UDroneServiceVisualizerComponent();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hakoniwa|Drone Service")
	int32 DroneIndex = 0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hakoniwa|Drone Service")
	bool bUseConfiguredRobotName = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hakoniwa|Drone Service")
	FString RobotName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hakoniwa|Drone Service")
	FString FallbackRobotName = TEXT("Drone");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hakoniwa|Drone Service")
	bool bAutoInitializeService = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hakoniwa|Drone Service")
	bool bDisableLocalServiceWhenPduWriterExists = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hakoniwa|Drone Service")
	bool bRetryAutoInitializeService = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hakoniwa|Drone Service", meta = (ClampMin = "0.1"))
	float AutoInitializeRetryInterval = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hakoniwa|Drone Service")
	bool bUseInitSingle = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hakoniwa|Drone Service")
	FString DroneConfigTextPath = TEXT("Config/drone/rc/drone_config_0.json");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hakoniwa|Drone Service")
	FString ControllerConfigTextPath = TEXT("Config/controller/param-api-mixer.txt");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hakoniwa|Drone Service")
	FString DroneConfigDirPath = TEXT("Config/drone/rc-1");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hakoniwa|Drone Service")
	bool bEnableDataLogger = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hakoniwa|Drone Service")
	FString DebugLogPath;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hakoniwa|Runtime")
	bool bAdvanceService = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hakoniwa|Runtime")
	bool bUseRunFixedSteps = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hakoniwa|Runtime", meta = (ClampMin = "1"))
	int32 RunStepsPerTick = 20;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hakoniwa|Runtime")
	bool bUseAccumulatedRunSteps = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hakoniwa|Runtime", meta = (ClampMin = "0.0001"))
	float ServiceStepSeconds = 0.001f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hakoniwa|Runtime", meta = (ClampMin = "1"))
	int32 MaxRunStepsPerTick = 50;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hakoniwa|Runtime")
	int32 AdvanceTimeUsec = 20000;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hakoniwa|Runtime")
	bool bApplyControlInputFromOwner = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hakoniwa|Runtime")
	bool bPollControlInputInVisualizer = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hakoniwa|Runtime")
	bool bLogControlInput = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hakoniwa|Runtime")
	bool bHoldRadioControlPulse = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hakoniwa|Runtime", meta = (ClampMin = "0.0"))
	float RadioControlPulseHoldSeconds = 0.6f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hakoniwa|Runtime")
	float StickStrength = 0.1f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hakoniwa|Runtime")
	float StickYawStrength = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hakoniwa|Runtime")
	bool bInvertVerticalInput = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hakoniwa|Runtime")
	bool bInvertForwardInput = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hakoniwa|Runtime")
	bool bInvertHorizontalInput = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hakoniwa|Runtime")
	bool bInvertHeadingInput = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hakoniwa|Runtime")
	bool bLogVisualizerState = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hakoniwa|Runtime")
	bool bUpdateOwnerTransform = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hakoniwa|Runtime")
	bool bDriveOwnerVisualsFromService = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hakoniwa|Runtime")
	bool bLogServiceTime = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hakoniwa|Runtime")
	float UnrealScale = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hakoniwa|PDU")
	bool bWriteVisualPdus = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hakoniwa|PDU")
	FString PositionPduName = TEXT("pos");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hakoniwa|PDU")
	FString MotorPduName = TEXT("motor");

	UFUNCTION(BlueprintCallable, Category = "Hakoniwa|Drone Service")
	bool InitializeDroneService();

	UFUNCTION(BlueprintCallable, Category = "Hakoniwa|Drone Service")
	void StopDroneService();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

public:
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	UPROPERTY()
	UPduManager* PduManager = nullptr;

	UPROPERTY()
	TScriptInterface<IDroneControlOp> ControlOp;

	UPROPERTY()
	UDronePropellerComponent* Propeller = nullptr;

	UPROPERTY()
	UDroneLedComponent* DroneLed = nullptr;

	UPROPERTY()
	UFlightModeLedComponent* FlightModeLed = nullptr;

	bool bServiceStarted = false;
	bool bPositionPduDeclared = false;
	bool bMotorPduDeclared = false;
	float ServiceStepAccumulator = 0.0f;
	float ServiceTimeLogAccumulator = 0.0f;
	float ControlInputLogAccumulator = 0.0f;
	float VisualizerStateLogAccumulator = 0.0f;
	float AutoInitializeRetryAccumulator = 0.0f;
	float RadioControlPulseRemaining = 0.0f;
	int32 LastRadioControlButtonValue = -1;
	float ServiceStateDebugLogRemaining = 0.0f;

	bool LoadTextFileFromContent(const FString& RelativePath, FString& OutText) const;
	void FindPduManager();
	void ResolveRobotName();
	void FindControlOp();
	void FindOwnerVisualComponents();
	void ApplyControlInput(float DeltaTime);
	void AdvanceService(float DeltaTime);
	void UpdateOwnerFromService();
	void UpdateOwnerVisualsFromService();
	void WriteVisualPdus();
	void WritePositionPdu(const FVector& UnrealPosition, const FRotator& UnrealRotation);
	void WriteMotorPdu(double C1, double C2, double C3, double C4, double C5, double C6, double C7, double C8);
	void PutRadioControlButtonValue(int32 Value, const TCHAR* Reason);
	void RequestServiceStateDebugLog(float DurationSeconds);
	void LogServiceStateSnapshot(const TCHAR* Reason) const;
	FVector ServicePositionToUnreal(double X, double Y, double Z) const;
	FRotator ServiceAttitudeToUnreal(double RollRad, double PitchRad, double YawRad) const;
};
