#include "HakoniwaShmClient.h"
#include "Modules/ModuleManager.h"
#include "hako_capi.h"
#include "HakoniwaObjectInterface.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/Paths.h"

AHakoniwaShmClient::AHakoniwaShmClient()
{
    PrimaryActorTick.bCanEverTick = true;
}

void AHakoniwaShmClient::BeginPlay()
{
    Super::BeginPlay();

    UE_LOG(LogTemp, Log, TEXT("AHakoniwaShmClient BeginPlay()"));

    const FName ModuleName = "HakoniwaPdu";
    if (!FModuleManager::Get().IsModuleLoaded(ModuleName))
    {
        FModuleManager::Get().LoadModule(ModuleName);
    }

    if (pduManager) return;

    service = NewObject<UShmCommunicationService>(this);
    pduManager = NewObject<UPduManager>(this);

    if (bAutoInitialize)
    {
        InitializeClient();
    }
}

void AHakoniwaShmClient::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (pduManager && pduManager->IsServiceEnabled())
    {
        pduManager->StopService();
    }
    Super::EndPlay(EndPlayReason);
}

bool AHakoniwaShmClient::InitializeClient()
{
    if (service && service->IsServiceEnabled())
    {
        UE_LOG(LogTemp, Warning, TEXT("AHakoniwaShmClient::InitializeClient - Already initialized. Skipping."));
        return true;
    }

    UE_LOG(LogTemp, Log, TEXT("AHakoniwaShmClient InitializeClient() started"));

    // 1. Prepare absolute path to cpp_core_config.json
    FString FullConfigPath = FPaths::ProjectContentDir() / ConfigPath;
    if (!FPaths::FileExists(FullConfigPath))
    {
        UE_LOG(LogTemp, Error, TEXT("ConfigPath not found: %s"), *FullConfigPath);
        return false;
    }
    FString AbsoluteConfigPath = FPaths::ConvertRelativePathToFull(FullConfigPath);
    // Normalize path for the platform
    FPaths::NormalizeFilename(AbsoluteConfigPath);
    AbsoluteConfigPath = FPaths::CreateStandardFilename(AbsoluteConfigPath);

    UE_LOG(LogTemp, Log, TEXT("Using Config Path: %s"), *AbsoluteConfigPath);

    // 2. Initialize Hako Asset. Current shakoc.dll reads this path from HAKO_CONFIG_PATH.
    FPlatformMisc::SetEnvironmentVar(TEXT("HAKO_CONFIG_PATH"), *AbsoluteConfigPath);
    UE_LOG(LogTemp, Log, TEXT("Calling hako_asset_init with HAKO_CONFIG_PATH=%s"), *AbsoluteConfigPath);
    if (!hako_asset_init())
    {
        UE_LOG(LogTemp, Error, TEXT("hako_asset_init FAILED. Check HAKO_CONFIG_PATH and core mmap files."));
        return false;
    }

    if (!pduManager)
    {
        UE_LOG(LogTemp, Error, TEXT("pduManager is null."));
        return false;
    }

    // 3. Initialize UPduManager (This loads the PDU config from JSON)
    pduManager->Initialize(ConfigPath, service);

    // 4. Start Service (This will do registration)
    UE_LOG(LogTemp, Log, TEXT("Calling pduManager->StartService for asset: %s"), *AssetName);
    if (!pduManager->StartService(AssetName))
    {
        UE_LOG(LogTemp, Error, TEXT("pduManager->StartService failed."));
        return false;
    }

    // 5. Pre-declare PDUs by notifying all Hakoniwa objects
    UE_LOG(LogTemp, Log, TEXT("Notifying all Hakoniwa objects for PDU declaration..."));
    PreDeclareAllPDUs();

    UE_LOG(LogTemp, Log, TEXT("AHakoniwaShmClient initialized successfully."));
    return true;
}

void AHakoniwaShmClient::PreDeclareAllPDUs()
{
    TArray<AActor*> Actors;
    UGameplayStatics::GetAllActorsWithInterface(GetWorld(), UHakoniwaObjectInterface::StaticClass(), Actors);
    for (AActor* Actor : Actors)
    {
        UE_LOG(LogTemp, Log, TEXT("Notifying OnHakoInitialize to %s"), *Actor->GetName());
        IHakoniwaObjectInterface::Execute_OnHakoInitialize(Actor);
    }
}

void AHakoniwaShmClient::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    if (!pduManager || !service || !service->IsServiceEnabled())
    {
        return;
    }

    // Heartbeat
    hako_asset_notify_simtime(TCHAR_TO_ANSI(*AssetName), asset_time_usec);

    // Event Polling
    int ev = hako_asset_get_event(TCHAR_TO_ANSI(*AssetName));
    switch (ev)
    {
    case 1: // HakoSimAssetEvent_Start
        UE_LOG(LogTemp, Log, TEXT("Hako Event: START"));
        hako_asset_start_feedback(TCHAR_TO_ANSI(*AssetName), true);
        break;
    case 2: // HakoSimAssetEvent_Stop
        UE_LOG(LogTemp, Log, TEXT("Hako Event: STOP"));
        hako_asset_stop_feedback(TCHAR_TO_ANSI(*AssetName), true);
        break;
    case 3: // HakoSimAssetEvent_Reset
        UE_LOG(LogTemp, Log, TEXT("Hako Event: RESET"));
        asset_time_usec = 0;
        hako_asset_reset_feedback(TCHAR_TO_ANSI(*AssetName), true);
        break;
    default:
        break;
    }

    // Simulation Step Execution Control
    if (GetSimulationState_Implementation() != 2 /* Running */)
    {
        return;
    }

    if (!hako_asset_is_pdu_created())
    {
        return;
    }

    if (hako_asset_is_simulation_mode())
    {
        hako_time_t world_time = hako_asset_get_worldtime();
        hako_time_t next_asset_time_usec = asset_time_usec + delta_time_usec;
        if (next_asset_time_usec <= world_time)
        {
            asset_time_usec = next_asset_time_usec;
            hako_asset_notify_simtime(TCHAR_TO_ANSI(*AssetName), asset_time_usec);
            // At this point, the simulation clock has advanced.
            // PDU read/writes will happen in actors' Tick.
        }
    }
    else if (hako_asset_is_pdu_sync_mode(TCHAR_TO_ANSI(*AssetName)))
    {
        hako_asset_notify_write_pdu_done(TCHAR_TO_ANSI(*AssetName));
    }
}

void AHakoniwaShmClient::Start_Implementation()
{
    UE_LOG(LogTemp, Log, TEXT("AHakoniwaShmClient Start() - Sending simevent_start"));
    hako_simevent_start();
}

void AHakoniwaShmClient::Stop_Implementation()
{
    UE_LOG(LogTemp, Log, TEXT("AHakoniwaShmClient Stop() - Sending simevent_stop"));
    hako_simevent_stop();
}

void AHakoniwaShmClient::Reset_Implementation()
{
    UE_LOG(LogTemp, Log, TEXT("AHakoniwaShmClient Reset() - Sending simevent_reset"));
    hako_simevent_reset();
}

int32 AHakoniwaShmClient::GetSimulationState_Implementation() const
{
    if (!service || !service->IsServiceEnabled())
    {
        return -1; // Unknown/Not Initialized
    }
    return hako_simevent_get_state();
}
