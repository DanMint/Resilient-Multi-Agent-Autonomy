// config.hpp -- all runtime configuration, read once from environment variables.
#pragma once

#include <string>
#include <map>
#include <vector>

struct Config {
    // identity + peers
    std::string drone_id = "D?";
    std::string self_ip;
    std::map<std::string, std::string> peers;  // id -> ip (excludes self)

    // brain (localhost HTTP service)
    std::string brain_host = "127.0.0.1";
    int brain_port = 7000;
    int brain_timeout_s = 25;

    // gossip
    int gossip_port = 7100;
    int gossip_timeout_s = 1;

    // control loop
    double tick_seconds = 2.0;

    // mission / world
    double start_x = 0.0, start_y = 0.0;
    double goal_x = 10.0, goal_y = 0.0;
    std::string goal = "waypoint-A";
    std::vector<std::string> allowed_actions = {"MOVE", "WAIT", "SCAN", "REQUEST_VERIFICATION"};

    // dynamics
    double move_step = 1.0;
    double arrive_radius = 0.75;

    // battery
    double battery_start = 100.0;
    double battery_floor = 15.0;
    double move_cost = 1.0;
    double scan_cost = 0.5;
    double idle_cost = 0.1;

    // safety shield
    double geo_min_x = -1e9, geo_min_y = -1e9, geo_max_x = 1e9, geo_max_y = 1e9;  // effectively off unless set
    double min_peer_distance = 0.75;   // collision threshold
    double min_confidence = 0.35;      // below this -> block
    int    max_hops = 3;               // memory beyond this many hops -> quarantine
    double quarantine_trust = 0.5;     // memory below this trust -> quarantine
    int    recover_after = 3;          // consecutive anomalies before degrading autonomy

    // experiment hook: seed a poisoned memory that propagates over gossip
    bool inject_poison = false;

    static Config from_env();
};
