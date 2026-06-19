# External Dependencies

このプロジェクトは、公開リポジトリに含めない外部コンポーネントとネイティブバイナリを前提にしています。

## 公開リポジトリに含めないもの

- `Plugins/HakoniwaPdu`
- `Binaries/Win64/shakoc.dll`
- `Binaries/Win64/hako_service_c.dll`
- `Plugins/HakoniwaDroneService/Source/ThirdParty/hako_service_c/lib/Win64/hako_service_c.lib`
- `Plugins/HakoniwaPdu/Source/ThirdParty/shakoc/lib/Win64/shakoc.lib`

## 期待する配置

ビルド前に、外部から取得またはビルドした依存物を以下の位置に配置します。

```text
Plugins/
  HakoniwaPdu/
    Source/
      ThirdParty/
        shakoc/
          lib/
            Win64/
              shakoc.lib
  HakoniwaDroneService/
    Source/
      ThirdParty/
        hako_service_c/
          include/
            ...
          lib/
            Win64/
              hako_service_c.lib
Binaries/
  Win64/
    shakoc.dll
    hako_service_c.dll
```

`HakoniwaDrone.Build.cs` は `shakoc.lib` と `Binaries/Win64/shakoc.dll` を参照します。`HakoniwaDroneService.Build.cs` と `FHakoDroneServiceRc` は `hako_service_c.lib` と `Binaries/Win64/hako_service_c.dll` を参照します。

## 現時点の制約

`HakoniwaDroneService` は現在 Win64 向けの設定のみを持ちます。Win64 以外では `hako_service_c` のロードは無効です。

依存物の取得元、ビルド方法、バージョン固定方法は今後のビルド手順で整理します。
