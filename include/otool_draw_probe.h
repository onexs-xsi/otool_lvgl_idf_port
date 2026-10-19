#pragma once
#include <atomic>
#include <cstdint>

namespace otool::draw_probe {
// One host writer; multiple rendering readers. Full uptime prevents an expired
// window becoming active again after a 71-minute low-word wrap. Expiring an old
// revision cannot cancel a newly armed window. No allocation, locks or 64-bit
// atomic operations on the drawing path.
class Window {
public:
    bool arm(unsigned seconds, uint64_t now_us) {
        if (seconds > 15) return false;
        const uint32_t revision = revision_.load();
        revision_.store(revision + 1);
        const uint64_t until = seconds ? now_us + uint64_t(seconds) * 1000000 : 0;
        low_.store(uint32_t(until)); high_.store(uint32_t(until >> 32));
        revision_.store(revision + 2);
        if (!seconds) inactive_.store(revision + 2);
        return true;
    }
    bool enabled() const {
        const uint32_t revision = revision_.load();
        return !(revision & 1) && revision != inactive_.load();
    }
    uint32_t remaining_us(uint64_t now_us) {
        const uint32_t revision = revision_.load();
        if ((revision & 1) || revision == inactive_.load()) return 0;
        const uint32_t low = low_.load(), high = high_.load();
        if (revision_.load() != revision) return 0;
        const uint64_t until = (uint64_t(high) << 32) | low;
        if (now_us >= until) { inactive_.store(revision); return 0; }
        const uint64_t remaining = until - now_us;
        return remaining <= 15000000 ? uint32_t(remaining) : 0;
    }
private:
    std::atomic<uint32_t> revision_{0}, inactive_{0}, low_{0}, high_{0};
};
bool arm(unsigned seconds);
uint32_t remaining_us();
}
