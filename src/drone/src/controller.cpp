// controller.cpp
#include "controller.hpp"

#include <cmath>

void Controller::execute_action(const std::string& action, WorldState& w, double step_scale) const {
    if (action == "MOVE" && w.battery > cfg_.battery_floor) {
        double step = cfg_.move_step * step_scale;
        auto [nx, ny] = step_toward(w.x, w.y, w.goal_x, w.goal_y, step);
        w.x = nx;
        w.y = ny;
        w.battery -= cfg_.move_cost * step_scale;
        double dx = w.goal_x - w.x, dy = w.goal_y - w.y;
        if (std::sqrt(dx * dx + dy * dy) <= cfg_.arrive_radius) w.arrived = true;
    } else if (action == "SCAN") {
        w.battery -= cfg_.scan_cost;
    } else {
        // WAIT / REQUEST_VERIFICATION / anything else: hold position, idle cost.
        w.battery -= cfg_.idle_cost;
    }
    if (w.battery < 0.0) w.battery = 0.0;
}

void Controller::apply(const Decision& d, WorldState& w) const {
    switch (d.verdict) {
        case Verdict::ALLOW:   execute_action(d.action, w, 1.0); break;
        case Verdict::DEGRADE: execute_action(d.action, w, 0.5); break;
        case Verdict::BLOCK:   execute_action("WAIT", w, 1.0); break;
        case Verdict::RECOVER: w.autonomy_level = 1; execute_action("WAIT", w, 1.0); break;
    }
}
