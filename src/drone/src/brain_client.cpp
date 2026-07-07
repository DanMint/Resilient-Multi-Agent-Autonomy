// brain_client.cpp
#include "brain_client.hpp"
#include "http_client.hpp"
#include "json.hpp"

#include <exception>

BrainClient::BrainClient(std::string host, int port, int timeout_s)
    : host_(std::move(host)), port_(port), timeout_s_(timeout_s) {}

namespace {

mj::Value memories_to_json(const std::vector<MemoryItem>& items) {
    mj::Value arr = mj::Value::Array();
    for (const auto& m : items) {
        mj::Value o = mj::Value::Object();
        o.set("id", mj::Value::Str(m.id));
        o.set("content", mj::Value::Str(m.content));
        o.set("trust_score", mj::Value::Num(m.trust_score));
        o.set("hop_count", mj::Value::Num(m.hop_count));
        arr.push_back(o);
    }
    return arr;
}

Proposal fallback(const std::string& why) {
    Proposal p;
    p.proposed_action = "WAIT";
    p.reason = why;
    p.confidence = 0.0;
    p.mode = "unreachable";
    return p;
}

}  // namespace

Proposal BrainClient::request_plan(const WorldState& w, const MemoryStore& mem) {
    // ---- build request ----
    mj::Value req = mj::Value::Object();
    req.set("robot_id", mj::Value::Str(w.id));

    mj::Value pos = mj::Value::Array();
    pos.push_back(mj::Value::Num(w.x));
    pos.push_back(mj::Value::Num(w.y));
    req.set("position", pos);

    req.set("goal", mj::Value::Str(w.goal));

    mj::Value acts = mj::Value::Array();
    for (const auto& a : w.allowed_actions) acts.push_back(mj::Value::Str(a));
    req.set("allowed_actions", acts);

    req.set("trusted_memories", memories_to_json(mem.trusted()));
    req.set("quarantined_memories", memories_to_json(mem.quarantined()));

    const std::string body = req.dump();

    // ---- call brain + parse (never throw out of this function) ----
    try {
        HttpResponse res = http::post(host_, port_, "/plan", body, "application/json", timeout_s_);
        if (res.status != 200) return fallback("brain returned HTTP " + std::to_string(res.status));

        mj::Value r = mj::parse(res.body);

        Proposal p;
        p.mode = "normal";  // will be overwritten if present
        if (const mj::Value* v = r.find("proposed_action")) p.proposed_action = v->as_string("WAIT");
        if (const mj::Value* v = r.find("reason"))          p.reason = v->as_string();
        if (const mj::Value* v = r.find("confidence"))      p.confidence = v->as_number(0.0);
        if (const mj::Value* v = r.find("mode"))            p.mode = v->as_string("normal");
        if (const mj::Value* v = r.find("used_memory_ids"))
            if (v->is_array())
                for (const auto& e : v->arr) p.used_memory_ids.push_back(e.as_string());
        return p;
    } catch (const std::exception& e) {
        return fallback(std::string("brain unreachable: ") + e.what());
    } catch (...) {
        return fallback("brain unreachable: unknown error");
    }
}
