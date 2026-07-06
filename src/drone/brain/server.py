from fastapi import FastAPI, Request

app = FastAPI(title="Drone AI Brain", version="2.0.0",
              description="Local planning brain. Proposes actions; the C++ body "
                          "decides and enforces safety.")

@app.get("/health")
async def health() -> dict:
    return {
        "status": "ok",
        "service": "drone-brain",
        "state": "S0_IDLE",
        # "reasoner": "llm" if using_llm else "rule",
        # "model": cfg.hf_model if using_llm else None,
    }

@app.post("/plan")
async def plan() -> Any:
    try:
        raw = await request.json()
    except:
        raw = None

    return "STILL DEVELOPOING"