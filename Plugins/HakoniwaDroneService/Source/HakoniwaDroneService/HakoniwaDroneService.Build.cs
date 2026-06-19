using System.IO;
using UnrealBuildTool;

public class HakoniwaDroneService : ModuleRules
{
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
			// External native dependencies are intentionally not committed. See docs/dependencies.md.
			string ThirdPartyPath = Path.Combine(ModuleDirectory, "../ThirdParty/hako_service_c");
			PublicIncludePaths.Add(Path.Combine(ThirdPartyPath, "include"));

			string LibPath = Path.Combine(ThirdPartyPath, "lib/Win64/hako_service_c.lib");
			PublicAdditionalLibraries.Add(LibPath);

			string DllPath = Path.Combine(ModuleDirectory, "../../../../Binaries/Win64/hako_service_c.dll");
			RuntimeDependencies.Add(DllPath);
			PublicDelayLoadDLLs.Add("hako_service_c.dll");
			PublicDefinitions.Add("HAKO_DRONE_SERVICE_ENABLE=1");
		}
	}
}
