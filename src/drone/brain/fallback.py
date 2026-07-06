"""
fallback.py -- SF FALLBACK.

Every invalid, risky, or malformed path converges here and produces a safe,
low-confidence proposal (never an execution, never an error the body has to
special-case). Preference order is WAIT -> SCAN -> REQUEST_VERIFICATION -> HOLD
-> IDLE, constrained to allowed_actions when they're known; if the request was
so malformed we don't even know the action set, we default to WAIT.
"""

from __future__ import annotations

from typing import Any, Optional

from .action_parser import ParsedPlan
from .config import Config

_PREFERENCE = ("WAIT", "SCAN", "REQUEST_VERIFICATION", "HOLD", "IDLE")


def _safe_action(allowed: Optional[list[str]]) -> str:
    if allowed:
        allowed_set = set(allowed)
        for a in _PREFERENCE:
            if a in allowed_set:
                return a
        return allowed[0]          # honour the contract even if nothing ideal
    return "WAIT"


def recover_allowed_actions(raw: Any) -> Optional[list[str]]:
    """Best-effort: pull allowed_actions out of a raw body that failed S2."""
    if isinstance(raw, dict):
        aa = raw.get("allowed_actions")
        if isinstance(aa, list) and all(isinstance(a, str) for a in aa) and aa:
            return aa
    return None


def make_fallback(reason: str, allowed: Optional[list[str]], cfg: Config) -> ParsedPlan:
    return ParsedPlan(
        proposed_action=_safe_action(allowed),
        reason=f"fallback: {reason}"[:300],
        used_memory_ids=[],
        confidence=cfg.fallback_confidence,
    )
