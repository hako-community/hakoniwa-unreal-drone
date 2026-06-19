// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "PduManager.h"
#include "WebSocketCommunicationService.h"
#include "HakoniwaClientInterface.h"
#include "HakoniwaWebClient.generated.h"

/**
 * 
 */
UCLASS(Blueprintable, BlueprintType)
class HAKONIWADRONE_API AHakoniwaWebClient : public AActor, public IHakoniwaClientInterface
{
	GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hakoniwa")
    FString ConfigPath = "Config/webavatar.json";

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hakoniwa")
    FString WebSocketUrl = "ws://127.0.0.1:8765";

    virtual void Start_Implementation() override;
    virtual void Stop_Implementation() override;
    virtual void Reset_Implementation() override;

    virtual UPduManager* GetPduManager_Implementation() const override { return pduManager; }
    virtual int32 GetSimulationState_Implementation() const override { return 0; }

private:
    UPROPERTY()
    UWebSocketCommunicationService* service = nullptr;
    UPROPERTY()
    UPduManager* pduManager = nullptr;
};
