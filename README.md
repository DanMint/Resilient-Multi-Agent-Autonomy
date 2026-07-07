# Resilient-Multi-Agent-Autonomy

## How to run brain for the demo


export HF_API_TOKEN=hf_your_NEW_token          
export HF_MODEL="openai/gpt-oss-120b:cheapest"
export BRAIN_USE_LLM=1
uvicorn src.drone.brain.main:app --host 127.0.0.1 --port 7000

---- tests of bain ----
python demo_client.py                    # normal plan
python demo_client.py --scenario inject  # injection/quarantined context
python demo_client.py --scenario bad     # malformed request -> safe fallback