#include "HakoDroneServiceRc.h"

#include "HAL/PlatformProcess.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/Paths.h"

#if PLATFORM_WINDOWS
extern "C"
{
	__declspec(dllimport) int drone_service_rc_init(int enable_datalog, const char* drone_config_dir_path, const char* debug_logpath);
	__declspec(dllimport) int drone_service_rc_init_single(const char* drone_config_text, const char* controller_config_text, int logger_enable, const char* debug_logpath);
	__declspec(dllimport) int drone_service_rc_start();
	__declspec(dllimport) int drone_service_rc_run();
	__declspec(dllimport) int drone_service_rc_advance_timesteps_usec(unsigned long long time_usec);
	__declspec(dllimport) int drone_service_rc_stop();
	__declspec(dllimport) int drone_service_rc_reset();
	__declspec(dllimport) int drone_service_rc_put_vertical(int index, double value);
	__declspec(dllimport) int drone_service_rc_put_horizontal(int index, double value);
	__declspec(dllimport) int drone_service_rc_put_heading(int index, double value);
	__declspec(dllimport) int drone_service_rc_put_forward(int index, double value);
	__declspec(dllimport) int drone_service_rc_put_radio_control_button(int index, int value);
	__declspec(dllimport) int drone_service_rc_put_magnet_control_button(int index, int value);
	__declspec(dllimport) int drone_service_rc_put_camera_control_button(int index, int value);
	__declspec(dllimport) int drone_service_rc_put_home_control_button(int index, int value);
	__declspec(dllimport) int drone_service_rc_put_mode_change_button(int index, int value);
	__declspec(dllimport) int drone_service_rc_get_position(int index, double* x, double* y, double* z);
	__declspec(dllimport) int drone_service_rc_get_attitude(int index, double* x, double* y, double* z);
	__declspec(dllimport) int drone_service_rc_get_controls(int index, double* c1, double* c2, double* c3, double* c4, double* c5, double* c6, double* c7, double* c8);
	__declspec(dllimport) int drone_service_rc_get_body_velocity(int index, double* x, double* y, double* z);
	__declspec(dllimport) int drone_service_rc_get_body_angular_velocity(int index, double* x, double* y, double* z);
	__declspec(dllimport) int drone_service_rc_get_propeller_wind(int index, double* x, double* y, double* z);
	__declspec(dllimport) int drone_service_rc_get_battery_status(int index, double* full_voltage, double* curr_voltage, double* curr_temp, unsigned int* status, unsigned int* cycles);
	__declspec(dllimport) int drone_service_rc_get_internal_state(int index, int* state);
	__declspec(dllimport) int drone_service_rc_get_flight_mode(int index, int* mode);
	__declspec(dllimport) unsigned long long drone_service_rc_get_time_usec(int index);
	__declspec(dllimport) int drone_service_rc_put_disturbance(int index, double d_temp, double d_wind_x, double d_wind_y, double d_wind_z);
	__declspec(dllimport) int drone_service_rc_put_collision(int index, double contact_position_x, double contact_position_y, double contact_position_z, double restitution_coefficient);
}
#endif

namespace
{
void* GHakoServiceDllHandle = nullptr;

const char* EmptyToNull(const FTCHARToUTF8& Converted, const FString& Original)
{
	return Original.IsEmpty() ? nullptr : Converted.Get();
}
}

bool FHakoDroneServiceRc::LoadDll(FString* OutError)
{
#if PLATFORM_WINDOWS
	if (GHakoServiceDllHandle != nullptr)
	{
		return true;
	}

	const FString DllPath = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir() / TEXT("Binaries/Win64/hako_service_c.dll"));
	if (!FPaths::FileExists(DllPath))
	{
		if (OutError)
		{
			*OutError = FString::Printf(TEXT("hako_service_c.dll not found: %s"), *DllPath);
		}
		return false;
	}

	GHakoServiceDllHandle = FPlatformProcess::GetDllHandle(*DllPath);
	if (GHakoServiceDllHandle == nullptr)
	{
		if (OutError)
		{
			*OutError = FString::Printf(TEXT("Failed to load hako_service_c.dll: %s"), *DllPath);
		}
		return false;
	}
	return true;
#else
	if (OutError)
	{
		*OutError = TEXT("hako_service_c is currently configured only for Win64.");
	}
	return false;
#endif
}

void FHakoDroneServiceRc::UnloadDll()
{
#if PLATFORM_WINDOWS
	if (GHakoServiceDllHandle != nullptr)
	{
		FPlatformProcess::FreeDllHandle(GHakoServiceDllHandle);
		GHakoServiceDllHandle = nullptr;
	}
#endif
}

bool FHakoDroneServiceRc::IsDllLoaded()
{
	return GHakoServiceDllHandle != nullptr;
}

int32 FHakoDroneServiceRc::Init(int32 bEnableDatalog, const FString& DroneConfigDirPath, const FString& DebugLogPath)
{
	FString Error;
	if (!LoadDll(&Error))
	{
		UE_LOG(LogTemp, Error, TEXT("%s"), *Error);
		return -1;
	}

	FTCHARToUTF8 ConfigDir(*DroneConfigDirPath);
	FTCHARToUTF8 DebugLog(*DebugLogPath);
	return drone_service_rc_init(bEnableDatalog, ConfigDir.Get(), EmptyToNull(DebugLog, DebugLogPath));
}

int32 FHakoDroneServiceRc::InitSingle(const FString& DroneConfigText, const FString& ControllerConfigText, bool bLoggerEnable, const FString& DebugLogPath)
{
	FString Error;
	if (!LoadDll(&Error))
	{
		UE_LOG(LogTemp, Error, TEXT("%s"), *Error);
		return -1;
	}

	FTCHARToUTF8 DroneConfig(*DroneConfigText);
	FTCHARToUTF8 ControllerConfig(*ControllerConfigText);
	FTCHARToUTF8 DebugLog(*DebugLogPath);
	return drone_service_rc_init_single(DroneConfig.Get(), ControllerConfig.Get(), bLoggerEnable ? 1 : 0, EmptyToNull(DebugLog, DebugLogPath));
}

int32 FHakoDroneServiceRc::Start()
{
	return drone_service_rc_start();
}

int32 FHakoDroneServiceRc::Run()
{
	return drone_service_rc_run();
}

int32 FHakoDroneServiceRc::AdvanceTimeUsec(uint64 TimeUsec)
{
	return drone_service_rc_advance_timesteps_usec(static_cast<unsigned long long>(TimeUsec));
}

int32 FHakoDroneServiceRc::Stop()
{
	return drone_service_rc_stop();
}

int32 FHakoDroneServiceRc::Reset()
{
	return drone_service_rc_reset();
}

int32 FHakoDroneServiceRc::PutVertical(int32 Index, double Value)
{
	return drone_service_rc_put_vertical(Index, Value);
}

int32 FHakoDroneServiceRc::PutHorizontal(int32 Index, double Value)
{
	return drone_service_rc_put_horizontal(Index, Value);
}

int32 FHakoDroneServiceRc::PutHeading(int32 Index, double Value)
{
	return drone_service_rc_put_heading(Index, Value);
}

int32 FHakoDroneServiceRc::PutForward(int32 Index, double Value)
{
	return drone_service_rc_put_forward(Index, Value);
}

int32 FHakoDroneServiceRc::PutRadioControlButton(int32 Index, int32 Value)
{
	return drone_service_rc_put_radio_control_button(Index, Value);
}

int32 FHakoDroneServiceRc::PutMagnetControlButton(int32 Index, int32 Value)
{
	return drone_service_rc_put_magnet_control_button(Index, Value);
}

int32 FHakoDroneServiceRc::PutCameraControlButton(int32 Index, int32 Value)
{
	return drone_service_rc_put_camera_control_button(Index, Value);
}

int32 FHakoDroneServiceRc::PutHomeControlButton(int32 Index, int32 Value)
{
	return drone_service_rc_put_home_control_button(Index, Value);
}

int32 FHakoDroneServiceRc::PutModeChangeButton(int32 Index, int32 Value)
{
	return drone_service_rc_put_mode_change_button(Index, Value);
}

int32 FHakoDroneServiceRc::GetPosition(int32 Index, FHakoDroneServiceVector& OutPosition)
{
	return drone_service_rc_get_position(Index, &OutPosition.X, &OutPosition.Y, &OutPosition.Z);
}

int32 FHakoDroneServiceRc::GetAttitude(int32 Index, FHakoDroneServiceVector& OutAttitude)
{
	return drone_service_rc_get_attitude(Index, &OutAttitude.X, &OutAttitude.Y, &OutAttitude.Z);
}

int32 FHakoDroneServiceRc::GetControls(int32 Index, double& C1, double& C2, double& C3, double& C4, double& C5, double& C6, double& C7, double& C8)
{
	return drone_service_rc_get_controls(Index, &C1, &C2, &C3, &C4, &C5, &C6, &C7, &C8);
}

int32 FHakoDroneServiceRc::GetBodyVelocity(int32 Index, FHakoDroneServiceVector& OutVelocity)
{
	return drone_service_rc_get_body_velocity(Index, &OutVelocity.X, &OutVelocity.Y, &OutVelocity.Z);
}

int32 FHakoDroneServiceRc::GetBodyAngularVelocity(int32 Index, FHakoDroneServiceVector& OutAngularVelocity)
{
	return drone_service_rc_get_body_angular_velocity(Index, &OutAngularVelocity.X, &OutAngularVelocity.Y, &OutAngularVelocity.Z);
}

int32 FHakoDroneServiceRc::GetPropellerWind(int32 Index, FHakoDroneServiceVector& OutWind)
{
	return drone_service_rc_get_propeller_wind(Index, &OutWind.X, &OutWind.Y, &OutWind.Z);
}

int32 FHakoDroneServiceRc::GetBatteryStatus(int32 Index, FHakoDroneBatteryStatus& OutBatteryStatus)
{
	unsigned int Status = 0;
	unsigned int Cycles = 0;
	const int32 Result = drone_service_rc_get_battery_status(
		Index,
		&OutBatteryStatus.FullVoltage,
		&OutBatteryStatus.CurrentVoltage,
		&OutBatteryStatus.CurrentTemperature,
		&Status,
		&Cycles);
	OutBatteryStatus.Status = Status;
	OutBatteryStatus.Cycles = Cycles;
	return Result;
}

int32 FHakoDroneServiceRc::GetInternalState(int32 Index, int32& OutState)
{
	int State = 0;
	const int32 Result = drone_service_rc_get_internal_state(Index, &State);
	OutState = State;
	return Result;
}

int32 FHakoDroneServiceRc::GetFlightMode(int32 Index, int32& OutMode)
{
	int Mode = 0;
	const int32 Result = drone_service_rc_get_flight_mode(Index, &Mode);
	OutMode = Mode;
	return Result;
}

uint64 FHakoDroneServiceRc::GetTimeUsec(int32 Index)
{
	return static_cast<uint64>(drone_service_rc_get_time_usec(Index));
}

int32 FHakoDroneServiceRc::PutDisturbance(int32 Index, double Temperature, double WindX, double WindY, double WindZ)
{
	return drone_service_rc_put_disturbance(Index, Temperature, WindX, WindY, WindZ);
}

int32 FHakoDroneServiceRc::PutCollision(int32 Index, double ContactX, double ContactY, double ContactZ, double RestitutionCoefficient)
{
	return drone_service_rc_put_collision(Index, ContactX, ContactY, ContactZ, RestitutionCoefficient);
}
