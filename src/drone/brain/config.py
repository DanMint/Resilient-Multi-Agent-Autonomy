"""
config.py -- runtime configuration for the Python AI brain.

Everything is environment-driven so the same image runs offline (rule-based
planner, zero network) or against a *free* Hugging Face model (set BRAIN_USE_LLM=1
and provide HF_API_TOKEN). Nothing here talks to the simulator or actuators --
per the design invariant, the brain only proposes.

    Brain thinks. Body decides. Shield protects. Controller acts.
"""

from __future__ import annotations

import os
from dataclasses import dataclass


def _get_bool(name: str, default: bool) -> bool:
    raw = os.environ.get(name)
    if raw is None:
        return default
    return raw.strip().lower() in {"1", "true", "yes", "on"}


def _get_float(name: str, default: float) -> float:
    try:
        return float(os.environ[name])
    except (KeyError, ValueError):
        return default


def _get_int(name: str, default: int) -> int:
    try:
        return int(os.environ[name])
    except (KeyError, ValueError):
        return default


@dataclass
class Config:
    # --- HTTP server (localhost only; the C++ body calls us) ----------------
    host: str = "127.0.0.1"
    port: int = 7000                       # POST http://127.0.0.1:7000/plan

    # --- reasoner backend ---------------------------------------------------
    use_llm: bool = False                  # False => deterministic rule planner
    # Free Hugging Face inference (OpenAI-compatible router). A single HF token
    # is enough; every HF account gets monthly Inference-Provider credits, and
    # small models are also served on the free serverless tier.
    hf_api_token: str = ""
    hf_model: str = "Qwen/Qwen2.5-7B-Instruct"
    hf_base_url: str = "https://router.huggingface.co/v1"
    llm_timeout_s: float = 20.0
    llm_max_tokens: int = 512
    llm_temperature: float = 0.2           # low => steadier JSON

    # --- risk gate / mode selection (S4, S5) --------------------------------
    cautious_threshold: float = 0.5        # risk >= this => cautious planning
    low_trust_threshold: float = 0.5       # trusted mem below this adds risk
    max_hops: int = 3                      # hop_count above this adds risk

    # --- self-check / response shaping (S9, S10) ----------------------------
    max_confidence: float = 0.98           # above this is "overstated"
    cautious_confidence_cap: float = 0.6   # cautious plans can't be very sure
    fallback_confidence: float = 0.2

    # --- misc ---------------------------------------------------------------
    debug: bool = False                    # include fsm_trace in the response
    log_level: str = "INFO"

    @classmethod
    def from_env(cls) -> "Config":
        return cls(
            host=os.environ.get("BRAIN_HOST", cls.host),
            port=_get_int("BRAIN_PORT", cls.port),
            use_llm=_get_bool("BRAIN_USE_LLM", cls.use_llm),
            # accept either HF_API_TOKEN or the conventional HF_TOKEN
            hf_api_token=os.environ.get("HF_API_TOKEN")
            or os.environ.get("HF_TOKEN", cls.hf_api_token),
            hf_model=os.environ.get("HF_MODEL", cls.hf_model),
            hf_base_url=os.environ.get("HF_BASE_URL", cls.hf_base_url),
            llm_timeout_s=_get_float("BRAIN_LLM_TIMEOUT", cls.llm_timeout_s),
            llm_max_tokens=_get_int("BRAIN_LLM_MAX_TOKENS", cls.llm_max_tokens),
            llm_temperature=_get_float("BRAIN_LLM_TEMPERATURE", cls.llm_temperature),
            cautious_threshold=_get_float("BRAIN_CAUTIOUS_THRESHOLD", cls.cautious_threshold),
            low_trust_threshold=_get_float("BRAIN_LOW_TRUST", cls.low_trust_threshold),
            max_hops=_get_int("BRAIN_MAX_HOPS", cls.max_hops),
            debug=_get_bool("BRAIN_DEBUG", cls.debug),
            log_level=os.environ.get("BRAIN_LOG_LEVEL", cls.log_level),
        )
