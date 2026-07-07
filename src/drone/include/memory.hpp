// memory.hpp -- long-term memory with provenance, trust, and quarantine.
//
// This is the body's authority over what the drone "knows". Every item carries
// a trust score, a source (self / sensor / peer id), and a hop count. Items
// that are too low-trust or that have travelled too many hops are quarantined:
// still stored (for auditing and for the brain to reason *about*), but never
// treated as trusted ground truth. Thread-safe: the gossip listener writes
// concurrently with the control loop reading.
#pragma once

#include <string>
#include <vector>
#include <mutex>

struct MemoryItem {
    std::string id;
    std::string content;
    double trust_score = 1.0;
    std::string source = "self";  // self | sensor | <peer id>
    int  hop_count = 0;
    bool quarantined = false;
};

class MemoryStore {
public:
    MemoryStore(std::string id_prefix, double quarantine_trust, int max_hops);

    // Record a locally observed memory. Returns the assigned id (e.g. "D1-m3").
    std::string observe(const std::string& content, double trust,
                        const std::string& source, int hop_count);

    // Ingest a memory received from a peer: increments hop count, records the
    // sender as source, and (re)evaluates quarantine. Deduplicated by id.
    void ingest_from_peer(const std::string& peer_id, MemoryItem item);

    std::vector<MemoryItem> trusted() const;
    std::vector<MemoryItem> quarantined() const;

    bool contains(const std::string& id) const;
    bool is_quarantined(const std::string& id) const;  // false if unknown

    // Newest item (any source) for gossip. Returns false if the store is empty.
    bool newest_shareable(MemoryItem& out) const;

    size_t count_trusted() const;
    size_t count_quarantined() const;

private:
    bool is_risky(double trust, int hop) const;  // -> should be quarantined
    bool contains_nolock(const std::string& id) const;

    mutable std::mutex mu_;
    std::vector<MemoryItem> items_;
    long counter_ = 0;
    std::string id_prefix_;
    double quarantine_trust_;
    int max_hops_;
};
