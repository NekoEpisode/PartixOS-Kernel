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

    fdt_header = (fdt_header_t *)fdt_ptr;
    if (fdt_header->magic != FDT_MAGIC) return;
    if (fdt_header->version < 16) return;

    fdt_struct  = (const uint8_t *)fdt_ptr + fdt_header->off_dt_struct;
    fdt_struct_size = fdt_header->size_dt_struct;
    fdt_strings = (const char *)fdt_ptr + fdt_header->off_dt_strings;

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

/* Auto fallback to QEMU */
uint64_t fdt_get_uart_base(void) {
    return fdt_uart_base ? fdt_uart_base : 0x10000000;
}

uint64_t fdt_get_plic_base(void) {
    return fdt_plic_base ? fdt_plic_base : 0x0C000000;
}

uint64_t fdt_get_pcie_base(void) {
    return fdt_pcie_base ? fdt_pcie_base : 0x30000000;
}

uint64_t fdt_get_timebase_freq(void) {
    return fdt_timebase_freq;
}
