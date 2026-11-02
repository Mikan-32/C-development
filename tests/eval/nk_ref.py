"""ねむのき 分析の参照実装（AI-2 品質判定・AI-3 段階推定・AI-4 スコア・AI-5 規則の一部）。

C の実装（src/analysis, src/feedback）の答え合わせに使う。仕様は docs/spec/ai/ を正とし、
この実装と仕様が食い違ったら仕様に合わせて直す。
"""
from __future__ import annotations

import statistics
from dataclasses import dataclass, field

ALGO_VER = "0.1.0"
EPOCH_MIN = 0.5

# ---- AI-3 の設定値（docs/spec/ai/AI-3_stage-estimation.md 4 章） ----
WEIGHTS = {-4: 0.04, -3: 0.04, -2: 0.06, -1: 0.10, 0: 0.50, 1: 0.14, 2: 0.12}
THETA_WAKE = 2.5
POSTURE_WEIGHT = 10
ONSET_RUN = 20
DEEP_HALF_WINDOW = 10
DEEP_MIN_RUN = 6
AWAKENING_MIN_RUN = 4

UNKNOWN, WAKE, LIGHT, DEEP = 0, 1, 2, 3

# ---- AI-2 の理由コード ----
Q = {"Q01": 1 << 1, "Q02": 1 << 2, "Q03": 1 << 3, "Q04": 1 << 4, "Q05": 1 << 5, "Q06": 1 << 6,
     "Q07": 1 << 7, "Q08": 1 << 8, "Q09": 1 << 9, "Q10": 1 << 10, "Q11": 1 << 11}
F_OFFBED, F_MIC_BUSY = 1 << 2, 1 << 3


def reasons_to_list(bits: int) -> list[str]:
    return [k for k, v in Q.items() if bits & v]


@dataclass
class NightResult:
    quality: str
    reasons: list[str]
    confidence: str | None = None
    stage_confidence: str | None = None
    tib_min: float | None = None
    valid_min: float | None = None
    tst_min: float | None = None
    sol_min: float | None = None
    waso_min: float | None = None
    awakenings: int | None = None
    se: float | None = None
    onset_idx: int | None = None
    final_wake_idx: int | None = None
    deep_min: float | None = None
    light_min: float | None = None
    wake_min: float | None = None
    unknown_min: float | None = None
    turns: int | None = None
    snore_min: float | None = None
    breath_bpm: float | None = None
    hypnogram: list[int] = field(default_factory=list)
    score: int | None = None
    score_parts: dict | None = None
    score_reference_only: bool = False


# ------------------------------------------------------------------ AI-2
def judge_quality(session: dict, epochs: list[dict]) -> tuple[str, int]:
    n = len(epochs)
    d_min = n * EPOCH_MIN
    valid = [e for e in epochs if e["acc_cov"] >= 0.8]
    v_min = len(valid) * EPOCH_MIN
    bits = 0
    if not session.get("mic_used", True):
        bits |= Q["Q01"]
    if any(e["flags"] & F_MIC_BUSY for e in epochs):
        bits |= Q["Q08"]
    if session.get("end_reason") == "BATTERY_LOW":
        bits |= Q["Q09"]
    # 欠損区間（2 エポック以上）があれば Q04
    run = 0
    for e in epochs:
        run = run + 1 if e["acc_cov"] < 0.8 else 0
        if run >= 2:
            bits |= Q["Q04"]
            break
    if session.get("acc_start_failed"):
        return "FAILED", bits | Q["Q02"]
    if session.get("analysis_error"):
        return "FAILED", bits | Q["Q11"]
    if d_min < 30 or v_min < 30:
        return "FAILED", bits | Q["Q03"]
    insufficient = False
    if v_min < 180:
        bits |= Q["Q06"]; insufficient = True
    if v_min / d_min < 0.7:
        bits |= Q["Q05"]; insufficient = True
    offbed = sum(1 for e in valid if e["flags"] & F_OFFBED)
    if valid and offbed / len(valid) > 0.3:
        bits |= Q["Q07"]; insufficient = True
    return ("INSUFFICIENT" if insufficient else "OK"), bits


# ------------------------------------------------------------------ AI-3
def _activity(e: dict) -> float | None:
    if e["acc_cov"] < 0.8 or e.get("move_n") is None:
        return None
    return e["move_n"] + POSTURE_WEIGHT * (e.get("posture") or 0)


def _runs(seq: list[int], value: int):
    """value が続く区間を (開始, 長さ) で返す。"""
    i, n = 0, len(seq)
    while i < n:
        if seq[i] == value:
            j = i
            while j < n and seq[j] == value:
                j += 1
            yield i, j - i
            i = j
        else:
            i += 1


def estimate_stages(epochs: list[dict], theta: float = THETA_WAKE) -> list[int]:
    a = [_activity(e) for e in epochs]
    n = len(a)
    st = [UNKNOWN] * n
    for t in range(n):
        if a[t] is None:
            continue
        num = den = 0.0
        for k, w in WEIGHTS.items():
            j = t + k
            if 0 <= j < n:
                num += w * (a[j] if a[j] is not None else 0.0)
                den += w
        st[t] = WAKE if num / den > theta else LIGHT  # LIGHT = 睡眠（深い・浅いは後で分ける）
    # 整える規則 1：覚醒にはさまれた 3 エポック未満の睡眠 → 覚醒
    for s, ln in list(_runs(st, LIGHT)):
        if ln < 3 and s > 0 and s + ln < n and st[s - 1] == WAKE and st[s + ln] == WAKE:
            for i in range(s, s + ln):
                st[i] = WAKE
    # 整える規則 2：睡眠にはさまれた 1 エポックの覚醒で a ≤ 3 → 睡眠
    for t in range(1, n - 1):
        if st[t] == WAKE and st[t - 1] == LIGHT and st[t + 1] == LIGHT and a[t] is not None and a[t] <= 3:
            st[t] = LIGHT
    return st


def _deep_pass(epochs, st):
    a = [_activity(e) for e in epochs]
    n = len(st)
    cand = [False] * n
    for t in range(n):
        if st[t] != LIGHT:
            continue
        lo, hi = max(0, t - DEEP_HALF_WINDOW), min(n, t + DEEP_HALF_WINDOW + 1)
        win = a[lo:hi]
        if any(x is None or x > 1 for x in win):
            continue
        br = [epochs[i]["breath_bpm"] for i in range(lo, hi)
              if (epochs[i].get("breath_q") or 0) >= 0.6 and epochs[i].get("breath_bpm") is not None]
        if len(br) >= 6:
            m = statistics.fmean(br)
            if m <= 0 or statistics.pstdev(br) / m >= 0.1:
                continue
        cand[t] = True
    out = list(st)
    t = 0
    while t < n:
        if cand[t]:
            j = t
            while j < n and cand[j]:
                j += 1
            if j - t >= DEEP_MIN_RUN:
                for i in range(t, j):
                    out[i] = DEEP
            t = j
        else:
            t += 1
    return out


def analyze_night(session: dict, epochs: list[dict], history_onsets: list[float] | None = None,
                  theta: float = THETA_WAKE) -> NightResult:
    quality, bits = judge_quality(session, epochs)
    res = NightResult(quality=quality, reasons=reasons_to_list(bits))
    n = len(epochs)
    valid_n = sum(1 for e in epochs if e["acc_cov"] >= 0.8)
    res.tib_min = valid_n * EPOCH_MIN
    res.valid_min = res.tib_min
    res.unknown_min = (n - valid_n) * EPOCH_MIN
    if quality == "FAILED":
        return res
    st = estimate_stages(epochs, theta)
    onset = next((s for s, ln in _runs(st, LIGHT) if ln >= ONSET_RUN), None)
    if onset is None:
        res.tst_min = 0.0
        res.hypnogram = [UNKNOWN if x == UNKNOWN else WAKE for x in st]
        res.confidence = "LOW"
        return res
    final = max(i for i, x in enumerate(st) if x == LIGHT)
    for i in range(n):  # 寝つく前と最後の目覚めの後の睡眠は覚醒として扱う
        if st[i] == LIGHT and (i < onset or i > final):
            st[i] = WAKE
    st = _deep_pass(epochs, st)
    sleep_n = sum(1 for x in st if x in (LIGHT, DEEP))
    res.onset_idx, res.final_wake_idx = onset, final
    res.tst_min = sleep_n * EPOCH_MIN
    res.sol_min = onset * EPOCH_MIN
    mid = st[onset:final + 1]
    res.waso_min = sum(1 for x in mid if x == WAKE) * EPOCH_MIN
    res.awakenings = sum(1 for _, ln in _runs(mid, WAKE) if ln >= AWAKENING_MIN_RUN)
    res.se = round(res.tst_min / res.tib_min, 4) if res.tib_min else None
    res.deep_min = st.count(DEEP) * EPOCH_MIN
    res.light_min = st.count(LIGHT) * EPOCH_MIN
    res.wake_min = st.count(WAKE) * EPOCH_MIN
    res.turns = sum(1 for e in epochs if e["acc_cov"] >= 0.8 and e.get("posture"))
    res.hypnogram = st
    # 確からしさ
    offbed = any(e["flags"] & F_OFFBED for e in epochs if e["acc_cov"] >= 0.8)
    if quality == "OK" and valid_n / n >= 0.9 and session.get("placement") == "OK" and not offbed:
        res.confidence = "HIGH"
    elif quality == "OK":
        res.confidence = "MED"
    else:
        res.confidence = "LOW"
    res.stage_confidence = "MED" if res.confidence in ("HIGH", "MED") else "LOW"
    if res.tst_min and res.deep_min / res.tst_min > 0.35:
        res.stage_confidence = "LOW"
    # いびき・呼吸（AI-2 の 2 章）
    if session.get("mic_used", True):
        ok_mic = [e for e in epochs if e["mic_cov"] >= 0.8]
        if len(ok_mic) >= 0.7 * n:
            res.snore_min = round(sum(e.get("snore_s") or 0 for e in ok_mic) / 60, 1)
    sleep_eps = [epochs[i] for i, x in enumerate(st) if x in (LIGHT, DEEP)]
    good = [e["breath_bpm"] for e in sleep_eps if (e.get("breath_q") or 0) >= 0.6 and e.get("breath_bpm")]
    if sleep_eps and len(good) >= 0.5 * len(sleep_eps):
        res.breath_bpm = round(statistics.median(good), 1)
    # スコア
    onset_clock = session["start_clock_min"] + res.sol_min
    res.score, res.score_parts = compute_score(res, session.get("target_sleep_min", 420),
                                               onset_clock, history_onsets)
    res.score_reference_only = quality == "INSUFFICIENT"
    if "Q07" in res.reasons:
        # ベッドの上にない疑い：動きが記録されず満点に近くなるため、参考値としても出さない（AI-7 の失敗例 1）
        res.score, res.score_parts = None, None
    return res


# ------------------------------------------------------------------ AI-4
def _lin(x, full, zero):
    """full で 1、zero で 0、間は直線。"""
    if full > zero:
        return 1.0 if x >= full else 0.0 if x <= zero else (x - zero) / (full - zero)
    return 1.0 if x <= full else 0.0 if x >= zero else (zero - x) / (zero - full)


def sol_rate(sol):
    return 1.0 if sol <= 15 else 0.8 if sol <= 30 else 0.55 if sol <= 45 else 0.3 if sol <= 60 else 0.1


def circ_diff(a, b):
    d = abs(a - b) % 1440
    return min(d, 1440 - d)


def compute_score(r: NightResult, target: float, onset_clock: float, history_onsets):
    parts = {
        "duration": (30, _lin(r.tst_min, target, target - 180)),
        "latency": (15, sol_rate(r.sol_min)),
        "continuity": (25, _lin(r.waso_min, 20, 120)),
        "efficiency": (15, _lin(r.se, 0.85, 0.60)),
    }
    if history_onsets and len(history_onsets) >= 3:
        # 18:00 起点に直してから中央値を取る（日付をまたぐため）
        rel = sorted(((h - 1080) % 1440) for h in history_onsets)
        med = (statistics.median(rel) + 1080) % 1440
        parts["regularity"] = (15, _lin(circ_diff(onset_clock, med), 30, 150))
    w = sum(p[0] for p in parts.values())
    if w < 70:
        return None, None
    s = round(100 * sum(p[0] * p[1] for p in parts.values()) / w)
    return s, {k: round(p[0] * p[1], 2) for k, p in parts.items()}


def score_label(s):
    if s is None:
        return None
    return "とても良い" if s >= 85 else "良い" if s >= 70 else "まずまず" if s >= 55 else "回復を優先"


# ------------------------------------------------------------------ AI-5（規則の一部）
def feedback_candidates(nights: list[dict], target=420) -> list[str]:
    """nights：古い順。各要素は {quality, onset_clock, tst, sol, weekend, sound, score}。直近 7 夜を見る。"""
    ok7 = [x for x in nights[-7:] if x["quality"] == "OK"]
    out = []
    rel = lambda m: (m - 1080) % 1440
    if len(ok7) >= 4:
        r = [rel(x["onset_clock"]) for x in ok7]
        if max(r) - min(r) >= 90:
            out.append("FB-01")
    wd = [rel(x["onset_clock"]) for x in ok7 if not x["weekend"]]
    we = [rel(x["onset_clock"]) for x in ok7 if x["weekend"]]
    if len(wd) >= 2 and len(we) >= 1 and statistics.median(we) - statistics.median(wd) >= 60:
        out.append("FB-02")
    if sum(1 for x in ok7 if x["tst"] < target - 30) >= 3:
        out.append("FB-03")
    if sum(1 for x in ok7 if x["sol"] > 30) >= 3:
        out.append("FB-04")
    w_s = [x["sol"] for x in ok7 if x["sound"]]
    wo_s = [x["sol"] for x in ok7 if not x["sound"]]
    if len(w_s) >= 2 and len(wo_s) >= 2 and statistics.fmean(wo_s) - statistics.fmean(w_s) >= 5:
        out.append("FB-05")
    prev = [x["score"] for x in nights[-14:-7] if x["quality"] == "OK" and x["score"] is not None]
    cur = [x["score"] for x in ok7 if x["score"] is not None]
    good = False
    if len(prev) >= 3 and len(cur) >= 3 and statistics.fmean(cur) - statistics.fmean(prev) >= 5:
        good = True
    if len(ok7) >= 3:
        r = [rel(x["onset_clock"]) for x in ok7]
        if max(r) - min(r) < 60 or sum(1 for x in ok7 if x["tst"] >= target) >= 4:
            good = True
    if good:
        out.append("FB-11")
    last5 = nights[-5:]
    if sum(1 for x in last5 if x["quality"] == "INSUFFICIENT" and set(x.get("reasons", [])) & {"Q05", "Q07"}) >= 2:
        out.append("FB-13")
    return out
