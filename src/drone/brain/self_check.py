"""
self_check.py -- S9 SELF-CHECK.

Soft checks the brain runs on its *own* proposal before returning it. These are
not the hard safety shield (that lives in the C++ body); they catch a reasoner
that has been nudged off the rails. Any hit raises SelfCheckError (S9 suspicious
-> SF). Checks:

  * used a quarantined memory id,
  * reason contains safety-bypass / injection language,
  * confidence overstated (globally, or above the cautious cap in cautious mode),
  * proposal contradicts a trusted observation (e.g. MOVE while a trusted memory
    reports the path blocked / danger ahead).
"""

from __future__ import annotations

from .action_parser import ParsedPlan
from .config import Config
from .context_builder import ContextView
from .risk_gate import SUSPICIOUS_PATTERNS

_BLOCK_WORDS = ("blocked", "danger", "hostile", "obstacle", "no-go", "keep out")
_ADVANCE_WORDS = ("MOVE", "MOVE_TO", "GOTO", "GO_TO", "NAVIGATE", "ADVANCE", "PROCEED")


class SelfCheckError(ValueError):
    """Raised when the brain's own proposal looks unsafe/suspicious (S9 -> SF)."""


def check(plan: ParsedPlan, view: ContextView, mode: str, cfg: Config) -> None:
    # 1. quarantined memory used
    bad = [x for x in plan.used_memory_ids if x in view.quarantined_ids]
    if bad:
        raise SelfCheckError(f"proposal relies on quarantined memory {bad}")

    # 2. safety-bypass / injection language in the stated reason
    for pat in SUSPICIOUS_PATTERNS:
        if pat.search(plan.reason or ""):
            raise SelfCheckError("reason contains bypass/injection language")

    # 3. overstated confidence
    if plan.confidence > cfg.max_confidence:
        raise SelfCheckError(f"overstated confidence ({plan.confidence:.2f})")
    if mode == "cautious" and plan.confidence > cfg.cautious_confidence_cap:
        raise SelfCheckError(
            f"confidence {plan.confidence:.2f} too high for cautious mode")

    # 4. contradicts a trusted observation
    if plan.proposed_action.upper() in _ADVANCE_WORDS:
        for m in view.trusted:
            content = (m.content or "").lower()
            if any(w in content for w in _BLOCK_WORDS):
                raise SelfCheckError(
                    f"advancing contradicts trusted memory '{m.id}'")
