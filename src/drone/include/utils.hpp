// utils.hpp -- startup helpers.
#pragma once

#include "config.hpp"

namespace Utils {

// Ping each configured peer once. Warns and continues (does not exit) so drones
// can start independently; returns the number of peers currently reachable.
int check_network(const Config& cfg);

}  // namespace Utils
