# 0012. Android のパッケージ名を io.github.mikan32.nemunoki にする

- 状態：提案
- 日付：2026-10-31
- 関係：android/README.md の手順 1（「パッケージ名はチームで決めて ADR に残す」）

## 背景

- パッケージ名（applicationId）は、端末の中でアプリを見分ける ID であり、Google Play に公開すると変えられない。
- 慣例では、自分たちが持っているドメインを逆にした名前を使う。チームは独自のドメインを持っていない。
- Kotlin のパッケージ名は、JNI の関数名にそのまま入る（`Java_パッケージ_クラス_メソッド`）。名前に `_` があると関数名では `_1` に置き換える必要があって間違えやすく、`-` は使えない。

## 決定

| 項目 | 値 |
|---|---|
| applicationId | `io.github.mikan32.nemunoki` |
| Gradle の namespace | `io.github.mikan32.nemunoki` |
| プラットフォーム層の Kotlin のパッケージ | `io.github.mikan32.nemunoki.platform` |
| デバッグ版 | `applicationIdSuffix = ".debug"`（リリース版と同じ端末に並べて入れられる） |
| アプリの表示名 | ねむのき（デバッグ版は「ねむのき（開発）」） |

- `io.github.<GitHub のユーザー名>` は、独自ドメインを持たない開発者がよく使う形。ユーザー名「Mikan-32」の `-` は使えないので外す。
- SDL の Java 部分は SDL のパッケージ（`org.libsdl.app`）のまま使い、自分たちのクラスだけを上の名前に置く。

## 結果

- JNI の関数名は、たとえば `Java_io_github_mikan32_nemunoki_platform_NativeBridge_nkOnScheduled` の形になる。
- 公開後は変えられないので、学校やチームでドメインを用意できる場合は、公開前に見直す。
- android/README.md の手順で Android Studio のプロジェクトを作るときに、この値を使う。
