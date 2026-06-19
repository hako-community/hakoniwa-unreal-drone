// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DronePropellerComponent.h"
#include "DroneLedComponent.h"
#include "FlightModeLedComponent.h"
#include "DroneCollisionComponent.h"
#include "PduManager.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Kismet/KismetRenderingLibrary.h"
#include "Components/SceneCaptureComponent2D.h"
#include "HakoniwaObjectInterface.h"

#include "HakoniwaAvatar.generated.h"

UCLASS(Blueprintable, BlueprintType)
class HAKONIWADRONE_API AHakoniwaAvatar : public AActor, public IHakoniwaObjectInterface
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	AHakoniwaAvatar();
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hakoniwa")
	bool bUseConfiguredRobotName = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hakoniwa")
	FString DroneName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hakoniwa")
	FString FallbackDroneName = "Drone";

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hakoniwa|Runtime")
	bool bReadVisualStateFromPdu = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hakoniwa|Debug")
	bool bLogVisualPduHeartbeat = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hakoniwa|Debug", meta = (ClampMin = "0.1"))
	float VisualPduHeartbeatIntervalSec = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hakoniwa|Smoothing")
	bool bEnableTransformSmoothing = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hakoniwa|Smoothing", meta = (ClampMin = "0.0"))
	float LocationInterpSpeed = 30.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hakoniwa|Smoothing", meta = (ClampMin = "0.0"))
	float RotationInterpSpeed = 30.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hakoniwa|Smoothing", meta = (ClampMin = "0.0"))
	float SnapDistanceThreshold = 5000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hakoniwa|Propeller")
	bool bUseLastMotorControlsWhenMissing = true;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	USceneCaptureComponent2D* PiPCapture = nullptr;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	UTextureRenderTarget2D* RT_PiP = nullptr;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Camera")
	UTextureRenderTarget2D* GetPiPRenderTarget() const { return RT_PiP; }

    // IHakoniwaObjectInterface
    virtual void OnHakoInitialize_Implementation() override;
    virtual void OnHakoReset_Implementation() override;

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;
	UPduManager* GetPduManager() { return pduManager; }
private:
	bool isDeclared = false;
	FTransform InitialTransform;
	UPROPERTY()
	UPduManager* pduManager = nullptr;
	UPROPERTY()
	UDronePropellerComponent* Motor = nullptr;
	UPROPERTY()
	UDroneLedComponent* DroneState = nullptr;
	UPROPERTY()
	UFlightModeLedComponent* FlightMode = nullptr;
	UPROPERTY()
	UDroneCollisionComponent* Collision = nullptr;
	void ResolveDroneName();
	void DeclarePdu();
	void DoTask(float DeltaTime);
	void LogPduChannelCheck();
	TArray<uint8> Read(const FString& PduName);
	bool ReadWithMeta(const FString& PduName, TArray<uint8>& OutBuffer, FPduBufferMeta& OutMeta);
	void RecordLatencySample(double DelayMs, float DeltaTime, bool bHasSample);
	void ApplySmoothedTransform(float DeltaTime);
	bool is_motor_activated = false;
	bool bPduChannelCheckLogged = false;
	float VisualPduHeartbeatElapsed = 0.0f;
	float SmoothingLogElapsed = 0.0f;
	FVector TargetLocation = FVector::ZeroVector;
	FRotator TargetRotation = FRotator::ZeroRotator;
	bool bHasTargetTransform = false;
	uint64 LatestPosCounter = 0;
	double LatestPosDelayMs = 0.0;
	uint64 NoPduFrames = 0;
	uint64 ActorUpdateSkippedCount = 0;
	uint64 LatestMotorCounter = 0;
	double LatestMotorDelayMs = 0.0;
	uint64 LatestMotorTimeUsec = 0;
	uint8 LatestMotorMode = 0;
	uint64 LatestMotorFlags = 0;
	uint64 NoMotorFrames = 0;
	uint64 MotorApplySkippedCount = 0;
	uint64 PropStatsReadOk = 0;
	uint64 PropStatsReadFail = 0;
	uint64 PropStatsNoMotorFrames = 0;
	uint64 PropStatsApplyCount = 0;
	uint64 PropStatsSkippedCount = 0;
	uint64 PropStatsZeroValueCount = 0;
	uint64 PropStatsSameValueCount = 0;
	uint64 PropStatsUsedLastControlsCount = 0;
	uint64 PropStatsInvalidValueCount = 0;
	double PropStatsElapsed = 0.0;
	double PropStatsDelaySumMs = 0.0;
	double PropStatsDelayMaxMs = 0.0;
	double PropStatsFpsSum = 0.0;
	uint64 PropStatsFpsSamples = 0;
	float PropStatsMotorMin = TNumericLimits<float>::Max();
	float PropStatsMotorMax = -TNumericLimits<float>::Max();
	float LastMotorControls[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
	bool bHasLastMotorControls = false;
	float LastPropStatsAngles[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
	bool bHasLastPropStatsAngles = false;
	TArray<double> LatencyStatSamples;
	double LatencyStatsElapsed = 0.0;
	double LatencyStatsDelaySumMs = 0.0;
	double LatencyStatsFpsSum = 0.0;
	double LatencyStatsDeltaSum = 0.0;
	double LatencyStatsDeltaMax = 0.0;
	uint64 LatencyStatsFrameSamples = 0;
	uint64 LatencyStatsExcludedSamples = 0;
	uint64 LatencyStatsStartSkipped = 0;
	uint64 LatencyStatsStartNoPduFrames = 0;

};
