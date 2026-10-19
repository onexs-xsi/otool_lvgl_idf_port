#pragma once
#include <atomic>
#include <cstdint>

// Always-on bounded measurements. No allocation, formatting or logging on the
// rendering/ISR path. Durations/counters use lock-free 32-bit atomics. Event
// timestamps retain both halves of uptime, including events older than 71 min.
namespace otool::metrics {
enum Kind : unsigned { handler, scheduling, lock, flush, rotation, cache_sync,
                       present_wait, dma_interval, snapshot, fat_info, rendering,
                       vital_update, status_update, draw_dispatch, cache_invalidate,
                       panel_submit, draw_fill, draw_border, draw_line, draw_label,
                       draw_letter, draw_image, record_lock_wait, record_lock_hold,
                       ble_snapshot, telemetry_encode, telemetry_publish, kind_count };
constexpr uint32_t budgets[kind_count] = {50000, 20000, 20000, 50000, 20000,
                                        5000, 50000, 30000, 20000, 10000, 50000, 20000, 20000,
                                        30000, 5000, 10000, 5000, 5000, 2000, 5000, 2000, 10000,
                                        20000, 20000, 10000, 20000, 5000};
constexpr const char* names[kind_count] = {"lvgl_handler", "schedule_delay", "lvgl_lock",
    "display_flush", "ppa_rotation", "cache_writeback", "present_wait", "dma_interval", "widget_snapshot", "fat_capacity_query", "lvgl_render",
    "vital_ui_update", "record_ui_status", "lvgl_draw_dispatch", "cache_invalidate", "panel_submit",
    "draw_fill", "draw_border", "draw_line", "draw_label", "draw_letter", "draw_image",
    "record_lock_wait", "record_lock_hold", "ble_metadata_snapshot", "telemetry_encode", "telemetry_publish"};
struct Sample { uint32_t count, last_us, max_us, over_budget, total_us_mod32; };
struct Event { uint32_t sequence; uint64_t time_us; uint32_t kind, duration_us, context_pc; };
class Recorder {
public:
    static constexpr unsigned event_count = 32;
    void record(Kind kind, uint32_t duration_us, uint64_t now_us, uint32_t context_pc=0) {
        auto& item = samples_[kind];
        item.count.fetch_add(1, std::memory_order_relaxed);
        item.total.fetch_add(duration_us, std::memory_order_relaxed);
        item.last.store(duration_us, std::memory_order_relaxed);
        uint32_t maximum = item.maximum.load(std::memory_order_relaxed);
        while (duration_us > maximum && !item.maximum.compare_exchange_weak(maximum, duration_us, std::memory_order_relaxed)) {}
        if (duration_us <= budgets[kind]) return;
        item.over.fetch_add(1, std::memory_order_relaxed);
        // At most one event per kind/second, but count every missed budget.
        const uint32_t low_time = uint32_t(now_us);
        uint32_t previous = item.event_time.load(std::memory_order_relaxed);
        if (previous && uint32_t(low_time - previous) < 1000000) return;
        if (!item.event_time.compare_exchange_strong(previous, low_time, std::memory_order_relaxed)) return;
        const uint32_t sequence = cursor_.fetch_add(1, std::memory_order_relaxed) + 1;
        auto& event = events_[(sequence - 1) % event_count];
        event.sequence.store(0, std::memory_order_release);
        event.time.store(low_time, std::memory_order_relaxed);
        event.time_high.store(uint32_t(now_us >> 32), std::memory_order_relaxed);
        event.kind.store(kind, std::memory_order_relaxed);
        event.duration.store(duration_us, std::memory_order_relaxed);
        event.context.store(context_pc, std::memory_order_relaxed);
        event.sequence.store(sequence, std::memory_order_release);
    }
    Sample sample(unsigned kind) const {
        const auto& item = samples_[kind];
        return {item.count.load(), item.last.load(), item.maximum.load(), item.over.load(), item.total.load()};
    }
    uint32_t cursor() const { return cursor_.load(); }
    bool event(uint32_t sequence, Event& out) const {
        if (!sequence) return false;
        const auto& item = events_[(sequence - 1) % event_count];
        if (item.sequence.load(std::memory_order_acquire) != sequence) return false;
        out = {sequence, (uint64_t(item.time_high.load()) << 32) | item.time.load(), item.kind.load(), item.duration.load(), item.context.load()};
        return item.sequence.load(std::memory_order_acquire) == sequence;
    }
private:
    struct AtomicSample {
        std::atomic<uint32_t> count{0}, last{0}, maximum{0}, over{0}, total{0}, event_time{0};
    } samples_[kind_count];
    struct AtomicEvent {
        std::atomic<uint32_t> sequence{0}, time{0}, time_high{0}, kind{0}, duration{0}, context{0};
    } events_[event_count];
    std::atomic<uint32_t> cursor_{0};
};
static_assert(std::atomic<uint32_t>::is_always_lock_free, "Metrics must be ISR safe");
extern Recorder recorder;
}
