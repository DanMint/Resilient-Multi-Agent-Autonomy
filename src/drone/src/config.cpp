// config.cpp -- populate Config from environment variables set by docker-compose.
#include "config.hpp"

#include <cstdlib>
#include <sstream>
#include <unistd.h>

namespace {

std::string env_str(const char* key, const std::string& def) {
    const char* v = std::getenv(key);
    return (v && *v) ? std::string(v) : def;
}
double env_double(const char* key, double def) {
    const char* v = std::getenv(key);
    if (!v || !*v) return def;
    try { return std::stod(v); } catch (...) { return def; }
}
int env_int(const char* key, int def) {
    const char* v = std::getenv(key);
    if (!v || !*v) return def;
    try { return std::stoi(v); } catch (...) { return def; }
}
bool env_bool(const char* key, bool def) {
    const char* v = std::getenv(key);
    if (!v || !*v) return def;
    std::string s(v);
    return s == "1" || s == "true" || s == "TRUE" || s == "yes" || s == "on";
}

std::vector<std::string> split(const std::string& s, char sep) {
    std::vector<std::string> out;
    std::string cur;
    std::istringstream is(s);
    while (std::getline(is, cur, sep)) {
        // trim surrounding whitespace
        size_t a = cur.find_first_not_of(" \t");
        size_t b = cur.find_last_not_of(" \t");
        if (a != std::string::npos) out.push_back(cur.substr(a, b - a + 1));
    }
    return out;
}

std::string hostname_fallback() {
    char buf[256];
    if (gethostname(buf, sizeof(buf)) == 0) return std::string(buf);
    return "D?";
}

}  // namespace

Config Config::from_env() {
    Config c;

    c.drone_id = env_str("DRONE_ID", hostname_fallback());
    c.self_ip  = env_str("SELF_IP", "");

    // PEERS="D1=172.20.0.10,D2=172.20.0.20" -- skip our own id
    for (const auto& entry : split(env_str("PEERS", ""), ',')) {
        size_t eq = entry.find('=');
        if (eq == std::string::npos) continue;
        std::string id = entry.substr(0, eq);
        std::string ip = entry.substr(eq + 1);
        if (id.empty() || ip.empty() || id == c.drone_id) continue;
        c.peers[id] = ip;
    }

    c.brain_host      = env_str("BRAIN_HOST", c.brain_host);
    c.brain_port      = env_int("BRAIN_PORT", c.brain_port);
    c.brain_timeout_s = env_int("BRAIN_TIMEOUT_S", c.brain_timeout_s);

    c.gossip_port      = env_int("GOSSIP_PORT", c.gossip_port);
    c.gossip_timeout_s = env_int("GOSSIP_TIMEOUT_S", c.gossip_timeout_s);

    c.tick_seconds = env_double("TICK_SECONDS", c.tick_seconds);

    c.start_x = env_double("START_X", c.start_x);
    c.start_y = env_double("START_Y", c.start_y);
    c.goal_x  = env_double("GOAL_X", c.goal_x);
    c.goal_y  = env_double("GOAL_Y", c.goal_y);
    c.goal    = env_str("GOAL", c.goal);

    auto actions = split(env_str("ALLOWED_ACTIONS", ""), ',');
    if (!actions.empty()) c.allowed_actions = actions;

    c.move_step     = env_double("MOVE_STEP", c.move_step);
    c.arrive_radius = env_double("ARRIVE_RADIUS", c.arrive_radius);

    c.battery_start = env_double("BATTERY_START", c.battery_start);
    c.battery_floor = env_double("BATTERY_FLOOR", c.battery_floor);
    c.move_cost     = env_double("MOVE_COST", c.move_cost);
    c.scan_cost     = env_double("SCAN_COST", c.scan_cost);
    c.idle_cost     = env_double("IDLE_COST", c.idle_cost);

    c.geo_min_x = env_double("GEO_MIN_X", c.geo_min_x);
    c.geo_min_y = env_double("GEO_MIN_Y", c.geo_min_y);
    c.geo_max_x = env_double("GEO_MAX_X", c.geo_max_x);
    c.geo_max_y = env_double("GEO_MAX_Y", c.geo_max_y);

    c.min_peer_distance = env_double("MIN_PEER_DISTANCE", c.min_peer_distance);
    c.min_confidence    = env_double("MIN_CONFIDENCE", c.min_confidence);
    c.max_hops          = env_int("MAX_HOPS", c.max_hops);
    c.quarantine_trust  = env_double("QUARANTINE_TRUST", c.quarantine_trust);
    c.recover_after     = env_int("RECOVER_AFTER", c.recover_after);

    c.inject_poison = env_bool("INJECT_POISON", c.inject_poison);

    return c;
}
