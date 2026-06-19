#include "DronePropellerComponent.h"
//#include "Components/AudioComponent.h"
#include "Kismet/GameplayStatics.h"

UDronePropellerComponent::UDronePropellerComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
}

void UDronePropellerComponent::BeginPlay()
{
    Super::BeginPlay();
#if 0
    if (bEnableAudio && AudioCue)
    {
        AudioComponent = NewObject<UAudioComponent>(this);
        AudioComponent->bAutoActivate = false;
        AudioComponent->SetSound(AudioCue);
        AudioComponent->RegisterComponentWithWorld(GetWorld());
        AudioComponent->AttachToComponent(this, FAttachmentTransformRules::KeepRelativeTransform);
    }
#endif
    TArray<USceneComponent*> Components;
    GetOwner()->GetComponents<USceneComponent>(Components);

    UE_LOG(LogTemp, Log, TEXT("Drone Propeller Component: BeginPlay()"));
    UE_LOG(LogTemp, Log, TEXT("[HakoProp][Config] component=%s max_rotation_speed=%.1f tick_enabled=%d smoothing=%d accel=%.1f decel=%.1f stop=%.1f"),
        *GetName(),
        MaxRotationSpeed,
        PrimaryComponentTick.IsTickFunctionEnabled() ? 1 : 0,
        bEnablePropellerSmoothing ? 1 : 0,
        PropellerAccelInterpSpeed,
        PropellerDecelInterpSpeed,
        PropellerStopInterpSpeed);
    if (FMath::IsNearlyEqual(MaxRotationSpeed, 1.0f))
    {
        UE_LOG(LogTemp, Warning, TEXT("[HakoProp][Warning] max_rotation_speed_is_default value=%.1f recommended=20000..36000 component=%s"),
            MaxRotationSpeed,
            *GetName());
    }
    for (USceneComponent* Comp : Components)
    {
        FString Name = Comp->GetName();
        UE_LOG(LogTemp, Log, TEXT("Found Component: %s"), *Name);  // �� ���O�o��

        if (Name == TEXT("Propeller1")) { Propeller1 = Comp; UE_LOG(LogTemp, Log, TEXT("Propeller1 matched")); }
        else if (Name == TEXT("Propeller2")) { Propeller2 = Comp; UE_LOG(LogTemp, Log, TEXT("Propeller2 matched")); }
        else if (Name == TEXT("Propeller3")) { Propeller3 = Comp; UE_LOG(LogTemp, Log, TEXT("Propeller3 matched")); }
        else if (Name == TEXT("Propeller4")) { Propeller4 = Comp; UE_LOG(LogTemp, Log, TEXT("Propeller4 matched")); }
        else if (Name == TEXT("Propeller5")) { Propeller5 = Comp; UE_LOG(LogTemp, Log, TEXT("Propeller5 matched")); }
        else if (Name == TEXT("Propeller6")) { Propeller6 = Comp; UE_LOG(LogTemp, Log, TEXT("Propeller6 matched")); }
    }

}

void UDronePropellerComponent::RotatePropeller(USceneComponent* Propeller, float DutyRate, int32 PropellerIndex)
{
    if (!Propeller) return;

    float RotationSpeed = MaxRotationSpeed * DutyRate;
    if (PropellerIndex >= 0 && PropellerIndex < 4)
    {
        LastPropellerRpm[PropellerIndex] = (RotationSpeed / 360.0f) * 60.0f;
    }
    //UE_LOG(LogTemp, Log, TEXT("Rotating %s: DutyRate = %f, Speed = %f"), *Propeller->GetName(), DutyRate, RotationSpeed);
    FRotator RotationDelta(0.f, RotationSpeed * GetWorld()->GetDeltaSeconds(), 0.f);

    Propeller->AddLocalRotation(RotationDelta);
}

void UDronePropellerComponent::PlayAudio(float ControlValue)
{
#if 0
    if (!AudioComponent ||  !TargetCamera ) return;

    float Distance = FVector::Dist(TargetCamera->GetActorLocation(), GetOwner()->GetActorLocation());
    float Volume = 1.0f - FMath::Clamp((Distance - MinDistance) / (MaxDistance - MinDistance), 0.0f, 1.0f);

    if (!AudioComponent->IsPlaying() && ControlValue > 0)
    {
        AudioComponent->Play();
    }
    else if (AudioComponent->IsPlaying() && ControlValue == 0)
    {
        AudioComponent->Stop();
    }

    if (AudioComponent->IsPlaying())
    {
        AudioComponent->SetVolumeMultiplier(Volume);
    }
#endif
}

float UDronePropellerComponent::SmoothControl(float Current, float Target, float DeltaTime) const
{
    const float AbsCurrent = FMath::Abs(Current);
    const float AbsTarget = FMath::Abs(Target);
    const bool bStopping = FMath::IsNearlyZero(AbsTarget, 0.0001f);
    const float InterpSpeed = bStopping
        ? PropellerStopInterpSpeed
        : (AbsTarget > AbsCurrent ? PropellerAccelInterpSpeed : PropellerDecelInterpSpeed);

    if (InterpSpeed <= 0.0f || DeltaTime <= 0.0f)
    {
        return Target;
    }

    const float Smoothed = FMath::FInterpTo(Current, Target, DeltaTime, InterpSpeed);
    if (bStopping && FMath::IsNearlyZero(Smoothed, 0.0001f))
    {
        return 0.0f;
    }
    return Smoothed;
}

void UDronePropellerComponent::Rotate(float c1, float c2, float c3, float c4)
{
    //UE_LOG(LogTemp, Warning, TEXT("Rotate called: c1=%.2f, c2=%.2f, c3=%.2f, c4=%.2f"), c1, c2, c3, c4);
    const float RawControls[4] = { c1, c2, c3, c4 };
    float AppliedControls[4] = { c1, c2, c3, c4 };
    const float DeltaTime = GetWorld() ? GetWorld()->GetDeltaSeconds() : 0.0f;

    for (int32 Index = 0; Index < 4; ++Index)
    {
        LastRawControls[Index] = RawControls[Index];
    }

    if (bEnablePropellerSmoothing)
    {
        if (!bHasVisualControls)
        {
            for (int32 Index = 0; Index < 4; ++Index)
            {
                LastVisualControls[Index] = RawControls[Index];
            }
            bHasVisualControls = true;
        }
        else
        {
            for (int32 Index = 0; Index < 4; ++Index)
            {
                LastVisualControls[Index] = SmoothControl(LastVisualControls[Index], RawControls[Index], DeltaTime);
            }
        }

        for (int32 Index = 0; Index < 4; ++Index)
        {
            AppliedControls[Index] = LastVisualControls[Index];
        }
    }
    else
    {
        for (int32 Index = 0; Index < 4; ++Index)
        {
            LastVisualControls[Index] = RawControls[Index];
        }
        bHasVisualControls = true;
    }

    RotatePropeller(Propeller1, AppliedControls[0], 0);
    RotatePropeller(Propeller2, -AppliedControls[1], 1);
    if (Propeller3) RotatePropeller(Propeller3, AppliedControls[2], 2);
    if (Propeller4) RotatePropeller(Propeller4, -AppliedControls[3], 3);
    if (Propeller5) RotatePropeller(Propeller5, AppliedControls[0], INDEX_NONE);
    if (Propeller6) RotatePropeller(Propeller6, AppliedControls[1], INDEX_NONE);

    if (bEnableAudio)
    {
        PlayAudio(AppliedControls[0]);  // ��\�l�Ƃ���c1
    }
}
void UDronePropellerComponent::GetPropellerAngles(float& OutP1, float& OutP2, float& OutP3, float& OutP4) const
{
    OutP1 = Propeller1 ? Propeller1->GetRelativeRotation().Yaw : 0.0f;
    OutP2 = Propeller2 ? Propeller2->GetRelativeRotation().Yaw : 0.0f;
    OutP3 = Propeller3 ? Propeller3->GetRelativeRotation().Yaw : 0.0f;
    OutP4 = Propeller4 ? Propeller4->GetRelativeRotation().Yaw : 0.0f;
}

void UDronePropellerComponent::GetLastPropellerRpm(float& OutP1, float& OutP2, float& OutP3, float& OutP4) const
{
    OutP1 = LastPropellerRpm[0];
    OutP2 = LastPropellerRpm[1];
    OutP3 = LastPropellerRpm[2];
    OutP4 = LastPropellerRpm[3];
}

void UDronePropellerComponent::GetLastRawControls(float& OutP1, float& OutP2, float& OutP3, float& OutP4) const
{
    OutP1 = LastRawControls[0];
    OutP2 = LastRawControls[1];
    OutP3 = LastRawControls[2];
    OutP4 = LastRawControls[3];
}

void UDronePropellerComponent::GetLastVisualControls(float& OutP1, float& OutP2, float& OutP3, float& OutP4) const
{
    OutP1 = LastVisualControls[0];
    OutP2 = LastVisualControls[1];
    OutP3 = LastVisualControls[2];
    OutP4 = LastVisualControls[3];
}
