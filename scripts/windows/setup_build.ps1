[CmdletBinding()]
param(
    [Parameter()]
    [string]$EngineRoot,

    [Parameter()]
    [string]$CoreSdkRoot,

    [Parameter()]
    [switch]$ValidateOnly
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

trap {
    Write-Host "[setup-build] ERROR: $($_.Exception.Message)" -ForegroundColor Red
    exit 1
}

function Write-Step {
    param([string]$Message)
    Write-Host "[setup-build] $Message"
}

function Assert-File {
    param(
        [string]$Path,
        [string]$Description
    )

    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) {
        throw "$Description was not found: $Path"
    }
}

function Resolve-DependencyFile {
    param(
        [string]$EnvironmentVariable,
        [string]$EnvironmentRelativePath,
        [string]$DefaultPath,
        [string]$FallbackPath,
        [string]$Description
    )

    $candidates = [System.Collections.Generic.List[string]]::new()
    $environmentDirectory = [Environment]::GetEnvironmentVariable($EnvironmentVariable, 'Process')
    if (-not [string]::IsNullOrWhiteSpace($environmentDirectory)) {
        $candidates.Add((Join-Path $environmentDirectory $EnvironmentRelativePath))
    }
    if (-not [string]::IsNullOrWhiteSpace($DefaultPath)) {
        $candidates.Add($DefaultPath)
    }
    if (-not [string]::IsNullOrWhiteSpace($FallbackPath)) {
        $candidates.Add($FallbackPath)
    }

    foreach ($candidate in $candidates) {
        if (Test-Path -LiteralPath $candidate -PathType Leaf) {
            $resolved = (Resolve-Path -LiteralPath $candidate).Path
            Write-Step "${Description}: $resolved"
            return $resolved
        }
    }

    throw "$Description was not found. Searched: $($candidates -join ', ')"
}

function Set-SdkEnvironmentFromRoot {
    param(
        [string]$Root,
        [ValidateSet('Core')]
        [string]$Sdk
    )

    if ([string]::IsNullOrWhiteSpace($Root)) {
        return
    }

    $resolvedRoot = (Resolve-Path -LiteralPath $Root).Path
    if ($Sdk -eq 'Core') {
        $env:HAKO_CORE_INC_PATH = Join-Path $resolvedRoot 'include'
        $env:HAKO_CORE_LIB_PATH = Join-Path $resolvedRoot 'lib'
        $env:HAKO_CORE_DLL_PATH = Join-Path $resolvedRoot 'bin'
        Write-Step "Core SDK environment set for this process: $resolvedRoot"
    }
}

$projectRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$projectFile = Join-Path $projectRoot 'HakoniwaDrone.uproject'
Assert-File -Path $projectFile -Description 'Unreal project'

$projectDescriptor = Get-Content -LiteralPath $projectFile -Raw | ConvertFrom-Json
$engineAssociation = [string]$projectDescriptor.EngineAssociation
if ([string]::IsNullOrWhiteSpace($engineAssociation)) {
    throw 'HakoniwaDrone.uproject does not define EngineAssociation.'
}

if ([string]::IsNullOrWhiteSpace($EngineRoot)) {
    $EngineRoot = Join-Path $env:ProgramFiles "Epic Games\UE_$engineAssociation"
}
$EngineRoot = (Resolve-Path -LiteralPath $EngineRoot).Path
$buildBatch = Join-Path $EngineRoot 'Engine\Build\BatchFiles\Build.bat'
Assert-File -Path $buildBatch -Description "Unreal Engine $engineAssociation Build.bat"
Write-Step "Unreal Engine: $EngineRoot"

$pluginDescriptor = Join-Path $projectRoot 'Plugins\HakoniwaPdu\HakoniwaPdu.uplugin'
$pduRegistryHeader = Join-Path $projectRoot 'Plugins\HakoniwaPdu\Source\ThirdParty\hakoniwa-pdu-registry\pdu\types\pdu.hpp'
Assert-File -Path $pluginDescriptor -Description 'HakoniwaPdu plugin descriptor'
Assert-File -Path $pduRegistryHeader -Description 'hakoniwa-pdu-registry submodule'

Set-SdkEnvironmentFromRoot -Root $CoreSdkRoot -Sdk Core

$defaultCoreRoot = Join-Path $env:APPDATA 'hakoCore-win'

$null = Resolve-DependencyFile `
    -EnvironmentVariable 'HAKO_CORE_INC_PATH' `
    -EnvironmentRelativePath 'hako_capi.h' `
    -DefaultPath (Join-Path $defaultCoreRoot 'include\hako_capi.h') `
    -FallbackPath (Join-Path $projectRoot 'Plugins\HakoniwaPdu\Source\ThirdParty\shakoc\include\hako_capi.h') `
    -Description 'shakoc header'

$null = Resolve-DependencyFile `
    -EnvironmentVariable 'HAKO_CORE_LIB_PATH' `
    -EnvironmentRelativePath 'shakoc.lib' `
    -DefaultPath (Join-Path $defaultCoreRoot 'lib\shakoc.lib') `
    -FallbackPath (Join-Path $projectRoot 'Plugins\HakoniwaPdu\Source\ThirdParty\shakoc\lib\Win64\shakoc.lib') `
    -Description 'shakoc import library'

$null = Resolve-DependencyFile `
    -EnvironmentVariable 'HAKO_CORE_DLL_PATH' `
    -EnvironmentRelativePath 'shakoc.dll' `
    -DefaultPath (Join-Path $defaultCoreRoot 'bin\shakoc.dll') `
    -FallbackPath (Join-Path $projectRoot 'Binaries\Win64\shakoc.dll') `
    -Description 'shakoc runtime DLL'

Write-Step 'Environment-based build dependencies are valid.'

# hakodrone.dll + mujoco.dll と機体の定義（SimModels/courses_drone）を native.lock.json の版で取ってくる。
# 取得済みなら素早く抜ける。HAKO_SKIP_FETCH_NATIVE=1 で飛ばせる（DLL が無くてもモジュールはコンパイルできる）。
Write-Step 'Fetching native libraries (native.lock.json)...'
& powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $PSScriptRoot 'fetch_native.ps1') -Root $projectRoot
if ($LASTEXITCODE -ne 0) {
    Write-Host "[setup-build] ERROR: fetch_native.ps1 failed (see above)." -ForegroundColor Red
    exit 1
}
# コンパイルに使うヘッダは ThirdParty/HakoDrone/Include で管理している。取ってきたものと違えば知らせる。
$trackedHeader = Join-Path $projectRoot 'ThirdParty\HakoDrone\Include\hakodrone.h'
$fetchedHeader = Join-Path $projectRoot 'ThirdParty\HakoDrone\Win64\hakodrone.h'
if ((Test-Path $trackedHeader) -and (Test-Path $fetchedHeader)) {
    if ([Convert]::ToBase64String([System.IO.File]::ReadAllBytes($trackedHeader)) -ne [Convert]::ToBase64String([System.IO.File]::ReadAllBytes($fetchedHeader))) {
        Write-Host "[setup-build] WARNING: ThirdParty/HakoDrone/Include/hakodrone.h differs from the fetched header. Copy the fetched one if the ABI changed." -ForegroundColor Yellow
    }
}

if ($ValidateOnly) {
    Write-Step 'ValidateOnly specified; skipping the Unreal build.'
    exit 0
}

Write-Step 'Building HakoniwaDroneEditor Win64 Development.'
& $buildBatch `
    'HakoniwaDroneEditor' `
    'Win64' `
    'Development' `
    "-Project=$projectFile" `
    '-WaitMutex' `
    '-NoHotReloadFromIDE' `
    '-NoXGE'

if ($LASTEXITCODE -ne 0) {
    Write-Host "[setup-build] ERROR: UnrealBuildTool failed with exit code $LASTEXITCODE." -ForegroundColor Red
    exit 1
}

Write-Step 'Build succeeded.'
