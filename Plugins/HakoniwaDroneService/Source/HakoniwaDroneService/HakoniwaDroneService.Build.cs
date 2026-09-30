using System.IO;
using UnrealBuildTool;

// ★ 2026-09-30（related/devai_dll/hakodrone_unreal_drone_migration_20260930.md・U2）:
//   hako_service_c（.lib をリンク）から hakodrone（実行時に読む）に替えた。
//   hakodrone 一式は scripts/windows/fetch_native.ps1（setup_build.ps1 から）が native.lock.json の版で
//   ThirdParty/HakoDrone/Win64 に、機体の定義を SimModels/courses_drone に置く。
//   ★ .lib をリンクしないので、取得物が無くてもコンパイルできる（読み込みは実行時に失敗して知らせる）。
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

		string ProjectRoot = Path.GetFullPath(Path.Combine(ModuleDirectory, "../../../.."));
		string HakoDroneRoot = Path.Combine(ProjectRoot, "ThirdParty/HakoDrone");
		// ヘッダは管理している（取得しなくてもコンパイルできる）
		PrivateIncludePaths.Add(Path.Combine(HakoDroneRoot, "Include"));

		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			// PolyForm の Notice 条件: DLL の受領者にも条文を渡す
			string LicensePath = Path.Combine(HakoDroneRoot, "LICENSE.md");
			if (!File.Exists(LicensePath))
			{
				throw new BuildException("hakodrone の条文がありません: " + LicensePath);
			}
			RuntimeDependencies.Add(LicensePath, StagedFileType.NonUFS);

			string DllDirectory = Path.Combine(HakoDroneRoot, "Win64");
			foreach (string DllName in new string[] { "mujoco.dll", "hakodrone.dll" })
			{
				string DllPath = Path.Combine(DllDirectory, DllName);
				if (File.Exists(DllPath))
				{
					// パッケージ版では実行ファイルの隣へ置く（依存 DLL の探索を安定させる）
					RuntimeDependencies.Add("$(TargetOutputDir)/" + DllName, DllPath);
				}
			}
			// 取得物と一緒に置かれる条文（MuJoCo・nlohmann/json）と manifest.json（どの版か）も同梱する
			foreach (string NoticeName in new string[] { "LICENSE-mujoco.txt", "THIRD_PARTY_NOTICES-mujoco.txt", "LICENSE-nlohmann-json.txt", "manifest.json" })
			{
				string NoticePath = Path.Combine(DllDirectory, NoticeName);
				if (File.Exists(NoticePath))
				{
					RuntimeDependencies.Add(NoticePath, StagedFileType.NonUFS);
				}
			}

			// 機体の定義（Content 外の生テキスト）はプロジェクト相対の位置を保ったまま Non-UFS で置く
			string SimModelsDirectory = Path.Combine(ProjectRoot, "SimModels");
			if (Directory.Exists(SimModelsDirectory))
			{
				foreach (string ModelPath in Directory.GetFiles(SimModelsDirectory, "*", SearchOption.AllDirectories))
				{
					RuntimeDependencies.Add(ModelPath, StagedFileType.NonUFS);
				}
			}
			PublicDefinitions.Add("HAKO_DRONE_SERVICE_ENABLE=1");
		}
		else
		{
			// ★ Android などは計画の A0〜A3（libhakodrone.so を実行時に読む）。いまは読み込みが失敗して知らせる。
			PublicDefinitions.Add("HAKO_DRONE_SERVICE_ENABLE=0");
		}
	}
}
