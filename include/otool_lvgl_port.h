/**
 * @file otool_lvgl_port.h
 * @brief LVGL IDF 移植层 - 主接口
 *
 * 提供 LVGL 与 ESP-IDF 的集成，包括：
 * - LVGL 任务管理
 * - 显示驱动 (默认/DSI/RGB)
 * - 触摸输入
 *
 * 使用示例:
 * @code
 * // 1. 初始化 LVGL 端口
 * otool_lvgl_port_cfg_t cfg = OTOOL_LVGL_PORT_INIT_CONFIG();
 * otool_lvgl_port_init(&cfg);
 *
 * // 2. 添加 DSI 显示 (支持 PPA 旋转)
 * otool_lvgl_disp_cfg_t disp_cfg = { ... };
 * otool_lvgl_disp_dsi_cfg_t dsi_cfg = { .flags.avoid_tearing = 1, .flags.use_ppa = 1 };
 * lv_display_t *disp = otool_lvgl_port_add_disp_dsi(&disp_cfg, &dsi_cfg);
 *
 * // 3. 添加触摸
 * otool_lvgl_touch_cfg_t touch_cfg = { .disp = disp, .handle = tp_handle };
 * lv_indev_t *touch = otool_lvgl_port_add_touch(&touch_cfg);
 *
 * // 4. 使用 LVGL
 * otool_lvgl_port_lock(0);
 * // ... LVGL 操作 ...
 * otool_lvgl_port_unlock();
 * @endcode
 */

#pragma once

#include "esp_err.h"
#include "esp_heap_caps.h"
#include "lvgl.h"
#include "otool_lvgl_port_disp.h"
#include "otool_lvgl_port_touch.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief LVGL 端口任务事件类型
 */
typedef enum {
    OTOOL_LVGL_PORT_EVENT_DISPLAY = 0x01,
    OTOOL_LVGL_PORT_EVENT_TOUCH   = 0x02,
    OTOOL_LVGL_PORT_EVENT_USER    = 0x80,
} otool_lvgl_port_event_type_t;

/**
 * @brief LVGL 端口任务事件
 */
typedef struct {
    otool_lvgl_port_event_type_t type;
    void *param;
} otool_lvgl_port_event_t;

/**
 * @brief 初始化配置结构
 */
typedef struct {
    int task_priority;          /*!< LVGL 任务优先级 */
    int task_stack;             /*!< LVGL 任务栈大小 */
    int task_affinity;          /*!< LVGL 任务核心亲和性 (-1 表示无亲和性) */
    int task_max_sleep_ms;      /*!< LVGL 任务最大休眠时间 */
    unsigned task_stack_caps;   /*!< LVGL 任务栈内存能力 (见 esp_heap_caps.h) */
    int timer_period_ms;        /*!< LVGL 定时器周期 (ms) */
} otool_lvgl_port_cfg_t;

/**
 * @brief LVGL 端口默认配置
 */
#define OTOOL_LVGL_PORT_INIT_CONFIG()              \
    {                                              \
        .task_priority = 4,                        \
        .task_stack = 16384,                       \
        .task_affinity = -1,                       \
        .task_max_sleep_ms = 500,                  \
        .task_stack_caps = MALLOC_CAP_INTERNAL | MALLOC_CAP_DEFAULT, \
        .timer_period_ms = 5,                      \
    }

/**
 * @brief 初始化 LVGL 端口
 *
 * @note 此函数初始化 LVGL 并创建定时器和任务
 *
 * @param cfg 配置参数
 * @return
 *      - ESP_OK 成功
 *      - ESP_ERR_INVALID_ARG 参数无效
 *      - ESP_ERR_INVALID_STATE esp_timer 库未初始化
 *      - ESP_ERR_NO_MEM 内存分配失败
 */
esp_err_t otool_lvgl_port_init(const otool_lvgl_port_cfg_t *cfg);

/**
 * @brief 反初始化 LVGL 端口
 *
 * @return
 *      - ESP_OK 成功
 *      - ESP_ERR_TIMEOUT 停止 LVGL 任务超时
 */
esp_err_t otool_lvgl_port_deinit(void);

/**
 * @brief 获取 LVGL 互斥锁
 *
 * @param timeout_ms 超时时间 (ms)，0 表示永久等待
 * @return
 *      - true 成功获取锁
 *      - false 获取锁失败
 */
bool otool_lvgl_port_lock(uint32_t timeout_ms);

/**
 * @brief 释放 LVGL 互斥锁
 */
void otool_lvgl_port_unlock(void);

/**
 * @brief 停止 LVGL 定时器
 *
 * @return
 *      - ESP_OK 成功
 *      - ESP_ERR_INVALID_STATE 定时器未运行
 */
esp_err_t otool_lvgl_port_stop(void);

/**
 * @brief 恢复 LVGL 定时器
 *
 * @return
 *      - ESP_OK 成功
 *      - ESP_ERR_INVALID_STATE 定时器未运行
 */
esp_err_t otool_lvgl_port_resume(void);

/**
 * @brief 唤醒 LVGL 任务
 *
 * @param event 事件类型
 * @param param 参数 (保留)
 * @return
 *      - ESP_OK 成功
 *      - ESP_ERR_NOT_SUPPORTED 未实现
 *      - ESP_ERR_INVALID_STATE 队列未初始化
 */
esp_err_t otool_lvgl_port_task_wake(otool_lvgl_port_event_type_t event, void *param);

#ifdef __cplusplus
}
#endif
