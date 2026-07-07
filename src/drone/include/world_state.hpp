// world_state.hpp -- the drone's mutable physical/logical state, plus a small
// geometry helper shared by the shield and the controller.
#pragma once

#include <string>
#include <map>
#include <utility>
#include <vector>
#include <cmath>

struct WorldState {
    std::string id;
    double x = 0.0, y = 0.0;
    double battery = 100.0;
    std::string goal;
    double goal_x = 0.0, goal_y = 0.0;
    std::vector<std::string> allowed_actions;
    int  autonomy_level = 0;   // 0 = full, 1 = degraded (recovery posture)
    bool arrived = false;
    // peer positions learned from gossip heartbeats; refreshed each tick
    std::map<std::string, std::pair<double, double>> peer_positions;
};

// One step of length `step` from (x,y) toward (gx,gy). If the goal is within
// `step`, returns the goal exactly (no overshoot).
inline std::pair<double, double> step_toward(double x, double y,
                                             double gx, double gy, double step) {
    double dx = gx - x, dy = gy - y;
    double d = std::sqrt(dx * dx + dy * dy);
    if (d <= step || d == 0.0) return {gx, gy};
    return {x + dx / d * step, y + dy / d * step};
}
