// utils.cpp
#include "utils.hpp"

#include <cstdlib>
#include <iostream>
#include <string>

namespace Utils {

int check_network(const Config& cfg) {
    if (cfg.peers.empty()) {
        std::cout << "[" << cfg.drone_id << "] no peers configured\n";
        return 0;
    }
    std::cout << "[" << cfg.drone_id << "] checking peer reachability via ping...\n";
    int reachable = 0;
    for (const auto& [id, ip] : cfg.peers) {
        std::string cmd = "ping -c 1 -W 1 " + ip + " > /dev/null 2>&1";
        if (std::system(cmd.c_str()) == 0) {
            std::cout << "  peer " << id << " (" << ip << ") reachable\n";
            ++reachable;
        } else {
            std::cout << "  peer " << id << " (" << ip << ") not reachable yet\n";
        }
    }
    std::cout << "[" << cfg.drone_id << "] " << reachable << "/" << cfg.peers.size()
              << " peers reachable\n";
    return reachable;
}

}  // namespace Utils
