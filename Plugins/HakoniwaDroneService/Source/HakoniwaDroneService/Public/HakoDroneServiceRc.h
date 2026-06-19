#pragma once

#include "CoreMinimal.h"

struct HAKONIWADRONESERVICE_API FHakoDroneServiceVector
{
	double X = 0.0;
	double Y = 0.0;
	double Z = 0.0;
};

struct HAKONIWADRONESERVICE_API FHakoDroneBatteryStatus
{
	double FullVoltage = 0.0;
	double CurrentVoltage = 0.0;
	double CurrentTemperature = 0.0;
	uint32 Status = 0;
	uint32 Cycles = 0;
};

class HAKONIWADRONESERVICE_API FHakoDroneServiceRc
{
public:
	static bool LoadDll(FString* OutError = nullptr);
	static void UnloadDll();
	static bool IsDllLoaded();

	static int32 Init(int32 bEnableDatalog, const FString& DroneConfigDirPath, const FString& DebugLogPath = FString());
	static int32 InitSingle(const FString& DroneConfigText, const FString& ControllerConfigText, bool bLoggerEnable, const FString& DebugLogPath = FString());
	static int32 Start();
	static int32 Run();
	static int32 AdvanceTimeUsec(uint64 TimeUsec);
	static int32 Stop();
	static int32 Reset();

	static int32 PutVertical(int32 Index, double Value);
	static int32 PutHorizontal(int32 Index, double Value);
	static int32 PutHeading(int32 Index, double Value);
	static int32 PutForward(int32 Index, double Value);
	static int32 PutRadioControlButton(int32 Index, int32 Value);
	static int32 PutMagnetControlButton(int32 Index, int32 Value);
	static int32 PutCameraControlButton(int32 Index, int32 Value);
	static int32 PutHomeControlButton(int32 Index, int32 Value);
	static int32 PutModeChangeButton(int32 Index, int32 Value);

	static int32 GetPosition(int32 Index, FHakoDroneServiceVector& OutPosition);
	static int32 GetAttitude(int32 Index, FHakoDroneServiceVector& OutAttitude);
	static int32 GetControls(int32 Index, double& C1, double& C2, double& C3, double& C4, double& C5, double& C6, double& C7, double& C8);
	static int32 GetBodyVelocity(int32 Index, FHakoDroneServiceVector& OutVelocity);
	static int32 GetBodyAngularVelocity(int32 Index, FHakoDroneServiceVector& OutAngularVelocity);
	static int32 GetPropellerWind(int32 Index, FHakoDroneServiceVector& OutWind);
	static int32 GetBatteryStatus(int32 Index, FHakoDroneBatteryStatus& OutBatteryStatus);
	static int32 GetInternalState(int32 Index, int32& OutState);
	static int32 GetFlightMode(int32 Index, int32& OutMode);
	static uint64 GetTimeUsec(int32 Index);

	static int32 PutDisturbance(int32 Index, double Temperature, double WindX, double WindY, double WindZ);
	static int32 PutCollision(int32 Index, double ContactX, double ContactY, double ContactZ, double RestitutionCoefficient);
};
