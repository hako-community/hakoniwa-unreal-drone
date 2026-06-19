// Fill out your copyright notice in the Description page of Project Settings.


#include "DroneCameraActor.h"
#include "hako_msgs/pdu_cpptype_conv_HakoCameraInfo.hpp" 
#include "hako_msgs/pdu_cpptype_conv_HakoCameraData.hpp" 
#include "hako_msgs/pdu_cpptype_conv_HakoCmdCameraMove.hpp" 
#include "hako_msgs/pdu_cpptype_conv_HakoCmdCamera.hpp" 
#include "pdu_convertor.hpp"
#include "IImageWrapperModule.h"
#include "IImageWrapper.h"
#include "Modules/ModuleManager.h"
#include "HakoniwaAvatar.h"
#include "HakoniwaClientInterface.h"
#include <Kismet/GameplayStatics.h>

ADroneCameraActor::ADroneCameraActor()
{
    PrimaryActorTick.bCanEverTick = true;
    PduManager_ = nullptr;
}

void ADroneCameraActor::BeginPlay()
{
    Super::BeginPlay();
    IsDeclared = false;

    // BeginPlay��Blueprint�R���|�[�l���g������
    // �܂��A�e�A�N�^�[���擾���܂�
    AActor* ParentActor = GetParentActor();

    if (ParentActor)
    {
        // �e�A�N�^�[�����R���|�[�l���g�̒�����SceneCaptureComponent2D��T���܂�
        SceneCapture = ParentActor->FindComponentByClass<USceneCaptureComponent2D>();
    }

    if (SceneCapture)
    {
        UE_LOG(LogTemp, Log, TEXT("Found SceneCaptureComponent2D on the Parent Actor!"));
        Initialize();
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("Could not find SceneCaptureComponent2D on the Parent Actor."));
    }

    UE_LOG(LogTemp, Log, TEXT("DroneCameraActor's BeginPlay has finished."));
}
bool ADroneCameraActor::IsReady()
{
    return false;
}
void ADroneCameraActor::OnHakoInitialize_Implementation()
{
    UE_LOG(LogTemp, Log, TEXT("ADroneCameraActor::OnHakoInitialize_Implementation called"));
    if (PduManager_ == nullptr) {
        TArray<AActor*> Actors;
        UGameplayStatics::GetAllActorsWithInterface(GetWorld(), UHakoniwaClientInterface::StaticClass(), Actors);
        if (Actors.Num() > 0)
        {
            PduManager_ = IHakoniwaClientInterface::Execute_GetPduManager(Actors[0]);
        }
    }
    if (PduManager_ != nullptr && !IsDeclared) {
        DeclarePdu(RobotName, PduManager_);
        IsDeclared = true;
    }
}

void ADroneCameraActor::OnHakoReset_Implementation()
{
    UE_LOG(LogTemp, Log, TEXT("ADroneCameraActor::OnHakoReset_Implementation called"));
    // Reset camera angle etc.
}

void ADroneCameraActor::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    if (PduManager_ != nullptr && IsDeclared)
    {
        CameraMoveRequest(PduManager_);
        CameraImageRequest(PduManager_);
    }
}

// ICameraControllerInterface �̊֐��Q

void ADroneCameraActor::Initialize()
{
    // SceneCapture ��T���i�G�f�B�^�z�u or �蓮�����j
    if (!SceneCapture)
    {
        UE_LOG(LogTemp, Error, TEXT("SceneCaptureComponent2D not found"));
        return;
    }

    // RenderTarget �𐶐�
    RenderTarget = NewObject<UTextureRenderTarget2D>(this);
    RenderTarget->InitAutoFormat(640, 480);
    RenderTarget->ClearColor = FLinearColor::Black;
    RenderTarget->UpdateResourceImmediate(true);

    // SceneCapture �ɃA�^�b�`
    SceneCapture->TextureTarget = RenderTarget;
    SceneCapture->CaptureSource = ESceneCaptureSource::SCS_FinalColorLDR;
    SceneCapture->bCaptureEveryFrame = false;
    SceneCapture->bCaptureOnMovement = false;

    // �����l
    ManualRotationDeg = 0.0f;
    MoveStep = 1.0f;
    CameraMoveUpDeg = -15.0f;
    CameraMoveDownDeg = 90.0f;
    // �g�p����PDU���̒�`�i�Œ�j
    PduCmdCamera = TEXT("hako_cmd_camera");
    PduCmdCameraMove = TEXT("hako_cmd_camera_move");
    PduCameraData = TEXT("hako_camera_data");
    PduCameraInfo = TEXT("hako_cmd_camera_info");

    UE_LOG(LogTemp, Log, TEXT("DroneCameraActor::Initialize completed"));
}

void ADroneCameraActor::UpdateCameraAngle()
{
    if (!SceneCapture) return;

    // Pitch �������}�j���A������A���̎��͌Œ�
    FRotator NewRotation = SceneCapture->GetRelativeRotation();
    NewRotation.Pitch = ManualRotationDeg;
    SceneCapture->SetRelativeRotation(NewRotation);
}

bool ADroneCameraActor::DeclarePdu(const FString& InRobotName, UPduManager* PduManager)
{
    if (!PduManager)
    {
        UE_LOG(LogTemp, Error, TEXT("DeclarePdu: PduManager is null"));
        return false;
    }

    RobotName = InRobotName;


    bool Result = true;
    Result &= PduManager->DeclarePduForRead(RobotName, PduCmdCamera);
    Result &= PduManager->DeclarePduForRead(RobotName, PduCmdCameraMove);
    Result &= PduManager->DeclarePduForWrite(RobotName, PduCameraData);
    Result &= PduManager->DeclarePduForWrite(RobotName, PduCameraInfo);

    if (Result)
    {
        UE_LOG(LogTemp, Log, TEXT("DeclarePdu: All declarations succeeded"));
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("DeclarePdu: One or more PDU declarations failed"));
    }
    return Result;
}

void ADroneCameraActor::RotateCamera(float Step)
{
    SetCameraAngle(ManualRotationDeg + Step);
}

void ADroneCameraActor::WriteCameraInfo(int32 InMoveCurrentId, UPduManager* PduManager)
{
    if (!PduManager || PduCameraInfo.IsEmpty())
    {
        UE_LOG(LogTemp, Error, TEXT("WriteCameraInfo: PduManager is null or Pdu name is not set"));
        return;
    }

    // PDU�T�C�Y�擾
    int32 PduSize = PduManager->GetPduSize(RobotName, PduCameraInfo);
    if (PduSize <= 0)
    {
        UE_LOG(LogTemp, Error, TEXT("WriteCameraInfo: Failed to get PDU size for %s:%s"), *RobotName, *PduCameraInfo);
        return;
    }

    TArray<uint8> Buffer;
    Buffer.SetNum(PduSize);

    // HakoCameraInfo �ϊ��\���̂��g���ăf�[�^�\�z
    HakoCpp_HakoCameraInfo CameraInfo;
    CameraInfo.request_id = InMoveCurrentId;
    CameraInfo.angle.x = 0.0f;
    CameraInfo.angle.y = ManualRotationDeg;
    CameraInfo.angle.z = 0.0f;

    hako::pdu::PduConvertor<HakoCpp_HakoCameraInfo, hako::pdu::msgs::hako_msgs::HakoCameraInfo> Conv;
    Conv.cpp2pdu(CameraInfo, (char*)Buffer.GetData(), Buffer.Num());

    // ��������
    if (!PduManager->FlushPduRawData(RobotName, PduCameraInfo, Buffer))
    {
        UE_LOG(LogTemp, Error, TEXT("WriteCameraInfo: Failed to flush PDU data"));
    }
    else
    {
        UE_LOG(LogTemp, Log, TEXT("WriteCameraInfo: Sent camera info. yaw=%.2f"), ManualRotationDeg);
    }
}
void ADroneCameraActor::WriteCameraDataPdu(UPduManager* PduManager)
{
    if (!PduManager || PduCameraData.IsEmpty()) {
        UE_LOG(LogTemp, Error, TEXT("WriteCameraDataPdu: Invalid state"));
        return;
    }

    int32 PduSize = PduManager->GetPduSize(RobotName, PduCameraData);
    if (PduSize <= 0) {
        UE_LOG(LogTemp, Error, TEXT("WriteCameraDataPdu: Failed to get PDU size"));
        return;
    }

    TArray<uint8> Buffer;
    Buffer.SetNum(PduSize);

    // �\���̍\�z
    HakoCpp_HakoCameraData CameraData;
    CameraData.request_id = CurrentId;

    // �^�C���X�^���v��ݒ�
    int64 NowUsec = FDateTime::UtcNow().ToUnixTimestamp() * 1000000 + (FPlatformTime::Cycles64() % 1000000);
    CameraData.image.header.stamp.sec = static_cast<int32>(NowUsec / 1000000);
    CameraData.image.header.stamp.nanosec = static_cast<uint32>(NowUsec % 1000000) * 1000;

    CameraData.image.header.frame_id = TCHAR_TO_ANSI(*RobotName);  // Unreal��FString��char*
    CameraData.image.format = (EncodeType == 0) ? "png" : "jpeg";

    // �o�b�t�@�R�s�[
    CameraData.image.data.resize(CompressedImageBytes.Num());
    FMemory::Memcpy(CameraData.image.data.data(), CompressedImageBytes.GetData(), CompressedImageBytes.Num());

    hako::pdu::PduConvertor<HakoCpp_HakoCameraData, hako::pdu::msgs::hako_msgs::HakoCameraData> Conv;
    Conv.cpp2pdu(CameraData, (char*)Buffer.GetData(), Buffer.Num());

    if (!PduManager->FlushPduRawData(RobotName, PduCameraData, Buffer)) {
        UE_LOG(LogTemp, Error, TEXT("WriteCameraDataPdu: Failed to flush PDU"));
    }
    else {
        UE_LOG(LogTemp, Log, TEXT("WriteCameraDataPdu: PDU sent successfully. Size = %d"), CompressedImageBytes.Num());
    }
}



void ADroneCameraActor::Scan()
{
    if (!RenderTarget || !SceneCapture)
    {
        UE_LOG(LogTemp, Error, TEXT("Scan: RenderTarget or SceneCapture not initialized"));
        return;
    }

    SceneCapture->CaptureScene();

    FTextureRenderTargetResource* RenderResource = RenderTarget->GameThread_GetRenderTargetResource();
    TArray<FColor> PixelData;
    FReadSurfaceDataFlags ReadDataFlags;
    ReadDataFlags.SetLinearToGamma(false);

    if (!RenderResource->ReadPixels(PixelData, ReadDataFlags) || PixelData.Num() == 0)
    {
        UE_LOG(LogTemp, Error, TEXT("Scan: Failed to read pixels from RenderTarget"));
        return;
    }

    int32 Width = RenderTarget->SizeX;
    int32 Height = RenderTarget->SizeY;

    // �㉺���] & RGB ���o
    RawImageBytes.Reset();
    RawImageBytes.AddUninitialized(Width * Height * 4);


    for (int32 y = 0; y < Height; y++)
    {
        for (int32 x = 0; x < Width; x++)
        {
            int32 SrcIndex = (Height - 1 - y) * Width + x;
            int32 DstIndex = (y * Width + x) * 4;
            const FColor& Color = PixelData[SrcIndex];
            RawImageBytes[DstIndex + 0] = Color.R;
            RawImageBytes[DstIndex + 1] = Color.G;
            RawImageBytes[DstIndex + 2] = Color.B;
            RawImageBytes[DstIndex + 3] = 255;
        }
    }

    // ImageWrapper ���g���ăG���R�[�h
    IImageWrapperModule& ImageWrapperModule = FModuleManager::LoadModuleChecked<IImageWrapperModule>(FName("ImageWrapper"));
    EImageFormat ImageFormat = (EncodeType == 0) ? EImageFormat::PNG : EImageFormat::JPEG;

    TSharedPtr<IImageWrapper> ImageWrapper = ImageWrapperModule.CreateImageWrapper(ImageFormat);

    if (ImageWrapper.IsValid() &&
        ImageWrapper->SetRaw(RawImageBytes.GetData(), RawImageBytes.Num(), Width, Height, ERGBFormat::RGBA, 8))
    {
        CompressedImageBytes = ImageWrapper->GetCompressed();
        UE_LOG(LogTemp, Log, TEXT("Scan: Image encoded. Size = %d bytes"), CompressedImageBytes.Num());
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("Scan: Failed to encode image"));
    }
}


void ADroneCameraActor::SetCameraAngle(float Angle)
{
    float NewPitch = Angle;

    if (NewPitch > 180.0f) NewPitch -= 360.0f;
    ManualRotationDeg = FMath::Clamp(NewPitch, CameraMoveUpDeg, CameraMoveDownDeg);
}

void ADroneCameraActor::UpdateCameraImageTexture()
{
    //TODO
}

void ADroneCameraActor::CameraImageRequest(UPduManager* PduManager)
{
    if (!PduManager || PduCmdCamera.IsEmpty()) {
        UE_LOG(LogTemp, Error, TEXT("CameraImageRequest: Invalid state"));
        return;
    }

    int32 PduSize = PduManager->GetPduSize(RobotName, PduCmdCamera);
    if (PduSize <= 0) {
        UE_LOG(LogTemp, Error, TEXT("CameraImageRequest: Failed to get PDU size"));
        return;
    }

    TArray<uint8> Buffer;
    Buffer.SetNum(PduSize);
    if (!PduManager->ReadPduRawData(RobotName, PduCmdCamera, Buffer)) {
        UE_LOG(LogTemp, Error, TEXT("CameraImageRequest: Failed to read PDU"));
        return;
    }

    HakoCpp_HakoCmdCamera Cmd;
    hako::pdu::PduConvertor<HakoCpp_HakoCmdCamera, hako::pdu::msgs::hako_msgs::HakoCmdCamera> Conv;
    Conv.pdu2cpp((char*)Buffer.GetData(), Cmd);

    if (Cmd.header.request) {
        RequestId = Cmd.request_id;
        if (CurrentId != RequestId) {
            CurrentId = RequestId;
            UE_LOG(LogTemp, Log, TEXT("CameraImageRequest: New request received. id=%d"), RequestId);
            Scan();  // �J�����摜���M�������Ă�
            WriteCameraDataPdu(PduManager);
        }
    }
}

void ADroneCameraActor::CameraMoveRequest(UPduManager* PduManager)
{
    if (!PduManager || PduCmdCameraMove.IsEmpty()) {
        UE_LOG(LogTemp, Error, TEXT("CameraMoveRequest: Invalid state"));
        return;
    }

    int32 PduSize = PduManager->GetPduSize(RobotName, PduCmdCameraMove);
    if (PduSize <= 0) {
        UE_LOG(LogTemp, Error, TEXT("CameraMoveRequest: Failed to get PDU size"));
        return;
    }

    TArray<uint8> Buffer;
    Buffer.SetNum(PduSize);
    if (!PduManager->ReadPduRawData(RobotName, PduCmdCameraMove, Buffer)) {
        UE_LOG(LogTemp, Error, TEXT("CameraMoveRequest: Failed to read PDU"));
        return;
    }

    HakoCpp_HakoCmdCameraMove CmdMove;
    hako::pdu::PduConvertor<HakoCpp_HakoCmdCameraMove, hako::pdu::msgs::hako_msgs::HakoCmdCameraMove> Conv;
    Conv.pdu2cpp((char*)Buffer.GetData(), CmdMove);

    if (CmdMove.header.request) {
        MoveRequestId = CmdMove.request_id;
        if (MoveCurrentIdInternal != MoveRequestId) {
            MoveCurrentIdInternal = MoveRequestId;
            float TargetAngle = -CmdMove.angle.y;  // Unity�Ɠ��l��Y�����]
            SetCameraAngle(TargetAngle);
            UpdateCameraAngle();
            WriteCameraInfo(MoveCurrentIdInternal, PduManager);
            UE_LOG(LogTemp, Log, TEXT("CameraMoveRequest: angle.y = %.2f (target = %.2f)"), CmdMove.angle.y, TargetAngle);
        }
    }
}

