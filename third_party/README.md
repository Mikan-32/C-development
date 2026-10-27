# third_party/ — 外部ライブラリ

外部ライブラリは git submodule として置き、決めたタグに固定します。

| ライブラリ | フォルダ | 版（タグ） | ライセンス | 用途 |
|---|---|---|---|---|
| SDL3 | `SDL/` | `release-3.4.18` | zlib | 画面、タッチ、音の再生、Android の入口 |
| LVGL | `lvgl/` | `v9.6.0` | MIT | GUI 部品 |
| SQLCipher | `sqlcipher/` | `v4.19.0` | BSD 系 | 暗号化したデータベース |
| curl | `curl/` | `curl-8_23_0` | curl（MIT 系） | HTTPS 通信 |
| cJSON | `cJSON/` | `v1.7.19` | MIT | JSON の読み書き |
| Unity | `Unity/` | `v2.7.0` | MIT | C の単体テスト |
| OpenSSL | 未定 | 未定 | Apache-2.0 | 暗号と TLS（SQLCipher と curl で共用） |
| Noto Sans JP | 未定（アセットとして同梱） | — | SIL OFL 1.1 | 日本語フォント |

## 追加のしかた（リポジトリ管理者が最初に 1 回だけ）

```powershell
.\scripts\add-submodules.ps1
git status
git commit -m "build(third_party): 外部ライブラリを submodule として追加"
```

ほかのメンバーは、取得後に次を実行するだけです。

```powershell
git submodule update --init --recursive
```

## 組み込むときの注意

- **cJSON**：`CMakeLists.txt` の `cmake_minimum_required` が古い（3.0）ため、`add_subdirectory` は使わず、`cJSON.c` を直接ビルドに加えます。
- **SQLCipher**：リポジトリのソースからは、そのままでは使えません。`sqlite3.c`（amalgamation）を生成する必要があり、生成には Tcl が要ります。「CI や WSL2 で生成して置いておく」などの方法を Issue で決めてください。
- **OpenSSL**：Android と Windows 向けのビルドには Perl などが必要で、手間がかかります。「CI でビルドしてキャッシュを共有する」などの方法を Issue で決めてください。決まるまでは submodule に入れていません。
- **curl**：使うのは HTTPS だけなので、CMake のオプションで使わないプロトコルを外して小さくします（`HTTP_ONLY` など）。
- **LVGL**：設定ファイル `lv_conf.h` はリポジトリで管理します（置き場所は `src/ui/` を想定）。

## 版を上げるとき

1. PR を作り、対象のフォルダで新しいタグに切り替えます。
   ```powershell
   git -C third_party/lvgl fetch --depth 1 origin tag v9.x.x
   git -C third_party/lvgl checkout v9.x.x
   git add third_party/lvgl
   ```
2. この表と `docs/tech-stack.md` の版を更新します。

## ライセンス表示

ここにあるライブラリのライセンスに従い、アプリの中にオープンソースのライセンス表示（設定画面など）が必要です。リリース前に用意します。
