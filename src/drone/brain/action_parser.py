"""
action_parser.py -- S8 PARSE ACTION.

Take the reasoner's raw output and turn it into a validated ParsedPlan, or raise
ParseError (S8 bad JSON/action -> SF). Validation enforces the contract:
  * output is a JSON object with a string proposed_action,
  * proposed_action is exactly one of allowed_actions,
  * used_memory_ids are all *known* ids (trusted or quarantined -- S9 later
    rejects any that are quarantined),
  * confidence is a number, clamped to [0, 1].

The JSON extractor tolerates a model wrapping its object in ```json fences or
surrounding prose, which real LLMs frequently do.
"""

from __future__ import annotations

import json
from dataclasses import dataclass, field

from .context_builder import ContextView


class ParseError(ValueError):
    """Raised when the reasoner output can't be parsed/validated (S8 -> SF)."""


@dataclass
class ParsedPlan:
    proposed_action: str
    reason: str = ""
    used_memory_ids: list[str] = field(default_factory=list)
    confidence: float = 0.5


def _extract_json(text: str) -> dict:
    text = text.strip()
    # strip a leading ```json / ``` fence if present
    if text.startswith("```"):
        text = text.split("```", 2)[1] if text.count("```") >= 2 else text[3:]
        if text.lstrip().lower().startswith("json"):
            text = text.lstrip()[4:]
    try:
        obj = json.loads(text)
        if isinstance(obj, dict):
            return obj
    except ValueError:
        pass
    # fall back to the first balanced {...} block
    start = text.find("{")
    while start != -1:
        depth = 0
        for i in range(start, len(text)):
            if text[i] == "{":
                depth += 1
            elif text[i] == "}":
                depth -= 1
                if depth == 0:
                    try:
                        obj = json.loads(text[start:i + 1])
                        if isinstance(obj, dict):
                            return obj
                    except ValueError:
                        break
        start = text.find("{", start + 1)
    raise ParseError("no JSON object found in reasoner output")


def parse(raw: str, view: ContextView) -> ParsedPlan:
    if not isinstance(raw, str) or not raw.strip():
        raise ParseError("empty reasoner output")

    obj = _extract_json(raw)

    action = obj.get("proposed_action")
    if not isinstance(action, str) or not action.strip():
        raise ParseError("missing 'proposed_action'")
    action = action.strip()
    if action not in set(view.allowed_actions):
        raise ParseError(f"action '{action}' not in allowed_actions")

    reason = obj.get("reason", "")
    if not isinstance(reason, str):
        reason = str(reason)

    used = obj.get("used_memory_ids", [])
    if used is None:
        used = []
    if not isinstance(used, list) or not all(isinstance(x, str) for x in used):
        raise ParseError("'used_memory_ids' must be a list of strings")
    known = view.trusted_ids | view.quarantined_ids
    unknown = [x for x in used if x not in known]
    if unknown:
        raise ParseError(f"unknown memory id(s): {unknown}")

    conf = obj.get("confidence", 0.5)
    if isinstance(conf, bool) or not isinstance(conf, (int, float)):
        raise ParseError("'confidence' must be a number")
    conf = max(0.0, min(1.0, float(conf)))

    return ParsedPlan(proposed_action=action, reason=reason,
                      used_memory_ids=used, confidence=conf)
