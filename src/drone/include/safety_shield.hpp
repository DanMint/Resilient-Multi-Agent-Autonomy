// safety_shield.hpp -- the hard, brain-independent safety layer.
//
// The shield never trusts the brain. It re-checks every proposal against
// invariants the brain cannot override: memory integrity, the action whitelist,
// a confidence floor, the battery floor, the geofence, and peer separation.
// Its verdict decides what the controller is actually allowed to do.
#pragma once

#include <string>
#include "config.hpp"
#include "world_state.hpp"
#include "memory.hpp"
#include "brain_client.hpp"

enum class Verdict { ALLOW, BLOCK, DEGRADE, RECOVER };

struct Decision {
    Verdict verdict = Verdict::BLOCK;
    std::string action = "WAIT";
    std::string reason;
};

class SafetyShield {
public:
    explicit SafetyShield(const Config& cfg) : cfg_(cfg) {}

    Decision review(const Proposal& p, const WorldState& w, const MemoryStore& mem) const;

private:
    const Config& cfg_;
    bool is_allowed(const std::string& action, const WorldState& w) const;
    std::string recover_action(const WorldState& w) const;
};
