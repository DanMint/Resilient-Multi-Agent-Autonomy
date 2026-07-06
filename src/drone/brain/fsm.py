"""
fsm.py -- the Finite State Machine coordinator.

One request enters at S0 IDLE via POST /plan and walks S1 -> S10. Any invalid,
risky, or malformed path raises FallbackSignal, which is caught once and routed
through SF FALLBACK before S10 RESPOND. The brain never reaches the simulator.

    Brain thinks. Body decides. Shield protects. Controller acts.

The FSM is deliberately dependency-free (no fastapi/pydantic) so it can be unit
tested offline. A planner can be injected for testing; otherwise one is chosen
from config (rule-based by default, free Hugging Face LLM if enabled).
"""

from __future__ import annotations

import logging
from enum import Enum
from typing import Any, Optional

from . import action_parser, context_builder, fallback, prompt_builder, risk_gate, self_check
from .config import Config
from .llm_client import LLMError
from .planner import make_planner
from .schemas import SchemaError, parse_plan_request

log = logging.getLogger("brain.fsm")


class State(str, Enum):
    S0_IDLE = "S0_IDLE"
    S1_RECEIVE = "S1_RECEIVE"
    S2_VALIDATE = "S2_VALIDATE"
    S3_CONTEXT = "S3_CONTEXT"
    S4_RISK_GATE = "S4_RISK_GATE"
    S5_SELECT_MODE = "S5_SELECT_MODE"
    S6_BUILD_PROMPT = "S6_BUILD_PROMPT"
    S7_REASONER = "S7_REASONER"
    S8_PARSE = "S8_PARSE"
    S9_SELF_CHECK = "S9_SELF_CHECK"
    SF_FALLBACK = "SF_FALLBACK"
    S10_RESPOND = "S10_RESPOND"


class FallbackSignal(Exception):
    """Raised by any state to converge on SF. Carries a stage + reason."""

    def __init__(self, stage: str, reason: str) -> None:
        super().__init__(f"[{stage}] {reason}")
        self.stage = stage
        self.reason = reason


class BrainFSM:
    def __init__(self, cfg: Config, planner: Optional[object] = None) -> None:
        self.cfg = cfg
        self.planner = planner if planner is not None else make_planner(cfg, log)
        self.last_trace: list[str] = []

    # ------------------------------------------------------------------ #
    # public entry point (called by the /plan handler for every request)
    # ------------------------------------------------------------------ #
    def plan(self, raw: Any) -> dict:
        self.last_trace = [State.S1_RECEIVE.value]     # S0->S1 on POST /plan
        allowed_hint = fallback.recover_allowed_actions(raw)
        try:
            req = self._s2_validate(raw)
            allowed_hint = req.allowed_actions
            view = self._s3_context(req)
            risk = self._s4_risk_gate(view)
            mode = self._s5_select_mode(risk)
            pin = self._s6_build_prompt(view, mode, risk)
            raw_plan = self._s7_reason(pin)
            parsed = self._s8_parse(raw_plan, view)
            self._s9_self_check(parsed, view, mode)
            return self._s10_respond(parsed, mode)
        except FallbackSignal as f:
            log.info("fallback at %s: %s", f.stage, f.reason)
            self._trace(State.SF_FALLBACK)
            fb = fallback.make_fallback(f.reason, allowed_hint, self.cfg)
            return self._s10_respond(fb, "fallback")

    # ------------------------------------------------------------------ #
    # states
    # ------------------------------------------------------------------ #
    def _trace(self, s: State) -> None:
        self.last_trace.append(s.value)

    def _s2_validate(self, raw: Any):
        self._trace(State.S2_VALIDATE)
        try:
            return parse_plan_request(raw)
        except SchemaError as e:
            raise FallbackSignal("S2", f"invalid schema: {e}") from e

    def _s3_context(self, req):
        self._trace(State.S3_CONTEXT)
        return context_builder.build_view(req)

    def _s4_risk_gate(self, view):
        self._trace(State.S4_RISK_GATE)
        return risk_gate.assess(view, self.cfg)

    def _s5_select_mode(self, risk) -> str:
        self._trace(State.S5_SELECT_MODE)
        return risk_gate.select_mode(risk, self.cfg)

    def _s6_build_prompt(self, view, mode, risk):
        self._trace(State.S6_BUILD_PROMPT)
        return prompt_builder.build(view, mode, risk)

    def _s7_reason(self, pin) -> str:
        self._trace(State.S7_REASONER)
        try:
            return self.planner.plan(pin)
        except LLMError as e:
            raise FallbackSignal("S7", f"reasoner timeout/error: {e}") from e

    def _s8_parse(self, raw_plan, view):
        self._trace(State.S8_PARSE)
        try:
            return action_parser.parse(raw_plan, view)
        except action_parser.ParseError as e:
            raise FallbackSignal("S8", f"bad JSON/action: {e}") from e

    def _s9_self_check(self, parsed, view, mode) -> None:
        self._trace(State.S9_SELF_CHECK)
        try:
            self_check.check(parsed, view, mode, self.cfg)
        except self_check.SelfCheckError as e:
            raise FallbackSignal("S9", f"self-check: {e}") from e

    def _s10_respond(self, plan, mode: str) -> dict:
        self._trace(State.S10_RESPOND)
        resp = {
            "proposed_action": plan.proposed_action,
            "reason": plan.reason,
            "used_memory_ids": list(plan.used_memory_ids),
            "confidence": max(0.0, min(1.0, float(plan.confidence))),
            "mode": mode,
        }
        if self.cfg.debug:
            resp["fsm_trace"] = list(self.last_trace)
        return resp
