// Fill out your copyright notice in the Description page of Project Settings.


#include "DroneControl.h"
#include "HakoniwaClientInterface.h"
#include "HakoniwaAvatar.h"

#include <Kismet/GameplayStatics.h>

// Sets default values for this component's properties
UDroneControl::UDroneControl()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = true;

	// ...
}


// Called when the game starts
void UDroneControl::BeginPlay()
{
	Super::BeginPlay();

    AActor* Owner = GetOwner();
    if (!Owner) return;

    if (bDisableOnVisualizerOnlyAvatar)
    {
        if (const AHakoniwaAvatar* AvatarOwner = Cast<AHakoniwaAvatar>(Owner))
        {
            if (AvatarOwner->bReadVisualStateFromPdu)
            {
                bDisabledForVisualizerOnly = true;
                SetComponentTickEnabled(false);
                UE_LOG(LogTemp, Log, TEXT("DroneControl: disabled for visualizer-only Avatar owner=%s"),
                    *Owner->GetName());
                return;
            }
        }
    }

    // --- IDroneControlOp�����������R���|�[�l���g���擾 ---
    // ���̕��@���ƁAPdu�łł�Xbox�łł��A�����R�[�h�ŃR���|�[�l���g���擾�ł��܂�
    ControlOp = Owner->FindComponentByInterface(UDroneControlOp::StaticClass());

    if (!ControlOp)
    {
        UE_LOG(LogTemp, Warning, TEXT("DroneControl: IDroneControlOp component not found!"));
    }
    else {
        //ControlOp->DoInitialize(RobotName);
        IDroneControlOp::Execute_DoInitialize(ControlOp.GetObject(), RobotName);
    }

    // --- CameraControllerInterface�����������R���|�[�l���g���擾 ---
    CameraController = Owner->FindComponentByInterface(UCameraControllerInterface::StaticClass());

    if (!CameraController)
    {
        UE_LOG(LogTemp, Warning, TEXT("DroneControl: CameraControllerInterface component not found!"));
    }
}


// Called every frame
void UDroneControl::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
    if (bDisabledForVisualizerOnly)
    {
        return;
    }
    if (!ControlOp)
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
    if (PduManager_ != nullptr) {
        if (!ControlOp.GetObject())
        {
            return;
        }
        if (!IDroneControlOp::Execute_IsReady(ControlOp.GetObject())) {
            return;
        }
        if (CameraController && CameraController.GetObject() && CameraController->IsReady()) {
            HandleCameraControl(DeltaTime);
        }
    }
}

void UDroneControl::HandleCameraControl(float DeltaTime)
{
    if (!ControlOp || !ControlOp.GetObject() || !CameraController || !CameraController.GetObject())
    {
        return;
    }
    if (IDroneControlOp::Execute_IsYButtonPressed(ControlOp.GetObject())) {
        UE_LOG(LogTemp, Log, TEXT("SHOT!!"));
        CameraController->Scan();
        CameraController->WriteCameraDataPdu(PduManager_);
    }
    if (IDroneControlOp::Execute_IsUpButtonPressed(ControlOp.GetObject()))
    {
        UE_LOG(LogTemp, Log, TEXT("Up Pressed"));
        is_pressed_up = true;
    }
    else if (IDroneControlOp::Execute_IsUpButtonReleased(ControlOp.GetObject()))
    {
        UE_LOG(LogTemp, Log, TEXT("Up Released"));
        is_pressed_up = false;
    }
    if (is_pressed_up)
    {
        camera_move_button_time_duration += DeltaTime;
        if (camera_move_button_time_duration > camera_move_button_threshold_speedup)
        {
            CameraController->RotateCamera(move_step * 3.0);
        }
        else
        {
            CameraController->RotateCamera(move_step);
        }
        CameraController->UpdateCameraAngle();
    }
    if (IDroneControlOp::Execute_IsDownButtonPressed(ControlOp.GetObject()))
    {
        UE_LOG(LogTemp, Log, TEXT("Down Pressed"));
        is_pressed_down = true;
    }
    else if (IDroneControlOp::Execute_IsDownButtonReleased(ControlOp.GetObject()))
    {
        UE_LOG(LogTemp, Log, TEXT("Down Released"));
        is_pressed_down = false;
    }

    if (is_pressed_down)
    {
        camera_move_button_time_duration += DeltaTime;
        if (camera_move_button_time_duration > camera_move_button_threshold_speedup)
        {
            CameraController->RotateCamera(-move_step * 3.0);
        }
        else
        {
            CameraController->RotateCamera(-move_step);
        }
        CameraController->UpdateCameraAngle();
    }

    if (!is_pressed_down && !is_pressed_up)
    {
        camera_move_button_time_duration = 0;
    }
}
