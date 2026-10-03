#include "HakoDroneServiceRc.h"

#include "HAL/PlatformMisc.h"
#include "HAL/PlatformProcess.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

#include "hakodrone.h"

// ★ 2026-09-30（related/devai_dll/hakodrone_unreal_drone_migration_20260930.md・U2）:
//   中身を hako_service_c（drone-core の物理＋FC）から hakodrone（MuJoCo の物理＋内蔵 FC）に替えた。
//   関数の名前と引数は変えていない（呼ぶ側 UDroneServiceVisualizerComponent を変えずに済むように）。
//   ★ DLL は実行時に読む（.lib をリンクしない）。取得物が無くてもモジュールはコンパイルでき、
//     将来の Android（libhakodrone.so）も同じ形で読める。
//   ★ 写し方は hakoniwa-godot-drone-courses の DroneServiceRC（C#）と同じ（機体も同じ courses_drone）。

DEFINE_LOG_CATEGORY_STATIC(LogHakoDroneService, Log, All);

namespace
{
// ---- hakodrone の関数（実行時に読む）----
using FVersion = const char* (*)();
using FAbiVersion = int (*)();
using FOpenMem = hd_sim* (*)(const char* const*, const char* const*, int, const char*, const char*, const char*, char*, int);
using FClose = void (*)(hd_sim*);
using FTime = double (*)(hd_sim*);
using FStep = int (*)(hd_sim*, double);
using FGetState = int (*)(hd_sim*, int, hd_state*);
using FGetControls = int (*)(hd_sim*, int, double*, int);
using FGetBattery = int (*)(hd_sim*, int, hd_battery*);
using FRcEnable = int (*)(hd_sim*, int, int);
using FRcSetMode = int (*)(hd_sim*, int, int);
using FRcGetConfig = int (*)(hd_sim*, int, hd_rc_config*);
using FRcSetConfig = int (*)(hd_sim*, int, const hd_rc_config*);
using FRcUpdate = int (*)(hd_sim*, int, const double*, int, const int*, int, int);
using FRcGetInfo = int (*)(hd_sim*, int, hd_rc_info*);
using FSetWind = int (*)(hd_sim*, double, double, double);
using FApplyImpulse = int (*)(hd_sim*, int, double, double, double, double, double, double, double);

void* GMujocoHandle = nullptr;
void* GHakoDroneHandle = nullptr;
FOpenMem pOpenMem = nullptr;
FClose pClose = nullptr;
FTime pTime = nullptr;
FStep pStep = nullptr;
FGetState pGetState = nullptr;
FGetControls pGetControls = nullptr;
FGetBattery pGetBattery = nullptr;
FRcEnable pRcEnable = nullptr;
FRcSetMode pRcSetMode = nullptr;
FRcGetConfig pRcGetConfig = nullptr;
FRcSetConfig pRcSetConfig = nullptr;
FRcUpdate pRcUpdate = nullptr;
FRcGetInfo pRcGetInfo = nullptr;
FSetWind pSetWind = nullptr;
FApplyImpulse pApplyImpulse = nullptr;

hd_sim* GSim = nullptr;

// ---- RC（hakoniwa-mujoco-drone include/rc/rc_input.hpp）----
//   軸: 0 = ヨー（右回りが +）／ 1 = 上下（上が +）／ 2 = 左右（右が +）／ 3 = 前後（前が +）
//   ボタン: 3 = モード切替 ／ 4 = アーム・ディスアーム（押した瞬間だけ効く）
constexpr int32 AxisTurnLR = 0, AxisUpDown = 1, AxisMoveLR = 2, AxisMoveFB = 3;
constexpr int32 NumAxis = 6, NumButton = 16;
constexpr int32 ButtonModeChange = 3, ButtonArm = 4;
double GAxis[NumAxis] = {0, 0, 0, 0, 0, 0};
int GButton[NumButton] = {0};

// ★ 手ごたえ: Put* の値 1 あたり、旧版（hako_service_c）がどれだけ動いたかの実測（courses の計画 §10.2）。
//   倒し切りの上限は機体の定義（courses_drone の meta の fc_tuning.manual）の値を開いたあとに読む。
// ★ 前後・左右だけ courses（10.8）と違う: Unreal の旧設定（Content/Config/controller/param-api-mixer.txt）は
//   PID_POS_VX/VY_Kp が 20（courses は 5）で、値 0.05・0.1 で 0.83・1.65 m/s（16.5 m/s／値 1・線形）だった（U4 の実測）。
constexpr double OldVelPerUnit = 16.5;       // 前後・左右 [m/s]／値 1
constexpr double OldClimbPerUnit = 1.04;     // 上下 [m/s]／値 1
constexpr double OldYawDegPerUnit = 540.0;   // ヨー [度/秒]／値 1
// ★ 2026-10-03（A3・ユーザ決定）: ヨーの速さの上限。旧版の効きのまま倒し切ると 540 度/秒になり、
//   courses_drone ではモータが上限に張り付いて、上昇の直後などに傾き 40 度・14 m 流されて姿勢を崩した
//   （Quest 3 で「フリップして飛んで行く」・Windows でも同じ）。180 度/秒なら傾き 10 度・流れ 1.4 m・回り過ぎ 46 度。
//   小さく倒したときの効き（値 1 あたり 540 度/秒）は変えず、ここで頭打ちにする。
constexpr double MaxYawRateDeg = 180.0;
// ★ 2026-10-03（A3）: ヨーは目標まで 0.5 秒かけて上げ下げする（シミュレーションの時間で数える）。
//   急に入れると一瞬だけ大きな回す力が要り、モータが下限・上限に張り付く。その間 FC は傾きを立て直す余力が無く、
//   ARM（Quest）では丸めの差から左右の対称が崩れてひっくり返った（hakodrone を Quest で直接動かして再現:
//   急に入れると 90 度/秒でも傾き 12 度・540 度/秒で落下 ／ 0.5 秒かけると 180 度/秒でも 0.06 度）。
constexpr double YawRampSeconds = 0.5;
double GYawTarget = 0.0;   // PutHeading が決める目標（RC の軸の値）
double GMaxVelXy = 2.8, GMaxClimb = 2.8, GMaxYawRateDeg = 720.0;

// ★ 機体の定義（プロジェクト直下の SimModels。パッケージ版でも同じ相対位置に Non-UFS で置かれる）
const TCHAR* ModelDirRelative = TEXT("SimModels/courses_drone");
const char* ModelFiles[] = {"test_world.xml", "courses_drone.mjcf.xml", "courses_drone_meta.json", "courses_drone_actuator.json"};

double Clamp1(double V) { return V > 1.0 ? 1.0 : (V < -1.0 ? -1.0 : V); }

#if PLATFORM_WINDOWS
FString ResolveDllDirectory()
{
#if WITH_EDITOR
	// 開発中の上書き口（droneshow-unreal-framework と同じ）: HAKO_NATIVE_LIB_DIR（; 区切り）の中で hakodrone.dll がある最初のフォルダ
	const FString Override = FPlatformMisc::GetEnvironmentVariable(TEXT("HAKO_NATIVE_LIB_DIR"));
	if (!Override.IsEmpty())
	{
		TArray<FString> Directories;
		Override.ParseIntoArray(Directories, TEXT(";"), true);
		for (FString Directory : Directories)
		{
			Directory.TrimStartAndEndInline();
			if (!Directory.IsEmpty() && FPaths::FileExists(FPaths::Combine(Directory, TEXT("hakodrone.dll"))))
			{
				UE_LOG(LogHakoDroneService, Log, TEXT("HAKO_NATIVE_LIB_DIR から hakodrone.dll を読みます: %s"), *Directory);
				return Directory;
			}
		}
		UE_LOG(LogHakoDroneService, Warning, TEXT("HAKO_NATIVE_LIB_DIR（%s）に hakodrone.dll が無いため、ThirdParty から読みます。"), *Override);
	}
#endif
	const FString Development = FPaths::Combine(FPaths::ProjectDir(), TEXT("ThirdParty/HakoDrone/Win64"));
	if (FPaths::FileExists(FPaths::Combine(Development, TEXT("hakodrone.dll"))))
	{
		return Development;
	}
	// パッケージ版では Build.cs が実行ファイルの隣へ DLL を置く
	return FPlatformProcess::BaseDir();
}
#endif

template <typename T>
bool Resolve(T& Out, const TCHAR* Name, FString* OutError)
{
	Out = reinterpret_cast<T>(FPlatformProcess::GetDllExport(GHakoDroneHandle, Name));
	if (Out == nullptr && OutError)
	{
		*OutError = FString::Printf(TEXT("hakodrone.dll に関数 '%s' がありません（ABI を確認してください）"), Name);
	}
	return Out != nullptr;
}

void ClearInputs()
{
	GYawTarget = 0.0;
	for (double& A : GAxis) A = 0.0;
	for (int& B : GButton) B = 0;
}

// Dt 秒ぶん、ヨーの軸を目標へ近づけてから送る
bool SendInputs(double Dt)
{
	const double MaxDelta = (MaxYawRateDeg / GMaxYawRateDeg) * Dt / YawRampSeconds;
	GAxis[AxisTurnLR] += FMath::Clamp(GYawTarget - GAxis[AxisTurnLR], -MaxDelta, MaxDelta);
	return GSim != nullptr && pRcUpdate(GSim, 0, GAxis, NumAxis, GButton, NumButton, 1) == HD_OK;
}

// ★ hakodrone の状態（x 北・y 東・z 上・度・FRD）を drone-core の約束（x 前・y 左・z 上・rad）に直す
bool GetState(int32 Index, hd_state& Out)
{
	return GSim != nullptr && pGetState(GSim, Index, &Out) == HD_OK;
}
}  // namespace

bool FHakoDroneServiceRc::LoadDll(FString* OutError)
{
	if (GHakoDroneHandle != nullptr)
	{
		return true;
	}
#if PLATFORM_WINDOWS
	const FString Dir = ResolveDllDirectory();
	const FString MujocoPath = FPaths::Combine(Dir, TEXT("mujoco.dll"));
	const FString HakoDronePath = FPaths::Combine(Dir, TEXT("hakodrone.dll"));
	if (!FPaths::FileExists(MujocoPath) || !FPaths::FileExists(HakoDronePath))
	{
		if (OutError)
		{
			*OutError = FString::Printf(TEXT("hakodrone.dll または mujoco.dll がありません（setup_build.ps1 で取ってきてください）: %s"), *Dir);
		}
		return false;
	}
	// ★ 2 つは必ず同じフォルダから読む（同じ名前の DLL が混ざらないように）。mujoco を先に。
	FPlatformProcess::PushDllDirectory(*Dir);
	GMujocoHandle = FPlatformProcess::GetDllHandle(*MujocoPath);
	GHakoDroneHandle = FPlatformProcess::GetDllHandle(*HakoDronePath);
	FPlatformProcess::PopDllDirectory(*Dir);
	if (GMujocoHandle == nullptr || GHakoDroneHandle == nullptr)
	{
		if (OutError)
		{
			*OutError = FString::Printf(TEXT("hakodrone.dll の読み込みに失敗しました: %s"), *Dir);
		}
		UnloadDll();
		return false;
	}
#elif PLATFORM_ANDROID
	// ★ 2026-10-03（A2）: APK の lib/arm64-v8a に入れた .so を標準の名前で読む（dlopen はそこを探す）。
	//   libhakodrone.so は libmujoco.so に依存するので先に読む（courses の HakoLibLoader と同じ順番）。
	const FString HakoDronePath = TEXT("libhakodrone.so");
	GMujocoHandle = FPlatformProcess::GetDllHandle(TEXT("libmujoco.so"));
	GHakoDroneHandle = FPlatformProcess::GetDllHandle(*HakoDronePath);
	if (GMujocoHandle == nullptr || GHakoDroneHandle == nullptr)
	{
		if (OutError)
		{
			*OutError = TEXT("libhakodrone.so または libmujoco.so を読めません（APK に入っているか: HakoniwaDroneService_APL.xml）");
		}
		UnloadDll();
		return false;
	}
#else
	if (OutError)
	{
		*OutError = TEXT("hakodrone はいま Win64 と Android だけに置いています。");
	}
	return false;
#endif

	FAbiVersion AbiFn = reinterpret_cast<FAbiVersion>(FPlatformProcess::GetDllExport(GHakoDroneHandle, TEXT("hd_abi_version")));
	FVersion VersionFn = reinterpret_cast<FVersion>(FPlatformProcess::GetDllExport(GHakoDroneHandle, TEXT("hd_version")));
	const int32 Abi = AbiFn ? AbiFn() : -1;
	if (Abi != HD_ABI_VERSION)
	{
		if (OutError)
		{
			*OutError = FString::Printf(TEXT("hakodrone.dll の関数の形の版が違います（DLL %d ／ ヘッダ %d）: %s"), Abi, HD_ABI_VERSION, *HakoDronePath);
		}
		UnloadDll();
		return false;
	}

	bool bOk = true;
	bOk &= Resolve(pOpenMem, TEXT("hd_open_mem"), OutError);
	bOk &= Resolve(pClose, TEXT("hd_close"), OutError);
	bOk &= Resolve(pTime, TEXT("hd_time"), OutError);
	bOk &= Resolve(pStep, TEXT("hd_step"), OutError);
	bOk &= Resolve(pGetState, TEXT("hd_get_state"), OutError);
	bOk &= Resolve(pGetControls, TEXT("hd_get_controls"), OutError);
	bOk &= Resolve(pGetBattery, TEXT("hd_get_battery"), OutError);
	bOk &= Resolve(pRcEnable, TEXT("hd_rc_enable"), OutError);
	bOk &= Resolve(pRcSetMode, TEXT("hd_rc_set_mode"), OutError);
	bOk &= Resolve(pRcGetConfig, TEXT("hd_rc_get_config"), OutError);
	bOk &= Resolve(pRcSetConfig, TEXT("hd_rc_set_config"), OutError);
	bOk &= Resolve(pRcUpdate, TEXT("hd_rc_update"), OutError);
	bOk &= Resolve(pRcGetInfo, TEXT("hd_rc_get_info"), OutError);
	bOk &= Resolve(pSetWind, TEXT("hd_set_wind"), OutError);
	bOk &= Resolve(pApplyImpulse, TEXT("hd_apply_impulse"), OutError);
	if (!bOk)
	{
		UnloadDll();
		return false;
	}
	UE_LOG(LogHakoDroneService, Log, TEXT("hakodrone %s（ABI %d）を読みました: %s"),
		VersionFn ? UTF8_TO_TCHAR(VersionFn()) : TEXT("?"), Abi, *HakoDronePath);
	return true;
}

void FHakoDroneServiceRc::UnloadDll()
{
	Stop();
	pOpenMem = nullptr; pClose = nullptr; pTime = nullptr; pStep = nullptr; pGetState = nullptr;
	pGetControls = nullptr; pGetBattery = nullptr; pRcEnable = nullptr; pRcSetMode = nullptr;
	pRcGetConfig = nullptr; pRcSetConfig = nullptr; pRcUpdate = nullptr; pRcGetInfo = nullptr;
	pSetWind = nullptr; pApplyImpulse = nullptr;
	if (GHakoDroneHandle != nullptr)
	{
		FPlatformProcess::FreeDllHandle(GHakoDroneHandle);
		GHakoDroneHandle = nullptr;
	}
	if (GMujocoHandle != nullptr)
	{
		FPlatformProcess::FreeDllHandle(GMujocoHandle);
		GMujocoHandle = nullptr;
	}
}

bool FHakoDroneServiceRc::IsDllLoaded()
{
	return GHakoDroneHandle != nullptr;
}

// ★ 機体の定義を開く（courses_drone）。RC を位置モード（GPS）で有効にし、倒し切りの上限を機体から読む。
static int32 OpenModel()
{
	FString Error;
	if (!FHakoDroneServiceRc::LoadDll(&Error))
	{
		UE_LOG(LogHakoDroneService, Error, TEXT("%s"), *Error);
		return -1;
	}
	FHakoDroneServiceRc::Stop();

	const FString Dir = FPaths::Combine(FPaths::ProjectDir(), ModelDirRelative);
	constexpr int32 NumFiles = UE_ARRAY_COUNT(ModelFiles);
	// ★ 中身を UTF-8 の NUL 終端の列にして渡す（hd_open_mem は束の中身で開く・一時ファイルを作らない）
	TArray<TArray<ANSICHAR>> Utf8;
	Utf8.SetNum(NumFiles);
	const char* TextPtrs[NumFiles];
	for (int32 I = 0; I < NumFiles; ++I)
	{
		const FString Path = FPaths::Combine(Dir, UTF8_TO_TCHAR(ModelFiles[I]));
		FString Content;
		// ★ LoadFileToString は .pak の中も読める（将来の Android でも同じ口で開ける）
		if (!FFileHelper::LoadFileToString(Content, *Path))
		{
			UE_LOG(LogHakoDroneService, Error, TEXT("機体の定義がありません: %s（setup_build.ps1 が取ってきます）"), *Path);
			return -1;
		}
		const FTCHARToUTF8 Conv(*Content);
		Utf8[I].Append(Conv.Get(), Conv.Length());
		Utf8[I].Add('\0');
		TextPtrs[I] = Utf8[I].GetData();
	}
	char Err[512] = {0};
	GSim = pOpenMem(ModelFiles, TextPtrs, NumFiles, ModelFiles[0], ModelFiles[2], ModelFiles[3], Err, sizeof(Err));
	if (GSim == nullptr)
	{
		UE_LOG(LogHakoDroneService, Error, TEXT("hd_open_mem に失敗しました: %s"), UTF8_TO_TCHAR(Err));
		return -1;
	}
	ClearInputs();
	pRcEnable(GSim, 0, 1);
	pRcSetMode(GSim, 0, HD_RC_GPS);
	hd_rc_config C;
	if (pRcGetConfig(GSim, 0, &C) == HD_OK)
	{
		GMaxVelXy = C.max_vel_xy;
		GMaxClimb = C.max_climb;
		GMaxYawRateDeg = C.max_yaw_rate_deg;
		C.deadzone = 0.0;   // 不感帯とカーブは Unreal の入力の側で掛け済み
		C.expo = 0.0;
		pRcSetConfig(GSim, 0, &C);
	}
	UE_LOG(LogHakoDroneService, Log, TEXT("機体 courses_drone を開きました（上限 %.1f m/s・%.1f m/s・%.0f 度/秒）"), GMaxVelXy, GMaxClimb, GMaxYawRateDeg);
	return 0;
}

int32 FHakoDroneServiceRc::Init(int32 bEnableDatalog, const FString& DroneConfigDirPath, const FString& DebugLogPath)
{
	// ★ drone-core の設定のフォルダは使わない（機体は SimModels/courses_drone）
	return OpenModel();
}

int32 FHakoDroneServiceRc::InitSingle(const FString& DroneConfigText, const FString& ControllerConfigText, bool bLoggerEnable, const FString& DebugLogPath)
{
	// ★ drone-core の設定の文字列（drone_config_0.json・param-api-mixer.txt）は使わない。
	//   同じ物理を写した hakodrone の機体 courses_drone を開く。
	return OpenModel();
}

int32 FHakoDroneServiceRc::Start()
{
	return GSim != nullptr ? 0 : -1;   // 開いた時点で走れる
}

int32 FHakoDroneServiceRc::Run()
{
	if (GSim == nullptr) return -1;
	SendInputs(0.001);
	return pStep(GSim, 0.001);
}

int32 FHakoDroneServiceRc::AdvanceTimeUsec(uint64 TimeUsec)
{
	if (GSim == nullptr) return -1;
	const double Dt = static_cast<double>(TimeUsec) * 1e-6;
	SendInputs(Dt);
	return pStep(GSim, Dt);
}

int32 FHakoDroneServiceRc::Stop()
{
	if (GSim != nullptr && pClose != nullptr)
	{
		pClose(GSim);
	}
	GSim = nullptr;
	return 0;
}

int32 FHakoDroneServiceRc::Reset()
{
	return OpenModel();   // 開き直す（地上・停止の状態に戻る）
}

// ---- 操作。drone-core の約束は **正が 下降・右・右ヨー・後退**。hakodrone の軸は 上・右・右ヨー・前 が +。
//      座標系の食い違いは無い（courses で旧版と同じ入力で同じ向きへ動くことを確かめた）。反転は上下と前後だけ。
int32 FHakoDroneServiceRc::PutVertical(int32 Index, double Value)
{
	GAxis[AxisUpDown] = Clamp1(-Value * OldClimbPerUnit / GMaxClimb);
	return 0;
}

int32 FHakoDroneServiceRc::PutHorizontal(int32 Index, double Value)
{
	GAxis[AxisMoveLR] = Clamp1(Value * OldVelPerUnit / GMaxVelXy);
	return 0;
}

int32 FHakoDroneServiceRc::PutHeading(int32 Index, double Value)
{
	const double RateDeg = FMath::Clamp(Value * OldYawDegPerUnit, -MaxYawRateDeg, MaxYawRateDeg);
	GYawTarget = Clamp1(RateDeg / GMaxYawRateDeg);   // 実際の軸は SendInputs で 0.5 秒かけて近づける
	return 0;
}

int32 FHakoDroneServiceRc::PutForward(int32 Index, double Value)
{
	GAxis[AxisMoveFB] = Clamp1(-Value * OldVelPerUnit / GMaxVelXy);
	return 0;
}

int32 FHakoDroneServiceRc::PutRadioControlButton(int32 Index, int32 Value)
{
	GButton[ButtonArm] = Value != 0 ? 1 : 0;
	return 0;
}

int32 FHakoDroneServiceRc::PutModeChangeButton(int32 Index, int32 Value)
{
	GButton[ButtonModeChange] = Value != 0 ? 1 : 0;
	return 0;
}

// ★ hakodrone に無いボタン（Q-U4: 名前は残して何もしない）
int32 FHakoDroneServiceRc::PutMagnetControlButton(int32 Index, int32 Value) { return 0; }
int32 FHakoDroneServiceRc::PutCameraControlButton(int32 Index, int32 Value) { return 0; }
int32 FHakoDroneServiceRc::PutHomeControlButton(int32 Index, int32 Value) { return 0; }

// ---- 読み出し ----
int32 FHakoDroneServiceRc::GetPosition(int32 Index, FHakoDroneServiceVector& OutPosition)
{
	hd_state S;
	if (!GetState(Index, S)) return -1;
	OutPosition.X = S.x;     // 前（北）
	OutPosition.Y = -S.y;    // 左（hakodrone は東が +）
	OutPosition.Z = S.z;     // 上
	return 0;
}

int32 FHakoDroneServiceRc::GetAttitude(int32 Index, FHakoDroneServiceVector& OutAttitude)
{
	hd_state S;
	if (!GetState(Index, S)) return -1;
	OutAttitude.X = FMath::DegreesToRadians(S.roll_deg);
	OutAttitude.Y = -FMath::DegreesToRadians(S.pitch_deg);   // FRD → FLU
	OutAttitude.Z = -FMath::DegreesToRadians(S.yaw_deg);
	return 0;
}

int32 FHakoDroneServiceRc::GetControls(int32 Index, double& C1, double& C2, double& C3, double& C4, double& C5, double& C6, double& C7, double& C8)
{
	C1 = C2 = C3 = C4 = C5 = C6 = C7 = C8 = 0.0;
	if (GSim == nullptr) return -1;
	double C[8] = {0, 0, 0, 0, 0, 0, 0, 0};
	const int N = pGetControls(GSim, Index, C, 8);
	if (N <= 0) return -1;
	C1 = C[0]; C2 = C[1]; C3 = C[2]; C4 = C[3]; C5 = C[4]; C6 = C[5]; C7 = C[6]; C8 = C[7];
	return 0;
}

int32 FHakoDroneServiceRc::GetBodyVelocity(int32 Index, FHakoDroneServiceVector& OutVelocity)
{
	hd_state S;
	if (!GetState(Index, S)) return -1;
	const double Vx = S.vx, Vy = -S.vy;                      // 世界: x 北・y 西（左）
	const double Psi = -FMath::DegreesToRadians(S.yaw_deg);  // 左回りが +
	OutVelocity.X = FMath::Cos(Psi) * Vx + FMath::Sin(Psi) * Vy;
	OutVelocity.Y = -FMath::Sin(Psi) * Vx + FMath::Cos(Psi) * Vy;
	OutVelocity.Z = S.vz;
	return 0;
}

// ★ hakodrone に無い量（部品からは呼んでいない）
int32 FHakoDroneServiceRc::GetBodyAngularVelocity(int32 Index, FHakoDroneServiceVector& OutAngularVelocity)
{
	OutAngularVelocity = FHakoDroneServiceVector();
	return GSim != nullptr ? 0 : -1;
}

int32 FHakoDroneServiceRc::GetPropellerWind(int32 Index, FHakoDroneServiceVector& OutWind)
{
	OutWind = FHakoDroneServiceVector();
	return GSim != nullptr ? 0 : -1;
}

int32 FHakoDroneServiceRc::GetBatteryStatus(int32 Index, FHakoDroneBatteryStatus& OutBatteryStatus)
{
	OutBatteryStatus = FHakoDroneBatteryStatus();
	if (GSim == nullptr) return -1;
	hd_battery B;
	if (pGetBattery(GSim, Index, &B) != HD_OK) return -1;
	OutBatteryStatus.FullVoltage = B.full_voltage;
	OutBatteryStatus.CurrentVoltage = B.voltage;
	OutBatteryStatus.CurrentTemperature = 0.0;   // ★ 温度・充電回数はモデル化していない
	OutBatteryStatus.Status = B.empty != 0 ? 1u : 0u;
	OutBatteryStatus.Cycles = 0;
	return 0;
}

int32 FHakoDroneServiceRc::GetInternalState(int32 Index, int32& OutState)
{
	// drone-core の番号: 0 = 離陸（アーム済みで地上）・1 = 飛行中・3 = 停止（着陸中の 2 は区別しない）
	OutState = 3;
	hd_state S;
	if (!GetState(Index, S)) return -1;
	OutState = S.armed == 0 ? 3 : (S.z < 0.1 ? 0 : 1);
	return 0;
}

int32 FHakoDroneServiceRc::GetFlightMode(int32 Index, int32& OutMode)
{
	OutMode = 0;
	if (GSim == nullptr) return -1;
	hd_rc_info Info;
	if (pRcGetInfo(GSim, Index, &Info) != HD_OK) return -1;
	OutMode = Info.mode;   // 0 = 角度（ATTI）/ 1 = 高度保持 / 2 = 位置（GPS）
	return 0;
}

uint64 FHakoDroneServiceRc::GetTimeUsec(int32 Index)
{
	return GSim != nullptr ? static_cast<uint64>(pTime(GSim) * 1e6) : 0;
}

int32 FHakoDroneServiceRc::PutDisturbance(int32 Index, double Temperature, double WindX, double WindY, double WindZ)
{
	// ★ 温度は無い。風は全機共通・水平だけ（drone-core の FLU → 吹いてくる方位）
	if (GSim == nullptr) return -1;
	const double North = WindX, East = -WindY;
	const double Speed = FMath::Sqrt(North * North + East * East);
	// hd_set_wind の向きは「吹いてくる」方位（0 = 北風）。風が向かう向きの反対。
	const double FromDeg = FMath::RadiansToDegrees(FMath::Atan2(-East, -North));
	return pSetWind(GSim, Speed, FromDeg, 0.0);
}

int32 FHakoDroneServiceRc::PutCollision(int32 Index, double ContactX, double ContactY, double ContactZ, double RestitutionCoefficient)
{
	// ★ drone-core の衝突の口に相当するものは無い（hd_apply_impulse は力積を渡す別の口）。部品からは呼んでいない（Q-U4）。
	return GSim != nullptr ? 0 : -1;
}
