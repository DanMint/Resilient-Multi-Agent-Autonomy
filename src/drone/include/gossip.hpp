// gossip.hpp -- peer-to-peer heartbeat + memory sharing over plain TCP.
//
// This is the multi-agent substrate and the propagation channel for the
// experiments: a compromised drone shares whatever memory it holds, and each
// receiver's own trust/quarantine logic (in MemoryStore) is the defense. A
// background thread listens for peer messages; the control loop pushes a
// heartbeat and one memory item to each peer every tick (best-effort).
#pragma once

#include <string>
#include <map>
#include <utility>
#include <mutex>
#include <atomic>
#include <thread>
#include "config.hpp"
#include "memory.hpp"

class Gossip {
public:
    Gossip(const Config& cfg, MemoryStore& mem);
    ~Gossip();

    void start();  // bind + spawn the listener thread
    void stop();   // stop the listener and join

    // Called each tick: broadcast heartbeat (self position) + newest memory.
    void push(double self_x, double self_y, const MemoryStore& mem);

    // Snapshot of peer positions learned from heartbeats.
    std::map<std::string, std::pair<double, double>> peer_positions() const;

private:
    void listen_loop();
    void handle_message(const std::string& line);

    const Config& cfg_;
    MemoryStore& mem_;
    int listen_fd_ = -1;
    std::thread thread_;
    std::atomic<bool> running_{false};
    mutable std::mutex pmu_;
    std::map<std::string, std::pair<double, double>> peers_;
};
