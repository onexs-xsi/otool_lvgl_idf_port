#include "otool_present_fence.h"
#include "sdkconfig.h"
#include <atomic>

namespace otool::presentation {
namespace {
std::atomic<uint32_t> first{0}, second{0}, active{0}, display_channel{0}, selections{0}, stale{0}, errors{0}, overflow{0};
}
void bind(const void* a, const void* b) {
    active.store(0);
    display_channel.store(0);
    first.store(uintptr_t(a)); second.store(uintptr_t(b));
}
uintptr_t selected_for(const void* requested) {
    const uintptr_t address = uintptr_t(requested);
    return address && (address == first.load() || address == second.load()) ? active.load() : 0;
}
void note_stale_callback() { stale.fetch_add(1); }
Stats stats() {
    const uint32_t current = active.load();
    return {current != 0, current == first.load() && current ? 0U : current == second.load() && current ? 1U : 255U,
            selections.load(), stale.load(), errors.load(), overflow.load()};
}
}

#if CONFIG_IDF_TARGET_ESP32P4
#include "esp_attr.h"
#include "esp_private/dw_gdma.h"
#include "soc/reg_base.h"

namespace {
// These descriptors and atomics stay in internal RAM. The ISR never accesses
// framebuffer pixels, allocates memory or formats a log. IDF 6.1 places
// link_list_get_item/use_link_list in IRAM for LCD_DSI_ISR_HANDLER_IN_IRAM.
struct Mapping { std::atomic<uintptr_t> item{0}; std::atomic<uint32_t> source{0}; };
Mapping mappings[8];
static_assert(std::atomic<uintptr_t>::is_always_lock_free && sizeof(uintptr_t) == 4);

void remember(dw_gdma_lli_handle_t item, uint32_t source) {
    const uintptr_t key = uintptr_t(item);
    for (auto& mapping : mappings) {
        if (mapping.item.load() == key) { mapping.source.store(source); return; }
        uintptr_t empty = 0;
        if (mapping.item.compare_exchange_strong(empty, UINTPTR_MAX)) {
            mapping.source.store(source); mapping.item.store(key); return;
        }
    }
    otool::presentation::overflow.fetch_add(1);
}
bool IRAM_ATTR observe(dw_gdma_channel_handle_t channel, dw_gdma_link_list_handle_t list) {
    const uintptr_t key = uintptr_t(dw_gdma_link_list_get_item(list,0));
    for (auto& mapping : mappings) {
        if (mapping.item.load() != key) continue;
        const uint32_t source = mapping.source.load();
        if (!source || (source != otool::presentation::first.load() && source != otool::presentation::second.load())) return false;
        // Publish before programming/starting the selected list. The old DMA
        // block has completed; the selected framebuffer must now stay stable.
        otool::presentation::display_channel.store(uintptr_t(channel));
        otool::presentation::active.store(source);
        otool::presentation::selections.fetch_add(1);
        return true;
    }
    return false;
}
}
extern "C" {
esp_err_t __real_dw_gdma_lli_config_transfer(dw_gdma_lli_handle_t,const dw_gdma_block_transfer_config_t*);
esp_err_t __wrap_dw_gdma_lli_config_transfer(dw_gdma_lli_handle_t item,const dw_gdma_block_transfer_config_t* config) {
    const esp_err_t result = __real_dw_gdma_lli_config_transfer(item,config);
    if (result == ESP_OK) {
        if (config->dst.addr == MIPI_DSI_BRG_MEM_BASE) remember(item,config->src.addr);
        else for (auto& mapping : mappings) if (mapping.item.load() == uintptr_t(item)) mapping.source.store(0);
    }
    return result;
}
esp_err_t __real_dw_gdma_channel_use_link_list(dw_gdma_channel_handle_t,dw_gdma_link_list_handle_t);
esp_err_t IRAM_ATTR __wrap_dw_gdma_channel_use_link_list(dw_gdma_channel_handle_t channel,dw_gdma_link_list_handle_t list) {
    const bool tracked = channel && list && observe(channel,list);
    const bool display = channel && uintptr_t(channel) == otool::presentation::display_channel.load();
    if (display && !tracked) otool::presentation::active.store(0);
    const esp_err_t result = __real_dw_gdma_channel_use_link_list(channel,list);
    if (display && result != ESP_OK) {
        otool::presentation::active.store(0);
        otool::presentation::errors.fetch_add(1);
    }
    return result;
}
}
#endif
