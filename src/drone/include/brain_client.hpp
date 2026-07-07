// brain_client.hpp -- talks to the Python brain over localhost HTTP.
//
// The brain is an *untrusted proposer*: it returns a suggested action, never a
// command. Any failure (brain down, timeout, malformed JSON, non-200) collapses
// to a safe fallback proposal so the control loop always has something valid.
#pragma once

#include <string>
#include <vector>
#include "world_state.hpp"
#include "memory.hpp"

struct Proposal {
    std::string proposed_action = "WAIT";
    std::string reason;
    std::vector<std::string> used_memory_ids;
    double confidence = 0.0;
    std::string mode = "unreachable";  // normal | cautious | fallback | unreachable
};

class BrainClient {
public:
    BrainClient(std::string host, int port, int timeout_s);

    // Build the plan request from current state + memory, POST /plan, parse the
    // proposal. Never throws; returns a fallback proposal on any error.
    Proposal request_plan(const WorldState& w, const MemoryStore& mem);

private:
    std::string host_;
    int port_;
    int timeout_s_;
};
