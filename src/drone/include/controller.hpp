// controller.hpp -- applies the shield's decision to the world (actuation).
//
// This is the only place the drone's state actually changes as a result of a
// decision. ALLOW executes at full effect, DEGRADE at reduced effect, BLOCK
// substitutes a safe WAIT, and RECOVER drops into degraded autonomy and holds.
#pragma once

#include "config.hpp"
#include "world_state.hpp"
#include "safety_shield.hpp"

class Controller {
public:
    explicit Controller(const Config& cfg) : cfg_(cfg) {}

    void apply(const Decision& d, WorldState& w) const;

private:
    const Config& cfg_;
    void execute_action(const std::string& action, WorldState& w, double step_scale) const;
};
