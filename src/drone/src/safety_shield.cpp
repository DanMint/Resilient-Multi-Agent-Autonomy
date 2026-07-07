// safety_shield.cpp
#include "safety_shield.hpp"

#include <cmath>

bool SafetyShield::is_allowed(const std::string& action, const WorldState& w) const {
    for (const auto& a : w.allowed_actions) if (a == action) return true;
    return false;
}

std::string SafetyShield::recover_action(const WorldState& w) const {
    if (is_allowed("REQUEST_VERIFICATION", w)) return "REQUEST_VERIFICATION";
    return "WAIT";
}

Decision SafetyShield::review(const Proposal& p, const WorldState& w, const MemoryStore& mem) const {
    const std::string action = p.proposed_action;

    // 1. Integrity: a proposal that leans on quarantined memory is a strong
    //    compromise signal -> enter recovery, don't just block this tick.
    for (const auto& id : p.used_memory_ids)
        if (mem.is_quarantined(id))
            return {Verdict::RECOVER, recover_action(w), "proposal relied on quarantined memory " + id};

    // 2. Integrity: citing memory the body has never seen (hallucinated id).
    for (const auto& id : p.used_memory_ids)
        if (!mem.contains(id))
            return {Verdict::BLOCK, "WAIT", "proposal cited unknown memory " + id};

    // 3. Whitelist: the action must be permitted for this drone.
    if (!is_allowed(action, w))
        return {Verdict::BLOCK, "WAIT", "action '" + action + "' not in allowed set"};

    // 4. Confidence floor.
    if (p.confidence < cfg_.min_confidence)
        return {Verdict::BLOCK, "WAIT", "confidence " + std::to_string(p.confidence) + " below floor"};

    // 5. Physical checks apply to motion.
    if (action == "MOVE") {
        if (w.battery <= cfg_.battery_floor)
            return {Verdict::BLOCK, "WAIT", "battery at/below floor"};

        auto [nx, ny] = step_toward(w.x, w.y, w.goal_x, w.goal_y, cfg_.move_step);

        if (nx < cfg_.geo_min_x || nx > cfg_.geo_max_x || ny < cfg_.geo_min_y || ny > cfg_.geo_max_y)
            return {Verdict::BLOCK, "WAIT", "move would exit geofence"};

        for (const auto& [pid, pos] : w.peer_positions) {
            double dx = nx - pos.first, dy = ny - pos.second;
            if (std::sqrt(dx * dx + dy * dy) < cfg_.min_peer_distance)
                return {Verdict::BLOCK, "WAIT", "move too close to peer " + pid};
        }

        // Under degraded autonomy we still move, but cautiously.
        if (w.autonomy_level >= 1)
            return {Verdict::DEGRADE, "MOVE", "autonomy degraded: reduced step"};
    }

    return {Verdict::ALLOW, action, "ok"};
}
