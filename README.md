# Hakoniwa Unreal Drone

このリポジトリは、箱庭ドローンシミュレータのビジュアライズ機能を提供する Unreal Engine プロジェクトです。
Unreal Engine 5.8 を使用し、WebSocket または共有メモリ経由で取得した PDU 情報からドローンの状態を描画します。


## セットアップ

1. 本リポジトリをクローンします。
   ```bash
   git clone <repository_url>
   ```
2. Unreal Engine 5.8 をインストールします。
3. `HakoniwaDrone.uproject` を Unreal Editor で開きます。
4. 必要な外部SDKをインストールし、ビルドを実行してプロジェクトを起動します。

Windowsでは、環境変数または標準インストール先から外部依存物を検証し、Editorビルドを次のスクリプトで実行できます。

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\windows\setup_build.ps1
```

詳細とオプションは [docs/dependencies.md](docs/dependencies.md) を参照してください。

## 外部依存

このリポジトリには、以下の外部コンポーネント本体およびネイティブバイナリは含めていません。

- `shakoc`（共有メモリ連携）
- `hakodrone`（`hakodrone.dll` ＋ `mujoco.dll`・ローカル RC）と機体の定義 `SimModels/courses_drone`

`HakoniwaPdu` プラグインのソースは本リポジトリに含まれます。`shakoc` はネイティブライブラリとして別途インストールが必要です。
`hakodrone` は `native.lock.json` の版を `scripts/windows/fetch_native.ps1`（`setup_build.ps1` から呼ばれる）が [releases-channel](https://github.com/hako-community/releases-channel) から取得し、`ThirdParty/HakoDrone/Win64` と `SimModels/courses_drone` に置きます（git には入れません）。

環境変数、標準インストール先、フォールバック配置の詳細は [docs/dependencies.md](docs/dependencies.md) を参照してください。依存物の取得元、ビルド方法、バージョン固定方法は今後のビルド手順で整理します。

## 対応モード

- WebSocket 連携: `AHakoniwaWebClient` と `BP_HakoniwaWebClient` が PDU 通信を管理します。
- 共有メモリ連携: `AHakoniwaShmClient` と `BP_HakoniwaShmClient` が箱庭コアとの同期を管理します。
- ローカル RC（オフライン）: `Plugins/HakoniwaDroneService` が `hakodrone`（内蔵 FC ＋ MuJoCo の物理）を Unreal の中で動かし、`UDroneServiceVisualizerComponent` が機体状態と表示を連携します。箱庭に繋がずに Unreal 単体で飛ばせます。

## 利用可能な Level

- `AvatarWeb.umap`  — ドローンを Web 経由で制御する際に使用するレベルです。
- `AvatarWeb-2.umap`  — Web 経由の 2 機体構成向けレベルです。
- `AvatarShm.umap`  — 共有メモリ経由で箱庭コアと連携するレベルです。
- `AvatarLocal.umap`  — 箱庭に繋がず、`hakodrone` で Unreal の中だけで飛ばすレベルです（操作は下の「オフラインの操作」）。

## オフラインの操作（AvatarLocal）

PIE を始めたらビューポートを一度クリックします。START ボタンは箱庭（共有メモリ）用なので、オフラインでは押しません。

| 操作 | ゲームパッド | キーボード |
|---|---|---|
| アーム／ディスアーム（押すたびに切り替わる） | A（PS 系は ×） | Space |
| 上昇・下降 | 左スティック 上下 | W / S |
| ヨー | 左スティック 左右 | A / D |
| 前進・後退 | 右スティック 上下 | ↑ / ↓ |
| 左右 | 右スティック 左右 | ← / → |

* Xbox 型（XInput）のゲームパッドはそのまま使えます。
* それ以外の HID ゲームパッドは **GameInput** で読みます。Windows に Microsoft GameInput の再頒布が必要です（管理者の PowerShell で `winget install --id Microsoft.GameInput -e`）。
  割り当ては `Config/DefaultInput.ini` の `[/Script/GameInputBase.GameInputDeveloperSettings]` に機種（VID/PID）ごとに書きます。いまは HORIPAD mini4（PS4 互換）を登録しています。
* エディタが前面にいないあいだは入力を中立にします（GameInput は背景では全軸 0 を送るため）。

## 主要な Blueprint / コンポーネント

- `BP_HakoniwaAvatar` — ドローン本体を表すブループリント。
- `BP_HakoniwaWebClient` — WebSocket 通信と PDU 管理を行うブループリント。
- `BP_HakoniwaShmClient` — 共有メモリ通信と PDU 管理を行うブループリント。
- `WBP_SimStart` — シミュレーション操作用のウィジェット。
- `UDroneServiceVisualizerComponent` — ローカル RC の状態取得、入力反映、表示更新を行う C++ コンポーネント。
- `UDroneControlLocalInput` — ローカル RC 用に Unreal のゲームパッド・キーボードを直接読む C++ コンポーネント。

Blueprint は `Content/Blueprints` フォルダ内に配置されています。

## コンフィグレーション

JSON 形式の実行時設定は `Content/Config` に集約しています。クライアントやコンポーネントの `Config/...` というパス指定は、Unreal の `Content` ディレクトリからの相対パスとして解決されます。

- `Content/Config/webavatar.json`
- `Content/Config/webavatar-2.json`
- `Content/Config/drone_config.json`
- `Content/Config/avatar-drone.json`
- `Content/Config/sharesim-drone.json`

主な設定ファイルの役割は以下です。

- `webavatar.json` — 1 機体構成の WebSocket/共有メモリ連携向け PDU 定義。
- `webavatar-2.json` — 2 機体構成の WebSocket/共有メモリ連携向け PDU 定義。
- `avatar-drone.json` — 基本的なドローン avatar 用 PDU 定義。
- `sharesim-drone.json` — ShareSim、複数ドローン、共有オブジェクトを含むサンプル PDU 定義。
- `drone_config.json` — ドローン表示・挙動向けの基本設定。

## コードの概要

本プロジェクトでは `AHakoniwaWebClient` または `AHakoniwaShmClient` が PDU 通信を管理し、`AHakoniwaAvatar` にドローンの位置と回転を反映させます。`UDronePropellerComponent` は各プロペラを回転させ、モーターの回転数を視覚化します。

`Plugins/HakoniwaDroneService` は、`hakodrone` の C の口を実行時に読み込む（.lib をリンクしない）薄い wrapper です。関数名は旧 `hako_service_c` 版と同じ（`FHakoDroneServiceRc`）で、機体は `SimModels/courses_drone` を開きます。現時点では Win64 のみを対象にしています（Android は将来対応）。

## ライセンス

本リポジトリは MIT License の下で公開されています。詳細は `LICENSE` ファイルを参照してください。
