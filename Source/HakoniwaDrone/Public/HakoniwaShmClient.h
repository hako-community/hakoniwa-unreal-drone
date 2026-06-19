#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PduManager.h"
#include "ShmCommunicationService.h"
#include "HakoniwaClientInterface.h"
#include "HakoniwaShmClient.generated.h"

UCLASS(Blueprintable, BlueprintType)
class HAKONIWADRONE_API AHakoniwaShmClient : public AActor, public IHakoniwaClientInterface
{
    GENERATED_BODY()

public:
    AHakoniwaShmClient();

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hakoniwa")
    FString ConfigPath = "Config/webavatar.json";

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hakoniwa")
    FString AssetName = "UnrealAsset";

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hakoniwa")
    bool bAutoInitialize = false;

    virtual void Start_Implementation() override;
    virtual void Stop_Implementation() override;
    virtual void Reset_Implementation() override;

    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void Tick(float DeltaTime) override;

    UFUNCTION(BlueprintCallable, Category = "Hakoniwa")
    bool InitializeClient();

    virtual UPduManager* GetPduManager_Implementation() const override { return pduManager; }
    virtual int32 GetSimulationState_Implementation() const override;


private:
    UPROPERTY()
    UShmCommunicationService* service = nullptr;
    UPROPERTY()
    UPduManager* pduManager = nullptr;

    long long asset_time_usec = 0;
    long long delta_time_usec = 1000; // 1ms default

    void PreDeclareAllPDUs();
};
