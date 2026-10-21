# OBJ Loader

添付 PDF のサンプルに合わせた C++ コンソールアプリです。
cube.obj と sphere.obj を読み込み、位置・白色の頂点カラー・UV・法線・接線と、
三角形ごとのインデックスをすべて表示します。3D 描画は行いません。

## 実行方法

1. `objLoader.sln` を Visual Studio 2022 で開きます。
2. 構成を `Debug`、プラットフォームを `x64` にします。
3. `Ctrl + F5` でビルドして実行します。

必要な環境は「C++ によるデスクトップ開発」、MSVC v143、Windows SDK です。
この PC では既存の開発環境を使用できます。
DirectXMath は Windows SDK に含まれるため、追加の DirectX SDK は不要です。
tinyobjloader とモデルは同梱済みで、実行時のダウンロードはありません。

ビルド時に実行ファイルの横へ `Models` フォルダーを自動コピーします。
引数を省略すると cube、sphere の順に読み込みます。
独自の OBJ を読み込む場合は、引数にパスを指定できます。

```powershell
.\x64\Debug\objLoader.exe
.\x64\Debug\objLoader.exe Models\cube.obj
.\x64\Debug\objLoader.exe > result.txt
```

四角形などの面は tinyobjloader で三角形に変換し、各面の頂点を展開します。
添付 cube は 36 頂点・36 インデックス・12 三角形になります。
添付 sphere は 336 頂点・336 インデックス・112 三角形になります。
UV の V は DirectX 用に `1 - v` へ変換します。
UV がない場合は `(0, 0)`、法線がない場合は `(0, 0, 1)` を使います。
UV の退化時には法線と直交する接線を補い、ゼロ除算を避けています。
読み込みに失敗した場合は標準エラーへ理由を出力し、終了コード 1 を返します。

## 外部ライブラリ

- 配布元: https://github.com/tinyobjloader/tinyobjloader
- 取得した release ブランチのコミット: `45636bdcef1a4fec140346b90c0b50bf0bc3e23b`
- ファイル: `tiny_obj_loader.h`
- ライセンス: MIT (`tinyobjloader-LICENSE.txt` を同梱)

`TINYOBJLOADER_IMPLEMENTATION` は `objLoader.cpp` だけで定義しています。

## 確認済みの内容

- Visual Studio 2022 / x64 の Debug・Release ビルド成功。
- 添付モデルの頂点属性、三角形数、インデックス、接線の長さと直交性を確認。
- 複数オブジェクト、負のインデックス、UV・法線省略時の読み込みを確認。
- 空ファイル、不正なインデックス、存在しないファイルのエラー処理を確認。
- 作業フォルダーが異なる場所から起動しても、同梱モデルを読み込み可能。
