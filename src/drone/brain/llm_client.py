"""
llm_client.py -- thin wrapper around a FREE Hugging Face model.

It calls Hugging Face's OpenAI-compatible Inference router:

    POST https://router.huggingface.co/v1/chat/completions
    Authorization: Bearer <HF token>

A single Hugging Face token is enough -- every account gets monthly
Inference-Provider credits, and small instruct models are also served on the
free serverless tier. Get a token at https://huggingface.co/settings/tokens
(fine-grained, with "Make calls to Inference Providers").

Only the Python standard library is used, so the reasoning core needs no extra
dependencies. Timeouts and model/HTTP errors are surfaced as LLMError, which the
FSM turns into a fallback (S7 timeout/error -> SF).

Equivalent higher-level clients, if you'd rather add a dependency:

    # pip install huggingface_hub
    from huggingface_hub import InferenceClient
    client = InferenceClient(api_key=HF_TOKEN)
    out = client.chat.completions.create(model=HF_MODEL, messages=messages)

    # pip install openai
    from openai import OpenAI
    client = OpenAI(base_url="https://router.huggingface.co/v1", api_key=HF_TOKEN)
    out = client.chat.completions.create(model=HF_MODEL, messages=messages)

Free-friendly model ids to try (set HF_MODEL): "Qwen/Qwen2.5-7B-Instruct",
"meta-llama/Llama-3.2-3B-Instruct", "HuggingFaceH4/zephyr-7b-beta",
"mistralai/Mistral-7B-Instruct-v0.3". You can pin a provider with a suffix,
e.g. "Qwen/Qwen2.5-7B-Instruct:hf-inference".
"""

from __future__ import annotations

import json
import socket
import time
import urllib.error
import urllib.request

from .config import Config


class LLMError(RuntimeError):
    """Any failure talking to the model (timeout, HTTP error, bad payload)."""


class LLMConfigError(LLMError):
    """Misconfiguration, e.g. LLM planner requested without a token."""


class HuggingFaceClient:
    def __init__(self, cfg: Config) -> None:
        if not cfg.hf_api_token:
            raise LLMConfigError(
                "HF_API_TOKEN (or HF_TOKEN) is required to use the LLM planner")
        self.token = cfg.hf_api_token
        self.model = cfg.hf_model
        self.url = cfg.hf_base_url.rstrip("/") + "/chat/completions"
        self.timeout = cfg.llm_timeout_s
        self.max_tokens = cfg.llm_max_tokens
        self.temperature = cfg.llm_temperature

    def chat(self, messages: list[dict]) -> str:
        """Return the assistant message content, or raise LLMError."""
        payload = {
            "model": self.model,
            "messages": messages,
            "max_tokens": self.max_tokens,
            "temperature": self.temperature,
            "stream": False,
        }
        data = json.dumps(payload).encode()
        headers = {
            "Authorization": f"Bearer {self.token}",
            "Content-Type": "application/json",
        }

        # One retry if the model is warming up (503), then give up -> fallback.
        last_err: Exception | None = None
        for attempt in range(2):
            req = urllib.request.Request(self.url, data=data, headers=headers,
                                         method="POST")
            try:
                with urllib.request.urlopen(req, timeout=self.timeout) as resp:
                    body = json.loads(resp.read().decode())
                return self._extract(body)
            except urllib.error.HTTPError as e:
                detail = e.read().decode(errors="replace")[:400]
                if e.code == 503 and attempt == 0:
                    last_err = LLMError(f"model loading (503): {detail}")
                    time.sleep(1.5)
                    continue
                raise LLMError(f"HTTP {e.code}: {detail}") from e
            except (socket.timeout, TimeoutError) as e:
                raise LLMError(f"timeout after {self.timeout}s") from e
            except urllib.error.URLError as e:
                raise LLMError(f"network error: {e.reason}") from e
            except (ValueError, KeyError) as e:
                raise LLMError(f"bad response payload: {e}") from e

        raise LLMError(str(last_err) if last_err else "unknown LLM error")

    @staticmethod
    def _extract(body: dict) -> str:
        if isinstance(body, dict) and body.get("error"):
            raise LLMError(f"provider error: {body['error']}")
        try:
            content = body["choices"][0]["message"]["content"]
        except (KeyError, IndexError, TypeError) as e:
            raise LLMError(f"unexpected response shape: {e}") from e
        if not isinstance(content, str) or not content.strip():
            raise LLMError("empty completion")
        return content
