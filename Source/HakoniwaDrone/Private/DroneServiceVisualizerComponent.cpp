#include "DroneServiceVisualizerComponent.h"

#include "DroneLedComponent.h"
#include "DroneGameControllerPduWriterComponent.h"
#include "DronePropellerComponent.h"
#include "FlightModeLedComponent.h"
#include "HakoDroneServiceRc.h"
#include "HakoniwaAvatar.h"
#include "HakoniwaClientInterface.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "geometry_msgs/pdu_cpptype_conv_Twist.hpp"
#include "hako_mavlink_msgs/pdu_cpptype_conv_HakoHilActuatorControls.hpp"
#include "pdu_convertor.hpp"

UDroneServiceVisualizerComponent::UDroneServiceVisualizerComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UDroneServiceVisualizerComponent::BeginPlay()
{
	Super::BeginPlay();
	if (const AHakoniwaAvatar* AvatarOwner = Cast<AHakoniwaAvatar>(GetOwner()))
	{
		if (AvatarOwner->bReadVisualStateFromPdu)
		{
			bAutoInitializeService = false;
			bApplyControlInputFromOwner = false;
			bAdvanceService = false;
			bUpdateOwnerTransform = false;
			bDriveOwnerVisualsFromService = false;
			bWriteVisualPdus = false;
			SetComponentTickEnabled(false);
			UE_LOG(LogTemp, Log, TEXT("DroneServiceVisualizer: disabled for visualizer-only Avatar owner=%s"),
				*GetOwner()->GetName());
			return;
		}
	}
	if (bDisableLocalServiceWhenPduWriterExists && GetOwner() != nullptr && GetOwner()->FindComponentByClass<UDroneGameControllerPduWriterComponent>() != nullptr)
	{
		bAutoInitializeService = false;
		bApplyControlInputFromOwner = false;
		bAdvanceService = false;
		bUpdateOwnerTransform = false;
		bDriveOwnerVisualsFromService = false;
		bWriteVisualPdus = false;
		UE_LOG(LogTemp, Log, TEXT("DroneServiceVisualizer: local service disabled because GameControllerPduWriter exists on owner=%s"),
			*GetOwner()->GetName());
	}
	UE_LOG(LogTemp, Log, TEXT("DroneServiceVisualizer: BeginPlay owner=%s auto_init=%d apply_input=%d advance=%d update_transform=%d drive_visuals=%d write_pdus=%d"),
		GetOwner() ? *GetOwner()->GetName() : TEXT("None"),
		bAutoInitializeService ? 1 : 0,
		bApplyControlInputFromOwner ? 1 : 0,
		bAdvanceService ? 1 : 0,
		bUpdateOwnerTransform ? 1 : 0,
		bDriveOwnerVisualsFromService ? 1 : 0,
		bWriteVisualPdus ? 1 : 0);
	FindPduManager();
	ResolveRobotName();
	FindControlOp();
	FindOwnerVisualComponents();

	if (bAutoInitializeService)
	{
		UE_LOG(LogTemp, Log, TEXT("DroneServiceVisualizer: auto initialize requested"));
		InitializeDroneService();
	}
}

void UDroneServiceVisualizerComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	StopDroneService();
	Super::EndPlay(EndPlayReason);
}

void UDroneServiceVisualizerComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!bServiceStarted)
	{
		if (bAutoInitializeService && bRetryAutoInitializeService)
		{
			AutoInitializeRetryAccumulator += DeltaTime;
			if (AutoInitializeRetryAccumulator >= AutoInitializeRetryInterval)
			{
				AutoInitializeRetryAccumulator = 0.0f;
				UE_LOG(LogTemp, Log, TEXT("DroneServiceVisualizer: retrying auto initialize owner=%s"),
					GetOwner() ? *GetOwner()->GetName() : TEXT("None"));
				InitializeDroneService();
			}
		}
		if (bServiceStarted)
		{
			ResolveRobotName();
		}
	}

	if (!bServiceStarted)
	{
		if (bLogVisualizerState)
		{
			VisualizerStateLogAccumulator += DeltaTime;
			if (VisualizerStateLogAccumulator >= 1.0f)
			{
				VisualizerStateLogAccumulator = 0.0f;
				UE_LOG(LogTemp, Warning, TEXT("DroneServiceVisualizer: tick skipped; service is not started owner=%s auto_init=%d"),
					GetOwner() ? *GetOwner()->GetName() : TEXT("None"),
					bAutoInitializeService ? 1 : 0);
			}
		}
		return;
	}
	ResolveRobotName();

	if (bLogVisualizerState)
	{
		VisualizerStateLogAccumulator += DeltaTime;
		if (VisualizerStateLogAccumulator >= 1.0f)
		{
			VisualizerStateLogAccumulator = 0.0f;
			UE_LOG(LogTemp, Log, TEXT("DroneServiceVisualizer: tick active owner=%s robot=%s control=%s ready=%d pdu_manager=%s service_time=%llu"),
				GetOwner() ? *GetOwner()->GetName() : TEXT("None"),
				*RobotName,
				ControlOp ? *ControlOp.GetObject()->GetName() : TEXT("None"),
				(ControlOp && IDroneControlOp::Execute_IsReady(ControlOp.GetObject())) ? 1 : 0,
				PduManager ? TEXT("valid") : TEXT("null"),
				static_cast<unsigned long long>(FHakoDroneServiceRc::GetTimeUsec(DroneIndex)));
		}
	}

	

	if (bApplyControlInputFromOwner)
	{
		ApplyControlInput(DeltaTime);
	}

	if (bAdvanceService)
	{
		AdvanceService(DeltaTime);
	}

	if (bUpdateOwnerTransform)
	{
		UpdateOwnerFromService();
	}

	if (bDriveOwnerVisualsFromService)
	{
		UpdateOwnerVisualsFromService();
	}

	if (bWriteVisualPdus)
	{
		WriteVisualPdus();
	}
}

bool UDroneServiceVisualizerComponent::InitializeDroneService()
{
	if (bServiceStarted)
	{
		return true;
	}

	UE_LOG(LogTemp, Log, TEXT("DroneServiceVisualizer: InitializeDroneService start owner=%s use_init_single=%d drone_config=%s controller_config=%s config_dir=%s"),
		GetOwner() ? *GetOwner()->GetName() : TEXT("None"),
		bUseInitSingle ? 1 : 0,
		*DroneConfigTextPath,
		*ControllerConfigTextPath,
		*DroneConfigDirPath);

	FString Error;
	if (!FHakoDroneServiceRc::LoadDll(&Error))
	{
		UE_LOG(LogTemp, Error, TEXT("DroneServiceVisualizer: %s"), *Error);
		return false;
	}

	int32 Result = -1;
	if (bUseInitSingle)
	{
		FString DroneConfigText;
		FString ControllerConfigText;
		if (!LoadTextFileFromContent(DroneConfigTextPath, DroneConfigText))
		{
			UE_LOG(LogTemp, Error, TEXT("DroneServiceVisualizer: InitializeDroneService failed while loading drone config"));
			return false;
		}
		if (!LoadTextFileFromContent(ControllerConfigTextPath, ControllerConfigText))
		{
			UE_LOG(LogTemp, Error, TEXT("DroneServiceVisualizer: InitializeDroneService failed while loading controller config"));
			return false;
		}
		UE_LOG(LogTemp, Log, TEXT("DroneServiceVisualizer: calling InitSingle drone_config_len=%d controller_config_len=%d"),
			DroneConfigText.Len(),
			ControllerConfigText.Len());
		Result = FHakoDroneServiceRc::InitSingle(DroneConfigText, ControllerConfigText, bEnableDataLogger, DebugLogPath);
	}
	else
	{
		const FString FullConfigDir = FPaths::ConvertRelativePathToFull(FPaths::ProjectContentDir() / DroneConfigDirPath);
		UE_LOG(LogTemp, Log, TEXT("DroneServiceVisualizer: calling Init config_dir=%s"), *FullConfigDir);
		Result = FHakoDroneServiceRc::Init(bEnableDataLogger ? 1 : 0, FullConfigDir, DebugLogPath);
	}

	if (Result != 0)
	{
		UE_LOG(LogTemp, Error, TEXT("DroneServiceVisualizer: drone service init failed: %d"), Result);
		return false;
	}

	Result = FHakoDroneServiceRc::Start();
	if (Result != 0)
	{
		UE_LOG(LogTemp, Error, TEXT("DroneServiceVisualizer: drone service start failed: %d"), Result);
		return false;
	}

	bServiceStarted = true;
	AutoInitializeRetryAccumulator = 0.0f;
	ServiceStepAccumulator = 0.0f;
	ServiceTimeLogAccumulator = 0.0f;
	RadioControlPulseRemaining = 0.0f;
	LastRadioControlButtonValue = -1;
	ServiceStateDebugLogRemaining = 0.0f;
	UE_LOG(LogTemp, Log, TEXT("DroneServiceVisualizer: drone service started"));
	return true;
}

void UDroneServiceVisualizerComponent::StopDroneService()
{
	if (!bServiceStarted)
	{
		return;
	}

	const int32 Result = FHakoDroneServiceRc::Stop();
	if (Result != 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("DroneServiceVisualizer: drone service stop returned %d"), Result);
	}
	bServiceStarted = false;
}

bool UDroneServiceVisualizerComponent::LoadTextFileFromContent(const FString& RelativePath, FString& OutText) const
{
	const FString FullPath = FPaths::ConvertRelativePathToFull(FPaths::ProjectContentDir() / RelativePath);
	if (!FPaths::FileExists(FullPath))
	{
		UE_LOG(LogTemp, Error, TEXT("DroneServiceVisualizer: config file not found: %s"), *FullPath);
		return false;
	}
	if (!FFileHelper::LoadFileToString(OutText, *FullPath))
	{
		UE_LOG(LogTemp, Error, TEXT("DroneServiceVisualizer: failed to read config file: %s"), *FullPath);
		return false;
	}
	return true;
}

void UDroneServiceVisualizerComponent::FindPduManager()
{
	if (PduManager != nullptr || GetWorld() == nullptr)
	{
		return;
	}

	TArray<AActor*> Actors;
	UGameplayStatics::GetAllActorsWithInterface(GetWorld(), UHakoniwaClientInterface::StaticClass(), Actors);
	if (Actors.Num() > 0)
	{
		PduManager = IHakoniwaClientInterface::Execute_GetPduManager(Actors[0]);
	}
}


void UDroneServiceVisualizerComponent::ResolveRobotName()
{
	if (PduManager == nullptr)
	{
		FindPduManager();
	}
	if (PduManager == nullptr)
	{
		if (RobotName.IsEmpty())
		{
			RobotName = FallbackRobotName;
		}
		return;
	}

	const FString PreviousName = RobotName;
	if (bUseConfiguredRobotName || RobotName.IsEmpty())
	{
		const FString ConfiguredName = PduManager->GetDefaultRobotName();
		if (!ConfiguredName.IsEmpty())
		{
			RobotName = ConfiguredName;
		}
	}
	if (RobotName.IsEmpty())
	{
		RobotName = FallbackRobotName;
	}

	if (RobotName != PreviousName)
	{
		UE_LOG(LogTemp, Log, TEXT("DroneServiceVisualizer: Resolved RobotName=%s"), *RobotName);
	}
}
void UDroneServiceVisualizerComponent::FindControlOp()
{
	if (ControlOp || GetOwner() == nullptr)
	{
		return;
	}
	ResolveRobotName();
	

	ControlOp = GetOwner()->FindComponentByInterface(UDroneControlOp::StaticClass());
	if (!ControlOp)
	{
		TArray<UActorComponent*> Components;
		GetOwner()->GetComponents(Components);
		for (UActorComponent* Component : Components)
		{
			if (Component != nullptr && Component->GetClass()->ImplementsInterface(UDroneControlOp::StaticClass()))
			{
				ControlOp.SetObject(Component);
				ControlOp.SetInterface(Cast<IDroneControlOp>(Component));
				break;
			}
		}
	}
	if (ControlOp)
	{
		IDroneControlOp::Execute_DoInitialize(ControlOp.GetObject(), RobotName);
		UE_LOG(LogTemp, Log, TEXT("DroneServiceVisualizer: ControlOp found on %s for RobotName=%s"),
			*GetOwner()->GetName(),
			*RobotName);
	}
	else
	{
		TArray<UActorComponent*> Components;
		GetOwner()->GetComponents(Components);
		FString ComponentNames;
		for (UActorComponent* Component : Components)
		{
			if (Component != nullptr)
			{
				ComponentNames += Component->GetName();
				ComponentNames += TEXT(" ");
			}
		}
		UE_LOG(LogTemp, Warning, TEXT("DroneServiceVisualizer: ControlOp not found on %s components=[%s]"),
			*GetOwner()->GetName(),
			*ComponentNames);
	}
}

void UDroneServiceVisualizerComponent::FindOwnerVisualComponents()
{
	AActor* Owner = GetOwner();
	if (Owner == nullptr)
	{
		return;
	}

	if (Propeller == nullptr)
	{
		Propeller = Owner->FindComponentByClass<UDronePropellerComponent>();
	}
	if (DroneLed == nullptr)
	{
		DroneLed = Owner->FindComponentByClass<UDroneLedComponent>();
	}
	if (FlightModeLed == nullptr)
	{
		FlightModeLed = Owner->FindComponentByClass<UFlightModeLedComponent>();
	}
}

void UDroneServiceVisualizerComponent::ApplyControlInput(float DeltaTime)
{
	FindControlOp();
	if (!ControlOp)
	{
		if (bLogControlInput)
		{
			ControlInputLogAccumulator += DeltaTime;
			if (ControlInputLogAccumulator >= 1.0f)
			{
				ControlInputLogAccumulator = 0.0f;
				UE_LOG(LogTemp, Warning, TEXT("DroneServiceVisualizer: control input skipped; ControlOp is null"));
			}
		}
		return;
	}

	if (bPollControlInputInVisualizer)
	{
		IDroneControlOp::Execute_Run(ControlOp.GetObject());
	}

	if (!IDroneControlOp::Execute_IsReady(ControlOp.GetObject()))
	{
		if (bLogControlInput)
		{
			ControlInputLogAccumulator += DeltaTime;
			if (ControlInputLogAccumulator >= 1.0f)
			{
				ControlInputLogAccumulator = 0.0f;
				UE_LOG(LogTemp, Warning, TEXT("DroneServiceVisualizer: control input skipped; ControlOp is not ready"));
			}
		}
		return;
	}

	const FVector2D LeftStick = IDroneControlOp::Execute_GetLeftStickInput(ControlOp.GetObject());
	const FVector2D RightStick = IDroneControlOp::Execute_GetRightStickInput(ControlOp.GetObject());
	const double VerticalInput = (bInvertVerticalInput ? -LeftStick.X : LeftStick.X) * StickStrength;
	const double HeadingInput = (bInvertHeadingInput ? -LeftStick.Y : LeftStick.Y) * StickYawStrength;
	const double ForwardInput = (bInvertForwardInput ? -RightStick.X : RightStick.X) * StickStrength;
	const double HorizontalInput = (bInvertHorizontalInput ? -RightStick.Y : RightStick.Y) * StickStrength;
	const bool bAButtonPressed = IDroneControlOp::Execute_IsAButtonPressed(ControlOp.GetObject());
	const bool bAButtonReleased = IDroneControlOp::Execute_IsAButtonReleased(ControlOp.GetObject());
	const bool bBButtonPressed = IDroneControlOp::Execute_IsBButtonPressed(ControlOp.GetObject());
	const bool bBButtonReleased = IDroneControlOp::Execute_IsBButtonReleased(ControlOp.GetObject());
	const bool bXButtonPressed = IDroneControlOp::Execute_IsXButtonPressed(ControlOp.GetObject());
	const bool bXButtonReleased = IDroneControlOp::Execute_IsXButtonReleased(ControlOp.GetObject());
	const bool bYButtonPressed = IDroneControlOp::Execute_IsYButtonPressed(ControlOp.GetObject());
	const bool bYButtonReleased = IDroneControlOp::Execute_IsYButtonReleased(ControlOp.GetObject());

	if (bLogControlInput)
	{
		ControlInputLogAccumulator += DeltaTime;
		const bool bHasStickInput =
			FMath::Abs(LeftStick.X) > 0.01f ||
			FMath::Abs(LeftStick.Y) > 0.01f ||
			FMath::Abs(RightStick.X) > 0.01f ||
			FMath::Abs(RightStick.Y) > 0.01f;
		const bool bHasButtonEvent =
			bAButtonPressed || bAButtonReleased ||
			bBButtonPressed || bBButtonReleased ||
			bXButtonPressed || bXButtonReleased ||
			bYButtonPressed || bYButtonReleased;
		if (bHasButtonEvent || (bHasStickInput && ControlInputLogAccumulator >= 0.5f))
		{
			ControlInputLogAccumulator = 0.0f;
			UE_LOG(LogTemp, Log, TEXT("DroneServiceVisualizer: input raw L=(%.3f, %.3f) R=(%.3f, %.3f) svc V/Hdg/F/H=(%.3f, %.3f, %.3f, %.3f) A(P/R)=%d/%d B=%d/%d X=%d/%d Y=%d/%d PollInVisualizer=%d"),
				LeftStick.X,
				LeftStick.Y,
				RightStick.X,
				RightStick.Y,
				VerticalInput,
				HeadingInput,
				ForwardInput,
				HorizontalInput,
				bAButtonPressed ? 1 : 0,
				bAButtonReleased ? 1 : 0,
				bBButtonPressed ? 1 : 0,
				bBButtonReleased ? 1 : 0,
				bXButtonPressed ? 1 : 0,
				bXButtonReleased ? 1 : 0,
				bYButtonPressed ? 1 : 0,
				bYButtonReleased ? 1 : 0,
				bPollControlInputInVisualizer ? 1 : 0);
		}
	}

	FHakoDroneServiceRc::PutVertical(DroneIndex, VerticalInput);
	FHakoDroneServiceRc::PutHeading(DroneIndex, HeadingInput);
	FHakoDroneServiceRc::PutForward(DroneIndex, ForwardInput);
	FHakoDroneServiceRc::PutHorizontal(DroneIndex, HorizontalInput);

	if (bAButtonPressed)
	{
		if (bHoldRadioControlPulse && RadioControlPulseHoldSeconds > 0.0f)
		{
			RadioControlPulseRemaining = FMath::Max(RadioControlPulseRemaining, RadioControlPulseHoldSeconds);
			UE_LOG(LogTemp, Log, TEXT("DroneServiceVisualizer: radio control pulse started hold=%.3fs"), RadioControlPulseRemaining);
		}
		else
		{
			PutRadioControlButtonValue(1, TEXT("pressed"));
		}
		RequestServiceStateDebugLog(1.0f);
	}
	if (bAButtonReleased)
	{
		if (!bHoldRadioControlPulse || RadioControlPulseHoldSeconds <= 0.0f)
		{
			PutRadioControlButtonValue(0, TEXT("released"));
		}
		RequestServiceStateDebugLog(1.0f);
	}
	if (bHoldRadioControlPulse && RadioControlPulseHoldSeconds > 0.0f)
	{
		if (RadioControlPulseRemaining > 0.0f)
		{
			PutRadioControlButtonValue(1, TEXT("hold"));
			RadioControlPulseRemaining = FMath::Max(0.0f, RadioControlPulseRemaining - DeltaTime);
			if (RadioControlPulseRemaining <= 0.0f)
			{
				PutRadioControlButtonValue(0, TEXT("hold elapsed"));
				RequestServiceStateDebugLog(1.0f);
			}
		}
		else if (LastRadioControlButtonValue == 1)
		{
			PutRadioControlButtonValue(0, TEXT("idle"));
		}
	}
	if (bBButtonPressed)
	{
		FHakoDroneServiceRc::PutMagnetControlButton(DroneIndex, 1);
	}
	if (bBButtonReleased)
	{
		FHakoDroneServiceRc::PutMagnetControlButton(DroneIndex, 0);
	}
	if (bXButtonPressed)
	{
		FHakoDroneServiceRc::PutModeChangeButton(DroneIndex, 1);
	}
	if (bXButtonReleased)
	{
		FHakoDroneServiceRc::PutModeChangeButton(DroneIndex, 0);
	}
	if (bYButtonPressed)
	{
		FHakoDroneServiceRc::PutCameraControlButton(DroneIndex, 1);
	}
	if (bYButtonReleased)
	{
		FHakoDroneServiceRc::PutCameraControlButton(DroneIndex, 0);
	}
}

void UDroneServiceVisualizerComponent::AdvanceService(float DeltaTime)
{
	int32 RunSteps = 0;
	if (bUseAccumulatedRunSteps)
	{
		const float EffectiveStepSeconds = FMath::Max(ServiceStepSeconds, 0.0001f);
		const int32 EffectiveMaxSteps = FMath::Max(1, MaxRunStepsPerTick);
		ServiceStepAccumulator += DeltaTime;
		while (ServiceStepAccumulator >= EffectiveStepSeconds && RunSteps < EffectiveMaxSteps)
		{
			FHakoDroneServiceRc::Run();
			ServiceStepAccumulator -= EffectiveStepSeconds;
			++RunSteps;
		}
	}
	else if (bUseRunFixedSteps)
	{
		RunSteps = FMath::Max(1, RunStepsPerTick);
		for (int32 Index = 0; Index < RunSteps; ++Index)
		{
			FHakoDroneServiceRc::Run();
		}
	}
	else if (AdvanceTimeUsec > 0)
	{
		FHakoDroneServiceRc::AdvanceTimeUsec(static_cast<uint64>(AdvanceTimeUsec));
		RunSteps = -1;
	}
	else
	{
		FHakoDroneServiceRc::Run();
		RunSteps = 1;
	}

	if (bLogServiceTime)
	{
		ServiceTimeLogAccumulator += DeltaTime;
		if (ServiceTimeLogAccumulator >= 1.0f)
		{
			ServiceTimeLogAccumulator = 0.0f;
			UE_LOG(LogTemp, Log, TEXT("DroneServiceVisualizer: service_time_usec=%llu, run_steps=%d"),
				static_cast<unsigned long long>(FHakoDroneServiceRc::GetTimeUsec(DroneIndex)),
				RunSteps);
		}
	}

	if (ServiceStateDebugLogRemaining > 0.0f)
	{
		ServiceStateDebugLogRemaining = FMath::Max(0.0f, ServiceStateDebugLogRemaining - DeltaTime);
		if (bLogControlInput)
		{
			LogServiceStateSnapshot(TEXT("after advance"));
		}
	}
}

void UDroneServiceVisualizerComponent::PutRadioControlButtonValue(int32 Value, const TCHAR* Reason)
{
	if (LastRadioControlButtonValue != Value)
	{
		UE_LOG(LogTemp, Log, TEXT("DroneServiceVisualizer: PutRadioControlButton(%d) reason=%s"), Value, Reason);
	}
	const int32 Result = FHakoDroneServiceRc::PutRadioControlButton(DroneIndex, Value);
	if (Result != 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("DroneServiceVisualizer: PutRadioControlButton(%d) failed result=%d reason=%s"), Value, Result, Reason);
	}
	LastRadioControlButtonValue = Value;
}

void UDroneServiceVisualizerComponent::RequestServiceStateDebugLog(float DurationSeconds)
{
	ServiceStateDebugLogRemaining = FMath::Max(ServiceStateDebugLogRemaining, DurationSeconds);
}

void UDroneServiceVisualizerComponent::LogServiceStateSnapshot(const TCHAR* Reason) const
{
	int32 InternalState = -1;
	int32 FlightMode = -1;
	const int32 StateResult = FHakoDroneServiceRc::GetInternalState(DroneIndex, InternalState);
	const int32 FlightModeResult = FHakoDroneServiceRc::GetFlightMode(DroneIndex, FlightMode);

	double C1 = 0.0;
	double C2 = 0.0;
	double C3 = 0.0;
	double C4 = 0.0;
	double C5 = 0.0;
	double C6 = 0.0;
	double C7 = 0.0;
	double C8 = 0.0;
	const int32 ControlsResult = FHakoDroneServiceRc::GetControls(DroneIndex, C1, C2, C3, C4, C5, C6, C7, C8);

	UE_LOG(LogTemp, Log, TEXT("DroneServiceVisualizer: service state %s state=%d(state_ret=%d) flight_mode=%d(flight_ret=%d) controls_ret=%d C1..C4=[%.4f %.4f %.4f %.4f] time=%llu radio=%d"),
		Reason,
		InternalState,
		StateResult,
		FlightMode,
		FlightModeResult,
		ControlsResult,
		C1,
		C2,
		C3,
		C4,
		static_cast<unsigned long long>(FHakoDroneServiceRc::GetTimeUsec(DroneIndex)),
		LastRadioControlButtonValue);
}

void UDroneServiceVisualizerComponent::UpdateOwnerFromService()
{
	AActor* Owner = GetOwner();
	if (Owner == nullptr)
	{
		return;
	}

	FHakoDroneServiceVector Position;
	FHakoDroneServiceVector Attitude;
	if (FHakoDroneServiceRc::GetPosition(DroneIndex, Position) == 0)
	{
		Owner->SetActorLocation(ServicePositionToUnreal(Position.X, Position.Y, Position.Z));
	}
	if (FHakoDroneServiceRc::GetAttitude(DroneIndex, Attitude) == 0)
	{
		Owner->SetActorRotation(ServiceAttitudeToUnreal(Attitude.X, Attitude.Y, Attitude.Z));
	}
}

void UDroneServiceVisualizerComponent::UpdateOwnerVisualsFromService()
{
	FindOwnerVisualComponents();

	double C1 = 0.0;
	double C2 = 0.0;
	double C3 = 0.0;
	double C4 = 0.0;
	double C5 = 0.0;
	double C6 = 0.0;
	double C7 = 0.0;
	double C8 = 0.0;
	const bool bHasControls = FHakoDroneServiceRc::GetControls(DroneIndex, C1, C2, C3, C4, C5, C6, C7, C8) == 0;

	if (Propeller != nullptr && bHasControls)
	{
		Propeller->Rotate(
			static_cast<float>(C1),
			static_cast<float>(C2),
			static_cast<float>(C3),
			static_cast<float>(C4));
	}

	if (DroneLed != nullptr)
	{
		if (!bHasControls || C1 <= 0.001)
		{
			DroneLed->SetMode(EDroneMode::DISARM);
		}
		else
		{
			int32 InternalState = 0;
			if (FHakoDroneServiceRc::GetInternalState(DroneIndex, InternalState) == 0)
			{
				switch (InternalState)
				{
				case 0:
					DroneLed->SetMode(EDroneMode::TAKEOFF);
					break;
				case 1:
					DroneLed->SetMode(EDroneMode::HOVER);
					break;
				case 2:
					DroneLed->SetMode(EDroneMode::LANDING);
					break;
				default:
					DroneLed->SetMode(EDroneMode::DISARM);
					break;
				}
			}
		}
	}

	if (FlightModeLed != nullptr)
	{
		int32 FlightMode = 0;
		if (FHakoDroneServiceRc::GetFlightMode(DroneIndex, FlightMode) == 0)
		{
			FlightModeLed->SetMode(FlightMode == 0 ? EFlightMode::ATTI : EFlightMode::GPS);
		}
	}
}

void UDroneServiceVisualizerComponent::WriteVisualPdus()
{
	FindPduManager();
	if (PduManager == nullptr || !PduManager->IsServiceEnabled())
	{
		return;
	}

	AActor* Owner = GetOwner();
	if (Owner != nullptr)
	{
		WritePositionPdu(Owner->GetActorLocation(), Owner->GetActorRotation());
	}

	double C1 = 0.0;
	double C2 = 0.0;
	double C3 = 0.0;
	double C4 = 0.0;
	double C5 = 0.0;
	double C6 = 0.0;
	double C7 = 0.0;
	double C8 = 0.0;
	if (FHakoDroneServiceRc::GetControls(DroneIndex, C1, C2, C3, C4, C5, C6, C7, C8) == 0)
	{
		WriteMotorPdu(C1, C2, C3, C4, C5, C6, C7, C8);
	}
}

void UDroneServiceVisualizerComponent::WritePositionPdu(const FVector& UnrealPosition, const FRotator& UnrealRotation)
{
	if (!bPositionPduDeclared)
	{
		bPositionPduDeclared = PduManager->DeclarePduForWrite(RobotName, PositionPduName);
		if (!bPositionPduDeclared)
		{
			return;
		}
	}

	const int32 PduSize = PduManager->GetPduSize(RobotName, PositionPduName);
	if (PduSize <= 0)
	{
		return;
	}

	HakoCpp_Twist Twist = {};
	Twist.linear.x = UnrealPosition.Y / UnrealScale;
	Twist.linear.y = -UnrealPosition.X / UnrealScale;
	Twist.linear.z = UnrealPosition.Z / UnrealScale;
	Twist.angular.x = -FMath::DegreesToRadians(UnrealRotation.Roll);
	Twist.angular.y = FMath::DegreesToRadians(UnrealRotation.Pitch);
	Twist.angular.z = -FMath::DegreesToRadians(UnrealRotation.Yaw);

	TArray<uint8> Buffer;
	Buffer.SetNum(PduSize);
	hako::pdu::PduConvertor<HakoCpp_Twist, hako::pdu::msgs::geometry_msgs::Twist> Converter;
	Converter.cpp2pdu(Twist, reinterpret_cast<char*>(Buffer.GetData()), Buffer.Num());
	PduManager->FlushPduRawData(RobotName, PositionPduName, Buffer);
}

void UDroneServiceVisualizerComponent::WriteMotorPdu(double C1, double C2, double C3, double C4, double C5, double C6, double C7, double C8)
{
	if (!bMotorPduDeclared)
	{
		bMotorPduDeclared = PduManager->DeclarePduForWrite(RobotName, MotorPduName);
		if (!bMotorPduDeclared)
		{
			return;
		}
	}

	const int32 PduSize = PduManager->GetPduSize(RobotName, MotorPduName);
	if (PduSize <= 0)
	{
		return;
	}

	HakoCpp_HakoHilActuatorControls Motor = {};
	Motor.controls[0] = static_cast<float>(C1);
	Motor.controls[1] = static_cast<float>(C2);
	Motor.controls[2] = static_cast<float>(C3);
	Motor.controls[3] = static_cast<float>(C4);
	Motor.controls[4] = static_cast<float>(C5);
	Motor.controls[5] = static_cast<float>(C6);
	Motor.controls[6] = static_cast<float>(C7);
	Motor.controls[7] = static_cast<float>(C8);

	TArray<uint8> Buffer;
	Buffer.SetNum(PduSize);
	hako::pdu::PduConvertor<HakoCpp_HakoHilActuatorControls, hako::pdu::msgs::hako_mavlink_msgs::HakoHilActuatorControls> Converter;
	Converter.cpp2pdu(Motor, reinterpret_cast<char*>(Buffer.GetData()), Buffer.Num());
	PduManager->FlushPduRawData(RobotName, MotorPduName, Buffer);
}

FVector UDroneServiceVisualizerComponent::ServicePositionToUnreal(double X, double Y, double Z) const
{
	return FVector(static_cast<float>(-Y * UnrealScale), static_cast<float>(X * UnrealScale), static_cast<float>(Z * UnrealScale));
}

FRotator UDroneServiceVisualizerComponent::ServiceAttitudeToUnreal(double RollRad, double PitchRad, double YawRad) const
{
	const float RollDeg = FMath::RadiansToDegrees(static_cast<float>(RollRad));
	const float PitchDeg = FMath::RadiansToDegrees(static_cast<float>(PitchRad));
	const float YawDeg = FMath::RadiansToDegrees(static_cast<float>(YawRad));
	return FRotator(PitchDeg, -YawDeg, -RollDeg);
}
