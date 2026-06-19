// Copyright Epic Games, Inc. All Rights Reserved.

using System.IO;
using UnrealBuildTool;

public class HakoniwaDrone : ModuleRules
{
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
            // External native dependencies are intentionally not committed. See docs/dependencies.md.
            string ShakocLibPath = Path.Combine(ModuleDirectory, "../../Plugins/HakoniwaPdu/Source/ThirdParty/shakoc/lib/Win64/shakoc.lib");
            PublicAdditionalLibraries.Add(ShakocLibPath);

            string ShakocDllPath = Path.Combine(ModuleDirectory, "../../Binaries/Win64/shakoc.dll");
            RuntimeDependencies.Add(ShakocDllPath);
            PublicDelayLoadDLLs.Add("shakoc.dll");
        }

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });
		
		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
