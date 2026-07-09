/**
 ******************************************************************************
 * @file    app_config.h
 * @author  CC4060 DSP Driver Team
 * @version V1.0.0
 * @date    2026-07-07
 * @brief   CC4060 AC6921A BLE桥接芯片配置
 *          基于杰理AC692x SDK V2.4
 *
 * @note    硬件配置:
 *          - 芯片: JL AC6921A (PI32架构, BR21平台, 160MHz)
 *          - BLE: 5.0, GATT服务UUID 0xAE00
 *          - UART: 连接AK7738 MCU, 115200bps
 *          - 功能: 透明UART↔BLE桥接
 ******************************************************************************
 */

#ifndef __APP_CONFIG_H__
#define __APP_CONFIG_H__

/* ============================================================================
 * SDK功能开关
 * ============================================================================ */

/* 蓝牙模式配置 */
#define TCFG_BREDR_ENABLE           0       /* 禁用经典蓝牙(仅BLE) */
#define TCFG_BLE_ENABLE             1       /* 使能BLE */
#define TCFG_BT_ENABLE              (TCFG_BREDR_ENABLE | TCFG_BLE_ENABLE)

/* BLE角色: 仅Server(外设) */
#define TCFG_BLE_GAP_ROLE           0       /* 0=Server, 1=Client */

/* 禁用不需要的功能 */
#define TCFG_SDMMC_ENABLE           0       /* 禁用SD卡 */
#define TCFG_FM_ENABLE              0       /* 禁用FM */
#define TCFG_LINEIN_ENABLE          0       /* 禁用LineIn */
#define TCFG_USB_ENABLE             0       /* 禁用USB */
#define TCFG_AI_SPEAKER_ENABLE      0       /* 禁用AI音箱 */
#define TCFG_RCSP_BTMATE_EN         0       /* 禁用RCSP(使用透传模式) */
#define TCFG_DEEPBRAIN_AI_EN        0       /* 禁用DeepBrain AI */
#define TCFG_TWS_ENABLE             0       /* 禁用TWS */

/* ============================================================================
 * UART配置 (连接AK7738 MCU)
 * ============================================================================ */

/* UART0: 调试串口 (可选) */
#define TCFG_DEBUG_UART_TX_PORT     IO_PORTA_05
#define TCFG_DEBUG_UART_RX_PORT     IO_PORTA_06
#define TCFG_DEBUG_UART_BAUDRATE    460800

/* UART1: 与AK7738 MCU通信 */
#define TCFG_MCU_UART_TX_PORT       IO_PORTB_04   /* TX → MCU RX */
#define TCFG_MCU_UART_RX_PORT       IO_PORTB_05   /* RX ← MCU TX */
#define TCFG_MCU_UART_BAUDRATE      115200        /* 115200bps */
#define TCFG_MCU_UART_BUF_SIZE      512           /* 接收缓冲区 */

/* ============================================================================
 * BLE配置
 * ============================================================================ */

/* 设备名称 (BLE广播名) */
#define BLE_DEVICE_NAME             "CC4060_DSP"
#define BLE_DEVICE_NAME_LEN         9

/* BLE连接参数 */
#define BLE_CONN_INTERVAL_MIN       8       /* 10ms (8*1.25ms) */
#define BLE_CONN_INTERVAL_MAX       16      /* 20ms */
#define BLE_CONN_LATENCY            0       /* 无从机延迟 */
#define BLE_CONN_SUPERVISION        200     /* 2s超时 */

/* BLE MTU */
#define BLE_ATT_MTU_MAX             247     /* 最大MTU */
#define BLE_ATT_MTU_DEFAULT         23      /* 默认MTU */

/* 广播参数 */
#define BLE_ADV_INTERVAL            160     /* 100ms广播间隔 */
#define BLE_ADV_TIMEOUT             0       /* 永不超时(持续广播) */

/* ============================================================================
 * 系统配置
 * ============================================================================ */

/* 时钟配置 */
#define TCFG_SYS_CLOCK              CLK_16M   /* 系统时钟源 */
#define TCFG_SYS_FREQ               160000000 /* 160MHz主频 */

/* 看门狗 */
#define TCFG_WDT_ENABLE             1
#define TCFG_WDT_TIMEOUT_MS         5000

/* LED指示 */
#define TCFG_LED_PORT               IO_PORTA_00   /* 状态LED */
#define TCFG_LED_ACTIVE_LEVEL       0              /* 低电平点亮 */

/* 按键 (可选) */
#define TCFG_KEY_ENABLE             0

/* Flash配置 */
#define TCFG_FLASH_SIZE             (256 * 1024)  /* 256KB */
#define TCFG_CODE_SIZE              (200 * 1024)  /* 代码区200KB */
#define TCFG_VM_SIZE                (16 * 1024)   /* VM区16KB */
#define TCFG_BTIF_SIZE              (16 * 1024)   /* BT IF区16KB */

#endif /* __APP_CONFIG_H__ */
