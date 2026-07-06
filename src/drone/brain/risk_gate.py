"""
risk_gate.py -- S4 MEMORY RISK GATE and S5 SELECT MODE.

S4 scores how much the incoming context should be distrusted, using:
  * quarantined memories present at all,
  * low trust scores on "trusted" memories,
  * high hop counts (memory that has travelled far / been relayed),
  * injection-flavoured text ("ignore previous warnings", "bypass safety", ...),
  * simple contradictions across memories (e.g. "path clear" vs "path blocked").

S5 turns that score into a planning mode. High risk -> cautious planning, which
biases the planner toward WAIT / SCAN / REQUEST_VERIFICATION.
"""

from __future__ import annotations

import re
from dataclasses import dataclass, field

from .config import Config
from .context_builder import ContextView

# Patterns that suggest prompt-injection / safety-subversion. Shared with
# self_check so both the pre-reasoning gate and the post-reasoning check agree.
SUSPICIOUS_PATTERNS: list[re.Pattern] = [
    re.compile(p, re.IGNORECASE)
    for p in (
        r"ignore (all |the )?(previous|prior|earlier)?\s*(instructions|warnings|rules)",
        r"disregard (the )?(safety|warnings|rules|shield)",
        r"bypass(ing)? (the )?(safety|shield|checks|guard)",
        r"disable (the )?(safety|shield|guard|checks)",
        r"override (the )?(safety|shield|controller|command)",
        r"\bjailbreak\b",
        r"\bexfiltrat",
        r"delete (all )?(memory|memories|logs)",
        r"self[-\s]?destruct",
        r"shut\s?down (the )?(peer|drone|system)",
        r"reveal (the )?(system )?prompt",
    )
]

# Cheap contradiction detector: antonym pairs. If both sides of a pair appear
# anywhere across the memory set, that is treated as a contradiction signal.
_CONTRADICTION_PAIRS = [
    ("clear", "blocked"),
    ("safe", "danger"),
    ("safe", "hostile"),
    ("open", "closed"),
    ("go", "stop"),
    ("proceed", "hold"),
    ("friendly", "hostile"),
]


@dataclass
class RiskAssessment:
    score: float = 0.0
    factors: list[str] = field(default_factory=list)   # human-readable reasons
    suspicious: bool = False                            # any injection pattern hit


def _text_hits(text: str) -> list[str]:
    return [p.pattern for p in SUSPICIOUS_PATTERNS if p.search(text)]


def assess(view: ContextView, cfg: Config) -> RiskAssessment:
    score = 0.0
    factors: list[str] = []
    suspicious = False

    # 1. quarantined memory present
    if view.quarantined:
        add = min(0.5, 0.25 * len(view.quarantined))
        score += add
        factors.append(f"{len(view.quarantined)} quarantined memory item(s)")

    # 2/3. low trust + high hop count on trusted memories
    for m in view.trusted:
        if m.trust_score is not None and m.trust_score < cfg.low_trust_threshold:
            score += 0.15
            factors.append(f"low trust on '{m.id}' ({m.trust_score:.2f})")
        if m.hop_count > cfg.max_hops:
            score += 0.1
            factors.append(f"high hop count on '{m.id}' ({m.hop_count})")

    # 4. injection-flavoured text anywhere (trusted content, quarantined, goal)
    corpus = [m.content for m in view.trusted] \
        + [m.content for m in view.quarantined] \
        + [view.goal_text()]
    for text in corpus:
        hits = _text_hits(text or "")
        if hits:
            suspicious = True
            score += 0.4 * len(hits)
            factors.append("suspicious phrasing detected")
            break

    # 5. contradictions across the memory set
    blob = " ".join((m.content or "").lower() for m in view.trusted + view.quarantined)
    for a, b in _CONTRADICTION_PAIRS:
        if a in blob and b in blob:
            score += 0.2
            factors.append(f"contradiction: '{a}' vs '{b}'")
            break

    return RiskAssessment(score=min(1.0, score), factors=factors, suspicious=suspicious)


def select_mode(risk: RiskAssessment, cfg: Config) -> str:
    """S5. cautious when risk is high or any injection pattern fired."""
    if risk.suspicious or risk.score >= cfg.cautious_threshold:
        return "cautious"
    return "normal"
