"""AI-7 評価：合成データで、品質判定・段階推定・スコア・フィードバック・文言を確かめる。

使い方（リポジトリ直下で）：
    python tests/eval/evaluate.py              # 評価して tests/eval/out/ に結果を書く
    python tests/eval/evaluate.py --golden     # 期待値データ tests/golden/analysis/ も作り直す

合成データは「仕様が意図どおりに動くか」を確かめるためのもので、実際の睡眠に対する精度を示すものではない。
"""
from __future__ import annotations

import argparse
import json
import math
import pathlib
import random
import re
import statistics
import sys
import zlib

sys.path.insert(0, str(pathlib.Path(__file__).parent))
import nk_ref as R  # noqa: E402

ROOT = pathlib.Path(__file__).resolve().parents[2]
OUT = pathlib.Path(__file__).parent / "out"
GOLDEN = ROOT / "tests" / "golden" / "analysis"

T_WAKE, T_QWAKE, T_LIGHT, T_DEEP, T_REM = "W", "Q", "L", "D", "R"  # 正解の状態（Q＝じっとしたまま起きている）


# ------------------------------------------------------------------ 合成データ
SCEN = {
    # 名前: (寝つきの分, 寝つく前の「じっと起きている」割合, 中途覚醒の回数, 1 回の長さ(分), 同じベッドの人, いびき)
    "normal":     dict(sol=(8, 20), quiet=0.5, wakes=(1, 3), wake_len=(1, 4), partner=False, snore=False),
    "short":      dict(sol=(8, 20), quiet=0.5, wakes=(1, 3), wake_len=(1, 4), partner=False, snore=False, dur=(320, 360)),
    "long_sol":   dict(sol=(40, 75), quiet=0.75, wakes=(1, 3), wake_len=(1, 5), partner=False, snore=False),
    "fragmented": dict(sol=(10, 25), quiet=0.5, wakes=(5, 9), wake_len=(3, 12), partner=False, snore=False),
    "partner":    dict(sol=(8, 20), quiet=0.5, wakes=(1, 3), wake_len=(1, 4), partner=True, snore=False),
    "snorer":     dict(sol=(8, 20), quiet=0.5, wakes=(1, 3), wake_len=(1, 4), partner=False, snore=True),
}


def poisson(rng, lam):
    if lam <= 0:
        return 0
    L, k, p = math.exp(-lam), 0, 1.0
    while True:
        p *= rng.random()
        if p <= L:
            return k
        k += 1


def truth_hypnogram(rng, sc, n_epochs):
    sol_ep = int(rng.uniform(*sc["sol"]) * 2)
    seq = [T_QWAKE if rng.random() < sc["quiet"] else T_WAKE for _ in range(sol_ep)]
    t, cycle = sol_ep, 0
    end_sleep = max(0, n_epochs - int(rng.uniform(2, 15) * 2))
    while t < end_sleep:
        clen = int(rng.uniform(80, 100) * 2)
        deep_share = max(0.0, 0.45 - 0.15 * cycle)
        rem_share = min(0.35, 0.10 + 0.08 * cycle)
        for i in range(clen):
            f = i / clen
            if f < 0.15:
                s = T_LIGHT
            elif f < 0.15 + deep_share:
                s = T_DEEP
            elif f < 1 - rem_share:
                s = T_LIGHT
            else:
                s = T_REM
            seq.append(s)
        t += clen
        cycle += 1
    seq = (seq[:end_sleep] + [T_WAKE] * n_epochs)[:n_epochs]
    for _ in range(rng.randint(*sc["wakes"])):  # 中途覚醒を差し込む
        if end_sleep - 20 <= sol_ep + 40:
            break
        pos = rng.randint(sol_ep + 40, end_sleep - 20)
        ln = int(rng.uniform(*sc["wake_len"]) * 2)
        for i in range(pos, min(pos + ln, end_sleep)):
            seq[i] = T_WAKE
    return seq


def features(rng, state, sc, offbed=False, breath=True):
    e = dict(acc_cov=1.0, mic_cov=1.0, move_n=0, move_mg=3.0, micro_mg=0.0, posture=0, snd_db_mean=-60.0,
             snd_db_p95=-55.0, snore_s=0, breath_bpm=None, breath_q=None, lux=0.5, flags=0)
    if state == T_WAKE:
        mv, post = poisson(rng, 6), rng.random() < 0.25
    elif state == T_QWAKE:
        mv, post = poisson(rng, 0.8), rng.random() < 0.03
    elif state == T_LIGHT:
        mv, post = (poisson(rng, 3) if rng.random() < 0.08 else 0), rng.random() < 0.03
    elif state == T_DEEP:
        mv, post = (poisson(rng, 2) if rng.random() < 0.01 else 0), rng.random() < 0.003
    else:  # REM
        mv, post = (poisson(rng, 1.5) if rng.random() < 0.04 else 0), rng.random() < 0.005
    if sc.get("partner") and rng.random() < 0.06:
        mv += poisson(rng, 4)
    if offbed:
        mv, post = (1 if rng.random() < 0.01 else 0), False
    e["move_n"], e["posture"] = min(30, mv), int(post)
    e["micro_mg"] = round(max(0.05, rng.gauss(0.2, 0.05)) if offbed else max(0.6, rng.gauss(2.0, 0.5)), 2)
    if e["micro_mg"] < 0.5:
        e["flags"] |= R.F_OFFBED
    if breath and not offbed and state in (T_LIGHT, T_DEEP, T_REM):
        mu, sd = {T_LIGHT: (15, 1.2), T_DEEP: (14, 0.4), T_REM: (16, 2.5)}[state]
        e["breath_bpm"], e["breath_q"] = round(rng.gauss(mu, sd), 2), round(rng.uniform(0.6, 0.9), 2)
    if sc.get("snore") and state in (T_LIGHT, T_DEEP) and rng.random() < 0.3:
        e["snore_s"] = rng.randint(10, 30)
    return e


def make_night(seed, scen="normal", start_clock=23 * 60 + 30, gap=None, offbed=False, dur_min=None,
               breath_prob=0.6, mic_used=True):
    rng = random.Random(seed)
    sc = SCEN.get(scen, SCEN["normal"])
    dur = dur_min or rng.uniform(*sc.get("dur", (440, 500)))
    n = int(dur * 2)
    truth = truth_hypnogram(rng, sc, n)
    breath = rng.random() < breath_prob
    eps = [features(rng, s, sc, offbed=offbed, breath=breath) for s in truth]
    if gap:
        for i in range(gap[0], min(n, gap[0] + gap[1])):
            eps[i].update(acc_cov=0.0, mic_cov=0.0, move_n=None, posture=None, micro_mg=None, flags=1 | 2)
    if not mic_used:
        for e in eps:
            e.update(mic_cov=0.0, snore_s=None, snd_db_mean=None, snd_db_p95=None)
    for i, e in enumerate(eps):
        e["idx"] = i
    session = dict(start_clock_min=start_clock % 1440, target_sleep_min=420, mic_used=mic_used,
                   placement="WARN" if offbed else "OK", end_reason="USER_STOP")
    return session, eps, truth


# ------------------------------------------------------------------ 正解の指標
def truth_metrics(truth):
    sleep = [s in (T_LIGHT, T_DEEP, T_REM) for s in truth]
    onset = next(i for i in range(len(truth)) if all(sleep[i:i + 20]) and i + 20 <= len(truth))
    final = max(i for i, s in enumerate(sleep) if s)
    tst = sum(sleep) * 0.5
    waso = sum(1 for i in range(onset, final + 1) if not sleep[i]) * 0.5
    return dict(tst=tst, sol=onset * 0.5, waso=waso, se=tst / (len(truth) * 0.5), onset=onset)


def kappa(pairs, labels):
    n = len(pairs)
    po = sum(1 for a, b in pairs if a == b) / n
    pe = sum((sum(1 for a, _ in pairs if a == l) / n) * (sum(1 for _, b in pairs if b == l) / n) for l in labels)
    return (po - pe) / (1 - pe) if pe < 1 else 1.0


def map_truth(s):
    return {T_WAKE: "W", T_QWAKE: "W", T_LIGHT: "L", T_REM: "L", T_DEEP: "D"}[s]


def map_est(x):
    return {R.WAKE: "W", R.LIGHT: "L", R.DEEP: "D", R.UNKNOWN: "U"}[x]


# ------------------------------------------------------------------ 評価
def eval_scenarios(theta=R.THETA_WAKE, nights_per=30):
    rows = {}
    for scen in SCEN:
        pairs, err = [], {"tst": [], "sol": [], "waso": [], "score": []}
        deep_tp = deep_fp = deep_fn = 0
        for k in range(nights_per):
            s, eps, truth = make_night(zlib.crc32(scen.encode()) % 99991 + k, scen)
            r = R.analyze_night(s, eps, theta=theta)
            tm = truth_metrics(truth)
            for t, e in zip(truth, r.hypnogram):
                pairs.append((map_truth(t), map_est(e)))
                deep_tp += map_truth(t) == "D" and map_est(e) == "D"
                deep_fp += map_truth(t) != "D" and map_est(e) == "D"
                deep_fn += map_truth(t) == "D" and map_est(e) != "D"
            err["tst"].append(r.tst_min - tm["tst"])
            err["sol"].append(r.sol_min - tm["sol"])
            err["waso"].append(r.waso_min - tm["waso"])
            tr = R.NightResult(quality="OK", reasons=[], tst_min=tm["tst"], sol_min=tm["sol"], waso_min=tm["waso"],
                               se=tm["se"])
            ts, _ = R.compute_score(tr, 420, 0, None)
            if r.score is not None:
                err["score"].append(r.score - ts)
        sw = [(a == "W", b == "W") for a, b in pairs]
        sens = sum(1 for a, b in sw if not a and not b) / max(1, sum(1 for a, _ in sw if not a))
        spec = sum(1 for a, b in sw if a and b) / max(1, sum(1 for a, _ in sw if a))
        acc = sum(1 for a, b in sw if a == b) / len(sw)
        rows[scen] = dict(
            sleep_wake_acc=round(acc, 3), sleep_sensitivity=round(sens, 3), wake_specificity=round(spec, 3),
            kappa_3=round(kappa(pairs, ["W", "L", "D"]), 3),
            deep_precision=round(deep_tp / max(1, deep_tp + deep_fp), 3),
            deep_recall=round(deep_tp / max(1, deep_tp + deep_fn), 3),
            tst_bias=round(statistics.fmean(err["tst"]), 1), tst_mae=round(statistics.fmean(map(abs, err["tst"])), 1),
            sol_bias=round(statistics.fmean(err["sol"]), 1), sol_mae=round(statistics.fmean(map(abs, err["sol"])), 1),
            waso_bias=round(statistics.fmean(err["waso"]), 1),
            score_mae=round(statistics.fmean(map(abs, err["score"])), 1),
        )
    return rows


def eval_quality():
    cases = []

    def add(name, expect_q, expect_reason, **kw):
        s, eps, _ = make_night(kw.pop("seed", 7), **kw)
        if kw.get("dur_min") and kw["dur_min"] < 30:
            pass
        r = R.analyze_night(s, eps)
        ok = r.quality == expect_q and (expect_reason is None or expect_reason in r.reasons)
        no_score_when_failed = not (r.quality == "FAILED" and r.score is not None)
        cases.append(dict(case=name, expected=f"{expect_q} {expect_reason or ''}".strip(), got=f"{r.quality} {','.join(r.reasons)}",
                          score=r.score, pass_=ok and no_score_when_failed))

    add("通常の夜", "OK", None)
    add("計測 20 分", "FAILED", "Q03", dur_min=20)
    add("計測 2 時間", "INSUFFICIENT", "Q06", dur_min=120)
    add("途中 2 時間の欠損", "OK", "Q04", gap=(300, 240))
    add("途中 3 時間の欠損", "INSUFFICIENT", "Q05", gap=(200, 360))
    add("机の上に置いた", "INSUFFICIENT", "Q07", offbed=True)
    add("マイクの許可なし", "OK", "Q01", mic_used=False)
    # 境界値：有効 29.5 分と 30 分、有効率 0.69 と 0.70
    for v_ep, exp in ((59, "FAILED"), (60, "INSUFFICIENT")):
        s, eps, _ = make_night(3, dur_min=v_ep / 2)
        cases.append(dict(case=f"境界：有効 {v_ep / 2} 分", expected=exp, got=R.judge_quality(s, eps)[0],
                          score=None, pass_=R.judge_quality(s, eps)[0] == exp))
    for miss, exp in ((289, "INSUFFICIENT"), (288, "OK")):
        s, eps, _ = make_night(4, dur_min=480, gap=(100, miss))  # 960 エポック中 miss 欠損
        q, _ = R.judge_quality(s, eps)
        cases.append(dict(case=f"境界：有効率 {round((960 - miss) / 960, 3)}", expected=exp, got=q, score=None, pass_=q == exp))
    return cases


def eval_feedback():
    """14 夜の並びを作り、期待する規則が出るかを確かめる。"""
    def seq(kind):
        nights, hist = [], []
        for d in range(14):
            weekend = d % 7 in (5, 6)
            start = 23 * 60 + 30
            sound = False
            scen = "normal"
            if kind == "irregular":
                start = [22 * 60 + 30, 25 * 60 + 10, 23 * 60, 24 * 60 + 50, 22 * 60 + 50, 25 * 60 + 30, 23 * 60 + 20][d % 7]
            if kind == "weekend_late" and weekend:
                start = 26 * 60
            if kind == "short":
                start = 25 * 60
            if kind == "sound_helps":
                sound = d % 2 == 0
                scen = "normal" if sound else "long_sol"
            if kind == "steady":
                start = 23 * 60 + rd.randint(-15, 15)
            s, eps, _ = make_night(500 + d + zlib.crc32(kind.encode()) % 1000, scen, start_clock=start,
                                   dur_min=330 if kind == "short" else None)
            r = R.analyze_night(s, eps, history_onsets=hist[-7:])
            onset_clock = (start + (r.sol_min or 0)) % 1440
            if r.quality == "OK":
                hist.append(onset_clock)
            nights.append(dict(quality=r.quality, onset_clock=onset_clock, tst=r.tst_min, sol=r.sol_min,
                               weekend=weekend, sound=sound, score=r.score, reasons=r.reasons))
        return nights

    rd = random.Random(1)
    expect = {"irregular": "FB-01", "weekend_late": "FB-02", "short": "FB-03", "sound_helps": "FB-05", "steady": "FB-11"}
    out = []
    for kind, rule in expect.items():
        got = R.feedback_candidates(seq(kind))
        out.append(dict(sequence=kind, expected=rule, got=got, pass_=rule in got))
    return out


BANNED = ["悪い", "ダメ", "だめ", "失敗", "睡眠不足", "すぎ", "異常", "危険", "病気", "必ず", "確実", "原因", "せい", "べき",
          "しなければ", "不眠症", "無呼吸"]


def lint_copy():
    """AI-5 の文型（4 章）と AI-2 の画面の文言に、使わない言葉が入っていないか。"""
    findings = []
    ai5 = (ROOT / "docs/spec/ai/AI-5_feedback.md").read_text(encoding="utf-8")
    sec = ai5.split("## 4. 文型")[1].split("## 5.")[0]
    ai2 = (ROOT / "docs/spec/ai/AI-2_data-quality.md").read_text(encoding="utf-8")
    # AI-2 は表の「画面の文言」と「次の行動」の列だけを見る（「取得失敗」は判定の名前で、画面の文ではないため）
    q_rows = [ln.split("|") for ln in ai2.splitlines() if re.match(r"\| Q\d\d", ln)]
    q_texts = [c[4] + " " + c[5] for c in q_rows]
    texts = [("AI-5", ln) for ln in sec.splitlines() if ln.strip()] + [("AI-2", t) for t in q_texts]
    for src, ln in texts:
        for w in BANNED:
            if w in ln:
                findings.append(dict(source=src, word=w, line=ln.strip()[:80]))
    return dict(lines_checked=len(texts), findings=findings)


def theta_sweep():
    res = {}
    for th in (1.0, 1.6, 2.5, 4.0, 6.0):
        rows = eval_scenarios(theta=th, nights_per=12)
        res[th] = dict(
            mean_acc=round(statistics.fmean(r["sleep_wake_acc"] for r in rows.values()), 3),
            mean_wake_spec=round(statistics.fmean(r["wake_specificity"] for r in rows.values()), 3),
            mean_tst_bias=round(statistics.fmean(r["tst_bias"] for r in rows.values()), 1),
            long_sol_sol_bias=rows["long_sol"]["sol_bias"])
    return res


def write_golden():
    GOLDEN.mkdir(parents=True, exist_ok=True)
    picks = {"g01_normal": dict(scen="normal", seed=11), "g02_long_sol": dict(scen="long_sol", seed=12),
             "g03_partner": dict(scen="partner", seed=13), "g04_gap": dict(scen="normal", seed=14, gap=(300, 240)),
             "g05_offbed": dict(scen="normal", seed=15, offbed=True), "g06_short_session": dict(scen="normal", seed=16, dur_min=20)}
    for name, kw in picks.items():
        seed = kw.pop("seed")
        scen = kw.pop("scen")
        s, eps, _ = make_night(seed, scen, **kw)
        r = R.analyze_night(s, eps)
        exp = {k: v for k, v in r.__dict__.items()}
        exp["score_label"] = R.score_label(r.score)
        (GOLDEN / f"{name}.input.json").write_text(json.dumps(dict(algo_ver=R.ALGO_VER, session=s, epochs=eps),
                                                              ensure_ascii=False), encoding="utf-8")
        (GOLDEN / f"{name}.expected.json").write_text(json.dumps(exp, ensure_ascii=False), encoding="utf-8")
    return list(picks)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--golden", action="store_true")
    args = ap.parse_args()
    OUT.mkdir(exist_ok=True)
    result = dict(algo_ver=R.ALGO_VER, scenarios=eval_scenarios(), quality=eval_quality(), feedback=eval_feedback(),
                  copy_lint=lint_copy(), theta_sweep=theta_sweep())
    if args.golden:
        result["golden"] = write_golden()
    (OUT / "results.json").write_text(json.dumps(result, ensure_ascii=False, indent=1), encoding="utf-8")
    print(json.dumps(result, ensure_ascii=False, indent=1))


if __name__ == "__main__":
    main()
