// Fill out your copyright notice in the Description page of Project Settings.


#include "HakoniwaAvatar.h"
#include "HakoniwaClientInterface.h"
#include "geometry_msgs/pdu_cpptype_conv_Twist.hpp"
#include "hako_mavlink_msgs/pdu_cpptype_conv_HakoHilActuatorControls.hpp"
#include "hako_msgs/pdu_cpptype_conv_DroneStatus.hpp"
#include "hako_msgs/pdu_cpptype_conv_ImpulseCollision.hpp"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformTime.h"
#include "pdu_convertor.hpp"
#include <Kismet/GameplayStatics.h>

#ifndef HAKO_PDU_LATENCY_LOG
#define HAKO_PDU_LATENCY_LOG 0
#endif

#ifndef HAKO_PROP_STATS_LOG
#define HAKO_PROP_STATS_LOG 1
#endif

#ifndef HAKO_PROP_DETAILED_LOG
#define HAKO_PROP_DETAILED_LOG 0
#endif

static TAutoConsoleVariable<float> CVarHakoPduLatencyStatsIntervalSec(
    TEXT("hako.Pdu.LatencyStatsIntervalSec"),
    5.0f,
    TEXT("Interval in seconds for Hakoniwa PDU recv-to-apply latency distribution logs."),
    ECVF_Default);

static TAutoConsoleVariable<float> CVarHakoPduLatencyStatsMaxDelayMs(
    TEXT("hako.Pdu.LatencyStatsMaxDelayMs"),
    100.0f,
    TEXT("Samples above this recv-to-apply delay are excluded from normal latency stats. Default excludes PIE teardown spikes."),
    ECVF_Default);

// Sets default values
AHakoniwaAvatar::AHakoniwaAvatar()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;
}

void AHakoniwaAvatar::ResolveDroneName()
{
    if (pduManager == nullptr)
    {
        return;
    }

    const FString PreviousName = DroneName;
    if (bUseConfiguredRobotName || DroneName.IsEmpty())
    {
        const FString ConfiguredName = pduManager->GetDefaultRobotName();
        if (!ConfiguredName.IsEmpty())
        {
            DroneName = ConfiguredName;
        }
    }
    if (DroneName.IsEmpty())
    {
        DroneName = FallbackDroneName;
    }

    if (DroneName != PreviousName)
    {
        UE_LOG(LogTemp, Log, TEXT("AHakoniwaAvatar: Resolved DroneName=%s"), *DroneName);
    }
}
void AHakoniwaAvatar::DeclarePdu()
{
    if (pduManager == nullptr) {
        TArray<AActor*> Actors;
        UGameplayStatics::GetAllActorsWithInterface(GetWorld(), UHakoniwaClientInterface::StaticClass(), Actors);
        if (Actors.Num() > 0)
        {
            pduManager = IHakoniwaClientInterface::Execute_GetPduManager(Actors[0]);
        }
    }
    
    if (pduManager == nullptr) {
        return;
    }

        ResolveDroneName();

	if (isDeclared == false) {
        // Only declare if service is enabled
        if (pduManager->IsServiceEnabled()) {
            UE_LOG(LogTemp, Log, TEXT("AHakoniwaAvatar: Starting PDU Declaration for %s"), *DroneName);
            
            bool bSuccess = true;
            if (bReadVisualStateFromPdu) {
                if (pduManager->DeclarePduForRead(DroneName, "motor")) {
                    UE_LOG(LogTemp, Log, TEXT("AHakoniwaAvatar: Successfully declared %s:motor"), *DroneName);
                } else {
                    UE_LOG(LogTemp, Error, TEXT("AHakoniwaAvatar: FAILED to declare %s:motor"), *DroneName);
                    bSuccess = false;
                }

                if (pduManager->DeclarePduForRead(DroneName, "pos")) {
                    UE_LOG(LogTemp, Log, TEXT("AHakoniwaAvatar: Successfully declared %s:pos"), *DroneName);
                } else {
                    UE_LOG(LogTemp, Error, TEXT("AHakoniwaAvatar: FAILED to declare %s:pos"), *DroneName);
                    bSuccess = false;
                }

                if (pduManager->DeclarePduForRead(DroneName, "status")) {
                    UE_LOG(LogTemp, Log, TEXT("AHakoniwaAvatar: Successfully declared %s:status"), *DroneName);
                } else {
                    UE_LOG(LogTemp, Error, TEXT("AHakoniwaAvatar: FAILED to declare %s:status"), *DroneName);
                    bSuccess = false;
                }
            } else {
                UE_LOG(LogTemp, Log, TEXT("AHakoniwaAvatar: Skipping visual PDU read declarations for local RC mode"));
            }

            if (pduManager->DeclarePduForWrite(DroneName, "impulse")) {
                UE_LOG(LogTemp, Log, TEXT("AHakoniwaAvatar: Successfully declared %s:impulse"), *DroneName);
            } else {
                UE_LOG(LogTemp, Error, TEXT("AHakoniwaAvatar: FAILED to declare %s:impulse"), *DroneName);
                bSuccess = false;
            }

            if (bSuccess) {
                isDeclared = true;
                UE_LOG(LogTemp, Log, TEXT("AHakoniwaAvatar: All PDUs declared successfully for %s"), *DroneName);
                LogPduChannelCheck();
            }
        }
    }
}
void AHakoniwaAvatar::DoTask(float DeltaTime)
{
    bool bPosRead = false;
    bool bMotorRead = false;
    bool bStatusRead = false;
    FVector PduPosition = FVector::ZeroVector;
    double PosDelayMs = 0.0;
    bool bHasPosLatencySample = false;
    double Motor0 = 0.0;
    HakoCpp_DroneStatus status = {};

    TArray<uint8> buffer;
    FPduBufferMeta PosMeta;
    ReadWithMeta("pos", buffer, PosMeta);
    if (buffer.Num() > 0) {
        bPosRead = true;
        HakoCpp_Twist pos;
        hako::pdu::PduConvertor<HakoCpp_Twist, hako::pdu::msgs::geometry_msgs::Twist> conv;
        conv.pdu2cpp((char*)buffer.GetData(), pos);
        PduPosition = FVector(pos.linear.x, pos.linear.y, pos.linear.z);
        FVector NewLocation(pos.linear.x * 100.0f, -pos.linear.y * 100.0f, pos.linear.z * 100.0f);

        FRotator NewRotation = FRotator(
            -FMath::RadiansToDegrees(pos.angular.y),   // Pitch ← ROSのPitch（Y軸）→ 符号反転
            -FMath::RadiansToDegrees(pos.angular.z),   // Yaw   ← ROSのYaw（Z軸） → 符号反転
            FMath::RadiansToDegrees(pos.angular.x)     // Roll  ← ROSのRoll（X軸）→ そのまま
        );

        if (FMath::IsFinite(NewLocation.X) && FMath::IsFinite(NewLocation.Y) && FMath::IsFinite(NewLocation.Z)) {
            AActor* ParentActor = this;
            if (ParentActor && IsValid(ParentActor))
            {
                FVector CurrentLocation = ParentActor->GetActorLocation();
                FRotator CurrentRotation = ParentActor->GetActorRotation();
                FVector CurrentScale = ParentActor->GetActorScale3D();
                #if 0
                // 急激な変更を避ける
                float MaxDistance = 1000.0f; // 1フレームでの最大移動距離
                if (FVector::Dist(CurrentLocation, NewLocation) > MaxDistance)
                {
                    UE_LOG(LogTemp, Warning, TEXT("Position change too large, skipping update"));
                    return;
                }
                #endif

                FTransform NewTransform(NewRotation, NewLocation, CurrentScale);
#if HAKO_PDU_LATENCY_LOG
                const double ApplyUeTime = FPlatformTime::Seconds();
                const double DelayMs = (ApplyUeTime - PosMeta.RecvUeTime) * 1000.0;
                LatestPosCounter = PosMeta.RecvCounter;
                LatestPosDelayMs = DelayMs;
                PosDelayMs = DelayMs;
                bHasPosLatencySample = true;
                UE_LOG(LogTemp, Log, TEXT("[HakoPDU][Apply] pdu=pos counter=%llu recv=%.6f apply=%.6f delay_ms=%.3f delta=%.6f frame=%llu loc=(%.3f,%.3f,%.3f) rot=(%.3f,%.3f,%.3f)"),
                    static_cast<unsigned long long>(PosMeta.RecvCounter),
                    PosMeta.RecvUeTime,
                    ApplyUeTime,
                    DelayMs,
                    DeltaTime,
                    static_cast<unsigned long long>(GFrameCounter),
                    NewLocation.X,
                    NewLocation.Y,
                    NewLocation.Z,
                    NewRotation.Pitch,
                    NewRotation.Yaw,
                    NewRotation.Roll);
#endif
                TargetLocation = NewTransform.GetLocation();
                TargetRotation = NewTransform.GetRotation().Rotator();
                if (!bEnableTransformSmoothing)
                {
                    bHasTargetTransform = true;
                    ParentActor->SetActorTransform(NewTransform, false, nullptr, ETeleportType::TeleportPhysics);
                }
                else if (!bHasTargetTransform)
                {
                    bHasTargetTransform = true;
                    ParentActor->SetActorTransform(NewTransform, false, nullptr, ETeleportType::TeleportPhysics);
                }
                else
                {
                    bHasTargetTransform = true;
                }
            }
            else
            {
                ++ActorUpdateSkippedCount;
            }
        }
        else {
            ++ActorUpdateSkippedCount;
            UE_LOG(LogTemp, Error, TEXT("Invalid position: (%f, %f, %f)"), NewLocation.X, NewLocation.Y, NewLocation.Z);
        }
    }
    else {
        ++NoPduFrames;
    }
    RecordLatencySample(PosDelayMs, DeltaTime, bHasPosLatencySample);

    FPduBufferMeta MotorMeta;
    buffer.Empty();
    ReadWithMeta("motor", buffer, MotorMeta);
    HakoCpp_HakoHilActuatorControls motor = {};
    float MotorControls[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
    if (buffer.Num() > 0) {
        bMotorRead = true;
        hako::pdu::PduConvertor<HakoCpp_HakoHilActuatorControls, hako::pdu::msgs::hako_mavlink_msgs::HakoHilActuatorControls> conv;
        conv.pdu2cpp((char*)buffer.GetData(), motor);
        MotorControls[0] = motor.controls[0];
        MotorControls[1] = motor.controls[1];
        MotorControls[2] = motor.controls[2];
        MotorControls[3] = motor.controls[3];
        LatestMotorTimeUsec = motor.time_usec;
        LatestMotorMode = motor.mode;
        LatestMotorFlags = motor.flags;
        Motor0 = motor.controls[0];
        const double MotorApplyTime = FPlatformTime::Seconds();
        const double MotorDelayMs = MotorMeta.RecvUeTime > 0.0 ? (MotorApplyTime - MotorMeta.RecvUeTime) * 1000.0 : 0.0;
        LatestMotorCounter = MotorMeta.RecvCounter;
        LatestMotorDelayMs = MotorDelayMs;
        ++PropStatsReadOk;
        PropStatsDelaySumMs += MotorDelayMs;
        PropStatsDelayMaxMs = FMath::Max(PropStatsDelayMaxMs, MotorDelayMs);

        bool bAllZero = true;
        bool bSameAsLast = bHasLastMotorControls;
        for (int32 Index = 0; Index < 4; ++Index)
        {
            const float Value = MotorControls[Index];
            PropStatsMotorMin = FMath::Min(PropStatsMotorMin, Value);
            PropStatsMotorMax = FMath::Max(PropStatsMotorMax, Value);
            if (!FMath::IsFinite(Value))
            {
                ++PropStatsInvalidValueCount;
            }
            if (!FMath::IsNearlyZero(Value, 0.0001f))
            {
                bAllZero = false;
            }
            if (bHasLastMotorControls && !FMath::IsNearlyEqual(Value, LastMotorControls[Index], 0.0001f))
            {
                bSameAsLast = false;
            }
        }
        if (bAllZero)
        {
            ++PropStatsZeroValueCount;
        }
        if (bSameAsLast)
        {
            ++PropStatsSameValueCount;
        }

#if HAKO_PROP_STATS_LOG && HAKO_PROP_DETAILED_LOG
        UE_LOG(LogTemp, Log, TEXT("[HakoProp][Read] success=1 counter=%llu recv=%.6f apply=%.6f delay_ms=%.3f stale_ms=%.3f size=%d no_motor_frames=%llu skipped=%llu"),
            static_cast<unsigned long long>(MotorMeta.RecvCounter),
            MotorMeta.RecvUeTime,
            MotorApplyTime,
            MotorDelayMs,
            MotorDelayMs,
            MotorMeta.DataSize,
            static_cast<unsigned long long>(NoMotorFrames),
            static_cast<unsigned long long>(MotorApplySkippedCount));
        UE_LOG(LogTemp, Log, TEXT("[HakoProp][Value] raw=(%.5f,%.5f,%.5f,%.5f) delta=(%.5f,%.5f,%.5f,%.5f) time_usec=%llu mode=%u flags=%llu zero=%d same=%d"),
            MotorControls[0],
            MotorControls[1],
            MotorControls[2],
            MotorControls[3],
            bHasLastMotorControls ? MotorControls[0] - LastMotorControls[0] : 0.0f,
            bHasLastMotorControls ? MotorControls[1] - LastMotorControls[1] : 0.0f,
            bHasLastMotorControls ? MotorControls[2] - LastMotorControls[2] : 0.0f,
            bHasLastMotorControls ? MotorControls[3] - LastMotorControls[3] : 0.0f,
            static_cast<unsigned long long>(motor.time_usec),
            static_cast<unsigned int>(motor.mode),
            static_cast<unsigned long long>(motor.flags),
            bAllZero ? 1 : 0,
            bSameAsLast ? 1 : 0);
#endif

        if (Motor)
        {
            Motor->Rotate(
                MotorControls[0],
                MotorControls[1],
                MotorControls[2],
                MotorControls[3]
            );
            ++PropStatsApplyCount;
        }
        else
        {
            ++MotorApplySkippedCount;
            ++PropStatsSkippedCount;
        }
        is_motor_activated = motor.controls[0] > 0.001;

#if HAKO_PROP_STATS_LOG && HAKO_PROP_DETAILED_LOG
        if (Motor)
        {
            UE_LOG(LogTemp, Log, TEXT("[HakoProp][Apply] dt=%.6f p1=%.5f p2=%.5f p3=%.5f p4=%.5f comps=(%s,%s,%s,%s) visible=(%d,%d,%d,%d) active=(%d,%d,%d,%d) owner=%s"),
                DeltaTime,
                MotorControls[0],
                MotorControls[1],
                MotorControls[2],
                MotorControls[3],
                Motor->Propeller1 ? *Motor->Propeller1->GetName() : TEXT("None"),
                Motor->Propeller2 ? *Motor->Propeller2->GetName() : TEXT("None"),
                Motor->Propeller3 ? *Motor->Propeller3->GetName() : TEXT("None"),
                Motor->Propeller4 ? *Motor->Propeller4->GetName() : TEXT("None"),
                Motor->Propeller1 && Motor->Propeller1->IsVisible() ? 1 : 0,
                Motor->Propeller2 && Motor->Propeller2->IsVisible() ? 1 : 0,
                Motor->Propeller3 && Motor->Propeller3->IsVisible() ? 1 : 0,
                Motor->Propeller4 && Motor->Propeller4->IsVisible() ? 1 : 0,
                Motor->Propeller1 && Motor->Propeller1->IsActive() ? 1 : 0,
                Motor->Propeller2 && Motor->Propeller2->IsActive() ? 1 : 0,
                Motor->Propeller3 && Motor->Propeller3->IsActive() ? 1 : 0,
                Motor->Propeller4 && Motor->Propeller4->IsActive() ? 1 : 0,
                *GetName());
        }
#endif

        for (int32 Index = 0; Index < 4; ++Index)
        {
            LastMotorControls[Index] = MotorControls[Index];
        }
        bHasLastMotorControls = true;
    }
    else {
        ++NoMotorFrames;
        ++PropStatsReadFail;
        ++PropStatsNoMotorFrames;
        if (bUseLastMotorControlsWhenMissing && bHasLastMotorControls && Motor)
        {
            Motor->Rotate(
                LastMotorControls[0],
                LastMotorControls[1],
                LastMotorControls[2],
                LastMotorControls[3]
            );
            Motor0 = LastMotorControls[0];
            is_motor_activated = LastMotorControls[0] > 0.001f;
            ++PropStatsUsedLastControlsCount;
            ++PropStatsApplyCount;
        }
        else if (!Motor)
        {
            ++MotorApplySkippedCount;
            ++PropStatsSkippedCount;
        }
#if HAKO_PROP_STATS_LOG && HAKO_PROP_DETAILED_LOG
        UE_LOG(LogTemp, Log, TEXT("[HakoProp][Read] success=0 latest_counter=%llu latest_delay_ms=%.3f no_motor_frames=%llu skipped=%llu"),
            static_cast<unsigned long long>(LatestMotorCounter),
            LatestMotorDelayMs,
            static_cast<unsigned long long>(NoMotorFrames),
            static_cast<unsigned long long>(MotorApplySkippedCount));
#endif
    }
    buffer = Read("status");
    if (buffer.Num() > 0) {
        bStatusRead = true;
        hako::pdu::PduConvertor<HakoCpp_DroneStatus, hako::pdu::msgs::hako_msgs::DroneStatus> conv;
        conv.pdu2cpp((char*)buffer.GetData(), status);
        if (FlightMode && status.flight_mode == 0) {
            FlightMode->SetMode(EFlightMode::ATTI);
        }
        else if (FlightMode) {
            FlightMode->SetMode(EFlightMode::GPS);
        }
    }
    if (is_motor_activated && buffer.Num() > 0 && DroneState) {
        switch (status.internal_state)
        {
        case 0:
            DroneState->SetMode(EDroneMode::TAKEOFF);
            break;
        case 1:
            DroneState->SetMode(EDroneMode::HOVER);
            break;
        case 2:
            DroneState->SetMode(EDroneMode::LANDING);
            break;
        default:
            break;
        }
    }
    else if (is_motor_activated == false && DroneState) {
        DroneState->SetMode(EDroneMode::DISARM);
    }
#if HAKO_PROP_STATS_LOG
    PropStatsElapsed += FMath::Max(0.0f, DeltaTime);
    if (DeltaTime > KINDA_SMALL_NUMBER)
    {
        PropStatsFpsSum += 1.0 / static_cast<double>(DeltaTime);
    }
    ++PropStatsFpsSamples;
    if (PropStatsElapsed >= 1.0)
    {
        const double AvgDelayMs = PropStatsReadOk > 0 ? PropStatsDelaySumMs / static_cast<double>(PropStatsReadOk) : 0.0;
        const double FpsAvg = PropStatsFpsSamples > 0 ? PropStatsFpsSum / static_cast<double>(PropStatsFpsSamples) : 0.0;
        const float MotorMin = PropStatsMotorMin == TNumericLimits<float>::Max() ? 0.0f : PropStatsMotorMin;
        const float MotorMax = PropStatsMotorMax == -TNumericLimits<float>::Max() ? 0.0f : PropStatsMotorMax;
        float PropAngle1 = 0.0f;
        float PropAngle2 = 0.0f;
        float PropAngle3 = 0.0f;
        float PropAngle4 = 0.0f;
        float PropRpm1 = 0.0f;
        float PropRpm2 = 0.0f;
        float PropRpm3 = 0.0f;
        float PropRpm4 = 0.0f;
        float RawControl1 = 0.0f;
        float RawControl2 = 0.0f;
        float RawControl3 = 0.0f;
        float RawControl4 = 0.0f;
        float VisualControl1 = 0.0f;
        float VisualControl2 = 0.0f;
        float VisualControl3 = 0.0f;
        float VisualControl4 = 0.0f;
        float MaxRotationSpeed = 0.0f;
        FString PropellerNames = TEXT("None,None,None,None");
        if (Motor)
        {
            Motor->GetPropellerAngles(PropAngle1, PropAngle2, PropAngle3, PropAngle4);
            Motor->GetLastPropellerRpm(PropRpm1, PropRpm2, PropRpm3, PropRpm4);
            Motor->GetLastRawControls(RawControl1, RawControl2, RawControl3, RawControl4);
            Motor->GetLastVisualControls(VisualControl1, VisualControl2, VisualControl3, VisualControl4);
            MaxRotationSpeed = Motor->MaxRotationSpeed;
            PropellerNames = FString::Printf(TEXT("%s,%s,%s,%s"),
                Motor->Propeller1 ? *Motor->Propeller1->GetName() : TEXT("None"),
                Motor->Propeller2 ? *Motor->Propeller2->GetName() : TEXT("None"),
                Motor->Propeller3 ? *Motor->Propeller3->GetName() : TEXT("None"),
                Motor->Propeller4 ? *Motor->Propeller4->GetName() : TEXT("None"));
        }

        UE_LOG(LogTemp, Log, TEXT("[HakoProp][Stats] read_ok=%llu read_fail=%llu no_motor_frames=%llu used_last=%llu has_last=%d avg_delay=%.3f max_delay=%.3f motor_min=%.5f motor_max=%.5f zero=%llu same=%llu invalid=%llu apply=%llu skipped=%llu max_speed=%.1f raw=(%.5f,%.5f,%.5f,%.5f) visual=(%.5f,%.5f,%.5f,%.5f) last=(%.5f,%.5f,%.5f,%.5f) rpm=(%.3f,%.3f,%.3f,%.3f) angle=(%.3f,%.3f,%.3f,%.3f) fps=%.1f latest_counter=%llu latest_delay=%.3f time_usec=%llu mode=%u flags=%llu state=%d flight_mode=%d status_read=%d use_last_when_missing=%d prop_smoothing=%d accel=%.1f decel=%.1f stop=%.1f"),
            static_cast<unsigned long long>(PropStatsReadOk),
            static_cast<unsigned long long>(PropStatsReadFail),
            static_cast<unsigned long long>(PropStatsNoMotorFrames),
            static_cast<unsigned long long>(PropStatsUsedLastControlsCount),
            bHasLastMotorControls ? 1 : 0,
            AvgDelayMs,
            PropStatsDelayMaxMs,
            MotorMin,
            MotorMax,
            static_cast<unsigned long long>(PropStatsZeroValueCount),
            static_cast<unsigned long long>(PropStatsSameValueCount),
            static_cast<unsigned long long>(PropStatsInvalidValueCount),
            static_cast<unsigned long long>(PropStatsApplyCount),
            static_cast<unsigned long long>(PropStatsSkippedCount),
            MaxRotationSpeed,
            RawControl1,
            RawControl2,
            RawControl3,
            RawControl4,
            VisualControl1,
            VisualControl2,
            VisualControl3,
            VisualControl4,
            LastMotorControls[0],
            LastMotorControls[1],
            LastMotorControls[2],
            LastMotorControls[3],
            PropRpm1,
            PropRpm2,
            PropRpm3,
            PropRpm4,
            PropAngle1,
            PropAngle2,
            PropAngle3,
            PropAngle4,
            FpsAvg,
            static_cast<unsigned long long>(LatestMotorCounter),
            LatestMotorDelayMs,
            static_cast<unsigned long long>(LatestMotorTimeUsec),
            static_cast<unsigned int>(LatestMotorMode),
            static_cast<unsigned long long>(LatestMotorFlags),
            status.internal_state,
            status.flight_mode,
            bStatusRead ? 1 : 0,
            bUseLastMotorControlsWhenMissing ? 1 : 0,
            Motor && Motor->bEnablePropellerSmoothing ? 1 : 0,
            Motor ? Motor->PropellerAccelInterpSpeed : 0.0f,
            Motor ? Motor->PropellerDecelInterpSpeed : 0.0f,
            Motor ? Motor->PropellerStopInterpSpeed : 0.0f);

        if (!Motor)
        {
            UE_LOG(LogTemp, Warning, TEXT("[HakoProp][Warning] motor_component_invalid read_ok=%llu read_fail=%llu used_last=%llu has_last=%d"),
                static_cast<unsigned long long>(PropStatsReadOk),
                static_cast<unsigned long long>(PropStatsReadFail),
                static_cast<unsigned long long>(PropStatsUsedLastControlsCount),
                bHasLastMotorControls ? 1 : 0);
        }
        if (PropStatsApplyCount == 0 && (PropStatsReadOk > 0 || bHasLastMotorControls))
        {
            UE_LOG(LogTemp, Warning, TEXT("[HakoProp][Warning] apply_count_zero read_ok=%llu read_fail=%llu has_last=%d comps=(%s)"),
                static_cast<unsigned long long>(PropStatsReadOk),
                static_cast<unsigned long long>(PropStatsReadFail),
                bHasLastMotorControls ? 1 : 0,
                *PropellerNames);
        }
        if (PropStatsInvalidValueCount > 0)
        {
            UE_LOG(LogTemp, Warning, TEXT("[HakoProp][Warning] invalid_motor_value count=%llu motor=(%.5f,%.5f,%.5f,%.5f) state=%d flight_mode=%d"),
                static_cast<unsigned long long>(PropStatsInvalidValueCount),
                LastMotorControls[0],
                LastMotorControls[1],
                LastMotorControls[2],
                LastMotorControls[3],
                status.internal_state,
                status.flight_mode);
        }
        if (PropStatsFpsSamples > 0 && PropStatsReadFail >= PropStatsFpsSamples)
        {
            UE_LOG(LogTemp, Warning, TEXT("[HakoProp][Warning] motor_read_failed_for_full_stats_window read_fail=%llu frames=%llu used_last=%llu has_last=%d"),
                static_cast<unsigned long long>(PropStatsReadFail),
                static_cast<unsigned long long>(PropStatsFpsSamples),
                static_cast<unsigned long long>(PropStatsUsedLastControlsCount),
                bHasLastMotorControls ? 1 : 0);
        }
        if (bHasLastPropStatsAngles)
        {
            const float Angles[4] = { PropAngle1, PropAngle2, PropAngle3, PropAngle4 };
            const float Rpms[4] = { PropRpm1, PropRpm2, PropRpm3, PropRpm4 };
            for (int32 Index = 0; Index < 4; ++Index)
            {
                const float AngleDelta = FMath::Abs(FMath::FindDeltaAngleDegrees(LastPropStatsAngles[Index], Angles[Index]));
                if (FMath::Abs(Rpms[Index]) > 1.0f && AngleDelta < 0.01f)
                {
                    UE_LOG(LogTemp, Warning, TEXT("[HakoProp][Warning] rpm_nonzero_but_angle_stopped p%d rpm=%.3f angle_delta=%.5f angle=%.3f comps=(%s)"),
                        Index + 1,
                        Rpms[Index],
                        AngleDelta,
                        Angles[Index],
                        *PropellerNames);
                }
            }
        }
        LastPropStatsAngles[0] = PropAngle1;
        LastPropStatsAngles[1] = PropAngle2;
        LastPropStatsAngles[2] = PropAngle3;
        LastPropStatsAngles[3] = PropAngle4;
        bHasLastPropStatsAngles = true;

        PropStatsElapsed = 0.0;
        PropStatsReadOk = 0;
        PropStatsReadFail = 0;
        PropStatsNoMotorFrames = 0;
        PropStatsApplyCount = 0;
        PropStatsSkippedCount = 0;
        PropStatsZeroValueCount = 0;
        PropStatsSameValueCount = 0;
        PropStatsUsedLastControlsCount = 0;
        PropStatsInvalidValueCount = 0;
        PropStatsDelaySumMs = 0.0;
        PropStatsDelayMaxMs = 0.0;
        PropStatsFpsSum = 0.0;
        PropStatsFpsSamples = 0;
        PropStatsMotorMin = TNumericLimits<float>::Max();
        PropStatsMotorMax = -TNumericLimits<float>::Max();
    }
#endif
    if (bLogVisualPduHeartbeat)
    {
        VisualPduHeartbeatElapsed += GetWorld() ? GetWorld()->GetDeltaSeconds() : 0.0f;
        if (VisualPduHeartbeatElapsed >= VisualPduHeartbeatIntervalSec)
        {
            VisualPduHeartbeatElapsed = 0.0f;
            UE_LOG(LogTemp, Log, TEXT("[AvatarHeartbeat] Robot:%s PosRead:%d MotorRead:%d StatusRead:%d Motor0:%.4f State:%d FlightMode:%d PDU_Pos:(%.3f,%.3f,%.3f) Unreal_Pos:%s"),
                *DroneName,
                bPosRead ? 1 : 0,
                bMotorRead ? 1 : 0,
                bStatusRead ? 1 : 0,
                Motor0,
                status.internal_state,
                status.flight_mode,
                PduPosition.X,
                PduPosition.Y,
                PduPosition.Z,
                *GetActorLocation().ToString());
#if HAKO_PDU_LATENCY_LOG
            const float Fps = DeltaTime > KINDA_SMALL_NUMBER ? 1.0f / DeltaTime : 0.0f;
            UE_LOG(LogTemp, Log, TEXT("[HakoPDU][Tick] fps=%.1f delta=%.6f pos_read=%d latest_counter=%llu latest_delay_ms=%.3f skipped=%llu no_pdu_frames=%llu"),
                Fps,
                DeltaTime,
                bPosRead ? 1 : 0,
                static_cast<unsigned long long>(LatestPosCounter),
                LatestPosDelayMs,
                static_cast<unsigned long long>(ActorUpdateSkippedCount),
                static_cast<unsigned long long>(NoPduFrames));
#endif
        }
    }
    if (Collision) {
        FDroneImpulseCollision impulse = Collision->GetAndResetCollision();
        if (impulse.bCollision) {
            HakoCpp_ImpulseCollision pdu_impulse;
            int32 PduSize = pduManager->GetPduSize(DroneName, "impulse");
            if (PduSize <= 0) {
                UE_LOG(LogTemp, Error, TEXT("impulse: Failed to get PDU size"));
                return;
            }
            TArray<uint8> wBuffer;
            wBuffer.SetNum(PduSize);

            pdu_impulse.collision = true;
            pdu_impulse.is_target_static = impulse.bIsTargetStatic;
            pdu_impulse.restitution_coefficient = impulse.RestitutionCoefficient;

            pdu_impulse.normal = { impulse.Normal.X, impulse.Normal.Y, impulse.Normal.Z };
            pdu_impulse.self_contact_vector = { impulse.SelfContactVector.X, impulse.SelfContactVector.Y, impulse.SelfContactVector.Z };
            pdu_impulse.target_contact_vector = { impulse.TargetContactVector.X, impulse.TargetContactVector.Y, impulse.TargetContactVector.Z };

            pdu_impulse.target_velocity = { impulse.TargetVelocity.X, impulse.TargetVelocity.Y, impulse.TargetVelocity.Z };
            pdu_impulse.target_angular_velocity = { impulse.TargetAngularVelocity.X, impulse.TargetAngularVelocity.Y, impulse.TargetAngularVelocity.Z };
            pdu_impulse.target_euler = { impulse.TargetEuler.X, impulse.TargetEuler.Y, impulse.TargetEuler.Z };
            pdu_impulse.target_inertia = { impulse.TargetInertia.X, impulse.TargetInertia.Y, impulse.TargetInertia.Z };

            pdu_impulse.target_mass = impulse.TargetMass;

            hako::pdu::PduConvertor<HakoCpp_ImpulseCollision, hako::pdu::msgs::hako_msgs::ImpulseCollision> Conv;
            Conv.cpp2pdu(pdu_impulse, (char*)wBuffer.GetData(), wBuffer.Num());

            if (!pduManager->FlushPduRawData(DroneName, "impulse", wBuffer)) {
                UE_LOG(LogTemp, Error, TEXT("impulse: Failed to flush PDU"));
            }
            else {
                UE_LOG(LogTemp, Log, TEXT("impulse: PDU sent successfully. Size = %d"), wBuffer.Num());
            }
        }
    }
}

void AHakoniwaAvatar::RecordLatencySample(double DelayMs, float DeltaTime, bool bHasSample)
{
#if HAKO_PDU_LATENCY_LOG
    const double StatsIntervalSec = FMath::Max(0.1, static_cast<double>(CVarHakoPduLatencyStatsIntervalSec.GetValueOnGameThread()));
    const double MaxNormalDelayMs = FMath::Max(0.0, static_cast<double>(CVarHakoPduLatencyStatsMaxDelayMs.GetValueOnGameThread()));
    const bool bIsNormalSample = bHasSample && FMath::IsFinite(DelayMs) && DelayMs >= 0.0 && DelayMs <= MaxNormalDelayMs;

    LatencyStatsElapsed += FMath::Max(0.0f, DeltaTime);
    if (bIsNormalSample)
    {
        LatencyStatSamples.Add(DelayMs);
        LatencyStatsDelaySumMs += DelayMs;
    }
    else if (bHasSample)
    {
        ++LatencyStatsExcludedSamples;
    }

    if (DeltaTime > KINDA_SMALL_NUMBER)
    {
        LatencyStatsFpsSum += 1.0 / static_cast<double>(DeltaTime);
    }
    LatencyStatsDeltaSum += static_cast<double>(DeltaTime);
    LatencyStatsDeltaMax = FMath::Max(LatencyStatsDeltaMax, static_cast<double>(DeltaTime));
    ++LatencyStatsFrameSamples;

    if (LatencyStatsElapsed < StatsIntervalSec)
    {
        return;
    }

    const uint64 SkippedInWindow = ActorUpdateSkippedCount - LatencyStatsStartSkipped;
    const uint64 NoPduFramesInWindow = NoPduFrames - LatencyStatsStartNoPduFrames;
    const double FpsAvg = LatencyStatsFrameSamples > 0 ? LatencyStatsFpsSum / static_cast<double>(LatencyStatsFrameSamples) : 0.0;
    const double DeltaAvg = LatencyStatsFrameSamples > 0 ? LatencyStatsDeltaSum / static_cast<double>(LatencyStatsFrameSamples) : 0.0;

    if (LatencyStatSamples.Num() > 0)
    {
        LatencyStatSamples.Sort();
        const int32 SampleCount = LatencyStatSamples.Num();
        const auto Percentile = [this, SampleCount](double Ratio)
        {
            const int32 Index = FMath::Clamp(FMath::CeilToInt(Ratio * static_cast<double>(SampleCount)) - 1, 0, SampleCount - 1);
            return LatencyStatSamples[Index];
        };

        UE_LOG(LogTemp, Log, TEXT("[HakoPDU][LatencyStats] samples=%d min=%.3f avg=%.3f p50=%.3f p95=%.3f p99=%.3f max=%.3f fps_avg=%.1f delta_avg=%.6f delta_max=%.6f no_pdu_frames=%llu skipped=%llu excluded=%llu"),
            SampleCount,
            LatencyStatSamples[0],
            LatencyStatsDelaySumMs / static_cast<double>(SampleCount),
            Percentile(0.50),
            Percentile(0.95),
            Percentile(0.99),
            LatencyStatSamples.Last(),
            FpsAvg,
            DeltaAvg,
            LatencyStatsDeltaMax,
            static_cast<unsigned long long>(NoPduFramesInWindow),
            static_cast<unsigned long long>(SkippedInWindow),
            static_cast<unsigned long long>(LatencyStatsExcludedSamples));
    }
    else
    {
        UE_LOG(LogTemp, Log, TEXT("[HakoPDU][LatencyStats] samples=0 fps_avg=%.1f delta_avg=%.6f delta_max=%.6f no_pdu_frames=%llu skipped=%llu excluded=%llu"),
            FpsAvg,
            DeltaAvg,
            LatencyStatsDeltaMax,
            static_cast<unsigned long long>(NoPduFramesInWindow),
            static_cast<unsigned long long>(SkippedInWindow),
            static_cast<unsigned long long>(LatencyStatsExcludedSamples));
    }

    LatencyStatSamples.Empty();
    LatencyStatsElapsed = 0.0;
    LatencyStatsDelaySumMs = 0.0;
    LatencyStatsFpsSum = 0.0;
    LatencyStatsDeltaSum = 0.0;
    LatencyStatsDeltaMax = 0.0;
    LatencyStatsFrameSamples = 0;
    LatencyStatsExcludedSamples = 0;
    LatencyStatsStartSkipped = ActorUpdateSkippedCount;
    LatencyStatsStartNoPduFrames = NoPduFrames;
#endif
}

void AHakoniwaAvatar::ApplySmoothedTransform(float DeltaTime)
{
    if (!bHasTargetTransform)
    {
        return;
    }

    AActor* TargetActor = this;
    if (!IsValid(TargetActor))
    {
        ++ActorUpdateSkippedCount;
        return;
    }

    const FVector CurrentLocation = TargetActor->GetActorLocation();
    const FRotator CurrentRotation = TargetActor->GetActorRotation();
    const float Distance = FVector::Dist(CurrentLocation, TargetLocation);

#if HAKO_PDU_LATENCY_LOG
    SmoothingLogElapsed += FMath::Max(0.0f, DeltaTime);
    if (SmoothingLogElapsed >= 1.0f)
    {
        SmoothingLogElapsed = 0.0f;
        UE_LOG(LogTemp, Log, TEXT("[HakoPDU][Smoothing] enabled=%d loc_speed=%.1f rot_speed=%.1f snap_dist=%.1f dist=%.3f target=(%.3f,%.3f,%.3f) current=(%.3f,%.3f,%.3f)"),
            bEnableTransformSmoothing ? 1 : 0,
            LocationInterpSpeed,
            RotationInterpSpeed,
            SnapDistanceThreshold,
            Distance,
            TargetLocation.X,
            TargetLocation.Y,
            TargetLocation.Z,
            CurrentLocation.X,
            CurrentLocation.Y,
            CurrentLocation.Z);
    }
#endif

    if (!bEnableTransformSmoothing)
    {
        return;
    }

    if (Distance > SnapDistanceThreshold)
    {
        TargetActor->SetActorLocationAndRotation(
            TargetLocation,
            TargetRotation,
            false,
            nullptr,
            ETeleportType::TeleportPhysics);
        return;
    }

    const FVector SmoothedLocation = FMath::VInterpTo(
        CurrentLocation,
        TargetLocation,
        DeltaTime,
        LocationInterpSpeed);

    const FRotator SmoothedRotation = FMath::RInterpTo(
        CurrentRotation,
        TargetRotation,
        DeltaTime,
        RotationInterpSpeed);

    TargetActor->SetActorLocationAndRotation(
        SmoothedLocation,
        SmoothedRotation,
        false,
        nullptr,
        ETeleportType::None);
}

void AHakoniwaAvatar::LogPduChannelCheck()
{
    if (pduManager == nullptr || bPduChannelCheckLogged)
    {
        return;
    }

    bPduChannelCheckLogged = true;
    UE_LOG(LogTemp, Log, TEXT("--- Registered PDU Channel Check for %s ---"), *DroneName);
    const FString Names[] = {
        TEXT("pos"),
        TEXT("motor"),
        TEXT("status"),
        TEXT("impulse"),
        TEXT("hako_cmd_game"),
        TEXT("Drone_status")
    };
    for (const FString& Name : Names)
    {
        const int32 ChannelId = pduManager->GetPduChannelId(DroneName, Name);
        const int32 PduSize = pduManager->GetPduSize(DroneName, Name);
        UE_LOG(LogTemp, Log, TEXT("PDU Name '%s': Channel ID = %d Size = %d"), *Name, ChannelId, PduSize);
    }
    UE_LOG(LogTemp, Log, TEXT("---------------------------------------"));
}

TArray<uint8> AHakoniwaAvatar::Read(const FString& PduName)
{
    TArray<uint8> buffer;

    int32 pdu_size = pduManager->GetPduSize(DroneName, PduName);
    if (pdu_size <= 0) {
        UE_LOG(LogTemp, Error, TEXT("Can not get pdu size..."));
        return buffer;
    }
    //UE_LOG(LogTemp, Log, TEXT("Read PDU TEST: robot=%s, pdu=%s, size=%d"),*DroneName, *PduName, pdu_size);
    buffer.SetNum(pdu_size);

    if (pduManager->ReadPduRawData(DroneName, PduName, buffer)) {
        //UE_LOG(LogTemp, Log, TEXT("Read PDU succeeded: robot=%s, pdu=%s, size=%d"), *DroneName, *PduName, pdu_size);
        return buffer;
    }
    else {
        //UE_LOG(LogTemp, Error, TEXT("Failed to read PDU: robot=%s, pdu=%s"), *DroneName, *PduName);
        return TArray<uint8>();
    }
}

bool AHakoniwaAvatar::ReadWithMeta(const FString& PduName, TArray<uint8>& OutBuffer, FPduBufferMeta& OutMeta)
{
    OutBuffer.Empty();
    OutMeta = FPduBufferMeta();

    int32 pdu_size = pduManager->GetPduSize(DroneName, PduName);
    if (pdu_size <= 0) {
        UE_LOG(LogTemp, Error, TEXT("Can not get pdu size..."));
        return false;
    }
    OutBuffer.SetNum(pdu_size);

    if (pduManager->ReadPduRawDataWithMeta(DroneName, PduName, OutBuffer, OutMeta)) {
        return true;
    }

    OutBuffer.Empty();
    return false;
}
// Called when the game starts or when spawned
void AHakoniwaAvatar::BeginPlay()
{
    Super::BeginPlay();
    InitialTransform = GetActorTransform();
#if HAKO_PDU_LATENCY_LOG
    UE_LOG(LogTemp, Log, TEXT("[HakoPDU][SmoothingConfig] enabled=%d loc_speed=%.1f rot_speed=%.1f snap_dist=%.1f"),
        bEnableTransformSmoothing ? 1 : 0,
        LocationInterpSpeed,
        RotationInterpSpeed,
        SnapDistanceThreshold);
#endif
    if (!Motor)
    {
        Motor = FindComponentByClass<UDronePropellerComponent>();
        if (!Motor)
        {
            UE_LOG(LogTemp, Error, TEXT("DronePropellerComponent not found!"));
        }
    }
    if (Motor)
    {
        UE_LOG(LogTemp, Log, TEXT("[HakoProp][Config] max_rotation_speed=%.1f use_last_when_missing=%d prop_smoothing=%d accel=%.1f decel=%.1f stop=%.1f"),
            Motor->MaxRotationSpeed,
            bUseLastMotorControlsWhenMissing ? 1 : 0,
            Motor->bEnablePropellerSmoothing ? 1 : 0,
            Motor->PropellerAccelInterpSpeed,
            Motor->PropellerDecelInterpSpeed,
            Motor->PropellerStopInterpSpeed);
    }
    if (!Collision)
    {
        Collision = FindComponentByClass<UDroneCollisionComponent>();
        if (!Collision)
        {
            UE_LOG(LogTemp, Error, TEXT("UDroneCollisionComponent not found!"));
        }
    }
    if (!DroneState)
    {
        DroneState = FindComponentByClass<UDroneLedComponent>();
        if (!DroneState)
        {
            UE_LOG(LogTemp, Error, TEXT("UDroneLedComponent not found!"));
        }
    }
    if (!FlightMode)
    {
        FlightMode = FindComponentByClass<UFlightModeLedComponent>();
        if (!FlightMode)
        {
            UE_LOG(LogTemp, Error, TEXT("UFlightModeLedComponent not found!"));
        }
    }


    if (!PiPCapture)
    {
        PiPCapture = FindComponentByClass<USceneCaptureComponent2D>();
        if (!PiPCapture)
        {
            UE_LOG(LogTemp, Error, TEXT("[%s] SceneCaptureComponent2D not found!"),
                *GetName());
        }
    }

    if (!RT_PiP)
    {
        RT_PiP = UKismetRenderingLibrary::CreateRenderTarget2D(
            this,            // Outer
            1280, 720,
            RTF_RGBA8
        );
        if (!RT_PiP)
        {
            UE_LOG(LogTemp, Error, TEXT("[%s] Failed to create RenderTarget"), *GetName());
        }
    }

    if (PiPCapture && RT_PiP)
    {
        PiPCapture->TextureTarget = RT_PiP;
        PiPCapture->CaptureSource = ESceneCaptureSource::SCS_FinalColorLDR;
        PiPCapture->FOVAngle = 80.f;
        PiPCapture->bCaptureEveryFrame = true;    
        PiPCapture->bCaptureOnMovement = true;

        // for light processing
        // PiPCapture->ShowFlags.MotionBlur = 0;
        // PiPCapture->ShowFlags.AmbientOcclusion = 0;
        // PiPCapture->ShowFlags.Bloom = 0;
        // PiPCapture->ShowFlags.DynamicShadows = 0;

        UE_LOG(LogTemp, Log, TEXT("[%s] PiP ready: %dx%d on %s"),
            *GetName(),
            RT_PiP->SizeX, RT_PiP->SizeY,
            *PiPCapture->GetName());
    }
}

void AHakoniwaAvatar::OnHakoInitialize_Implementation()
{
    UE_LOG(LogTemp, Log, TEXT("AHakoniwaAvatar::OnHakoInitialize_Implementation called"));
    DeclarePdu();
}

void AHakoniwaAvatar::OnHakoReset_Implementation()
{
    UE_LOG(LogTemp, Log, TEXT("AHakoniwaAvatar::OnHakoReset_Implementation called"));
    bHasTargetTransform = false;
    TargetLocation = InitialTransform.GetLocation();
    TargetRotation = InitialTransform.GetRotation().Rotator();
    SetActorTransform(InitialTransform, false, nullptr, ETeleportType::TeleportPhysics);
}

// Called every frame
void AHakoniwaAvatar::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

    if (!bReadVisualStateFromPdu) {
        return;
    }

    if (isDeclared) {
        DoTask(DeltaTime);
    }

    ApplySmoothedTransform(DeltaTime);
}
