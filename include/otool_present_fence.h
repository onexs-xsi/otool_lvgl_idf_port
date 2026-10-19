#pragma once
#include <cstdint>
namespace otool::presentation {
inline bool ready(uintptr_t requested, uintptr_t selected, uint32_t submitted, uint32_t observed) {
    // An in-flight IDF ISR can have selected the old framebuffer before the
    // app submits the next one, and invoke the user's callback afterwards.
    // Prefer the actual DMA selection; without it two later callbacks cover
    // both that in-flight ISR and a subsequent selection of the new buffer.
    return selected ? selected == requested : uint32_t(observed - submitted) >= 2;
}
void bind(const void* first, const void* second);
uintptr_t selected_for(const void* requested);
void note_stale_callback();
struct Stats {
    bool tracking;
    uint32_t selected_index, selections, stale_callbacks, selection_errors, mapping_overflow;
};
Stats stats();
}
