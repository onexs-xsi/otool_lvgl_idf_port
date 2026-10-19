#include "otool_draw_probe.h"
#include "otool_lvgl_metrics.h"
#include "esp_timer.h"
#include "src/draw/sw/lv_draw_sw.h"

namespace otool::draw_probe {
namespace {
Window window;
template<class Call> void measured(metrics::Kind kind, Call call) {
    if (!window.enabled()) { call(); return; }
    const uint64_t started = esp_timer_get_time();
    if (!window.remaining_us(started)) { call(); return; }
    call();
    const uint64_t now = esp_timer_get_time();
    metrics::recorder.record(kind, now - started, now);
}
}
bool arm(unsigned seconds) { return window.arm(seconds, esp_timer_get_time()); }
uint32_t remaining_us() { return window.remaining_us(esp_timer_get_time()); }
}

// Linker wrappers keep the installed SDK and managed LVGL sources intact.
// Counts and accumulated spans overlap (labels can contain letters), and two
// draw threads can run together; they are wall spans, not CPU utilization.
extern "C" {
void __real_lv_draw_sw_fill(lv_draw_task_t*, lv_draw_fill_dsc_t*, const lv_area_t*);
void __wrap_lv_draw_sw_fill(lv_draw_task_t* t, lv_draw_fill_dsc_t* d, const lv_area_t* a) {
    otool::draw_probe::measured(otool::metrics::draw_fill, [&]{__real_lv_draw_sw_fill(t,d,a);});
}
#define WRAP_AREA(name, descriptor, kind) \
    void __real_lv_draw_sw_##name(lv_draw_task_t*, const descriptor*, const lv_area_t*); \
    void __wrap_lv_draw_sw_##name(lv_draw_task_t* t, const descriptor* d, const lv_area_t* a) { \
        otool::draw_probe::measured(otool::metrics::kind, [&]{__real_lv_draw_sw_##name(t,d,a);}); \
    }
WRAP_AREA(border, lv_draw_border_dsc_t, draw_border)
WRAP_AREA(label, lv_draw_label_dsc_t, draw_label)
WRAP_AREA(letter, lv_draw_letter_dsc_t, draw_letter)
WRAP_AREA(image, lv_draw_image_dsc_t, draw_image)
#undef WRAP_AREA
void __real_lv_draw_sw_line(lv_draw_task_t*, const lv_draw_line_dsc_t*);
void __wrap_lv_draw_sw_line(lv_draw_task_t* t, const lv_draw_line_dsc_t* d) {
    otool::draw_probe::measured(otool::metrics::draw_line, [&]{__real_lv_draw_sw_line(t,d);});
}
}
