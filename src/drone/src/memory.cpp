// memory.cpp
#include "memory.hpp"

#include <utility>

MemoryStore::MemoryStore(std::string id_prefix, double quarantine_trust, int max_hops)
    : id_prefix_(std::move(id_prefix)),
      quarantine_trust_(quarantine_trust),
      max_hops_(max_hops) {}

bool MemoryStore::is_risky(double trust, int hop) const {
    return trust < quarantine_trust_ || hop > max_hops_;
}

bool MemoryStore::contains_nolock(const std::string& id) const {
    for (const auto& it : items_) if (it.id == id) return true;
    return false;
}

std::string MemoryStore::observe(const std::string& content, double trust,
                                 const std::string& source, int hop_count) {
    std::lock_guard<std::mutex> lk(mu_);
    MemoryItem it;
    it.id = id_prefix_ + "-m" + std::to_string(++counter_);
    it.content = content;
    it.trust_score = trust;
    it.source = source;
    it.hop_count = hop_count;
    it.quarantined = is_risky(trust, hop_count);
    items_.push_back(it);
    return it.id;
}

void MemoryStore::ingest_from_peer(const std::string& peer_id, MemoryItem item) {
    std::lock_guard<std::mutex> lk(mu_);
    if (item.id.empty() || contains_nolock(item.id)) return;  // dedupe by id
    item.hop_count += 1;
    item.source = peer_id;
    item.quarantined = is_risky(item.trust_score, item.hop_count);
    items_.push_back(item);
}

std::vector<MemoryItem> MemoryStore::trusted() const {
    std::lock_guard<std::mutex> lk(mu_);
    std::vector<MemoryItem> out;
    for (const auto& it : items_) if (!it.quarantined) out.push_back(it);
    return out;
}

std::vector<MemoryItem> MemoryStore::quarantined() const {
    std::lock_guard<std::mutex> lk(mu_);
    std::vector<MemoryItem> out;
    for (const auto& it : items_) if (it.quarantined) out.push_back(it);
    return out;
}

bool MemoryStore::contains(const std::string& id) const {
    std::lock_guard<std::mutex> lk(mu_);
    return contains_nolock(id);
}

bool MemoryStore::is_quarantined(const std::string& id) const {
    std::lock_guard<std::mutex> lk(mu_);
    for (const auto& it : items_) if (it.id == id) return it.quarantined;
    return false;
}

bool MemoryStore::newest_shareable(MemoryItem& out) const {
    std::lock_guard<std::mutex> lk(mu_);
    if (items_.empty()) return false;
    out = items_.back();
    return true;
}

size_t MemoryStore::count_trusted() const {
    std::lock_guard<std::mutex> lk(mu_);
    size_t n = 0;
    for (const auto& it : items_) if (!it.quarantined) ++n;
    return n;
}

size_t MemoryStore::count_quarantined() const {
    std::lock_guard<std::mutex> lk(mu_);
    size_t n = 0;
    for (const auto& it : items_) if (it.quarantined) ++n;
    return n;
}
