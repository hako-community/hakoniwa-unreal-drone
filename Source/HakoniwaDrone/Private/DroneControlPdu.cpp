// Fill out your copyright notice in the Description page of Project Settings.


#include "DroneControlPdu.h"
#include "hako_msgs/pdu_cpptype_conv_HakoCmdMagnetHolder.hpp" 
#include "hako_msgs/pdu_cpptype_conv_HakoStatusMagnetHolder.hpp"
#include "pdu_convertor.hpp"
#include "HakoniwaClientInterface.h"
#include "HakoniwaAvatar.h"
#include <Kismet/GameplayStatics.h>

// Sets default values for this component's properties
UDroneControlPdu::UDroneControlPdu()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = true;

	// ...
}


// Called when the game starts
void UDroneControlPdu::BeginPlay()
{
	Super::BeginPlay();

	AActor* Owner = GetOwner();
	UE_LOG(LogTemp, Log, TEXT("UDroneControlPdu: BeginPlay owner=%s owner_class=%s pdu=%s disable_on_visualizer_avatar=%d"),
		Owner ? *Owner->GetName() : TEXT("None"),
		Owner ? *Owner->GetClass()->GetName() : TEXT("None"),
		*PduName,
		bDisableOnVisualizerOnlyAvatar ? 1 : 0);

	if (bDisableOnVisualizerOnlyAvatar)
	{
		if (const AHakoniwaAvatar* AvatarOwner = Cast<AHakoniwaAvatar>(Owner))
		{
			if (AvatarOwner->bReadVisualStateFromPdu)
			{
				bDisabledForVisualizerOnly = true;
				SetComponentTickEnabled(false);
				UE_LOG(LogTemp, Log, TEXT("UDroneControlPdu: disabled for visualizer-only Avatar owner=%s pdu=%s"),
					*GetOwner()->GetName(),
					*PduName);
			}
		}
		else if (Owner != nullptr && Owner->GetClass()->GetName().Contains(TEXT("HakoniwaAvatar")))
		{
			bDisabledForVisualizerOnly = true;
			SetComponentTickEnabled(false);
			UE_LOG(LogTemp, Warning, TEXT("UDroneControlPdu: disabled because owner class name looks like Avatar but cast failed owner=%s class=%s pdu=%s"),
				*Owner->GetName(),
				*Owner->GetClass()->GetName(),
				*PduName);
		}
	}
	
}


// Called every frame
void UDroneControlPdu::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (bDisabledForVisualizerOnly)
	{
		return;
	}
	if (PduManager_ == nullptr) {
		if (GetOwner())
		{
			TArray<AActor*> Actors;
			UGameplayStatics::GetAllActorsWithInterface(GetOwner()->GetWorld(), UHakoniwaClientInterface::StaticClass(), Actors);
			if (Actors.Num() > 0)
			{
				PduManager_ = IHakoniwaClientInterface::Execute_GetPduManager(Actors[0]);
			}
		}
	}
	if (PduManager_ != nullptr && PduManager_->IsServiceEnabled()) {
		if (!IsDeclared) {
			DeclarePdu();
		}
		else {
			InputLogAccumulator += DeltaTime;
			IDroneControlOp::Execute_Run(this);
		}
	}
}

void UDroneControlPdu::InitializeComponent()
{
	Super::InitializeComponent();

	CurrStates.axis = {};
	CurrStates.button = {};
	PrevStates.axis = {};
	PrevStates.button = {};
}


void UDroneControlPdu::ResolveRobotName()
{
	if (PduManager_ == nullptr)
	{
		return;
	}

	const FString PreviousName = RobotName;
	if (bUseConfiguredRobotName || RobotName.IsEmpty())
	{
		const FString ConfiguredName = PduManager_->GetDefaultRobotName();
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
		UE_LOG(LogTemp, Log, TEXT("UDroneControlPdu: Resolved RobotName=%s"), *RobotName);
	}
}
void UDroneControlPdu::DeclarePdu()
{
	if (bDisabledForVisualizerOnly)
	{
		return;
	}
	if (!PduManager_)
	{
		UE_LOG(LogTemp, Error, TEXT("UDroneControlPdu: Can not get Pdu Manager"));
		return;
	}

	if (!PduManager_->IsServiceEnabled())
	{
		UE_LOG(LogTemp, Warning, TEXT("UDroneControlPdu: DeclarePdu called but PduManager service is not enabled yet"));
		return;
	}
	ResolveRobotName();

	

	UE_LOG(LogTemp, Log, TEXT("UDroneControlPdu: Starting PDU Declaration for %s: %s"), *RobotName, *PduName);
	bool bResult = PduManager_->DeclarePduForRead(RobotName, PduName);
	if (bResult)
	{
		UE_LOG(LogTemp, Log, TEXT("UDroneControlPdu: Successfully declared %s:%s"), *RobotName, *PduName);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("UDroneControlPdu: FAILED to declare %s:%s"), *RobotName, *PduName);
	}

#if 0 //not supported yet
...
#endif
	IsDeclared = bResult;
}

bool UDroneControlPdu::IsReady_Implementation()
{
	if (bDisabledForVisualizerOnly)
	{
		return false;
	}
	return IsDeclared;
}
// --- �C���^�t�F�[�X�֐��̎��� ---

void UDroneControlPdu::DoInitialize_Implementation(const FString& InRobotName)
{
	if (!InRobotName.IsEmpty())
	{
		this->RobotName = InRobotName;
	}
}
void UDroneControlPdu::Run_Implementation()
{
	if (bDisabledForVisualizerOnly)
	{
		return;
	}
	//read gamectrl
	int32 PduSize = PduManager_->GetPduSize(RobotName, PduName);
	if (PduSize <= 0)
	{
		UE_LOG(LogTemp, Error, TEXT("WriteCameraInfo: Failed to get PDU size for %s:%s"), *RobotName, *PduName);
		return;
	}
	TArray<uint8> Buffer;
	Buffer.SetNum(PduSize);
	if (PduManager_->ReadPduRawData(RobotName, PduName, Buffer)) {
		PrevStates = CurrStates;
		hako::pdu::PduConvertor<HakoCpp_GameControllerOperation, hako::pdu::msgs::hako_msgs::GameControllerOperation> Conv;
		Conv.pdu2cpp((char*)Buffer.GetData(), CurrStates);
		if (bLogInputState)
		{
			const bool bHasStickInput =
				FMath::Abs(CurrStates.axis[GameOps::StickTurnLR]) > InputLogDeadzone ||
				FMath::Abs(CurrStates.axis[GameOps::StickUpDown]) > InputLogDeadzone ||
				FMath::Abs(CurrStates.axis[GameOps::StickMoveLR]) > InputLogDeadzone ||
				FMath::Abs(CurrStates.axis[GameOps::StickMoveFB]) > InputLogDeadzone;
			const bool bHasButtonEvent =
				IsButtonPressed(GameOps::ArmButtonIndex) ||
				IsButtonReleased(GameOps::ArmButtonIndex) ||
				IsButtonPressed(GameOps::GrabBaggageButtonIndex) ||
				IsButtonReleased(GameOps::GrabBaggageButtonIndex) ||
				IsButtonPressed(GameOps::FlightModeChangeIndex) ||
				IsButtonReleased(GameOps::FlightModeChangeIndex) ||
				IsButtonPressed(GameOps::CameraButtonIndex) ||
				IsButtonReleased(GameOps::CameraButtonIndex);
			if (bHasButtonEvent || (bHasStickInput && InputLogAccumulator >= 0.5f))
			{
				InputLogAccumulator = 0.0f;
				UE_LOG(LogTemp, Log, TEXT("UDroneControlPdu: read %s:%s axis=[%.3f %.3f %.3f %.3f] button0=%d pressed=%d released=%d"),
					*RobotName,
					*PduName,
					CurrStates.axis[GameOps::StickTurnLR],
					CurrStates.axis[GameOps::StickUpDown],
					CurrStates.axis[GameOps::StickMoveLR],
					CurrStates.axis[GameOps::StickMoveFB],
					CurrStates.button[GameOps::ArmButtonIndex] ? 1 : 0,
					IsButtonPressed(GameOps::ArmButtonIndex) ? 1 : 0,
					IsButtonReleased(GameOps::ArmButtonIndex) ? 1 : 0);
			}
		}
	}
}

void UDroneControlPdu::Flush_Implementation()
{
	if (!PduManager_) {
		return;
	}
#if 0 //not supported yet
	/*
	 * Magnet
	 */
	if (bUseMagnet)
	{
		int32 PduSize = PduManager_->GetPduSize(RobotName, PduNameStatusMagnet);
		if (PduSize <= 0)
		{
			UE_LOG(LogTemp, Error, TEXT("WriteCameraInfo: Failed to get PDU size for %s:%s"), *RobotName, *PduNameStatusMagnet);
			return;
		}

		TArray<uint8> Buffer;
		Buffer.SetNum(PduSize);

		HakoCpp_HakoStatusMagnetHolder status_magnet;
		status_magnet.magnet_on = bStatusMagnet_MagnetOn;
		status_magnet.contact_on = bStatusMagnet_ContactOn;
		hako::pdu::PduConvertor<HakoCpp_HakoStatusMagnetHolder, hako::pdu::msgs::hako_msgs::HakoStatusMagnetHolder> Conv;
		Conv.cpp2pdu(status_magnet, (char*)Buffer.GetData(), Buffer.Num());
		PduManager_->FlushPduRawData(RobotName, PduNameStatusMagnet, Buffer);
	}
	UE_LOG(LogTemp, Log, TEXT("DoFlush Called. PDU writing logic should be implemented here."));
#endif
}
bool UDroneControlPdu::IsButtonPressed(int32 ButtonIndex) const
{
	// �u����͉�����Ă���vAND�u�O��͉�����Ă��Ȃ������v
	return CurrStates.button[ButtonIndex] && !PrevStates.button[ButtonIndex];
}

bool UDroneControlPdu::IsButtonReleased(int32 ButtonIndex) const
{
	// �u����͉�����Ă��Ȃ��vAND�u�O��͉�����Ă����v
	return !CurrStates.button[ButtonIndex] && PrevStates.button[ButtonIndex];
}

FVector2D UDroneControlPdu::GetLeftStickInput_Implementation()
{
	FVector2D vec;
	vec.X = CurrStates.axis[GameOps::StickUpDown];
	vec.Y = CurrStates.axis[GameOps::StickTurnLR];
	return vec;
}

FVector2D UDroneControlPdu::GetRightStickInput_Implementation()
{
	FVector2D vec;
	vec.X = CurrStates.axis[GameOps::StickMoveFB];
	vec.Y = CurrStates.axis[GameOps::StickMoveLR];
	return vec;
}

bool UDroneControlPdu::IsAButtonPressed_Implementation()
{
	return IsButtonPressed(GameOps::ArmButtonIndex);
}

bool UDroneControlPdu::IsAButtonReleased_Implementation()
{
	return IsButtonReleased(GameOps::ArmButtonIndex);
}

bool UDroneControlPdu::IsBButtonPressed_Implementation()
{
	return IsButtonPressed(GameOps::GrabBaggageButtonIndex);
}

bool UDroneControlPdu::IsBButtonReleased_Implementation()
{
	return IsButtonReleased(GameOps::GrabBaggageButtonIndex);
}

bool UDroneControlPdu::IsXButtonPressed_Implementation()
{
	return IsButtonPressed(GameOps::FlightModeChangeIndex);
}

bool UDroneControlPdu::IsXButtonReleased_Implementation()
{
	return IsButtonReleased(GameOps::FlightModeChangeIndex);
}

bool UDroneControlPdu::IsYButtonPressed_Implementation()
{
	return IsButtonPressed(GameOps::CameraButtonIndex);
}

bool UDroneControlPdu::IsYButtonReleased_Implementation()
{
	return IsButtonReleased(GameOps::CameraButtonIndex);
}

bool UDroneControlPdu::IsUpButtonPressed_Implementation()
{
	return IsButtonPressed(GameOps::CameraMoveUpIndex);
}

bool UDroneControlPdu::IsUpButtonReleased_Implementation()
{
	return IsButtonReleased(GameOps::CameraMoveUpIndex);
}

bool UDroneControlPdu::IsDownButtonPressed_Implementation()
{
	return IsButtonPressed(GameOps::CameraMoveDownIndex);
}

bool UDroneControlPdu::IsDownButtonReleased_Implementation()
{
	return IsButtonReleased(GameOps::CameraMoveDownIndex);
}
