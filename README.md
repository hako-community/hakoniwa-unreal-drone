# Hakoniwa Unreal Drone

このリポジトリは、箱庭ドローンシミュレータのビジュアライズ機能を提供する Unreal Engine プロジェクトです。
Unreal Engine 5.6 を使用し、WebSocket または共有メモリ経由で取得した PDU 情報からドローンの状態を描画します。


## セットアップ

1. 本リポジトリをクローンします。
   ```bash
   git clone <repository_url>
   ```
2. Unreal Engine 5.6 以降をインストールします。
3. `HakoniwaDrone.uproject` を Unreal Editor で開きます。
4. 必要な外部依存を配置し、ビルドを実行してプロジェクトを起動します。

## 外部依存

このリポジトリには、以下の外部コンポーネント本体およびネイティブバイナリは含めていません。

- `hakoniwa-pdu-unreal`
- `shakoc`
- `hako_service_c`

`HakoniwaDrone.uproject` では `HakoniwaPdu` プラグインを有効化しているため、ビルド時には `hakoniwa-pdu-unreal` を `Plugins/HakoniwaPdu` に配置する必要があります。`shakoc` と `hako_service_c` はネイティブライブラリとして別途配置が必要です。

配置先の詳細は [docs/dependencies.md](docs/dependencies.md) を参照してください。依存物の取得元、ビルド方法、バージョン固定方法は今後のビルド手順で整理します。

## 対応モード

- WebSocket 連携: `AHakoniwaWebClient` と `BP_HakoniwaWebClient` が PDU 通信を管理します。
- 共有メモリ連携: `AHakoniwaShmClient` と `BP_HakoniwaShmClient` が箱庭コアとの同期を管理します。
- ローカル RC service 連携: `Plugins/HakoniwaDroneService` が `hako_service_c` の RC API を Unreal から呼び出し、`UDroneServiceVisualizerComponent` が機体状態と表示を連携します。

## 利用可能な Level

- `AvatarWeb.umap`  — ドローンを Web 経由で制御する際に使用するレベルです。
- `AvatarWeb-2.umap`  — Web 経由の 2 機体構成向けレベルです。
- `AvatarShm.umap`  — 共有メモリ経由で箱庭コアと連携するレベルです。

## 主要な Blueprint / コンポーネント

- `BP_HakoniwaAvatar` — ドローン本体を表すブループリント。
- `BP_HakoniwaWebClient` — WebSocket 通信と PDU 管理を行うブループリント。
- `BP_HakoniwaShmClient` — 共有メモリ通信と PDU 管理を行うブループリント。
- `WBP_SimStart` — シミュレーション操作用のウィジェット。
- `UDroneServiceVisualizerComponent` — ローカル RC service の状態取得、入力反映、表示更新を行う C++ コンポーネント。

Blueprint は `Content/Blueprints` フォルダ内に配置されています。

## コンフィグレーション

JSON 形式の実行時設定は `Content/Config` に集約しています。クライアントやコンポーネントの `Config/...` というパス指定は、Unreal の `Content` ディレクトリからの相対パスとして解決されます。

- `Content/Config/webavatar.json`
- `Content/Config/webavatar-2.json`
- `Content/Config/drone_config.json`
- `Content/Config/avatar-drone.json`
- `Content/Config/sharesim-drone.json`
- `Content/Config/drone/**`
- `Content/Config/controller/**`

主な設定ファイルの役割は以下です。

- `webavatar.json` — 1 機体構成の WebSocket/共有メモリ連携向け PDU 定義。
- `webavatar-2.json` — 2 機体構成の WebSocket/共有メモリ連携向け PDU 定義。
- `avatar-drone.json` — 基本的なドローン avatar 用 PDU 定義。
- `sharesim-drone.json` — ShareSim、複数ドローン、共有オブジェクトを含むサンプル PDU 定義。
- `drone_config.json` — ドローン表示・挙動向けの基本設定。
- `drone/rc/drone_config_0.json` — `UDroneServiceVisualizerComponent` が `hako_service_c` の `InitSingle` で読むドローン設定。
- `controller/param-api-mixer.txt` — `UDroneServiceVisualizerComponent` が `hako_service_c` の `InitSingle` で読むコントローラ設定。

## コードの概要

本プロジェクトでは `AHakoniwaWebClient` または `AHakoniwaShmClient` が PDU 通信を管理し、`AHakoniwaAvatar` にドローンの位置と回転を反映させます。`UDronePropellerComponent` は各プロペラを回転させ、モーターの回転数を視覚化します。

`Plugins/HakoniwaDroneService` は、`hako_service_c` の RC API を Unreal から呼び出すための薄い wrapper です。現時点では Win64 のみを対象にしており、`hako_service_c` の実体はこのリポジトリには含めません。

## ライセンス

本リポジトリは MIT License の下で公開されています。詳細は `LICENSE` ファイルを参照してください。
