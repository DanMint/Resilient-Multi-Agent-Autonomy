// main.cpp -- drone body entry point.
#include "config.hpp"
#include "drone.hpp"

#include <iostream>

int main() {
    Config cfg = Config::from_env();

    std::cout << "=== drone body ===\n"
              << "  id    : " << cfg.drone_id << "\n"
              << "  ip    : " << (cfg.self_ip.empty() ? "(unset)" : cfg.self_ip) << "\n"
              << "  peers : " << cfg.peers.size() << "\n";
    for (const auto& [id, ip] : cfg.peers) std::cout << "    - " << id << " @ " << ip << "\n";

    Drone drone(cfg);
    drone.run();
    return 0;
}
