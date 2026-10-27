#!/usr/bin/env bash
# 外部ライブラリを git submodule として追加し、決めたタグに固定する（WSL2・Linux・macOS 用）
# 使い方：リポジトリ直下で bash scripts/add-submodules.sh
# 版を変えるときは、このファイル・add-submodules.ps1・third_party/README.md・docs/tech-stack.md をそろえて更新する
set -euo pipefail
cd "$(dirname "$0")/.."

libs=(
  "third_party/SDL       https://github.com/libsdl-org/SDL.git        release-3.4.18"
  "third_party/lvgl      https://github.com/lvgl/lvgl.git             v9.6.0"
  "third_party/sqlcipher https://github.com/sqlcipher/sqlcipher.git   v4.19.0"
  "third_party/curl      https://github.com/curl/curl.git             curl-8_23_0"
  "third_party/cJSON     https://github.com/DaveGamble/cJSON.git      v1.7.19"
  "third_party/Unity     https://github.com/ThrowTheSwitch/Unity.git  v2.7.0"
)

for entry in "${libs[@]}"; do
  read -r path url tag <<<"$entry"
  if [ -e "$path" ]; then
    echo "スキップ（すでにあります）：$path"
    continue
  fi
  echo "追加：$path @ $tag"
  git submodule add --depth 1 "$url" "$path"
  git -C "$path" fetch --depth 1 origin tag "$tag"
  git -C "$path" checkout "$tag"
  git config -f .gitmodules "submodule.$path.shallow" true
  git add "$path" .gitmodules
done

echo
echo "完了しました。git status で内容を確認してからコミットしてください。"
