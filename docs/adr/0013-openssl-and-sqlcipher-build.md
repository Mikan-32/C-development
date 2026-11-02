# 0013. OpenSSL は CI で作った配布物を使い、SQLCipher の amalgamation も CI で作る

- 状態：提案
- 日付：2026-10-31
- 関係：third_party/README.md（「ビルド方法を Issue で決めてください」）、docs/tech-stack.md（OpenSSL の版は未定）

## 背景

- SQLCipher（DB の暗号化）と libcurl（HTTPS）は、どちらも OpenSSL を使う（tech-stack.md）。
- OpenSSL を Windows でビルドするには Perl などが要り、Android 向けには NDK と Unix 系の環境が要る。メンバー全員の PC にこれらを入れるのは負担が大きい。
- SQLCipher は、ソースから `sqlite3.c`（amalgamation）を生成して使う。生成には Tcl が使われる（最近の SQLite は、同梱の JimTCL でも生成できる）。
- SQLCipher をビルドするときは、`SQLITE_HAS_CODEC`、`SQLITE_TEMP_STORE=2`（または 3）、`SQLITE_EXTRA_INIT=sqlcipher_extra_init`、`SQLITE_EXTRA_SHUTDOWN=sqlcipher_extra_shutdown` を定義し、暗号ライブラリ（OpenSSL など）をリンクする必要がある。

## 検討した案

| 案 | 内容 | 良い点 | 気になる点 |
|---|---|---|---|
| **A（推奨）** | CI（GitHub Actions）で OpenSSL のライブラリと SQLCipher の amalgamation を作り、GitHub Releases に置く。各自はスクリプトで取ってくる | 各自の PC に Perl・Tcl などが要らない。全員が同じものを使う | 最初にワークフローを書く手間がかかる。版を上げるときは CI を回す |
| B | vcpkg で OpenSSL（と SQLCipher）を入れる | ビルドの手順を自分で書かなくてよい | 各自の PC で最初のビルドに時間がかかる。submodule と仕組みが 2 つになる。Gradle との設定が増える |
| C | OpenSSL をやめ、curl は mbedTLS、SQLCipher は LibTomCrypt を使う | すべて CMake と clang だけでビルドできる | SQLCipher の LibTomCrypt 対応は使用例が少なく、LibTomCrypt は正式リリースが古い。tech-stack の変更が大きい |

## 決定

案 A とする。

| 項目 | 決定 |
|---|---|
| OpenSSL の版 | 3.5 系（長期サポート版）。静的ライブラリ |
| 作る対象 | Android の arm64-v8a と x86_64（NDK r28 以降、API 26）、Windows の x64（MSVC でビルドし、clang-cl からリンク） |
| Linux（CI のテスト） | Ubuntu の `libssl-dev` を使う |
| SQLCipher | v4.19.0 のタグから `sqlite3.c` と `sqlite3.h` を CI（Linux）で生成し、同じリリースに置く |
| 置き場所 | GitHub Releases（タグの例：`deps-2026.11.0`）。各ファイルの SHA-256 を記録する |
| 取得 | `scripts/fetch-deps.ps1`（と `.sh`）で `third_party/prebuilt/` に展開する（Git の管理外）。CMake は見つからないときに、このスクリプトを案内して止まる |
| SQLCipher の組み込み | CMake の静的ライブラリ `nk_sqlcipher` とし、上の 4 つの定義に加えて `SQLITE_THREADSAFE=1`（シリアライズ。platform-boundary.md の 6 章）と、OpenSSL を使う設定を付ける |
| curl | submodule から CMake でビルドし、TLS に OpenSSL を使う |
| CA 証明書 | Mozilla の CA 証明書（curl が配布する `cacert.pem`）をアプリに同梱し、`CURLOPT_CAINFO_BLOB` で渡す |

**CA 証明書を同梱する理由**：Android の端末にある証明書のフォルダは、ファイル名の付け方が新しい OpenSSL の探し方と合わない。また Android 14 以降は置き場所も変わった。端末に頼らず同梱するほうが確実である。

## 結果

- メンバーは Perl も Tcl も入れずにビルドできる（Windows は clang-cl、Android は NDK だけ）。
- 最初の作業として、「外部ライブラリの取得とビルド設定」の実装 Issue を立てる（CI のワークフロー、取得スクリプト、CMake への組み込み）。CS-3（DB）と CS-6（カーテン）は、これが終わってから始められる。
- OpenSSL のセキュリティ更新が出たら、ワークフローを回してリリースを作り直し、tech-stack.md と取得スクリプトの版・ハッシュを更新する。担当を決めておく。
- `cacert.pem` も古くなるので、リリースを作るたびに新しいものにする。
- third_party/README.md と tech-stack.md の「未定」を、この内容で更新する。
