// x86_64 PS/2 keyboard (8042) driver.
//
// Interrupt-driven: IRQ1 reads the scancode from port 0x60, translates
// set-1 scancodes to Linux evdev keycodes (KEY_*), and pushes {type,code,
// value} events into a ring buffer. The Partic layer polls via ps2kbd_poll
// and forwards to /dev/kbd. Keycode -> character mapping is left to user
// space.
//
// evdev keycode layout is identical to set-1 scancode for 0x01..0x6D with a
// few exceptions; extended keys (E0 prefix) get dedicated keycodes above
// the base range.

#include <stdint.h>

#define PS2_DATA   0x60
#define PS2_STATUS 0x64
#define PS2_CMD    0x64

#define STATUS_OUTPUT_FULL 0x01
#define STATUS_INPUT_FULL  0x02

#define EV_KEY 1

#define RING_SIZE 64

static uint16_t ring_type[RING_SIZE];
static uint16_t ring_code[RING_SIZE];
static uint32_t ring_value[RING_SIZE];
static volatile uint16_t ring_head;
static volatile uint16_t ring_tail;

// ── set-1 scancode → evdev keycode ─────────────────
// 0x01..0x53 mostly match evdev 1:1; the table below maps the exceptions.
// 0x54 = sysrq (evdev 99), 0x5B/0x5C/0x5D = Windows keys (125/126/127),
// 0x5E = power, 0x5F = sleep, 0x63 = wake.
static const uint8_t scancode_map[0x64] = {
    /* 0x00 */ 0,
    /* 0x01 */ 1,   /* ESC */
    /* 0x02 */ 2,   /* 1 */
    /* 0x03 */ 3,
    /* 0x04 */ 4,
    /* 0x05 */ 5,
    /* 0x06 */ 6,
    /* 0x07 */ 7,
    /* 0x08 */ 8,
    /* 0x09 */ 9,
    /* 0x0A */ 10,
    /* 0x0B */ 11,
    /* 0x0C */ 12,  /* MINUS */
    /* 0x0D */ 13,  /* EQUAL */
    /* 0x0E */ 14,  /* BACKSPACE */
    /* 0x0F */ 15,  /* TAB */
    /* 0x10 */ 16,  /* Q */
    /* 0x11 */ 17,
    /* 0x12 */ 18,
    /* 0x13 */ 19,
    /* 0x14 */ 20,
    /* 0x15 */ 21,
    /* 0x16 */ 22,
    /* 0x17 */ 23,
    /* 0x18 */ 24,
    /* 0x19 */ 25,
    /* 0x1A */ 26,  /* LEFTBRACE */
    /* 0x1B */ 27,  /* RIGHTBRACE */
    /* 0x1C */ 28,  /* ENTER */
    /* 0x1D */ 29,  /* LEFTCTRL */
    /* 0x1E */ 30,  /* A */
    /* 0x1F */ 31,
    /* 0x20 */ 32,
    /* 0x21 */ 33,
    /* 0x22 */ 34,
    /* 0x23 */ 35,
    /* 0x24 */ 36,
    /* 0x25 */ 37,
    /* 0x26 */ 38,
    /* 0x27 */ 39,  /* SEMICOLON */
    /* 0x28 */ 40,  /* APOSTROPHE */
    /* 0x29 */ 41,  /* GRAVE */
    /* 0x2A */ 42,  /* LEFTSHIFT */
    /* 0x2B */ 43,  /* BACKSLASH */
    /* 0x2C */ 44,  /* Z */
    /* 0x2D */ 45,
    /* 0x2E */ 46,
    /* 0x2F */ 47,
    /* 0x30 */ 48,
    /* 0x31 */ 49,
    /* 0x32 */ 50,
    /* 0x33 */ 51,  /* COMMA */
    /* 0x34 */ 52,  /* DOT */
    /* 0x35 */ 53,  /* SLASH */
    /* 0x36 */ 54,  /* RIGHTSHIFT */
    /* 0x37 */ 55,  /* KPASTERISK */
    /* 0x38 */ 56,  /* LEFTALT */
    /* 0x39 */ 57,  /* SPACE */
    /* 0x3A */ 58,  /* CAPSLOCK */
    /* 0x3B */ 59,  /* F1 */
    /* 0x3C */ 60,
    /* 0x3D */ 61,
    /* 0x3E */ 62,
    /* 0x3F */ 63,
    /* 0x40 */ 64,
    /* 0x41 */ 65,
    /* 0x42 */ 66,
    /* 0x43 */ 67,
    /* 0x44 */ 68,
    /* 0x45 */ 69,  /* NUMLOCK */
    /* 0x46 */ 70,  /* SCROLLLOCK */
    /* 0x47 */ 71,  /* KP7 */
    /* 0x48 */ 72,
    /* 0x49 */ 73,
    /* 0x4A */ 74,  /* KPMINUS */
    /* 0x4B */ 75,
    /* 0x4C */ 76,
    /* 0x4D */ 77,
    /* 0x4E */ 78,  /* KPPLUS */
    /* 0x4F */ 79,
    /* 0x50 */ 80,
    /* 0x51 */ 81,
    /* 0x52 */ 82,
    /* 0x53 */ 83,  /* KPDOT */
    /* 0x54 */ 99,  /* SYSRQ */
    /* 0x55 */ 84,  /* KPJPCOMMA -> evdev 84 (0x54 is keypad comma; use 84) */
    /* 0x56 */ 86,  /* 102ND */
    /* 0x57 */ 87,  /* F11 */
    /* 0x58 */ 88,  /* F12 */
    /* 0x59 */ 85,  /* ZENKAKUHANKAKU (JP) */
    /* 0x5A */ 0,
    /* 0x5B */ 125, /* LEFT META */
    /* 0x5C */ 126, /* RIGHT META */
    /* 0x5D */ 127, /* MENU/COMPOSE */
    /* 0x5E */ 116, /* POWER */
    /* 0x5F */ 142, /* SLEEP */
    /* 0x60 */ 0,
    /* 0x61 */ 0,
    /* 0x62 */ 0,
    /* 0x63 */ 143, /* WAKE */
};

// E0-prefixed scancodes: [e0, scancode] -> evdev keycode.
static const struct { uint8_t sc; uint8_t code; } e0_map[] = {
    { 0x1C, 96 },   /* KPENTER */
    { 0x1D, 97 },   /* RIGHTCTRL */
    { 0x35, 98 },   /* KPSLASH */
    { 0x37, 99 },   /* SYSRQ (alternate) */
    { 0x38, 100 },  /* RIGHTALT */
    { 0x47, 102 },  /* HOME */
    { 0x48, 103 },  /* UP */
    { 0x49, 104 },  /* PAGEUP */
    { 0x4B, 105 },  /* LEFT */
    { 0x4D, 106 },  /* RIGHT */
    { 0x4F, 107 },  /* END */
    { 0x50, 108 },  /* DOWN */
    { 0x51, 109 },  /* PAGEDOWN */
    { 0x52, 110 },  /* INSERT */
    { 0x53, 111 },  /* DELETE */
    { 0x5B, 125 },  /* LEFT META */
    { 0x5C, 126 },  /* RIGHT META */
    { 0x5D, 127 },  /* MENU */
    { 0x5E, 116 },  /* POWER */
    { 0x5F, 142 },  /* SLEEP */
    { 0x63, 143 },  /* WAKE */
};

static inline void outb(uint16_t port, uint8_t v) {
    __asm__ volatile("outb %0, %1" :: "a"(v), "Nd"(port));
}
static inline uint8_t inb(uint16_t port) {
    uint8_t v;
    __asm__ volatile("inb %1, %0" : "=a"(v) : "Nd"(port));
    return v;
}

static int wait_output(void) {
    for (int i = 0; i < 10000; i++) {
        if (inb(PS2_STATUS) & STATUS_OUTPUT_FULL) return 1;
    }
    return 0;
}

static int wait_input(void) {
    for (int i = 0; i < 10000; i++) {
        if (!(inb(PS2_STATUS) & STATUS_INPUT_FULL)) return 1;
    }
    return 0;
}

// Push one event into the ring. Drops if full.
static void push_event(uint16_t code, uint32_t value) {
    uint16_t next = (uint16_t)((ring_head + 1) % RING_SIZE);
    if (next == ring_tail) return;   // full
    ring_type[ring_head] = EV_KEY;
    ring_code[ring_head] = code;
    ring_value[ring_head] = value;
    ring_head = next;
}

// Called from the IRQ1 handler (interrupt context).
void ps2kbd_irq(void) {
    if (!(inb(PS2_STATUS) & STATUS_OUTPUT_FULL)) return;
    uint8_t sc = inb(PS2_DATA);

    static uint8_t e0_pending = 0;
    if (sc == 0xE0) { e0_pending = 1; return; }
    if (sc == 0xE1) { return; }   // pause: multi-byte, ignore for now

    uint8_t raw = sc & 0x7F;
    uint32_t value = (sc & 0x80) ? 0 : 1;   // break = 0, make = 1

    uint16_t code = 0;
    if (e0_pending) {
        for (unsigned int i = 0; i < sizeof(e0_map) / sizeof(e0_map[0]); i++) {
            if (e0_map[i].sc == raw) { code = e0_map[i].code; break; }
        }
        e0_pending = 0;
    } else if (raw < 0x64) {
        code = scancode_map[raw];
    }

    if (code != 0) push_event(code, value);
}

// 8042 self-test + enable keyboard.
// Returns 0 on success, nonzero if the controller is absent.
int ps2kbd_init(void) {
    // Disable devices, flush output buffer.
    if (!wait_input()) return -1;
    outb(PS2_CMD, 0xAD);   // disable PS/2 mouse
    if (!wait_input()) return -1;
    outb(PS2_CMD, 0xA7);   // disable PS/2 keyboard
    while (inb(PS2_STATUS) & STATUS_OUTPUT_FULL) inb(PS2_DATA);

    // Controller self-test (0xAA) -> 0x55.
    if (!wait_input()) return -1;
    outb(PS2_CMD, 0xAA);
    if (!wait_output()) return -1;
    uint8_t st = inb(PS2_DATA);
    if (st != 0x55) return -1;

    // Enable keyboard interface, reset keyboard.
    if (!wait_input()) return -1;
    outb(PS2_CMD, 0xAE);   // enable keyboard
    if (!wait_input()) return -1;
    outb(PS2_CMD, 0x60);   // write controller command byte
    if (!wait_input()) return -1;
    // 8042 命令字节 bit0 = 键盘数据产生 IRQ1。0x01 = 只开 IRQ1、不启用
    // translate（下面显式把键盘设成 scancode set 1，直通即可；若设 0x40
    // 会漏掉 IRQ 位，QEMU 只在 bit0 置位时拉 IRQ1 —— 键盘就永远没中断）。
    outb(PS2_DATA, 0x01);

    if (!wait_input()) return -1;
    outb(PS2_DATA, 0xFF);  // keyboard reset
    // Drain response bytes (ACK 0xFA, BAT 0xAA) with a small delay loop.
    for (int i = 0; i < 10; i++) {
        if (inb(PS2_STATUS) & STATUS_OUTPUT_FULL) inb(PS2_DATA);
    }

    // Select scancode set 1 (0xF0 0x01) so our map applies.
    if (!wait_input()) return -1;
    outb(PS2_DATA, 0xF0);
    if (!wait_input()) return -1;
    outb(PS2_DATA, 0x01);
    for (int i = 0; i < 4; i++) {
        if (inb(PS2_STATUS) & STATUS_OUTPUT_FULL) inb(PS2_DATA);
    }

    ring_head = 0;
    ring_tail = 0;
    return 0;
}

// Returns 1 and writes the raw 8-byte event {le16 type, le16 code,
// le32 value} to ev_out if available, else 0.
int ps2kbd_poll(void *ev_out) {
    if (ring_head == ring_tail) return 0;
    uint16_t type  = ring_type[ring_tail];
    uint16_t code  = ring_code[ring_tail];
    uint32_t value = ring_value[ring_tail];
    ring_tail = (uint16_t)((ring_tail + 1) % RING_SIZE);

    uint8_t *o = (uint8_t *)ev_out;
    o[0] = (uint8_t)(type & 0xFF);
    o[1] = (uint8_t)((type >> 8) & 0xFF);
    o[2] = (uint8_t)(code & 0xFF);
    o[3] = (uint8_t)((code >> 8) & 0xFF);
    o[4] = (uint8_t)(value & 0xFF);
    o[5] = (uint8_t)((value >> 8) & 0xFF);
    o[6] = (uint8_t)((value >> 16) & 0xFF);
    o[7] = (uint8_t)((value >> 24) & 0xFF);
    return 1;
}
