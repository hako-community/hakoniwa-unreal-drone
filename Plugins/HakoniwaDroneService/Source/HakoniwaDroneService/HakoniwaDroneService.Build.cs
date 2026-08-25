using System;
using System.IO;
using UnrealBuildTool;

public class HakoniwaDroneService : ModuleRules
{
    private static string EscapeDefinitionValue(string Value)
    {
        return Value.Replace("\\", "\\\\").Replace("\"", "\\\"");
    }

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

	public HakoniwaDroneService(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core"
		});

		PrivateDependencyModuleNames.AddRange(new string[] {
			"CoreUObject",
			"Engine",
			"Projects"
		});

		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			string ThirdPartyPath = Path.Combine(ModuleDirectory, "../ThirdParty/hako_service_c");
			string DefaultDroneRoot = Path.Combine(
				Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData),
				"hakoApps-win",
				"hakoSim");
			string DroneIncludeRoot = ResolveDirectory(
				"HAKO_DRONE_INC_PATH",
				Path.Combine(DefaultDroneRoot, "include"),
				"service/drone/drone_service_rc_api.h",
				null);
			string DroneIncludePath = DroneIncludeRoot != null
				? Path.Combine(DroneIncludeRoot, "service/drone")
				: Path.Combine(ThirdPartyPath, "include");
			PublicIncludePaths.Add(DroneIncludePath);

			string LibPath = ResolveFile(
				"HAKO_DRONE_LIB_PATH",
				Path.Combine(DefaultDroneRoot, "lib"),
				"hako_service_c.lib",
				Path.Combine(ThirdPartyPath, "lib/Win64/hako_service_c.lib"));
			PublicAdditionalLibraries.Add(LibPath);

			string DllPath = ResolveFile(
				"HAKO_DRONE_DLL_PATH",
				Path.Combine(DefaultDroneRoot, "bin"),
				"hako_service_c.dll",
				Path.Combine(ModuleDirectory, "../../../../Binaries/Win64/hako_service_c.dll"));
			if (File.Exists(DllPath))
			{
				RuntimeDependencies.Add(DllPath);
			}
			PublicDelayLoadDLLs.Add("hako_service_c.dll");
			PublicDefinitions.Add("HAKO_DRONE_SERVICE_ENABLE=1");
			PublicDefinitions.Add("HAKO_DRONE_DLL_PATH_FALLBACK=\"" + EscapeDefinitionValue(Path.GetDirectoryName(DllPath)) + "\"");
		}
	}
}
