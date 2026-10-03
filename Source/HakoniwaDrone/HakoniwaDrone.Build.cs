// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.IO;
using UnrealBuildTool;

public class HakoniwaDrone : ModuleRules
{
    private static string GetEnvPath(string Name)
    {
        string Value = Environment.GetEnvironmentVariable(Name);
        return string.IsNullOrWhiteSpace(Value) ? null : Value;
    }

    private static string ResolveDirectory(string EnvName, string DefaultPath, string RequiredFile, string FallbackPath)
    {
        string EnvPath = GetEnvPath(EnvName);
        if (EnvPath != null && File.Exists(Path.Combine(EnvPath, RequiredFile)))
        {
            return EnvPath;
        }

        if (!string.IsNullOrWhiteSpace(DefaultPath) && File.Exists(Path.Combine(DefaultPath, RequiredFile)))
        {
            return DefaultPath;
        }

        return FallbackPath;
    }

    private static string ResolveFile(string EnvName, string DefaultPath, string FileName, string FallbackPath)
    {
        string Directory = ResolveDirectory(EnvName, DefaultPath, FileName, null);
        return Directory != null ? Path.Combine(Directory, FileName) : FallbackPath;
    }

	public HakoniwaDrone(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		bEnableExceptions = true;
	
		PublicDependencyModuleNames.AddRange(new string[] { 
            "Core", "CoreUObject", "Engine", "InputCore", "EnhancedInput",
            "RenderCore", "RHI", "ImageWrapper",
            "HakoniwaPdu",
            "HakoniwaDroneService",
            "UMG",
        });

		PrivateDependencyModuleNames.AddRange(new string[] {  });

        if (Target.Platform == UnrealTargetPlatform.Win64)
        {
            // ★ 2026-10-03: GameInput の背景入力を有効にするため（UDroneControlLocalInput）
            PrivateDependencyModuleNames.Add("GameInputBase");

            string DefaultCoreRoot = Path.Combine(
                Environment.GetFolderPath(Environment.SpecialFolder.ApplicationData),
                "hakoCore-win");
            string ShakocLibPath = ResolveFile(
                "HAKO_CORE_LIB_PATH",
                Path.Combine(DefaultCoreRoot, "lib"),
                "shakoc.lib",
                Path.Combine(ModuleDirectory, "../../Plugins/HakoniwaPdu/Source/ThirdParty/shakoc/lib/Win64/shakoc.lib"));
            PublicAdditionalLibraries.Add(ShakocLibPath);

            string ShakocDllPath = ResolveFile(
                "HAKO_CORE_DLL_PATH",
                Path.Combine(DefaultCoreRoot, "bin"),
                "shakoc.dll",
                Path.Combine(ModuleDirectory, "../../Binaries/Win64/shakoc.dll"));
            if (File.Exists(ShakocDllPath))
            {
                RuntimeDependencies.Add(ShakocDllPath);
            }
            PublicDelayLoadDLLs.Add("shakoc.dll");
        }

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });
		
		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
