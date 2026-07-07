# Resilient-Multi-Agent-Autonomy

A distributed cyber-physical system (CPS) testbed for studying how teams of
autonomous drones continue operating **safely even when their AI reasoning is
compromised**.

## What this project is

Each drone uses a Large Language Model (LLM) to reason and propose actions.
Because LLMs can be manipulated through prompt injection, poisoned memory, or
malicious inter-agent messages (**semantic compromise**), they are treated as
**untrusted**.

This project explores one central question:

> **How can a multi-agent CPS continue its mission safely, contain compromise,
> and recover when an agent's reasoning layer can no longer be trusted?**

Rather than trusting the LLM to make decisions, the system treats it as an
untrusted **proposer**. A trusted runtime retains authority over safety,
memory, communication, and actuation, assuming compromise is inevitable and
focusing on containment and recovery instead of prevention.

The research is organized around three objectives:

- **Distributed CPS testbed** for measuring how semantic compromise propagates
  through multi-agent teams.
- **Containment controller** that limits the impact of compromised agents while
  preserving mission execution.
- **Adaptive attacks and recovery** mechanisms for isolating, degrading, and
  restoring compromised agents.

## Architecture at a glance

Each drone runs two cooperating parts inside its own container:

- **Body (C++ runtime) — the trusted authority.** Owns long-term memory,
  provenance, trust/quarantine, peer gossip, the hard safety shield, recovery,
  and the simulator/actuator interface. It makes the final command decision.
- **Brain (Python service) — the untrusted reasoner.** Receives a filtered
  planning context and returns a *proposed* action with an explanation and a
  confidence score. It cannot write long-term memory, talk to peers, or command
  actuators.

The invariant that holds the design together:

> **Brain thinks. Body decides. Shield protects. Controller acts.**

## The Brain

The brain is a small localhost HTTP service (FastAPI) that lives inside each
drone container. The body calls `POST /plan` with the current context, and the
brain returns JSON—a proposal only. `GET /health` reports liveness and the
active reasoning backend.

Every request passes through a finite state machine that funnels invalid,
risky, or malformed inputs into a safe fallback:

- **S1 Receive → S2 Validate** the request schema.
- **S3 Build context view** — separate trusted and quarantined memory.
- **S4 Risk gate → S5 Select mode** — assess semantic risk and choose
  *normal* or *cautious* planning.
- **S6 Build prompt → S7 Reason** — invoke the rule planner or an LLM.
- **S8 Parse → S9 Self-check** — validate actions and reject unsafe reasoning.
- **S10 Respond** — return the proposal.
- **SF Fallback** — return a conservative
  `WAIT` / `SCAN` / `REQUEST_VERIFICATION` action.

Because the brain is stateless and only proposes actions, a compromised LLM can
never directly control the drone. Every proposal must pass validation inside
the brain and the body's independent safety shield before execution.

## Status

- ✅ **Brain — v1 complete.** FSM implemented and tested. Supports both an
  offline deterministic planner and Hugging Face inference. Always returns
  validated proposals with safe fallback behavior.
- 🚧 **Body — in progress.** Startup networking and health checks are complete.
  Safety enforcement, long-term memory, gossip, containment, and recovery are
  under active development.

## Quick Start (Brain)

```bash
cd poison-cps-brain
pip install -r requirements.txt

# Offline deterministic planner
uvicorn src.drone.brain.main:app --host 127.0.0.1 --port 7000

# Hugging Face model
BRAIN_USE_LLM=1 HF_API_TOKEN=hf_xxx HF_MODEL="openai/gpt-oss-120b:cheapest" \
uvicorn src.drone.brain.main:app --host 127.0.0.1 --port 7000

# Running quick tests
python src/drone/brain/tests/demo_client.py                    # normal plan
python src/drone/brain/tests/demo_client.py --scenario inject  # injection/quarantined context
python src/drone/brain/tests/demo_client.py --scenario bad     # malformed request -> safe fallback
```
