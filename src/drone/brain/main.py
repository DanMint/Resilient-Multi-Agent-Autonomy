"""
main.py -- FastAPI server for the Python AI brain.

Exposes exactly two endpoints:
  * GET  /health -- liveness + which reasoner is active
  * POST /plan   -- run the FSM and return a proposed action

The brain is a localhost service inside one drone container. It returns a
proposal only; the C++ body applies the hard safety shield and makes the final
command decision.

    Brain thinks. Body decides. Shield protects. Controller acts.

Run:
    pip install -r requirements.txt
    # offline, deterministic rule planner:
    uvicorn brain.main:app --host 127.0.0.1 --port 7000
    # or with a free Hugging Face model:
    BRAIN_USE_LLM=1 HF_API_TOKEN=hf_xxx uvicorn brain.main:app --port 7000
"""

from __future__ import annotations

import logging
from typing import Any

from fastapi import FastAPI, Request

from .config import Config
from .fsm import BrainFSM
from .planner import LLMPlanner
from .schemas import PlanResponse

cfg = Config.from_env()
logging.basicConfig(level=getattr(logging, cfg.log_level.upper(), logging.INFO),
                    format="%(asctime)s %(name)s %(levelname)s %(message)s")
log = logging.getLogger("brain.main")

fsm = BrainFSM(cfg)
app = FastAPI(title="Drone AI Brain", version="2.0.0",
              description="Local planning brain. Proposes actions; the C++ body "
                          "decides and enforces safety.")


@app.get("/health")
async def health() -> dict:
    using_llm = isinstance(fsm.planner, LLMPlanner)
    return {
        "status": "ok",
        "service": "drone-brain",
        "state": "S0_IDLE",
        "reasoner": "llm" if using_llm else "rule",
        "model": cfg.hf_model if using_llm else None,
    }


@app.post("/plan", response_model=PlanResponse)
async def plan(request: Request) -> Any:
    # Accept the raw body so malformed input is handled by the FSM (S2 -> SF)
    # as a safe fallback proposal, rather than a 422 the C++ body must handle.
    try:
        raw = await request.json()
    except Exception:
        raw = None
    return fsm.plan(raw)
