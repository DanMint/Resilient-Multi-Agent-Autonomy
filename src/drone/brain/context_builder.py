"""
context_builder.py -- S3 BUILD CONTEXT VIEW.

Turn a validated request into a *temporary* planning view. This is not memory:
the brain never persists it, never writes it back, and never gossips it. Trusted
and quarantined memories are kept in strictly separate buckets so nothing
downstream can accidentally treat a quarantined item as usable.
"""

from __future__ import annotations

from dataclasses import dataclass, field
from typing import Any

from .schemas import MemoryItemData, PlanRequestData


@dataclass
class ContextView:
    robot_id: str
    position: list[float]
    goal: Any
    allowed_actions: list[str]
    trusted: list[MemoryItemData] = field(default_factory=list)
    quarantined: list[MemoryItemData] = field(default_factory=list)
    trusted_ids: set[str] = field(default_factory=set)
    quarantined_ids: set[str] = field(default_factory=set)

    def goal_text(self) -> str:
        """Best-effort human-readable goal, for prompt building / relevance."""
        g = self.goal
        if isinstance(g, str):
            return g
        if isinstance(g, dict):
            for k in ("label", "description", "name", "target"):
                if k in g:
                    return str(g[k])
        return str(g)


def build_view(req: PlanRequestData) -> ContextView:
    return ContextView(
        robot_id=req.robot_id,
        position=list(req.position),
        goal=req.goal,
        allowed_actions=list(req.allowed_actions),
        trusted=list(req.trusted_memories),
        quarantined=list(req.quarantined_memories),
        trusted_ids={m.id for m in req.trusted_memories},
        quarantined_ids={m.id for m in req.quarantined_memories},
    )
