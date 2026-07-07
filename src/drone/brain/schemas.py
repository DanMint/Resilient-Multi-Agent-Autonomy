"""
schemas.py -- the data contract.

Two layers live here on purpose:

  * Pydantic models (MemoryItem / PlanRequest / PlanResponse) describe the HTTP
    boundary and drive FastAPI's OpenAPI docs. They are imported by main.py.
  * Plain dataclasses (MemoryItemData / PlanRequestData) plus parse_plan_request()
    are what the FSM actually reasons over. They have ZERO third-party deps, so
    the whole reasoning core imports and unit-tests without pydantic/fastapi
    installed. This also matches the safety posture: the brain re-validates every
    request itself (S2) rather than trusting that some upstream layer did.

The pydantic import is guarded so this module still imports in a bare
environment (e.g. the offline test suite).
"""

from __future__ import annotations

from dataclasses import dataclass, field
from typing import Any, Optional

# --------------------------------------------------------------------------- #
# Optional pydantic layer (HTTP boundary / docs only)
# --------------------------------------------------------------------------- #
try:
    from pydantic import BaseModel, Field

    class MemoryItem(BaseModel):
        """A single memory record handed to the brain by the C++ runtime."""
        id: str
        content: str = ""
        trust_score: Optional[float] = None      # 0.0 .. 1.0
        hop_count: int = 0                        # provenance distance
        source: Optional[str] = None

    class PlanRequest(BaseModel):
        """POST /plan body. The runtime pre-approves everything in here."""
        robot_id: str
        position: Any                             # [x, y(, z)] or {x, y}
        goal: Any                                 # label, coord, or descriptor
        allowed_actions: list[str]
        trusted_memories: list[MemoryItem] = Field(default_factory=list)
        quarantined_memories: list[MemoryItem] = Field(default_factory=list)

    class PlanResponse(BaseModel):
        """Response body. A *proposal* only -- the C++ body still decides."""
        proposed_action: str
        reason: str = ""
        used_memory_ids: list[str] = Field(default_factory=list)
        confidence: float = 0.0
        mode: str = "normal"                      # normal | cautious | fallback

    _HAS_PYDANTIC = True
except Exception:                                 # pydantic not installed
    MemoryItem = PlanRequest = PlanResponse = None  # type: ignore
    _HAS_PYDANTIC = False


# --------------------------------------------------------------------------- #
# FSM-facing dataclasses (dependency-free)
# --------------------------------------------------------------------------- #
@dataclass
class MemoryItemData:
    id: str
    content: str = ""
    trust_score: Optional[float] = None
    hop_count: int = 0
    source: Optional[str] = None


@dataclass
class PlanRequestData:
    robot_id: str
    position: Any
    goal: Any
    allowed_actions: list[str]
    trusted_memories: list[MemoryItemData] = field(default_factory=list)
    quarantined_memories: list[MemoryItemData] = field(default_factory=list)


class SchemaError(ValueError):
    """Raised by parse_plan_request when the request is malformed (S2 -> SF)."""


# --------------------------------------------------------------------------- #
# S2 validator -- plain Python, no dependencies
# --------------------------------------------------------------------------- #
def _as_number_list(val: Any, name: str) -> list[float]:
    if isinstance(val, dict):
        keys = [k for k in ("x", "y", "z") if k in val]
        if not keys:
            raise SchemaError(f"'{name}' dict must contain x/y[/z]")
        val = [val[k] for k in keys]
    if not isinstance(val, (list, tuple)) or not (2 <= len(val) <= 3):
        raise SchemaError(f"'{name}' must be a 2-3 element coordinate")
    out: list[float] = []
    for v in val:
        if isinstance(v, bool) or not isinstance(v, (int, float)):
            raise SchemaError(f"'{name}' must contain numbers")
        out.append(float(v))
    return out


def _parse_memory(raw: Any, bucket: str) -> MemoryItemData:
    if not isinstance(raw, dict):
        raise SchemaError(f"each item in '{bucket}' must be an object")
    mid = raw.get("id")
    if not isinstance(mid, str) or not mid.strip():
        raise SchemaError(f"memory in '{bucket}' missing a string 'id'")
    content = raw.get("content", raw.get("text", ""))
    if content is None:
        content = ""
    if not isinstance(content, str):
        raise SchemaError(f"memory '{mid}' content must be a string")
    trust = raw.get("trust_score")
    if trust is not None:
        if isinstance(trust, bool) or not isinstance(trust, (int, float)):
            raise SchemaError(f"memory '{mid}' trust_score must be a number")
        trust = float(trust)
    hop = raw.get("hop_count", 0)
    if isinstance(hop, bool) or not isinstance(hop, int):
        raise SchemaError(f"memory '{mid}' hop_count must be an integer")
    source = raw.get("source")
    if source is not None and not isinstance(source, str):
        raise SchemaError(f"memory '{mid}' source must be a string")
    return MemoryItemData(id=mid, content=content, trust_score=trust,
                          hop_count=hop, source=source)


def parse_plan_request(raw: Any) -> PlanRequestData:
    """
    S2 VALIDATE SCHEMA. Turn an untrusted raw body into a typed request or raise
    SchemaError. Required fields per the HTTP contract: robot_id, position, goal,
    allowed_actions, trusted_memories, quarantined_memories (the last may be
    omitted / empty).
    """
    if not isinstance(raw, dict):
        raise SchemaError("request body must be a JSON object")

    robot_id = raw.get("robot_id")
    if not isinstance(robot_id, str) or not robot_id.strip():
        raise SchemaError("'robot_id' is required and must be a non-empty string")

    if "position" not in raw:
        raise SchemaError("'position' is required")
    position = _as_number_list(raw["position"], "position")

    goal = raw.get("goal")
    if goal is None or (isinstance(goal, str) and not goal.strip()):
        raise SchemaError("'goal' is required")

    actions = raw.get("allowed_actions")
    if not isinstance(actions, list) or not actions:
        raise SchemaError("'allowed_actions' must be a non-empty list")
    allowed: list[str] = []
    for a in actions:
        if not isinstance(a, str) or not a.strip():
            raise SchemaError("'allowed_actions' must contain non-empty strings")
        allowed.append(a.strip())

    def _mem_list(key: str) -> list[MemoryItemData]:
        val = raw.get(key, [])
        if val is None:
            return []
        if not isinstance(val, list):
            raise SchemaError(f"'{key}' must be a list")
        return [_parse_memory(m, key) for m in val]

    trusted = _mem_list("trusted_memories")
    quarantined = _mem_list("quarantined_memories")

    return PlanRequestData(
        robot_id=robot_id.strip(),
        position=position,
        goal=goal,
        allowed_actions=allowed,
        trusted_memories=trusted,
        quarantined_memories=quarantined,
    )
