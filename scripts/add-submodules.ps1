# 外部ライブラリを git submodule として追加し、決めたタグに固定する
# 使い方：リポジトリ管理者が最初に 1 回だけ、リポジトリ直下で .\scripts\add-submodules.ps1 を実行する
# 版を変えるときは、このファイル・third_party/README.md・docs/tech-stack.md をそろえて更新する

$ErrorActionPreference = "Stop"
Set-Location (Join-Path $PSScriptRoot "..")

function Invoke-Git {
    git @args
    if ($LASTEXITCODE -ne 0) { throw "git $args が失敗しました" }
}

$libs = @(
    @{ Path = "third_party/SDL";       Url = "https://github.com/libsdl-org/SDL.git";       Tag = "release-3.4.18" },
    @{ Path = "third_party/lvgl";      Url = "https://github.com/lvgl/lvgl.git";            Tag = "v9.6.0" },
    @{ Path = "third_party/sqlcipher"; Url = "https://github.com/sqlcipher/sqlcipher.git";  Tag = "v4.19.0" },
    @{ Path = "third_party/curl";      Url = "https://github.com/curl/curl.git";            Tag = "curl-8_23_0" },
    @{ Path = "third_party/cJSON";     Url = "https://github.com/DaveGamble/cJSON.git";     Tag = "v1.7.19" },
    @{ Path = "third_party/Unity";     Url = "https://github.com/ThrowTheSwitch/Unity.git"; Tag = "v2.7.0" }
)

foreach ($lib in $libs) {
    if (Test-Path $lib.Path) {
        Write-Host "スキップ（すでにあります）：$($lib.Path)"
        continue
    }
    Write-Host "追加：$($lib.Path) @ $($lib.Tag)"
    Invoke-Git submodule add --depth 1 $lib.Url $lib.Path
    Invoke-Git -C $lib.Path fetch --depth 1 origin tag $lib.Tag
    Invoke-Git -C $lib.Path checkout $lib.Tag
    Invoke-Git config -f .gitmodules "submodule.$($lib.Path).shallow" true
    Invoke-Git add $lib.Path .gitmodules
}

Write-Host ""
Write-Host "完了しました。git status で内容を確認してからコミットしてください。"
