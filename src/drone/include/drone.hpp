// drone.hpp -- the body orchestrator and final authority.
//
// Owns all subsystems and runs the control loop. Each tick it: refreshes peer
// positions from gossip, asks the brain for a proposal, runs it through the
// safety shield, applies the resulting decision, tracks anomalies (escalating
// to degraded autonomy after repeated trouble), and gossips its state.
//
//   Brain thinks. Body decides. Shield protects. Controller acts.
#pragma once

#include "config.hpp"
#include "world_state.hpp"
#include "memory.hpp"
#include "brain_client.hpp"
#include "safety_shield.hpp"
#include "controller.hpp"
#include "gossip.hpp"

class Drone {
public:
    explicit Drone(const Config& cfg);
    void run();  // startup checks + control loop (blocks until signalled)

private:
    void seed_memory();
    void tick(long n);

    Config cfg_;                 // declared first: referenced by shield_/controller_/gossip_
    WorldState world_;
    MemoryStore memory_;
    BrainClient brain_;
    SafetyShield shield_;
    Controller controller_;
    Gossip gossip_;

    int consecutive_anomalies_ = 0;
    bool arrived_logged_ = false;
};
