#ifndef HAKODRONE_H
#define HAKODRONE_H
/*
 * hakodrone C API（C4）— 教育用途の Python から呼ぶための最小の口。
 *
 * ★ 設計方針:
 *   * **同期**。`hd_takeoff()` は「着いたら返る」。教材は「順番に実行される」ほうが分かりやすい
 *   * **決定論**。同じスクリプトなら毎回同じ軌跡（教材の再現性）
 *   * **z は上が正**（教材向け）。内部の NED（下が正）は入口で吸収する
 *   * 失敗は戻り値で返す（例外にしない）。Python 側で分かりやすい例外に直す
 */

/* ★★★★ EDU3（2026-08-30）: **エクスポート指定**。
 *
 * ★★★★ **Windows ではこれが無いと「関数が 1 つも公開されない DLL」ができる。**
 *   Linux / macOS は既定で公開されるので、無くても気づけない —— ★★★ **黙って壊れる形**である。
 *   C# 側（`EduDroneService.cs` の P/Invoke）は `EntryPointNotFoundException` になる。
 *   ★ 確認は `dumpbin /exports hakodrone.dll` に `hd_*` が並ぶこと。
 *
 * ★★ `HAKODRONE_BUILD` はライブラリ本体をビルドするときだけ立つ（CMake が付ける）。
 *   利用側（試験・Godot・Python）では `dllimport` になる。
 * ★★★ **呼び出し規約は Cdecl のまま**。教育側の C# も `CallingConvention.Cdecl` で宣言しているので、
 *   ここを `__stdcall` にしてはいけない。 */
#if defined(_WIN32)
#  if defined(HAKODRONE_BUILD)
#    define HD_API __declspec(dllexport)
#  else
#    define HD_API __declspec(dllimport)
#  endif
#else
#  define HD_API __attribute__((visibility("default")))
#endif

#ifdef __cplusplus
extern "C" {
#endif

#define HD_OK       0
#define HD_ERROR   -1
#define HD_TIMEOUT -2

/* ★★★ 版（2026-09-29・DLL 配布の M-1。`related/devai_dll/hakodrone_dll_distribution_20260927.md` §4）。
 *
 * ★★★ なぜ要るか: 公開関数の名前は版をまたいで同じなので、**古い DLL が黙って動いてしまう**
 *   （09-13 版と 09-26 版は関数 41 個が同名で、物理・FC の中身だけが違った）。
 *   使う側は読んだ直後に `hd_version()` をログに出し、`hd_abi_version()` を自分の想定と比べる。
 *
 * `HD_ABI_VERSION` は**公開関数の形**（消した・引数や構造体の並びを変えた）が変わったら上げる整数。
 *   関数を足すだけ・中身だけを変えたときは上げない（版の付け方は上の文書 §4.3）。
 *   ★ C/C++ から使う側は、ヘッダの `HD_ABI_VERSION` と DLL の `hd_abi_version()` を比べれば、
 *     「ヘッダと DLL の組が合っているか」を実行時に確かめられる。 */
#define HD_ABI_VERSION 1

/* 版の文字列（例 "1.0.0+4336283"・開発中は "1.0.0-dev+4336283"・手元の変更があれば末尾に ".dirty"）。
 * ★ 静的な文字列を返す（解放しない）。いつ呼んでもよい（開く前でも）。 */
HD_API const char* hd_version(void);
/* 公開関数の形の版（`HD_ABI_VERSION` と同じ値を返す）。 */
HD_API int         hd_abi_version(void);

typedef struct hd_sim hd_sim;

typedef struct {
    double t;              /* シミュレーション時刻 [s] */
    double x, y, z;        /* 位置 [m]（x=北, y=東, ★ z=上） */
    double vx, vy, vz;     /* 速度 [m/s] */
    double roll_deg, pitch_deg, yaw_deg;
    double battery;        /* 残量 0..1 */
    int armed;
} hd_state;

HD_API hd_sim* hd_open(const char* world, const char* meta, const char* actuator, char* err, int errlen);

/* ★★★★ EDU1: **ファイルではなく「中身」で開く**（2026-08-29）。
 *
 * ★★★★ なぜ要るか: Godot の資産は `res://`（.pck の中）にあり、**ネイティブから開けない**。
 *   ★★ さらに world の XML は `<include file="...">` で**別ファイルを引く**ので、
 *     「3 本のテキストを渡す」だけでは足りない。**名前で引ける束**を渡す形にした。
 *   ★★★ 中では **MuJoCo の VFS** を使うので、**一時ファイルを 1 つも作らない**
 *     （Android で「書ける場所」を仮定しないため。案 B の目的そのもの）。
 *
 * @param names  ファイル名の配列（例 "world.xml" / "codrone.mjcf.xml" / "codrone_meta.json" …）
 *               ★ **階層は付けてよいが、引くときは基底名に落とされる**
 *                 （別の階層の同名ファイルは区別できない ＝ 知っていて選んだ制限）。
 * @param texts  同じ並びの中身（NUL 終端）。
 * @param n      枚数。
 * @param world / meta / actuator  束の中の名前。
 *
 * ★ 既存の `hd_open`（パス版）はそのまま残る。**どちらで開いても結果は同じ**であることは
 *   `tests/test_asset_bundle_edu1.cpp` が飛行の軌跡の厳密一致で固定している。 */
HD_API hd_sim* hd_open_mem(const char* const* names, const char* const* texts, int n,
                    const char* world, const char* meta, const char* actuator,
                    char* err, int errlen);

HD_API void    hd_close(hd_sim* h);

HD_API int     hd_count(hd_sim* h);
HD_API double  hd_time(hd_sim* h);
HD_API void    hd_set_speed(hd_sim* h, double v);
HD_API void    hd_set_timeout(hd_sim* h, double t);

HD_API int     hd_step(hd_sim* h, double seconds);
HD_API int     hd_get_state(hd_sim* h, int idx, hd_state* out);

/* ★★★★ ドローンショー Phase 2（2026-09-02）: **全機ぶんを 1 回で受け渡す**。
 *
 * ★★★★ **なぜ要るか**: 200 機のショーは 1 物理フレームあたり
 *   `hd_get_state` を機体ごとに 2 回（位置と姿勢）＋ `hd_move_to_async` を 1 回
 *   ＋ `hd_arrived` を 1 回、つまり **約 800 回の P/Invoke** を撃っていた。
 *   ★★★ 1 回の呼び出しが返す情報は 1 機ぶんしかないのに、**境界を跨ぐ費用は
 *   中身の量ではなく回数で決まる**。だから**回数のほうを削る**。
 * ★★ 中身は `hd_get_state` / `hd_move_to_async` と**同じもの**である
 *   （挙動を変えずに束ねただけ ＝ 飛行結果は 1 ビットも変わらない）。
 * ★ 部分取得もできる（`count` が機数より少なければ先頭から `count` 機ぶん）。
 */

/* 目標位置（`hd_move_to_batch` に渡す 1 機ぶん）。★ 並びは機体番号 0..count-1。 */
typedef struct {
    double x, y, z;        /* 目標位置 [m]（z は上が正 ＝ hd_state と同じ規約） */
    double yaw_deg;        /* 目標方位 [deg] */
} hd_target;

/* 先頭から `count` 機ぶんの状態を `out` へ書く。
 * @return 実際に書いた機数（0 以上）。`h` が無効なら HD_ERROR。
 *         ★ `count` が機数より多ければ**機数で頭打ち**にする（範囲外は書かない）。 */
HD_API int     hd_get_states(hd_sim* h, hd_state* out, int count);

/* 先頭から `count` 機ぶんへ目標位置を配る（`hd_move_to_async` と同じ意味）。
 * @return 実際に指令した機数（0 以上）。`h` が無効なら HD_ERROR。 */
HD_API int     hd_move_to_batch(hd_sim* h, const hd_target* in, int count);

/* ============================================================
 * ★★★★ O-15: 時刻つき軌道（ショー）—— `FcMode::Show` の口
 * ============================================================
 *
 * ★★★★ **ネイティブ側は C13 で既にできている**（`SimpleFC::load_trajectory` ／
 *   区間速度の前置補償まで）。★ 足りなかったのは **C API の口だけ**だった。
 *
 * ★★★ なぜ `hd_move_to_batch` では足りないか:
 *   あちらは「**いま**ここへ行け」であり、**到達駆動**である。ショーは
 *   **時間駆動** ——「**t 秒のときにここに居ろ**」でなければ、機体ごとの遅れが
 *   そのまま隊形の崩れになる（C13 実測: 1 m/s の移動で約 4 m 遅れ、25 席のうち 18 席しか埋まらない）。
 *   ★★★★ **参照するのは FC 自身の時計**である（`hd_fc_time`）。時計がずれていれば実行もずれる ——
 *     それが実機の姿なので、わざとそうしてある。
 *
 * ★ 使い方（1 回だけ配る。毎フレーム呼ぶものではない）:
 *     hd_traj_point pts[N] = { {0.0, 0,0,10, 0}, {5.0, 10,0,10, 90}, ... };
 *     hd_load_trajectory(h, i, pts, N, hd_fc_time(h, i));   // いまから始める
 *
 * ★★ 座標は **hd_state と同じ規約**（x=北 / y=東 / **z は上が正** / yaw は度）。
 *   ★ 内部の NED（z 下正・rad）への変換は口の中でやる。
 */

/* 時刻つき軌道の 1 点。 */
typedef struct {
    double t;              /* ショー開始からの経過時刻 [s]（★ 昇順に並べること） */
    double x, y, z;        /* 位置 [m]（★ z は上が正 ＝ hd_state と同じ） */
    double yaw_deg;        /* 方位 [deg] */
} hd_traj_point;

/* 機体 `idx` に時刻つき軌道を渡し、ショー（`FcMode::Show`）へ入れる。
 * @param start_time_s 軌道の t=0 に対応する **FC の時刻**（`hd_fc_time` の値）。
 * @return 受け取った点数（0 以上）／引数が不正なら HD_ERROR。
 * ★ `count == 0` は「軌道を消す」＝ `hd_clear_trajectory` と同じ。 */
HD_API int     hd_load_trajectory(hd_sim* h, int idx, const hd_traj_point* pts, int count,
                                  double start_time_s);

/* 軌道を消してホールドへ戻す。★ ショーの途中で抜けるときに使う。 */
HD_API int     hd_clear_trajectory(hd_sim* h, int idx);

/* 軌道を持っているか（1 / 0）。★ 終端に達したかは `hd_trajectory_done`。 */
HD_API int     hd_has_trajectory(hd_sim* h, int idx);

/* 軌道の終端に達したか（1 / 0）。★ 達しても最後の点で止まり続ける（勝手に降りない）。 */
HD_API int     hd_trajectory_done(hd_sim* h, int idx);

/* ★★★ 機体 `idx` の **FC 自身の時計** [s]。★ 軌道の起点はこれで指定する。
 *   ★★ 真の時刻（`hd_time`）とは**ずれる**（個体ごとの時計の誤差を持たせてあるため）。 */
HD_API double  hd_fc_time(hd_sim* h, int idx);

HD_API int     hd_takeoff(hd_sim* h, int idx, double alt_m);
HD_API int     hd_land(hd_sim* h, int idx);
HD_API int     hd_move_to(hd_sim* h, int idx, double x, double y, double z, double yaw_deg, double tol);
HD_API int     hd_move_by(hd_sim* h, int idx, double dx, double dy, double dz, double tol);
HD_API int     hd_set_yaw(hd_sim* h, int idx, double yaw_deg);
HD_API int     hd_set_velocity(hd_sim* h, int idx, double vx, double vy, double vz,
                        double yaw_rate_deg, double seconds);
HD_API int     hd_set_positioning(hd_sim* h, int idx, const char* mode);

/* ★★★★ E12 / D8: 教材から「風の日」と「GNSS が無いとき」を見せる（2026-08-29）。
 *
 * ★★★ **内蔵 FC を作ると決めた理由が教育（Scratch / Python）だった**のに、
 *   C5 の風は `swarm_app` からしか使えず、Python API には出ていなかった。
 *
 * hd_set_wind … 風を設定する。`speed_mps = 0` で風なし（既定）。
 *   `dir_deg` は**吹いてくる**方位（0 = 北風）。`turbulence` は乱れ強度（開けた土地 0.10〜0.20）。
 *   ★ `hd_step` を回す前でも後でもよい（次のステップから効く）。
 * hd_set_estimation … FC が真値を見るかどうか。**0 = 真値 / 1 = 推定**。
 *   ★★ `load()` の後では測位モードを変えられないので、**測位は `hd_open` の前に決める**
 *     （Python 側は `Sim(..., positioning=...)`）。ここで切り替えるのは姿勢だけ。
 */
HD_API int     hd_set_wind(hd_sim* h, double speed_mps, double dir_deg, double turbulence);
HD_API int     hd_set_estimation(hd_sim* h, int estimate_attitude);

/* ★★★★ 2026-09-01: **機体ごとの仕事を何スレッドで回すか**（ドローンショーの N 機むけ）。
 *   既定は 1 ＝ 従来どおり。★★★ **答えは 1 ビットも変わらない**
 *   （読むのは MuJoCo の read-only な状態、書くのは自分の機体の 6 自由度だけ）。
 *   ★★ センサ雑音を有効にしているときは、観測側だけ自動で 1 スレッドに戻る
 *     （共有の乱数列を引くため。並列にすると同じ引数で結果が変わってしまう）。
 *   ★★★★ **物理コア数を超えて指定しないこと** —— 4 コアの機械に 8 スレッドを与えると
 *     1 スレッドより遅くなる（実測 1.75 → 3.44 ms/step）。
 *   ★ 200 機で効く。25 機以下では待ち合わせのぶん損になることがある。 */
HD_API int     hd_set_threads(hd_sim* h, int threads);

/* ★ 非同期版（指示だけ出して即返る）。**複数機を同時に動かす**ときに使う。
 *   指示を全機に出してから hd_step() で時間を進める、という順番になる。 */
HD_API int     hd_takeoff_async(hd_sim* h, int idx, double alt_m);
HD_API int     hd_move_to_async(hd_sim* h, int idx, double x, double y, double z, double yaw_deg);
HD_API int     hd_land_async(hd_sim* h, int idx);
/* 目標に着いているか（1 = 着いた）。非同期版の完了判定に使う。 */
HD_API int     hd_arrived(hd_sim* h, int idx, double x, double y, double z, double tol);

/* =====================================================================
 * ★ C6: RC 手動操縦（プロポ／ゲームパッド）
 *
 * ★ 役割分担（2026-08-18 にユーザ確定した「案 2」）:
 *     デバイスを読むのは **Python 側**（pygame ＝ SDL2 / hidapi）。
 *     C 側はスティックの値を受け取るだけで、**OS にも接続方式にも依存しない**。
 *     Python プロセスの中でシムが動いているので、**通信も箱庭も PDU も要らない**。
 *
 * ★ 使い方（毎フレーム）:
 *     hd_rc_update(h, 0, axis, 6, button, 16, 1);
 *     hd_step(h, 1.0/60.0);
 *   実時間で飛ばしたいときは、呼び出し側が 1/60 秒ぶん待つ（Python 側のペーサ）。
 * ===================================================================== */

/* 飛行モード。実機のフライトモードに対応させてある。 */
#define HD_RC_ATTI 0   /* 角度モード。スティック=傾き、スロットル=推力そのもの（練習用）*/
#define HD_RC_ALT  1   /* 高度保持。スロットル中立で高度キープ */
#define HD_RC_GPS  2   /* 位置モード。スティック中立でその場に止まる（初学者向け）*/

typedef struct {
    double max_roll_deg;      /* ATTI / ALT のロール上限 [deg] */
    double max_pitch_deg;     /* 同ピッチ */
    double max_yaw_rate_deg;  /* ヨーレート上限 [deg/s] */
    double max_vel_xy;        /* GPS の水平速度上限 [m/s] */
    double max_climb;         /* ALT / GPS の昇降速度上限 [m/s] */
    int    throttle_self_center; /* 1 = ゲームパッド（中立=ホバー）/ 0 = プロポ（最下=停止）*/
    double throttle_span;     /* self_center のとき ±1 で比推力を何割振るか */
    double throttle_abs_max;  /* プロポのとき最上で重力の何倍を出すか */
    double deadzone;          /* 中立の不感帯 */
    double expo;              /* 0=線形 … 1=3乗 */
    double timeout_s;         /* この時間入力が来なければ「見失った」*/
    double lost_land_s;       /* 見失ってから自動着陸までの猶予 */
    double arm_gesture_s;     /* スロットル最下＋ヨー最大の保持時間 */
    double arm_gesture_stick; /* ★ ジェスチャのしきい値（ゲームパッドの丸ゲート対策で 0.30）*/
    double takeoff_alt;       /* 離陸ボタンで上がる高度 [m] */
} hd_rc_config;

typedef struct {
    int    enabled;      /* RC 操縦が有効か */
    int    armed;        /* アーム済みか */
    int    manual;       /* いま手動操縦中か */
    int    lost;         /* 送信機を見失っているか（RC フェイルセーフ）*/
    int    mode;         /* HD_RC_ATTI / HD_RC_ALT / HD_RC_GPS */
    double roll, pitch, yaw, thr;  /* 不感帯・エキスポ適用後のスティック（HUD 用）*/
    double gesture;      /* アーム／ディスアームのジェスチャの進み具合 0..1 */
    int    gesture_dir;  /* +1 = アーム側 / -1 = ディスアーム側 / 0 = なし */
} hd_rc_info;

HD_API int hd_rc_set_config(hd_sim* h, int idx, const hd_rc_config* c);
HD_API int hd_rc_get_config(hd_sim* h, int idx, hd_rc_config* c);
HD_API int hd_rc_enable(hd_sim* h, int idx, int on);
HD_API int hd_rc_set_mode(hd_sim* h, int idx, int mode);
/* 1 フレームぶんの入力を入れる。axis は 6 本・button は 16 個（箱庭の
 * hako_msgs/GameControllerOperation と同じ形）。
 * ★ connected=0 は「繋がっていない」であって「中立」ではない。
 *   0 を渡すとフェイルセーフが働く。ここを 1 のまま中立を流し続けると、
 *   送信機が死んでも機体は気づかない（実機で最も危ない作り込み）。 */
HD_API int hd_rc_update(hd_sim* h, int idx, const double* axis, int naxis,
                 const int* button, int nbutton, int connected);
HD_API int hd_rc_get_info(hd_sim* h, int idx, hd_rc_info* out);

/* =====================================================================
 * ★★★★ EDU7（2026-08-30）: **教育側の C# が必要とする 2 つの口**。
 *
 * ★★★ 教育アプリ（`education-godot-drone`）は drone-pro の DLL を 18 関数使っている。
 *   そのうち **16 は既存の `hd_*` で置き換えられた**が、**2 つだけ我々に無かった**:
 *     * **プロペラを回す**ための**ロータ指令**（`EduDronePlayer.cs` が毎フレーム引く）
 *     * **HUD の電池表示**のための**端子電圧**（`hd_state.battery` は残量だけ）
 *
 * ★★ **飛行そのものには 1 ミリも影響しない**（どちらも読み出しだけ）。
 * ===================================================================== */

/** ロータへの正規化指令 [0..1] を読み出す（**プロペラの描画用**）。
 *
 * @param out  書き込み先。`n` 本ぶんまで書く。
 * @return ★ **この機体のロータ総数**（負なら失敗）。★★ `n` が足りなくても総数を返すので、
 *         **`hd_get_controls(h, i, NULL, 0)` で本数だけ先に聞ける**。
 * ★ 4 発機なら 4、drone3 なら 8。**機体ごとに違う**（内蔵 FC は N 本汎用）。 */
HD_API int hd_get_controls(hd_sim* h, int idx, double* out, int n);

/** ★ 電池の状態。★★ `hd_state.battery`（残量 0..1）より詳しいものが要るとき。
 *  ★★★ **温度は持っていない**（モデル化していないので、無いものは返さない）。 */
typedef struct {
    double full_voltage;      /* 満充電の開回路電圧 [V] */
    double voltage;           /* ★ いまの端子電圧 [V]（負荷での降下を含む） */
    double nominal_voltage;   /* 公称電圧 [V] */
    double remaining;         /* ★ **使える容量**に対する残量 [0..1]（hd_state.battery と同じ） */
    double soc;               /* ★ **全容量**に対する充電状態 [0..1] */
    int    empty;             /* 1 = 使い切った */
} hd_battery;

HD_API int hd_get_battery(hd_sim* h, int idx, hd_battery* out);

/** ★★★★ EDU7（2026-08-30）: **外から力積を与える**（Quest3 で叩く・壁にぶつかる・鳥が当たる）。
 *
 * ★★★★ **これは「ゲームの罰」ではなく物理の口である。**
 *   叩かれた機体は本当に押され、IMU がそう答え、FC が自分で立て直す。
 *   ★★★ **制御を止めるような細工はしない**（実機に無い挙動を教材に持ち込まないため）。
 *
 * @param ix,iy,iz  力積 [N·s]。★ 座標は **hd_state と同じ**（x=北 / y=東 / **z=上**）。
 * @param ox,oy,oz  ★★★ **機体中心からの接触点のオフセット** [m]（世界の向き・z=上）。
 *                  **`0,0,0` なら回転しない**（重心を突く）。ずらすほど機体が回る。
 *                  ★ 呼ぶ側は「接触点 − `hd_get_state` の位置」を渡せばよい。
 * @param duration_s 効かせる時間 [s]。★ **0 以下なら 1 物理ステップ ＝ 打撃**。
 *                  壁に押しつけるような持続的な力なら、その秒数を渡す。
 * @return HD_OK / HD_ERROR。
 *
 * ★★ **重ねて呼ぶと上書き**する（足し込まない）—— 1 回の衝突は 1 回の打撃。
 * ★ **風は別の口**（`hd_set_wind`）。あちらは場であって、こちらは 1 回の出来事である。
 * ★★★ **障害物が MuJoCo の世界に在る場合は、この口は要らない** —— MuJoCo が接触を解く。
 *   この口が要るのは **当たり判定が Godot 側にしか無いとき**（XR の手・Godot で置いた壁）。 */
HD_API int hd_apply_impulse(hd_sim* h, int idx,
                            double ix, double iy, double iz,
                            double ox, double oy, double oz,
                            double duration_s);

/* ★ D2: 飛んだ軌跡を記録する（教材で「書いて・飛んで・**見る**」を成立させるため）。
 *   記録した CSV は `replay_viz_app` が箱庭 PDU に流し、Godot で再生できる。
 *   ★ Python 側は API がループを持つので、箱庭のアセット周期に乗せられない。
 *     そこで **記録 → 再生**の形にした（教材としても「走らせてから見る」で自然）。 */
HD_API int     hd_record_start(hd_sim* h, const char* path, double rate_hz);
HD_API int     hd_record_stop(hd_sim* h);

#ifdef __cplusplus
}
#endif
#endif /* HAKODRONE_H */
