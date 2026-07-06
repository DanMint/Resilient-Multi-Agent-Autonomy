"""
Python AI brain for a distributed CPS drone.

A localhost planning service: it receives a filtered planning context over HTTP
(POST /plan), reasons over an S0-S10 FSM, and returns a *proposed* action. It is
not the owner of the robot -- the C++ body owns memory, gossip, quarantine, the
safety shield, recovery, and actuation, and makes the final decision.

    Brain thinks. Body decides. Shield protects. Controller acts.

Importing this package pulls in only the dependency-free reasoning core, so the
FSM and its stages can be unit tested without fastapi/pydantic installed. The
HTTP server lives in `brain.main` and imports FastAPI lazily (i.e. only when you
import that module).
"""

from __future__ import annotations

from .config import Config
from .context_builder import ContextView, build_view
from .fsm import BrainFSM, FallbackSignal, State
from .planner import LLMPlanner, RulePlanner, make_planner
from .risk_gate import RiskAssessment, assess, select_mode
from .schemas import (MemoryItemData, PlanRequestData, SchemaError,
                      parse_plan_request)

__all__ = [
    "Config",
    "BrainFSM",
    "State",
    "FallbackSignal",
    "ContextView",
    "build_view",
    "RiskAssessment",
    "assess",
    "select_mode",
    "RulePlanner",
    "LLMPlanner",
    "make_planner",
    "MemoryItemData",
    "PlanRequestData",
    "SchemaError",
    "parse_plan_request",
]
