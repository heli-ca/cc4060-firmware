#!/usr/bin/env python3
"""
CC4060 UART Bridge Patch Script
Modifies SDK source files to add BLE-to-UART bridge functionality.

Modifications:
1. uart.c - Add UART1 bridge driver for CC4060 DSP communication
2. le_rcsp_module.c - Redirect BLE ae01 writes to UART, add UART-to-BLE forwarding
"""

import os
import sys

def patch_uart_c(filepath):
    """Add CC4060 UART bridge code to uart.c"""
    with open(filepath, 'r') as f:
        content = f.read()

    # ---- Block 1: CC4060 bridge implementation ----
    # Insert after #endif //end of UART update, before void uart_module_init()
    cc4060_block = r"""
/* ========== CC4060 UART BRIDGE BEGIN ========== */
#ifdef CC4060_UART_BRIDGE_EN

#define CC4060_RING_BUF_SIZE 512
static volatile u8 cc4060_ring_buf[CC4060_RING_BUF_SIZE];
static volatile u16 cc4060_ring_wr = 0;
static volatile u16 cc4060_ring_rd = 0;

extern void app_send_user_data_do(void *priv, u8 *data, u16 len);

static void cc4060_uart1_bridge_isr(u8 value, void *p, u8 isr_flag)
{
    if (UART_ISR_TYPE_DATA_COME == isr_flag) {
        u16 next = (cc4060_ring_wr + 1) % CC4060_RING_BUF_SIZE;
        if (next != cc4060_ring_rd) {
            cc4060_ring_buf[cc4060_ring_wr] = value;
            cc4060_ring_wr = next;
        }
    }
}

void cc4060_uart_bridge_init(void)
{
    printf("cc4060_uart_bridge_init\n");

    /* UART1 on USB DP/DM pins */
    JL_IOMAP->CON1 |= BIT(3) | BIT(2);
    JL_USB->CON0 |= BIT(0);
    JL_USB->IO_CON0 = BIT(9) | BIT(10) | BIT(11);   /* IO_MODE & default DP_DIE DM_DIE */
    JL_USB->IO_CON0 |= BIT(0);   /* TX DP */
    JL_USB->IO_CON0 |= BIT(3);   /* RX DM */
    JL_USB->IO_CON0 &= ~BIT(2);  /* tx dp */
    JL_USB->IO_CON0 &= ~BIT(5);  /* DM pulldown */
    JL_USB->IO_CON0 &= ~BIT(7);  /* DM pullup */

    JL_UART1->BAUD = (UART_CLK / 115200) / 4;
    uart_info[1].callback_fun = cc4060_uart1_bridge_isr;

    IRQ_REQUEST(IRQ_UART1_IDX, uart1_isr_fun);
    JL_UART1->CON0 = BIT(14) | BIT(13) | BIT(12) | BIT(10) | BIT(3) | BIT(0);

    cc4060_ring_wr = 0;
    cc4060_ring_rd = 0;

    printf("cc4060 bridge: UART1 init done, baud=115200\n");
}

void cc4060_uart_bridge_send(const u8 *data, u16 len)
{
    u16 i;
    for (i = 0; i < len; i++) {
        JL_UART1->BUF = data[i];
        __asm__ volatile("csync");
        while ((JL_UART1->CON0 & BIT(15)) == 0);    /* TX IDLE */
    }
}

void cc4060_uart_bridge_poll(void)
{
    static u8 frame_buf[512];
    static u16 frame_len = 0;

    while (cc4060_ring_rd != cc4060_ring_wr) {
        u8 byte = cc4060_ring_buf[cc4060_ring_rd];
        cc4060_ring_rd = (cc4060_ring_rd + 1) % CC4060_RING_BUF_SIZE;

        if (frame_len < sizeof(frame_buf)) {
            frame_buf[frame_len++] = byte;
        }

        /* Check for CC4060 frame tail (00 00) after minimum frame length */
        if (frame_len >= 8) {
            u8 last = frame_buf[frame_len - 1];
            u8 prev = frame_buf[frame_len - 2];
            if (last == 0x00 && prev == 0x00) {
                app_send_user_data_do(NULL, frame_buf, frame_len);
                frame_len = 0;
            }
        }

        /* Overflow protection: reset if frame too long */
        if (frame_len >= sizeof(frame_buf)) {
            frame_len = 0;
        }
    }
}

#endif /* CC4060_UART_BRIDGE_EN */
/* ========== CC4060 UART BRIDGE END ========== */
"""

    # Insert before uart_module_init()
    anchor = "void uart_module_init()"
    if anchor not in content:
        print("ERROR: Cannot find uart_module_init() in uart.c")
        return False
    content = content.replace(anchor, cc4060_block + "\n" + anchor)

    # ---- Block 2: Enable CC4060 bridge init in uart_module_init() ----
    # Insert after #endif //end of UART update, before /* uart1_rtx_cts(); */
    init_block = r"""
#if CC4060_UART_BRIDGE_EN
    cc4060_uart_bridge_init();
#endif
"""
    anchor2 = "    /* uart1_rtx_cts(); */"
    if anchor2 not in content:
        print("ERROR: Cannot find uart1_rtx_cts anchor in uart_module_init()")
        return False
    content = content.replace(anchor2, init_block + anchor2)

    with open(filepath, 'w') as f:
        f.write(content)
    print("OK: uart.c patched successfully")
    return True


def patch_le_rcsp_module_c(filepath):
    """Add CC4060 bridge forwarding to le_rcsp_module.c"""
    with open(filepath, 'r') as f:
        content = f.read()

    # ---- Block 1: Extern declarations ----
    # Insert before const u8 link_key_data[16]
    extern_block = r"""
/* ========== CC4060 BRIDGE EXTERN BEGIN ========== */
#ifdef CC4060_UART_BRIDGE_EN
extern void cc4060_uart_bridge_send(const u8 *data, u16 len);
extern void cc4060_uart_bridge_poll(void);
extern void cc4060_uart_bridge_init(void);
#endif
/* ========== CC4060 BRIDGE EXTERN END ========== */

"""
    anchor1 = "const u8 link_key_data[16]"
    if anchor1 not in content:
        print("ERROR: Cannot find link_key_data in le_rcsp_module.c")
        return False
    content = content.replace(anchor1, extern_block + anchor1)

    # ---- Block 2: Redirect BLE ae01 writes to UART bridge ----
    # In app_write_revieve_data(), replace the RCSP callback with bridge send
    old_code = """\t\t\tif (app_recieve_callback) {
\t\t\t\tapp_recieve_callback((void *)channel_priv, data, len);
\t\t\t}"""

    new_code = """\t\t\t#ifdef CC4060_UART_BRIDGE_EN
\t\t\tcc4060_uart_bridge_send(data, len);
\t\t\t#else
\t\t\tif (app_recieve_callback) {
\t\t\t\tapp_recieve_callback((void *)channel_priv, data, len);
\t\t\t}
\t\t\t#endif"""

    if old_code not in content:
        # Try with spaces instead of tabs
        old_code_spaces = old_code.replace('\t', '    ')
        if old_code_spaces in content:
            new_code_spaces = new_code.replace('\t', '    ')
            content = content.replace(old_code_spaces, new_code_spaces)
        else:
            print("WARNING: Cannot find app_recieve_callback block in app_write_revieve_data()")
            print("Trying line-by-line search...")
            # Fallback: search for the key line
            key_line = "app_recieve_callback((void *)channel_priv, data, len);"
            if key_line not in content:
                print("ERROR: Cannot find app_recieve_callback call at all")
                return False
            # Wrap just the callback line
            content = content.replace(
                key_line,
                "#ifdef CC4060_UART_BRIDGE_EN\n\t\t\tcc4060_uart_bridge_send(data, len);\n#else\n\t\t\t" + key_line + "\n#endif"
            )
    else:
        content = content.replace(old_code, new_code)

    # ---- Block 3: Add UART bridge poll call in server_thread_process ----
    # Insert poll call at the beginning of server_thread_process
    anchor3 = "if (!mini_cbuf_is_emtpy(&act_cmd_mctl))"
    poll_block = "#ifdef CC4060_UART_BRIDGE_EN\n    cc4060_uart_bridge_poll();\n#endif\n    "
    if anchor3 in content:
        content = content.replace(anchor3, poll_block + anchor3)
    else:
        print("WARNING: Cannot find server_thread_process anchor, poll won't be called")

    with open(filepath, 'w') as f:
        f.write(content)
    print("OK: le_rcsp_module.c patched successfully")
    return True


def patch_sdk_cfg_h(filepath):
    """Add CC4060_UART_BRIDGE_EN define to sdk_cfg.h"""
    with open(filepath, 'r') as f:
        content = f.read()

    define_block = """
/* CC4060 UART Bridge Enable */
#define CC4060_UART_BRIDGE_EN\t\t1
"""
    # Insert at the beginning of the file (after any initial comments)
    # Find first #ifndef or #define
    for marker in ['#ifndef', '#define', '#ifdef']:
        idx = content.find(marker)
        if idx >= 0:
            content = content[:idx] + define_block + "\n" + content[idx:]
            break
    else:
        # Fallback: prepend
        content = define_block + "\n" + content

    with open(filepath, 'w') as f:
        f.write(content)
    print("OK: sdk_cfg.h patched - CC4060_UART_BRIDGE_EN=1")
    return True


def patch_auto_test_c(filepath, sdk_root):
    """Remove auto_test/auto_test.c from SRCS_C in apps/Makefile to avoid pi32-clang incompatibility"""
    makefile_path = os.path.join(sdk_root, 'apps', 'Makefile')
    if not os.path.exists(makefile_path):
        print("SKIP: apps/Makefile not found")
        return True
    with open(makefile_path, 'r') as f:
        content = f.read()
    # Remove the auto_test/auto_test.c line from SRCS_C entirely
    # (can't use # comment with \ continuation - breaks Makefile syntax)
    patched = content.replace('auto_test/auto_test.c \\\n', '')
    if patched != content:
        with open(makefile_path, 'w') as f:
            f.write(patched)
        print("OK: Removed auto_test/auto_test.c from SRCS_C in apps/Makefile")
    else:
        print("OK: auto_test/auto_test.c already removed from SRCS_C or not found")
    return True


def main():
    sdk_root = sys.argv[1] if len(sys.argv) > 1 else '.'

    files = {
        'uart.c': os.path.join(sdk_root, 'apps', 'cpu', 'uart', 'uart.c'),
        'le_rcsp_module.c': os.path.join(sdk_root, 'apps', 'ble_stack', 'user', 'le_rcsp_module.c'),
        'sdk_cfg.h': os.path.join(sdk_root, 'apps', 'include', 'sdk_cfg.h'),
    }

    ok = True
    for name, path in files.items():
        if not os.path.exists(path):
            print(f"ERROR: {path} not found!")
            ok = False
            continue
        print(f"Patching {name}...")

    if not ok:
        sys.exit(1)

    # Apply patches
    if not patch_uart_c(files['uart.c']):
        ok = False
    if not patch_le_rcsp_module_c(files['le_rcsp_module.c']):
        ok = False
    if not patch_sdk_cfg_h(files['sdk_cfg.h']):
        ok = False

    # Remove auto_test.c from SRCS_C (incompatible with pi32-clang)
    if not patch_auto_test_c(None, sdk_root):
        ok = False

    if not ok:
        print("\nPATCH FAILED - some files could not be modified")
        sys.exit(1)

    print("\nAll CC4060 patches applied successfully!")


if __name__ == '__main__':
    main()
