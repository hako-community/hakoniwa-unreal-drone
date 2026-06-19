// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Sound/SoundCue.h"
#include "DronePropellerComponent.generated.h"


UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class HAKONIWADRONE_API UDronePropellerComponent : public UActorComponent
{
	GENERATED_BODY()

public:	
	// Sets default values for this component's properties
	UDronePropellerComponent();

protected:
	// Called when the game starts
	virtual void BeginPlay() override;

public:	
    UFUNCTION(BlueprintCallable, Category = "Drone")
    void Rotate(float c1, float c2, float c3, float c4);

    void GetPropellerAngles(float& OutP1, float& OutP2, float& OutP3, float& OutP4) const;
    void GetLastPropellerRpm(float& OutP1, float& OutP2, float& OutP3, float& OutP4) const;
    void GetLastRawControls(float& OutP1, float& OutP2, float& OutP3, float& OutP4) const;
    void GetLastVisualControls(float& OutP1, float& OutP2, float& OutP3, float& OutP4) const;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Propellers", meta = (ExposeOnSpawn = true))
    USceneComponent* Propeller1;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Propellers", meta = (ExposeOnSpawn = true))
    USceneComponent* Propeller2;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Propellers", meta = (ExposeOnSpawn = true))
    USceneComponent* Propeller3;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Propellers", meta = (ExposeOnSpawn = true))
    USceneComponent* Propeller4;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Propellers", meta = (ExposeOnSpawn = true))
    USceneComponent* Propeller5;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Propellers", meta = (ExposeOnSpawn = true))
    USceneComponent* Propeller6;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio")
    USoundCue* AudioCue;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio")
    bool bEnableAudio = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio")
    float MaxRotationSpeed = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio")
    float MaxDistance = 500.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio")
    float MinDistance = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Propellers|Smoothing")
    bool bEnablePropellerSmoothing = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Propellers|Smoothing", meta = (ClampMin = "0.0"))
    float PropellerAccelInterpSpeed = 40.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Propellers|Smoothing", meta = (ClampMin = "0.0"))
    float PropellerDecelInterpSpeed = 12.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Propellers|Smoothing", meta = (ClampMin = "0.0"))
    float PropellerStopInterpSpeed = 12.0f;

    //UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio")
    //ACameraActor* TargetCamera;

private:
    //UAudioComponent* AudioComponent;
    float LastPropellerRpm[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
    float LastRawControls[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
    float LastVisualControls[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
    bool bHasVisualControls = false;

    void RotatePropeller(USceneComponent* Propeller, float DutyRate, int32 PropellerIndex);
    float SmoothControl(float Current, float Target, float DeltaTime) const;
    void PlayAudio(float ControlValue);
		
};
