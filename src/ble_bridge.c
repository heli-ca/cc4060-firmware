/**
 ******************************************************************************
 * @file    ble_bridge.c
 * @author  CC4060 DSP Driver Team
 * @version V1.0.0
 * @date    2026-07-07
 * @brief   CC4060 AC6921A BLE↔UART透明桥接固件
 *          基于杰理AC692x SDK V2.4
 *
 * @note    功能概述:
 *          - BLE GATT服务 0xAE00, 特征ae01(写)/ae02(通知)
 *          - UART1连接AK7738 MCU, 115200bps
 *          - BLE ae01写入 → UART转发到MCU
 *          - UART接收MCU数据 → BLE ae02通知到手机
 *          - 协议帧头 0x01FE0000, CRC16校验
 *
 * @note    数据流:
 *          手机App ←BLE(ae02 notify)── AC6921A ──UART──→ AK7738 MCU
 *          手机App ──BLE(ae01 write)→ AC6921A ──UART──→ AK7738 MCU
 *
 * @note    编译依赖:
 *          - AC692x SDK V2.4 (apps/ble_stack/)
 *          - 需要修改SDK的le_server_module.c, 将回调指向本文件
 ******************************************************************************
 */

/* Includes ------------------------------------------------------------------*/
#include "app_config.h"
#include "system/includes.h"
#include "ble/le_server_module.h"

/* Private defines -----------------------------------------------------------*/

/* UART帧状态 */
#define UART_FRAME_HEADER_0         0x01
#define UART_FRAME_HEADER_1         0xFE
#define UART_FRAME_HEADER_2         0x00
#define UART_FRAME_HEADER_3         0x00

/* 桥接缓冲区 */
#define BRIDGE_RX_BUF_SIZE          512
#define BRIDGE_TX_BUF_SIZE          256
#define BRIDGE_MAX_PAYLOAD          240     /* MTU-7 = 240 (MTU=247时) */

/* 连接状态 */
#define CONN_STATE_DISCONNECTED     0
#define CONN_STATE_CONNECTED        1
#define CONN_STATE_AUTHENTICATED    2

/* LED闪烁模式 */
#define LED_MODE_OFF                0
#define LED_MODE_SLOW_BLINK         1   /* 1Hz - 等待连接 */
#define LED_MODE_FAST_BLINK         2   /* 5Hz - 连接中 */
#define LED_MODE_ON                 3   /* 常亮 - 已连接 */
#define LED_MODE_DATA               4   /* 闪烁 - 数据传输 */

/* Private variables ---------------------------------------------------------*/

/* UART接收缓冲区(环形) */
static uint8_t  uart_rx_buf[TCFG_MCU_UART_BUF_SIZE];
static uint16_t uart_rx_head = 0;
static uint16_t uart_rx_tail = 0;

/* BLE发送缓冲区 */
static uint8_t  ble_tx_buf[BRIDGE_TX_BUF_SIZE];

/* UART帧解析状态 */
static uint8_t  uart_frame_buf[BRIDGE_RX_BUF_SIZE];
static uint16_t uart_frame_pos = 0;
static uint16_t uart_frame_len = 0;
static uint8_t  uart_parse_state = 0;   /* 0-3=header, 4-5=cmd_id, 6-7=length, 8=payload+crc, 9-10=tail */

/* 连接状态 */
static uint8_t  conn_state = CONN_STATE_DISCONNECTED;
static uint16_t conn_handle = 0;

/* 统计计数器 */
static uint32_t stat_ble_rx_count = 0;      /* BLE→UART 帧计数 */
static uint32_t stat_uart_rx_count = 0;     /* UART→BLE 帧计数 */
static uint32_t stat_ble_rx_bytes = 0;      /* BLE接收总字节 */
static uint32_t stat_uart_rx_bytes = 0;     /* UART接收总字节 */
static uint32_t stat_error_count = 0;       /* 错误计数 */

/* LED控制 */
static uint8_t  led_mode = LED_MODE_OFF;
static uint32_t led_tick = 0;

/* Private function prototypes -----------------------------------------------*/
static void uart_bridge_init(void);
static void uart_bridge_send(const uint8_t *data, uint16_t len);
static void uart_rx_handler(uint8_t byte);
static void uart_frame_process(void);
static void ble_data_send(const uint8_t *data, uint16_t len);
static void ble_bridge_init(void);
static void led_update(void);
static void led_set_mode(uint8_t mode);
static uint16_t crc16_calc(const uint8_t *data, uint16_t len);

/* ============================================================================
 * UART桥接层
 * ============================================================================ */

/**
 * @brief UART初始化 - 连接AK7738 MCU
 */
static void uart_bridge_init(void)
{
    uart_dev_t uart1;
    
    /* 配置UART1引脚 */
    uart1.tx_pin = TCFG_MCU_UART_TX_PORT;
    uart1.rx_pin = TCFG_MCU_UART_RX_PORT;
    uart1.baud = TCFG_MCU_UART_BAUDRATE;
    uart1.parity = UART_PARITY_NONE;
    uart1.stop_bits = UART_STOP_BITS_1;
    uart1.flow_ctrl = UART_FLOW_CTRL_NONE;
    
    /* 初始化UART */
    uart_init(&uart1);
    
    /* 注册接收回调 */
    uart_set_rx_callback(&uart1, uart_rx_handler);
    
    /* 清空缓冲区 */
    uart_rx_head = 0;
    uart_rx_tail = 0;
    uart_frame_pos = 0;
    uart_parse_state = 0;
}

/**
 * @brief UART发送数据到MCU
 * @param data 数据指针
 * @param len 数据长度
 */
static void uart_bridge_send(const uint8_t *data, uint16_t len)
{
    if (len == 0 || data == NULL) return;
    
    uart_dev_t uart1;
    uart1.tx_pin = TCFG_MCU_UART_TX_PORT;
    
    uart_send(&uart1, data, len);
}

/**
 * @brief UART接收回调 - 每收到一个字节调用
 * @param byte 接收到的字节
 */
static void uart_rx_handler(uint8_t byte)
{
    /* 帧解析状态机 */
    switch (uart_parse_state) {
    case 0: /* 等待帧头第1字节 */
        if (byte == UART_FRAME_HEADER_0) {
            uart_frame_buf[0] = byte;
            uart_frame_pos = 1;
            uart_parse_state = 1;
        }
        break;
        
    case 1: /* 等待帧头第2字节 */
        if (byte == UART_FRAME_HEADER_1) {
            uart_frame_buf[1] = byte;
            uart_frame_pos = 2;
            uart_parse_state = 2;
        } else if (byte == UART_FRAME_HEADER_0) {
            /* 可能是新帧头 */
            uart_frame_buf[0] = byte;
            uart_frame_pos = 1;
        } else {
            uart_parse_state = 0;
        }
        break;
        
    case 2: /* 等待帧头第3字节 */
        if (byte == UART_FRAME_HEADER_2) {
            uart_frame_buf[2] = byte;
            uart_frame_pos = 3;
            uart_parse_state = 3;
        } else {
            uart_parse_state = 0;
        }
        break;
        
    case 3: /* 等待帧头第4字节 */
        if (byte == UART_FRAME_HEADER_3) {
            uart_frame_buf[3] = byte;
            uart_frame_pos = 4;
            uart_parse_state = 4;
        } else {
            uart_parse_state = 0;
        }
        break;
        
    case 4: /* 等待CMD_ID第1字节 (bytes[4]) */
        uart_frame_buf[uart_frame_pos++] = byte;
        uart_parse_state = 5;
        break;
        
    case 5: /* 等待CMD_ID第2字节 (bytes[5]) */
        uart_frame_buf[uart_frame_pos++] = byte;
        /* 解析CMD_ID (大端序) */
        /* cmd_id = (uart_frame_buf[4] << 8) | uart_frame_buf[5]; */
        uart_parse_state = 6;
        break;
        
    case 6: /* 等待Length第1字节 (bytes[6]) */
        uart_frame_buf[uart_frame_pos++] = byte;
        uart_parse_state = 7;
        break;
        
    case 7: /* 等待Length第2字节 (bytes[7]) */
        uart_frame_buf[uart_frame_pos++] = byte;
        /* 解析Length (大端序) */
        uart_frame_len = (uart_frame_buf[6] << 8) | uart_frame_buf[7];
        if (uart_frame_len > BRIDGE_RX_BUF_SIZE - 12) {
            /* 长度超限, 丢弃 */
            uart_parse_state = 0;
            stat_error_count++;
        } else {
            uart_parse_state = 8;
        }
        break;
        
    case 8: /* 等待Payload + CRC (length + 2字节) */
        uart_frame_buf[uart_frame_pos++] = byte;
        /* 已接收: 4(头) + 2(cmd_id) + 2(length) + payload+crc */
        if (uart_frame_pos >= (uint16_t)(8 + uart_frame_len + 2)) {
            /* Payload+CRC接收完毕, 等待尾部 */
            uart_parse_state = 9;
        }
        break;
        
    case 9: /* 等待Tail第1字节 (应为0x00) */
        uart_frame_buf[uart_frame_pos++] = byte;
        uart_parse_state = 10;
        break;
        
    case 10: /* 等待Tail第2字节 (应为0x00) */
        uart_frame_buf[uart_frame_pos++] = byte;
        /* 完整帧接收完毕 */
        uart_frame_process();
        uart_parse_state = 0;
        break;
        
    default:
        uart_parse_state = 0;
        break;
    }
}

/**
 * @brief 处理完整UART帧 - 转发到BLE
 */
static void uart_frame_process(void)
{
    uint16_t frame_total = uart_frame_pos;
    
    /* 验证CRC */
    /* CRC覆盖: CMD_ID(2B) + Length(2B) + Payload(N) */
    /* 帧格式: [头4B] [CMD_ID 2B] [长度2B] [数据 NB] [CRC16 2B] [尾部2B] */
    /* CRC位置: 尾部(2B)之前 */
    uint16_t crc_offset = frame_total - 4;  /* 跳过2字节CRC + 2字节尾部 */
    uint16_t recv_crc = (uart_frame_buf[crc_offset] << 8) | uart_frame_buf[crc_offset + 1];
    uint16_t calc_crc = crc16_calc(&uart_frame_buf[4], 2 + 2 + uart_frame_len);  /* CMD_ID + Length + Payload */
    
    if (recv_crc != calc_crc) {
        stat_error_count++;
        return;
    }
    
    /* 转发到BLE */
    if (conn_state >= CONN_STATE_CONNECTED) {
        ble_data_send(uart_frame_buf, frame_total);
        stat_uart_rx_count++;
        stat_uart_rx_bytes += frame_total;
        led_set_mode(LED_MODE_DATA);
    }
}

/* ============================================================================
 * BLE桥接层
 * ============================================================================ */

/**
 * @brief BLE初始化 - 配置GATT服务和广播
 */
static void ble_bridge_init(void)
{
    /* 设置设备名称 */
    ble_set_device_name(BLE_DEVICE_NAME, BLE_DEVICE_NAME_LEN);
    
    /* 设置广播参数 */
    ble_set_adv_interval(BLE_ADV_INTERVAL);
    ble_set_adv_timeout(BLE_ADV_TIMEOUT);
    
    /* 设置连接参数 */
    ble_set_conn_params(BLE_CONN_INTERVAL_MIN, BLE_CONN_INTERVAL_MAX,
                        BLE_CONN_LATENCY, BLE_CONN_SUPERVISION);
    
    /* 注册BLE回调 */
    ble_set_connect_callback(on_ble_connect);
    ble_set_disconnect_callback(on_ble_disconnect);
    ble_set_write_callback(on_ble_write);
    ble_set_mtu_callback(on_ble_mtu_update);
    
    /* 开始广播 */
    ble_start_adv();
}

/**
 * @brief BLE连接回调
 */
void on_ble_connect(uint16_t handle)
{
    conn_state = CONN_STATE_CONNECTED;
    conn_handle = handle;
    led_set_mode(LED_MODE_FAST_BLINK);
}

/**
 * @brief BLE断开回调
 */
void on_ble_disconnect(uint16_t handle, uint8_t reason)
{
    conn_state = CONN_STATE_DISCONNECTED;
    conn_handle = 0;
    led_set_mode(LED_MODE_SLOW_BLINK);
    
    /* 重新开始广播 */
    ble_start_adv();
}

/**
 * @brief BLE写入回调 (ae01特征)
 * @param handle 特征句柄
 * @param data 写入数据
 * @param len 数据长度
 */
void on_ble_write(uint16_t handle, const uint8_t *data, uint16_t len)
{
    if (len == 0 || data == NULL) return;
    
    /* ae01写入 → UART转发到MCU */
    uart_bridge_send(data, len);
    
    stat_ble_rx_count++;
    stat_ble_rx_bytes += len;
    led_set_mode(LED_MODE_DATA);
}

/**
 * @brief MTU更新回调
 */
void on_ble_mtu_update(uint16_t mtu)
{
    /* MTU更新, 可调整发送缓冲区大小 */
}

/**
 * @brief 通过BLE发送数据 (ae02通知)
 * @param data 数据指针
 * @param len 数据长度
 */
static void ble_data_send(const uint8_t *data, uint16_t len)
{
    if (conn_state < CONN_STATE_CONNECTED || len == 0) return;
    
    /* ae02通知句柄 = 0x0008 (来自profile_data定义) */
    #define AE02_NOTIFY_HANDLE    0x0008
    
    uint16_t offset = 0;
    uint16_t max_chunk = BRIDGE_MAX_PAYLOAD;
    
    while (offset < len) {
        uint16_t chunk = len - offset;
        if (chunk > max_chunk) chunk = max_chunk;
        
        int ret = att_server_notify(AE02_NOTIFY_HANDLE, 
                                    (uint8_t*)&data[offset], chunk);
        if (ret != 0) {
            /* 发送失败, 可能是缓冲区满 */
            os_time_dly(1);  /* 等待10ms重试 */
            continue;
        }
        
        offset += chunk;
    }
}

/* ============================================================================
 * CRC16计算
 * ============================================================================ */

/**
 * @brief CRC16计算 (Modbus, 多项式0xA001, 初始值0xFFFF)
 * @param data 数据指针
 * @param len 数据长度
 * @return CRC16值
 */
static uint16_t crc16_calc(const uint8_t *data, uint16_t len)
{
    uint16_t crc = 0xFFFF;
    
    for (uint16_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (uint8_t j = 0; j < 8; j++) {
            if (crc & 0x0001) {
                crc = (crc >> 1) ^ 0xA001;
            } else {
                crc >>= 1;
            }
        }
    }
    
    return crc;
}

/* ============================================================================
 * LED控制
 * ============================================================================ */

static void led_set_mode(uint8_t mode)
{
    led_mode = mode;
}

static void led_update(void)
{
    led_tick++;
    
    switch (led_mode) {
    case LED_MODE_OFF:
        gpio_set(TCFG_LED_PORT, !TCFG_LED_ACTIVE_LEVEL);
        break;
        
    case LED_MODE_SLOW_BLINK:
        /* 1Hz (500ms周期, tick=50) */
        gpio_set(TCFG_LED_PORT, (led_tick % 50 < 25) ? 
                 TCFG_LED_ACTIVE_LEVEL : !TCFG_LED_ACTIVE_LEVEL);
        break;
        
    case LED_MODE_FAST_BLINK:
        /* 5Hz (100ms周期, tick=10) */
        gpio_set(TCFG_LED_PORT, (led_tick % 10 < 5) ? 
                 TCFG_LED_ACTIVE_LEVEL : !TCFG_LED_ACTIVE_LEVEL);
        break;
        
    case LED_MODE_ON:
        gpio_set(TCFG_LED_PORT, TCFG_LED_ACTIVE_LEVEL);
        break;
        
    case LED_MODE_DATA:
        /* 数据传输闪烁, 100ms后恢复 */
        gpio_set(TCFG_LED_PORT, TCFG_LED_ACTIVE_LEVEL);
        if (led_tick > 10) {
            led_mode = (conn_state >= CONN_STATE_CONNECTED) ? 
                       LED_MODE_ON : LED_MODE_SLOW_BLINK;
            led_tick = 0;
        }
        break;
    }
}

/* ============================================================================
 * 主应用入口
 * ============================================================================ */

/**
 * @brief BLE桥接应用初始化
 * @note  在board_init()之后调用
 */
void ble_bridge_app_init(void)
{
    /* 初始化LED */
    gpio_set_direction(TCFG_LED_PORT, GPIO_DIR_OUTPUT);
    led_set_mode(LED_MODE_SLOW_BLINK);
    
    /* 初始化UART桥接 */
    uart_bridge_init();
    
    /* 初始化BLE */
    ble_bridge_init();
    
    printf("[CC4060] BLE Bridge initialized\n");
    printf("[CC4060] UART: %d bps, BLE: AE00 service\n", TCFG_MCU_UART_BAUDRATE);
}

/**
 * @brief 主循环任务
 * @note  在task_manager中周期调用
 */
void ble_bridge_task(void)
{
    /* LED更新 */
    led_update();
    
    /* 检查UART缓冲区是否有待发送数据 */
    /* (在回调模式下, 数据在uart_rx_handler中直接处理) */
    
    /* 看门狗喂狗 */
    wdt_feed();
}

/**
 * @brief 获取桥接统计信息
 * @param ble_to_uart_count BLE→UART帧计数输出
 * @param uart_to_ble_count UART→BLE帧计数输出
 * @param error_count 错误计数输出
 */
void ble_bridge_get_stats(uint32_t *ble_to_uart_count, 
                          uint32_t *uart_to_ble_count,
                          uint32_t *error_count)
{
    if (ble_to_uart_count) *ble_to_uart_count = stat_ble_rx_count;
    if (uart_to_ble_count) *uart_to_ble_count = stat_uart_rx_count;
    if (error_count) *error_count = stat_error_count;
}

/**
 * @brief 重置统计计数器
 */
void ble_bridge_reset_stats(void)
{
    stat_ble_rx_count = 0;
    stat_uart_rx_count = 0;
    stat_ble_rx_bytes = 0;
    stat_uart_rx_bytes = 0;
    stat_error_count = 0;
}
