#include "DroneGameControllerPduWriterComponent.h"

#include "DroneControlPdu.h"
#include "HakoniwaClientInterface.h"
#include "Kismet/GameplayStatics.h"
#include "hako_capi.h"
#include "pdu_convertor.hpp"

UDroneGameControllerPduWriterComponent::UDroneGameControllerPduWriterComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UDroneGameControllerPduWriterComponent::BeginPlay()
{
	Super::BeginPlay();
	FindPduManager();
	ResolveRobotName();
	FindControlOp();
}

void UDroneGameControllerPduWriterComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	FindPduManager();
	if (PduManager == nullptr || !PduManager->IsServiceEnabled())
	{
		return;
	}

	ResolveRobotName();
	FindControlOp();

	if (bAutoDeclarePdu && !bDeclared)
	{
		DeclarePdu();
	}
	if (!bDeclared || !ControlOp)
	{
		return;
	}

	if (bPollControlInputInWriter)
	{
		IDroneControlOp::Execute_Run(ControlOp.GetObject());
	}
	if (!IDroneControlOp::Execute_IsReady(ControlOp.GetObject()))
	{
		return;
	}

	WriteGameControllerPdu(DeltaTime);
}

void UDroneGameControllerPduWriterComponent::FindPduManager()
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

void UDroneGameControllerPduWriterComponent::ResolveRobotName()
{
	if (PduManager == nullptr)
	{
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
		UE_LOG(LogTemp, Log, TEXT("GameControllerPduWriter: Resolved RobotName=%s"), *RobotName);
	}
}

void UDroneGameControllerPduWriterComponent::FindControlOp()
{
	if (ControlOp || GetOwner() == nullptr)
	{
		return;
	}

	ControlOp = GetOwner()->FindComponentByInterface(UDroneControlOp::StaticClass());
	if (!ControlOp)
	{
		TArray<UActorComponent*> Components;
		GetOwner()->GetComponents(Components);
		for (UActorComponent* Component : Components)
		{
			if (Component != nullptr && Component != this && Component->GetClass()->ImplementsInterface(UDroneControlOp::StaticClass()))
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
		if (bAutoDisableUnityMappingForPduSource && Cast<UDroneControlPdu>(ControlOp.GetObject()) != nullptr)
		{
			bApplyUnityInputMapping = false;
			UE_LOG(LogTemp, Log, TEXT("GameControllerPduWriter: Unity input mapping disabled because ControlOp is UDroneControlPdu"));
		}
		UE_LOG(LogTemp, Log, TEXT("GameControllerPduWriter: ControlOp found on %s for RobotName=%s"),
			*GetOwner()->GetName(),
			*RobotName);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("GameControllerPduWriter: ControlOp not found on %s"),
			*GetOwner()->GetName());
	}
}

bool UDroneGameControllerPduWriterComponent::DeclarePdu()
{
	FindPduManager();
	if (PduManager == nullptr || !PduManager->IsServiceEnabled())
	{
		return false;
	}

	ResolveRobotName();
	UE_LOG(LogTemp, Log, TEXT("GameControllerPduWriter: declaring %s:%s for write"), *RobotName, *PduName);
	bDeclared = PduManager->DeclarePduForWrite(RobotName, PduName);
	if (bDeclared)
	{
		const int32 ChannelId = PduManager->GetPduChannelId(RobotName, PduName);
		const int32 PduSize = PduManager->GetPduSize(RobotName, PduName);
		UE_LOG(LogTemp, Log, TEXT("GameControllerPduWriter: declared %s:%s for write channel=%d size=%d global_write_for_drone_pro=%d"),
			*RobotName,
			*PduName,
			ChannelId,
			PduSize,
			bWriteGlobalPduForDronePro ? 1 : 0);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("GameControllerPduWriter: failed to declare %s:%s for write"), *RobotName, *PduName);
	}
	return bDeclared;
}

bool UDroneGameControllerPduWriterComponent::WriteGameControllerPdu(float DeltaTime)
{
	const int32 PduSize = PduManager->GetPduSize(RobotName, PduName);
	if (PduSize <= 0)
	{
		UE_LOG(LogTemp, Error, TEXT("GameControllerPduWriter: failed to get PDU size for %s:%s"), *RobotName, *PduName);
		return false;
	}

	const FVector2D LeftStick = IDroneControlOp::Execute_GetLeftStickInput(ControlOp.GetObject());
	const FVector2D RightStick = IDroneControlOp::Execute_GetRightStickInput(ControlOp.GetObject());
	const bool bAButtonPressed = IDroneControlOp::Execute_IsAButtonPressed(ControlOp.GetObject());
	const bool bAButtonReleased = IDroneControlOp::Execute_IsAButtonReleased(ControlOp.GetObject());

	const double VerticalInput = bApplyUnityInputMapping ? (bInvertVerticalInput ? -LeftStick.X : LeftStick.X) * StickStrength : LeftStick.X;
	const double HeadingInput = bApplyUnityInputMapping ? (bInvertHeadingInput ? -LeftStick.Y : LeftStick.Y) * StickYawStrength : LeftStick.Y;
	const double ForwardInput = bApplyUnityInputMapping ? (bInvertForwardInput ? -RightStick.X : RightStick.X) * StickStrength : RightStick.X;
	const double HorizontalInput = bApplyUnityInputMapping ? (bInvertHorizontalInput ? -RightStick.Y : RightStick.Y) * StickStrength : RightStick.Y;

	HakoCpp_GameControllerOperation Pdu = {};
	Pdu.axis[GameOps::StickTurnLR] = HeadingInput;
	Pdu.axis[GameOps::StickUpDown] = VerticalInput;
	Pdu.axis[GameOps::StickMoveLR] = HorizontalInput;
	Pdu.axis[GameOps::StickMoveFB] = ForwardInput;
	UpdateButtonPulse(DeltaTime, bAButtonPressed, bAButtonReleased, Pdu);

	if (IDroneControlOp::Execute_IsBButtonPressed(ControlOp.GetObject()))
	{
		ButtonStates[GameOps::GrabBaggageButtonIndex] = true;
	}
	else if (IDroneControlOp::Execute_IsBButtonReleased(ControlOp.GetObject()))
	{
		ButtonStates[GameOps::GrabBaggageButtonIndex] = false;
	}
	if (IDroneControlOp::Execute_IsXButtonPressed(ControlOp.GetObject()))
	{
		ButtonStates[GameOps::FlightModeChangeIndex] = true;
	}
	else if (IDroneControlOp::Execute_IsXButtonReleased(ControlOp.GetObject()))
	{
		ButtonStates[GameOps::FlightModeChangeIndex] = false;
	}
	if (IDroneControlOp::Execute_IsYButtonPressed(ControlOp.GetObject()))
	{
		ButtonStates[GameOps::CameraButtonIndex] = true;
	}
	else if (IDroneControlOp::Execute_IsYButtonReleased(ControlOp.GetObject()))
	{
		ButtonStates[GameOps::CameraButtonIndex] = false;
	}
	if (IDroneControlOp::Execute_IsUpButtonPressed(ControlOp.GetObject()))
	{
		ButtonStates[GameOps::CameraMoveUpIndex] = true;
	}
	else if (IDroneControlOp::Execute_IsUpButtonReleased(ControlOp.GetObject()))
	{
		ButtonStates[GameOps::CameraMoveUpIndex] = false;
	}
	if (IDroneControlOp::Execute_IsDownButtonPressed(ControlOp.GetObject()))
	{
		ButtonStates[GameOps::CameraMoveDownIndex] = true;
	}
	else if (IDroneControlOp::Execute_IsDownButtonReleased(ControlOp.GetObject()))
	{
		ButtonStates[GameOps::CameraMoveDownIndex] = false;
	}
	for (int32 Index = 1; Index < 15; ++Index)
	{
		Pdu.button[Index] = ButtonStates[Index];
	}

	TArray<uint8> Buffer;
	Buffer.SetNum(PduSize);
	hako::pdu::PduConvertor<HakoCpp_GameControllerOperation, hako::pdu::msgs::hako_msgs::GameControllerOperation> Converter;
	Converter.cpp2pdu(Pdu, reinterpret_cast<char*>(Buffer.GetData()), Buffer.Num());

	const bool bFlushed = PduManager->FlushPduRawData(RobotName, PduName, Buffer);
	if (!bFlushed)
	{
		UE_LOG(LogTemp, Error, TEXT("GameControllerPduWriter: failed to flush %s:%s"), *RobotName, *PduName);
		return false;
	}

	bool bGlobalWriteSucceeded = true;
	int32 GlobalWriteChannelId = -1;
	if (bWriteGlobalPduForDronePro)
	{
		const int32 ChannelId = PduManager->GetPduChannelId(RobotName, PduName);
		if (ChannelId < 0)
		{
			UE_LOG(LogTemp, Error, TEXT("GameControllerPduWriter: failed to get channel id for %s:%s"), *RobotName, *PduName);
			return false;
		}
		bGlobalWriteSucceeded = hako_asset_write_pdu_nolock(
			TCHAR_TO_ANSI(*RobotName),
			ChannelId,
			reinterpret_cast<const char*>(Buffer.GetData()),
			static_cast<size_t>(Buffer.Num()));
		if (!bGlobalWriteSucceeded)
		{
			UE_LOG(LogTemp, Error, TEXT("GameControllerPduWriter: failed global write for drone-pro %s:%s channel=%d size=%d"),
				*RobotName,
				*PduName,
				ChannelId,
				Buffer.Num());
			return false;
		}
		GlobalWriteChannelId = ChannelId;
	}

	if (bLogPduWrites)
	{
		InputLogAccumulator += DeltaTime;
		const bool bHasStickInput =
			FMath::Abs(Pdu.axis[GameOps::StickTurnLR]) > InputLogDeadzone ||
			FMath::Abs(Pdu.axis[GameOps::StickUpDown]) > InputLogDeadzone ||
			FMath::Abs(Pdu.axis[GameOps::StickMoveLR]) > InputLogDeadzone ||
			FMath::Abs(Pdu.axis[GameOps::StickMoveFB]) > InputLogDeadzone;
		const bool bButtonChanged = LastButton0Value != (Pdu.button[GameOps::ArmButtonIndex] ? 1 : 0);
		if (bButtonChanged || (bHasStickInput && InputLogAccumulator >= 0.5f))
		{
			InputLogAccumulator = 0.0f;
			UE_LOG(LogTemp, Log, TEXT("GameControllerPduWriter: write %s:%s channel=%d size=%d flush=1 global=%d axis=[%.3f %.3f %.3f %.3f] button0=%d raw L=(%.3f %.3f) R=(%.3f %.3f)"),
				*RobotName,
				*PduName,
				GlobalWriteChannelId,
				Buffer.Num(),
				(bWriteGlobalPduForDronePro && bGlobalWriteSucceeded) ? 1 : 0,
				Pdu.axis[GameOps::StickTurnLR],
				Pdu.axis[GameOps::StickUpDown],
				Pdu.axis[GameOps::StickMoveLR],
				Pdu.axis[GameOps::StickMoveFB],
				Pdu.button[GameOps::ArmButtonIndex] ? 1 : 0,
				LeftStick.X,
				LeftStick.Y,
				RightStick.X,
				RightStick.Y);
		}
	}

	LastButton0Value = Pdu.button[GameOps::ArmButtonIndex] ? 1 : 0;
	return true;
}

void UDroneGameControllerPduWriterComponent::UpdateButtonPulse(float DeltaTime, bool bAButtonPressed, bool bAButtonReleased, HakoCpp_GameControllerOperation& OutPdu)
{
	if (bAButtonPressed)
	{
		if (bHoldRadioControlPulse && RadioControlPulseHoldSeconds > 0.0f)
		{
			RadioControlPulseRemaining = FMath::Max(RadioControlPulseRemaining, RadioControlPulseHoldSeconds);
		}
		else
		{
			OutPdu.button[GameOps::ArmButtonIndex] = true;
			return;
		}
	}
	else if (bAButtonReleased && (!bHoldRadioControlPulse || RadioControlPulseHoldSeconds <= 0.0f))
	{
		OutPdu.button[GameOps::ArmButtonIndex] = false;
		return;
	}

	if (bHoldRadioControlPulse && RadioControlPulseHoldSeconds > 0.0f && RadioControlPulseRemaining > 0.0f)
	{
		OutPdu.button[GameOps::ArmButtonIndex] = true;
		RadioControlPulseRemaining = FMath::Max(0.0f, RadioControlPulseRemaining - DeltaTime);
	}
	else
	{
		OutPdu.button[GameOps::ArmButtonIndex] = false;
	}
}
