/**
 ******************************************************************************
 * @file    ble_bridge.h
 * @author  CC4060 DSP Driver Team
 * @version V1.0.0
 * @date    2026-07-07
 * @brief   CC4060 AC6921A BLE↔UART透明桥接 头文件
 *
 * @note    本模块实现AC6921A芯片上的BLE到UART透明桥接:
 *          - BLE GATT服务 0xAE00
 *            - ae01 (Write,  handle 0x0006): 手机→MCU
 *            - ae02 (Notify, handle 0x0008): MCU→手机
 *            - ae03 (Write,  handle 0x000b): 保留
 *            - ae04 (Notify, handle 0x000d): 保留
 *            - ae05 (Indicate, handle 0x0010): 保留
 *            - ae10 (R/W,    handle 0x0013): MTU协商
 *          - UART1连接AK7738 MCU (PB04-TX, PB05-RX, 115200bps)
 *          - 帧格式: [01 FE 00 00] [CMD_ID:2B BE] [LEN:2B BE] [DATA:NB] [CRC16:2B BE] [00 00]
 *
 * @note    SDK集成说明:
 *          1. 在 apps/ble_stack/user/le_server_module.c 中:
 *             - 将 att_read/write callback 指向本模块的回调函数
 *             - 或 在 le_server_module_init() 中注册 ble_bridge_gatt_cb
 *          2. 在 board.c 的 main() 中调用 ble_bridge_app_init()
 *          3. 在 task_manager 中周期调用 ble_bridge_task()
 ******************************************************************************
 */

#ifndef __BLE_BRIDGE_H__
#define __BLE_BRIDGE_H__

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include <stdint.h>
#include <stdbool.h>

/* Exported constants --------------------------------------------------------*/

/**
 * @brief BLE服务UUID定义 (与AC692x SDK profile_data[]对应)
 * @note  主服务UUID = 0xAE00 (NOT 0xFFE0)
 */
#define BLE_BRIDGE_SERVICE_UUID         0xAE00

/**
 * @brief 特征UUID定义
 */
#define BLE_CHAR_AE01_WRITE_UUID        0xAE01  /* 手机写入 → MCU (Write Without Response) */
#define BLE_CHAR_AE02_NOTIFY_UUID       0xAE02  /* MCU通知 → 手机 (Notify) */
#define BLE_CHAR_AE03_WRITE_UUID        0xAE03  /* 保留 (Write) */
#define BLE_CHAR_AE04_NOTIFY_UUID       0xAE04  /* 保留 (Notify) */
#define BLE_CHAR_AE05_INDICATE_UUID     0xAE05  /* 保留 (Indicate) */
#define BLE_CHAR_AE10_RW_UUID           0xAE10  /* MTU协商 (Read/Write) */

/**
 * @brief GATT特征句柄 (与SDK profile_data[]中的顺序对应)
 */
#define BLE_CHAR_AE01_HANDLE            0x0006  /* ae01 Write  */
#define BLE_CHAR_AE02_HANDLE            0x0008  /* ae02 Notify (CCCD handle = 0x0009) */
#define BLE_CHAR_AE03_HANDLE            0x000B  /* ae03 Write  */
#define BLE_CHAR_AE04_HANDLE            0x000D  /* ae04 Notify (CCCD handle = 0x000E) */
#define BLE_CHAR_AE05_HANDLE            0x0010  /* ae05 Indicate (CCCD handle = 0x0011) */
#define BLE_CHAR_AE10_HANDLE            0x0013  /* ae10 Read/Write */

/**
 * @brief UART帧协议常量
 */
#define BRIDGE_FRAME_HEADER_0           0x01
#define BRIDGE_FRAME_HEADER_1           0xFE
#define BRIDGE_FRAME_HEADER_2           0x00
#define BRIDGE_FRAME_HEADER_3           0x00
#define BRIDGE_FRAME_HEADER_SIZE        4
#define BRIDGE_FRAME_CMD_ID_SIZE        2
#define BRIDGE_FRAME_LENGTH_SIZE        2
#define BRIDGE_FRAME_CRC_SIZE           2
#define BRIDGE_FRAME_TAIL_SIZE          2
#define BRIDGE_FRAME_OVERHEAD           (BRIDGE_FRAME_HEADER_SIZE + BRIDGE_FRAME_CMD_ID_SIZE + BRIDGE_FRAME_LENGTH_SIZE + BRIDGE_FRAME_CRC_SIZE + BRIDGE_FRAME_TAIL_SIZE)  /* = 12 */

/**
 * @brief 桥接缓冲区大小
 */
#define BRIDGE_RX_BUF_SIZE              512     /* UART接收缓冲 */
#define BRIDGE_TX_BUF_SIZE              256     /* BLE发送缓冲 */
#define BRIDGE_MAX_PAYLOAD              240     /* 最大有效载荷 = MTU - 7 */

/**
 * @brief 连接状态码
 */
#define BRIDGE_CONN_DISCONNECTED        0x00
#define BRIDGE_CONN_CONNECTED           0x01
#define BRIDGE_CONN_AUTHENTICATED       0x02

/**
 * @brief LED指示模式
 */
#define BRIDGE_LED_OFF                  0x00    /* 熄灭 */
#define BRIDGE_LED_SLOW_BLINK           0x01    /* 1Hz慢闪 - 等待连接 */
#define BRIDGE_LED_FAST_BLINK           0x02    /* 5Hz快闪 - 连接中 */
#define BRIDGE_LED_ON                   0x03    /* 常亮 - 已连接 */
#define BRIDGE_LED_DATA                 0x04    /* 数据闪烁 - 传输中 */

/* Exported types ------------------------------------------------------------*/

/**
 * @brief 桥接统计信息结构体
 */
typedef struct {
    uint32_t ble_to_uart_frames;        /*!< BLE→UART 帧计数 */
    uint32_t uart_to_ble_frames;        /*!< UART→BLE 帧计数 */
    uint32_t ble_rx_bytes;              /*!< BLE接收总字节数 */
    uint32_t uart_rx_bytes;             /*!< UART接收总字节数 */
    uint32_t error_count;               /*!< 错误计数 (CRC错误/溢出等) */
    uint16_t current_mtu;               /*!< 当前协商MTU值 */
    uint8_t  conn_state;                /*!< 当前连接状态 */
} BridgeStatsTypeDef;

/**
 * @brief UART帧结构体 (用于解析后的帧数据)
 */
typedef struct {
    uint16_t payload_length;            /*!< 有效载荷长度 */
    uint16_t command_id;                /*!< 命令ID (大端序) */
    uint16_t crc_received;              /*!< 接收到的CRC16 */
    uint16_t crc_calculated;            /*!< 计算得到的CRC16 */
    bool     crc_valid;                 /*!< CRC校验结果 */
    const uint8_t *payload;             /*!< 有效载荷数据指针 */
} BridgeFrameTypeDef;

/**
 * @brief BLE GATT回调函数集合
 * @note  用于注册到SDK的le_server_module
 */
typedef struct {
    void (*on_connect)(uint16_t handle);
    void (*on_disconnect)(uint16_t handle, uint8_t reason);
    void (*on_write)(uint16_t handle, const uint8_t *data, uint16_t len);
    void (*on_mtu_update)(uint16_t mtu);
} BridgeGattCallbacksTypeDef;

/* Exported functions --------------------------------------------------------*/

/**
 * @brief 初始化BLE桥接模块
 * @note  在board_init()完成后调用
 *          - 初始化UART (115200bps, 8N1)
 *          - 初始化BLE GATT服务
 *          - 开始广播
 */
void ble_bridge_app_init(void);

/**
 * @brief 桥接主循环任务
 * @note  在task_manager中周期调用 (建议10ms周期)
 *          - 更新LED状态
 *          - 检查超时/错误
 *          - 喂看门狗
 */
void ble_bridge_task(void);

/**
 * @brief 获取桥接统计信息
 * @param stats 统计信息输出指针
 */
void ble_bridge_get_stats_full(BridgeStatsTypeDef *stats);

/**
 * @brief 获取桥接统计信息 (简化版)
 * @param ble_to_uart_count BLE→UART帧计数输出 (可传NULL)
 * @param uart_to_ble_count UART→BLE帧计数输出 (可传NULL)
 * @param error_count 错误计数输出 (可传NULL)
 */
void ble_bridge_get_stats(uint32_t *ble_to_uart_count,
                          uint32_t *uart_to_ble_count,
                          uint32_t *error_count);

/**
 * @brief 重置统计计数器
 */
void ble_bridge_reset_stats(void);

/**
 * @brief 获取当前连接状态
 * @return 连接状态码 (BRIDGE_CONN_xxx)
 */
uint8_t ble_bridge_get_conn_state(void);

/**
 * @brief 通过BLE发送数据 (内部使用或外部直接调用)
 * @param data 数据指针
 * @param len 数据长度
 * @return 0=成功, 非0=失败
 * @note  数据通过ae02特征(Notify)发送
 *        如果数据超过MTU限制，会自动分包
 */
int ble_bridge_send(const uint8_t *data, uint16_t len);

/**
 * @brief BLE GATT事件回调 (注册到SDK)
 * @param packet 事件数据包
 * @param size 数据包大小
 * @note  在le_server_module.c中注册此回调
 */
void ble_bridge_gatt_handler(uint8_t *packet, uint16_t size);

/* ============================================================================
 * SDK集成宏 (简化SDK修改)
 * ============================================================================ */

/**
 * @brief 在SDK的app_config.h或board.h中包含此头文件
 */
#define BLE_BRIDGE_INTEGRATION  1

/**
 * @brief SDK le_server_module.c 修改指引:
 *
 * 方法1 - 替换回调 (推荐):
 *   在 le_server_module.c 的 can_send_now_now() 或 att_write_callback 中:
 *   #include "ble_bridge.h"
 *   将写回调指向: ble_bridge_gatt_handler
 *
 * 方法2 - 使用le_rcsp_module.c:
 *   禁用RCSP, 在rcsp_bluetooth.c中:
 *   #include "ble_bridge.h"
 *   在rcsp_msg_handler[]中添加自定义处理
 *
 * 方法3 - 独立BLE任务:
 *   创建task_ble任务, 在其中初始化并运行桥接
 */

#ifdef __cplusplus
}
#endif

#endif /* __BLE_BRIDGE_H__ */
