#!/usr/bin/env python3
"""src/ui/ の文字列と AI-5 仕様書から使用文字を抽出し、Noto Sans JP サブセットを生成する"""
from pathlib import Path
import re
import subprocess
import sys

ROOT = Path(__file__).resolve().parent.parent
UI_DIR = ROOT / "src" / "ui"
AI5_MD = ROOT / "docs" / "spec" / "ai" / "AI-5_feedback.md"
OUT_TXT = ROOT / "assets" / "fonts" / "subset_chars.txt"
SRC_TTF = ROOT / "assets" / "fonts" / "NotoSansJP-Regular.ttf"
OUT_TTF = ROOT / "assets" / "fonts" / "NotoSansJP-Subset.ttf"


def collect_chars() -> str:
    chars = set(
        "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz"
        " !\"#$%&'()*+,-./:;<=>?@[\\]^_`{|}~。、・！？「」『』（）［］【】"
        "〜…―ー−＋×÷％／：；＜＞＝→←↑↓○●△▲□■◇◆★☆※〒々〆"
        "月火水木金土日年時分秒点回夜朝日週"
    )

    # src/ui/ 内の文字列リテラルを抽出
    for path in sorted(UI_DIR.glob("*.[ch]")):
        text = path.read_text(encoding="utf-8", errors="ignore")
        for lit in re.findall(r'"((?:\\.|[^"\\])*)"', text):
            chars.update(lit)

    # AI-5 の文型から日本語・記号を抽出
    if AI5_MD.exists():
        chars.update(AI5_MD.read_text(encoding="utf-8"))

    # 制御文字を除外してソート
    filtered = sorted(c for c in chars if ord(c) >= 0x20)
    return "".join(filtered)


def main() -> int:
    text = collect_chars()
    OUT_TXT.parent.mkdir(parents=True, exist_ok=True)
    OUT_TXT.write_text(text, encoding="utf-8")
    print(f"[Subset] 抽出文字数: {len(text)} 文字 -> {OUT_TXT}")

    if not SRC_TTF.exists():
        print(f"[Subset] 元フォント未配置のため文字リストのみ出力しました: {SRC_TTF}")
        return 0

    cmd = [
        sys.executable,
        "-m",
        "fontTools.subset",
        str(SRC_TTF),
        f"--text-file={OUT_TXT}",
        f"--output-file={OUT_TTF}",
        "--layout-features=*",
    ]
    res = subprocess.run(cmd, check=False)
    if res.returncode == 0:
        print(f"[Subset] サブセットフォント生成完了: {OUT_TTF} ({OUT_TTF.stat().st_size:,} bytes)")
    else:
        print("[Subset] fonttools が未導入の場合は `pip install fonttools` 後に再実行してください")
    return res.returncode


if __name__ == "__main__":
    raise SystemExit(main())
