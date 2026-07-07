"""
planner.py -- S7 REASONER.

Two backends, one interface: .plan(PlannerInput) -> raw JSON string. Returning a
string (not a dict) keeps S8 (parse/validate) meaningful for both -- the rule
planner serialises its decision exactly like a model would emit it.

  * RulePlanner  (V1): deterministic, no network. The default, so the service
                       runs and is testable fully offline.
  * LLMPlanner   (V2): calls a free Hugging Face model via HuggingFaceClient.
                       Errors/timeouts propagate as LLMError -> the FSM falls
                       back (S7 -> SF).

Neither planner ever executes anything. It only proposes.
"""

from __future__ import annotations

import json

from .config import Config
from .llm_client import HuggingFaceClient, LLMConfigError, LLMError
from .prompt_builder import PlannerInput

# Actions the rule planner treats as goal-directed / conservative.
_MOVE_ACTIONS = ("MOVE", "MOVE_TO", "GOTO", "GO_TO", "NAVIGATE", "ADVANCE", "PROCEED")
_CONSERVATIVE = ("WAIT", "SCAN", "REQUEST_VERIFICATION", "HOLD", "IDLE")


def _first_allowed(candidates, allowed: list[str]) -> str | None:
    allowed_set = set(allowed)
    for c in candidates:
        if c in allowed_set:
            return c
    return None


class RulePlanner:
    """A transparent baseline planner (paper-friendly: fully explainable)."""

    def plan(self, pin: PlannerInput) -> str:
        view = pin.view
        allowed = view.allowed_actions

        if pin.mode == "cautious":
            action = _first_allowed(_CONSERVATIVE, allowed) or allowed[0]
            reason = "cautious mode: " + (
                "; ".join(pin.risk.factors) if pin.risk.factors else "elevated risk")
            return json.dumps({
                "proposed_action": action,
                "reason": reason[:300],
                "used_memory_ids": [],
                "confidence": 0.4,
            })

        # normal mode: prefer a goal-directed action if one is allowed
        action = _first_allowed(_MOVE_ACTIONS, allowed)
        if action is None:
            action = _first_allowed(
                [a for a in allowed if a not in _CONSERVATIVE], allowed) or allowed[0]

        goal_text = view.goal_text().lower()
        used = [m.id for m in view.trusted
                if goal_text and any(tok in (m.content or "").lower()
                                     for tok in goal_text.split())][:3]
        reason = f"advancing toward goal '{view.goal_text()}' via {action}"
        return json.dumps({
            "proposed_action": action,
            "reason": reason[:300],
            "used_memory_ids": used,
            "confidence": 0.75,
        })


class LLMPlanner:
    def __init__(self, cfg: Config) -> None:
        self.client = HuggingFaceClient(cfg)

    def plan(self, pin: PlannerInput) -> str:
        # may raise LLMError; the FSM catches it at S7 and falls back
        return self.client.chat(pin.messages)


def make_planner(cfg: Config, log=None):
    """
    Pick a backend. LLM if explicitly enabled and a token is present; otherwise
    the deterministic rule planner (also the graceful fallback if the LLM is
    requested but misconfigured).
    """
    if cfg.use_llm:
        try:
            planner = LLMPlanner(cfg)
            if log:
                log.info("reasoner: Hugging Face model '%s'", cfg.hf_model)
            return planner
        except LLMConfigError as e:
            if log:
                log.warning("LLM requested but %s; using rule planner", e)
    if log:
        log.info("reasoner: rule-based planner (offline)")
    return RulePlanner()
