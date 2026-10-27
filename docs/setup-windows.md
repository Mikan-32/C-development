# Windows での開発環境の構築

日常の開発は Windows ネイティブで行います（理由は [ADR-0003](adr/0003-windows-native-development.md)）。上から順に進めてください。

## 1. Git と Git LFS

1. [Git for Windows](https://gitforwindows.org/) を入れます（Git LFS も一緒に入ります）。
2. PowerShell で次を実行します。
   ```powershell
   git config --global core.autocrlf false
   git lfs install
   git config --global user.name "あなたの名前"
   git config --global user.email "GitHub に登録したメールアドレス"
   ```

`core.autocrlf` を `false` にするのは、改行コードの扱いをリポジトリの `.gitattributes` に任せるためです。

## 2. Visual Studio Build Tools（C コンパイラ）

1. [Visual Studio のダウンロードページ](https://visualstudio.microsoft.com/ja/downloads/)から「Build Tools for Visual Studio」を入れます。
2. インストーラーで次を選びます。
   - ワークロード：**C++ によるデスクトップ開発**
   - 個別のコンポーネント：**C++ Clang Compiler for Windows** と **C++ CMake tools for Windows**（日本語表示では「Windows 用 C++ Clang コンパイラ」「Windows 用 C++ CMake ツール」）

コンパイラは MSVC ではなく clang-cl を使います。Android NDK と同じ clang 系にそろえるためです。

## 3. Android Studio

1. [Android Studio](https://developer.android.com/studio) を入れます。
2. **SDK Manager** で次を入れます。
   - SDK Platforms：**Android 16（API 36）**
   - SDK Tools：**NDK (Side by side)** の 28 以降と、**CMake** の 3.31 系
3. 環境変数 `ANDROID_NDK` に、入れた NDK のフォルダを設定します（`<版>` は実際のフォルダ名に置き換えます）。
   ```powershell
   [Environment]::SetEnvironmentVariable("ANDROID_NDK", "$env:LOCALAPPDATA\Android\Sdk\ndk\<版>", "User")
   ```

## 4. VS Code

1. [VS Code](https://code.visualstudio.com/) を入れます。
2. このリポジトリのフォルダを開くと、おすすめの拡張機能（clangd、CMake Tools など）を入れるよう案内が出るので、すべて入れます。

Microsoft 製の C/C++ 拡張の補完機能（IntelliSense）は clangd と競合するため、このリポジトリでは `.vscode/settings.json` で無効にしています。

## 5. mise（ツールの版をそろえる）

1. mise を入れます。
   ```powershell
   winget install jdx.mise
   ```
2. mise のツールがどこからでも使えるように、shims フォルダを PATH に追加します。PowerShell を開き直してから次を実行します。
   ```powershell
   $shims = "$env:LOCALAPPDATA\mise\shims"
   $userPath = [Environment]::GetEnvironmentVariable("Path", "User")
   [Environment]::SetEnvironmentVariable("Path", "$shims;$userPath", "User")
   ```
   shims を PATH に入れておくと、VS Code や Git フックからも、このリポジトリで決めた版のツールが使われます。
3. PowerShell を開き直し、リポジトリのフォルダで次を実行します。
   ```powershell
   mise trust
   mise install
   ```
   `mise.toml` に書かれた CMake、Ninja、lefthook、clang-format、clang-tidy が、決められた版で入ります。

> **最初に試す人へ**：clang-format と clang-tidy は、mise が uv を使って PyPI から入れます。Windows ではこの部分で問題が出ることがあります。うまくいかなかったら Issue に記録して、チームで対処を決めてください。

## 6. lefthook（コミット前の自動整形）

リポジトリのフォルダで 1 回だけ実行します。

```powershell
lefthook install
```

以後、コミットのたびに clang-format が自動で実行されます。

## 7. ビルドとテスト

**VS Code から**：下のステータスバーで構成プリセット `windows-clang` を選び、ビルドとテストを実行します。

**コマンドから**：スタートメニューの **Developer PowerShell for VS** を開いて実行します（clang-cl が Windows SDK を見つけるために、この環境が必要です）。

```powershell
cmake --preset windows-clang
cmake --build --preset windows-clang
ctest --preset windows-clang
```

## 8. WSL2（必要な人だけ）

UBSan や Valgrind を手元で使いたいときや、CI でだけ失敗するテストを再現したいときに使います。

1. Ubuntu 24.04 を WSL2 に入れます。
2. **WSL の中に別のクローンを作ります**（Windows 側の作業フォルダを `/mnt/c` から使うと、とても遅くなります）。
   ```bash
   sudo apt-get update && sudo apt-get install -y git git-lfs clang libclang-rt-18-dev cmake ninja-build
   cd ~ && git clone --recurse-submodules https://github.com/Mikan-32/C-development.git
   cd C-development
   cmake --preset linux-sanitize && cmake --build --preset linux-sanitize && ctest --preset linux-sanitize
   ```

## うまくいかないとき

| 症状 | 確認すること |
|---|---|
| `cmake --preset windows-clang` で clang-cl が見つからない | Developer PowerShell for VS で実行しているか。Build Tools に Clang のコンポーネントが入っているか |
| 全行が差分として表示される | `git config --global core.autocrlf false` を実行したか。その後 `git add --renormalize .` |
| `mise install` で clang-format が入らない | 上の「最初に試す人へ」を参照し、Issue に記録する |
