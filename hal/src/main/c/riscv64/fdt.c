#include "include/fdt.h"

static fdt_header_t  *fdt_header;
static const char    *fdt_strings;
static const uint8_t *fdt_struct;
static uint32_t       fdt_struct_size;

static uint64_t fdt_uart_base;
static uint64_t fdt_plic_base;
static uint64_t fdt_pcie_base;
static uint64_t fdt_timebase_freq;

enum { DEV_NONE = 0, DEV_UART, DEV_PLIC, DEV_PCIE };
static uint8_t pending_dev[64];
static uint64_t pending_reg[64];
static int      pending_ac[64];
static int      pending_sc[64];
static char     node_name[64][33];

static inline uint32_t be32(const uint8_t *p) {
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] << 8)  |  (uint32_t)p[3];
}

static inline uint64_t be64(const uint8_t *p) {
    return ((uint64_t)be32(p) << 32) | be32(p + 4);
}

static int str_eq(const char *a, const char *b) {
    while (*a && *b && *a == *b) { a++; b++; }
    return *a == *b;
}

/* Node name is "cpus" (possibly "cpus@..."/"cpus:..."). */
static int name_is_cpus(const char *n) {
    if (n[0] != 'c' || n[1] != 'p' || n[2] != 'u' || n[3] != 's') return 0;
    char c = n[4];
    return c == 0 || c == '@' || c == ':';
}

static int match_compatible(const uint8_t *data, uint32_t len,
                            const char *target) {
    uint32_t pos = 0;
    while (pos < len) {
        const char *s = (const char *)(data + pos);
        if (str_eq(s, target)) return 1;
        while (pos < len && data[pos] != 0) pos++;
        pos++;
    }
    return 0;
}

static void set_reg(int depth) {
    uint64_t base = pending_reg[depth];
    switch (pending_dev[depth]) {
    case DEV_UART: if (!fdt_uart_base) fdt_uart_base = base; break;
    case DEV_PLIC: if (!fdt_plic_base) fdt_plic_base = base; break;
    case DEV_PCIE: if (!fdt_pcie_base) fdt_pcie_base = base; break;
    }
}

void fdt_init(void *fdt_ptr) {
    fdt_uart_base = 0;
    fdt_plic_base = 0;
    fdt_pcie_base = 0;
    fdt_timebase_freq = 0;
    for (int i = 0; i < 64; i++) {
        pending_dev[i] = DEV_NONE;
        pending_reg[i] = 0;
        pending_ac[i] = 2;
        pending_sc[i] = 2;
        node_name[i][0] = 0;
    }

    if (!fdt_ptr) return;

    // FDT 是大端存储：header 字段必须逐字节大端读（be32），
    // 不能直接解引用结构体字段——小端机器上 magic 会读成字节交换值，
    // 偏移字段会读出放大 2^24 倍的错误值。
    const uint8_t *base = (const uint8_t *)fdt_ptr;
    if (be32(base) != FDT_MAGIC) return;
    uint32_t version = be32(base + 20);
    if (version < 16) return;

    fdt_header = (fdt_header_t *)fdt_ptr;   // 仅 fdt_get_base() 返回用
    // FDT header 字段偏移：magic 0, totalsize 4, off_dt_struct 8,
    // off_dt_strings 12, off_mem_rsvmap 16, version 20, ...,
    // size_dt_strings 32, size_dt_struct 36
    uint32_t off_struct  = be32(base + 8);
    uint32_t off_strings = be32(base + 12);
    uint32_t size_struct = be32(base + 36);

    fdt_struct  = base + off_struct;
    fdt_struct_size = size_struct;
    fdt_strings = (const char *)(base + off_strings);

    const uint8_t *p   = fdt_struct;
    const uint8_t *end = p + fdt_struct_size;
    int depth = 0;

    while (p < end) {
        uint32_t token = be32(p);
        p += 4;

        if (token == FDT_BEGIN_NODE) {
            depth++;
            if (depth < 64) {
                pending_dev[depth] = DEV_NONE;
                pending_reg[depth] = 0;
                pending_ac[depth] = pending_ac[depth - 1];
                pending_sc[depth] = pending_sc[depth - 1];
                int ni = 0;
                while (*p && p < end && ni < 32) node_name[depth][ni++] = (char)*p++;
                node_name[depth][ni] = 0;
            }
            while (*p && p < end) p++;
            p++;
            uint32_t a = (uint32_t)((uintptr_t)p & 3);
            if (a) p += 4 - a;

        } else if (token == FDT_END_NODE) {
            depth--;

        } else if (token == FDT_PROP) {
            uint32_t len     = be32(p); p += 4;
            uint32_t nameoff = be32(p); p += 4;
            const char *name = fdt_strings + nameoff;
            const uint8_t *data = p;

            if (depth >= 64) goto skip_prop;

            if (str_eq(name, "#address-cells") && len == 4)
                pending_ac[depth] = (int)be32(data);
            if (str_eq(name, "#size-cells") && len == 4)
                pending_sc[depth] = (int)be32(data);

            if (name_is_cpus(node_name[depth]) && str_eq(name, "timebase-frequency")
                    && len == 4)
                fdt_timebase_freq = be32(data);

            if (str_eq(name, "reg") && len >= 4) {
                int ac = pending_ac[depth];
                pending_reg[depth] = (ac == 2) ? be64(data) : be32(data);
                set_reg(depth);
            }

            if (str_eq(name, "compatible")) {
                if (!fdt_uart_base && match_compatible(data, len, "ns16550a"))
                    pending_dev[depth] = DEV_UART;
                else if (!fdt_plic_base && match_compatible(data, len, "riscv,plic0"))
                    pending_dev[depth] = DEV_PLIC;
                else if (!fdt_pcie_base && match_compatible(data, len, "pci-host-ecam-generic"))
                    pending_dev[depth] = DEV_PCIE;

                set_reg(depth);  // reg might have come first
            }

        skip_prop:
            p += len;
            uint32_t a2 = (uint32_t)((uintptr_t)p & 3);
            if (a2) p += 4 - a2;

        } else if (token == FDT_NOP) {
        } else if (token == FDT_END) {
            break;
        }
    }
}

/* FDT 基址（FdtScanner 使用；未初始化时为 0） */
uint64_t fdt_get_base(void) {
    return (uint64_t)fdt_header;
}

/* 平台设施基址：仅在 FDT 解析成功且节点存在时返回；0 = 未提供（无 fallback）。
 * 调用方（Partic 侧）必须显式处理 0：环境不完整就报 BOOT FAILED，
 * 绝不带着猜的地址继续跑。 */
uint64_t fdt_get_uart_base(void) {
    return fdt_uart_base;
}

uint64_t fdt_get_plic_base(void) {
    return fdt_plic_base;
}

uint64_t fdt_get_pcie_base(void) {
    return fdt_pcie_base;
}

uint64_t fdt_get_timebase_freq(void) {
    return fdt_timebase_freq;
}
