// Partic DWARF exception runtime — bare metal RISC-V 64
// Requires: libunwind.a linked in

typedef unsigned long long uint64_t;
typedef long long int64_t;
typedef unsigned int uint32_t;
typedef int int32_t;
typedef unsigned short uint16_t;
typedef short int16_t;
typedef unsigned char uint8_t;
typedef unsigned long uintptr_t;

#define PARTIC_EXN_CLASS 0x5052544300000000ULL

typedef int _Unwind_Reason_Code;
typedef uint64_t _Unwind_Exception_Class;
typedef void (*_Unwind_Exception_Cleanup_Fn)(_Unwind_Reason_Code, void *);
typedef int _Unwind_Action;

#define _URC_NO_REASON                0
#define _URC_FOREIGN_EXCEPTION_CAUGHT 1
#define _URC_FATAL_PHASE2_ERROR       2
#define _URC_FATAL_PHASE1_ERROR       3
#define _URC_NORMAL_STOP              4
#define _URC_END_OF_STACK             5
#define _URC_HANDLER_FOUND            6
#define _URC_INSTALL_CONTEXT          7
#define _URC_CONTINUE_UNWIND          8

#define _UA_SEARCH_PHASE   1
#define _UA_CLEANUP_PHASE  2
#define _UA_HANDLER_FRAME  4

struct _Unwind_Exception {
    _Unwind_Exception_Class exception_class;
    _Unwind_Exception_Cleanup_Fn exception_cleanup;
    uint64_t private_1;
    uint64_t private_2;
};

typedef struct _Unwind_Context _Unwind_Context;
typedef _Unwind_Reason_Code (*_Unwind_Personality_Fn)(
    int, _Unwind_Action, uint64_t, struct _Unwind_Exception *, _Unwind_Context *);

typedef struct {
    struct _Unwind_Exception unwind;
    void *exn_obj;
    void *vtable;
} ParticExnWrap;

extern uint64_t kr_malloc(uint64_t size);
extern void kr_free(uint64_t addr);
extern void *memset(void *s, int c, unsigned long n);

extern _Unwind_Reason_Code _Unwind_RaiseException(struct _Unwind_Exception *);
extern void _Unwind_Resume(struct _Unwind_Exception *);
extern uint64_t _Unwind_GetIP(_Unwind_Context *);
extern void _Unwind_SetIP(_Unwind_Context *, uint64_t);
extern uint64_t _Unwind_GetRegionStart(_Unwind_Context *);
extern uint64_t _Unwind_GetLanguageSpecificData(_Unwind_Context *);
extern uint64_t _Unwind_GetGR(_Unwind_Context *, int);
extern void _Unwind_SetGR(_Unwind_Context *, int, uint64_t);

static uint64_t read_uleb128(const uint8_t **p) {
    uint64_t r = 0; int s = 0;
    while (1) {
        uint8_t b = **p; (*p)++;
        r |= (uint64_t)(b & 0x7f) << s;
        if (!(b & 0x80)) break;
        s += 7;
    }
    return r;
}

static int64_t read_sleb128(const uint8_t **p) {
    uint64_t r = 0; int s = 0; uint8_t b;
    do { b = **p; (*p)++; r |= (uint64_t)(b & 0x7f) << s; s += 7; } while (b & 0x80);
    if (s < 64 && (b & 0x40)) r |= -(1ULL << s);
    return (int64_t)r;
}

static uint64_t read_enc_val(const uint8_t **p, uint8_t enc) {
    if (enc == 0xff) return 0;
    int indirect = (enc & 0x80) != 0;
    int pcrel    = (enc & 0x10) != 0;
    enc &= 0x0f;
    const uint8_t *start = *p;
    uint64_t v = 0;
    switch (enc) {
        case 0x00: v = *(uint64_t*)*p; *p += 8; break;
        case 0x01: v = read_uleb128(p); break;
        case 0x02: v = *(uint16_t*)*p; *p += 2; break;
        case 0x03: v = *(uint32_t*)*p; *p += 4; break;
        case 0x0b: v = *(int32_t*)*p;  *p += 4; break;
        case 0x0c: v = *(int64_t*)*p;  *p += 8; break;
        case 0x0d: v = *(int16_t*)*p;  *p += 2; break;
        default: return 0;
    }
    if (pcrel) v += (uint64_t)(uintptr_t)start;
    if (indirect) v = *(uint64_t*)(uintptr_t)v;
    return v;
}

static int vtable_isa(void *obj_vt, void *catch_vt) {
    if (obj_vt == catch_vt) return 1;
    while (obj_vt) {
        void *parent = *(void**)obj_vt;
        if (parent == catch_vt) return 1;
        obj_vt = parent;
    }
    return 0;
}

_Unwind_Reason_Code partic_personality(
    int version, _Unwind_Action actions, uint64_t exceptionClass,
    struct _Unwind_Exception *exn, _Unwind_Context *ctx)
{
    if (version != 1) return _URC_CONTINUE_UNWIND;
    if (exceptionClass != PARTIC_EXN_CLASS) return _URC_CONTINUE_UNWIND;

    ParticExnWrap *wrap = (ParticExnWrap*)exn;
    void *exn_vt = wrap->vtable;

    uint64_t lsda = _Unwind_GetLanguageSpecificData(ctx);
    uint64_t ip = _Unwind_GetIP(ctx);
    uint64_t region = _Unwind_GetRegionStart(ctx);
    uint64_t ip_off = ip - region;
    if (ip_off > 0) ip_off--;

    if (!lsda) return _URC_CONTINUE_UNWIND;

    const uint8_t *p = (const uint8_t *)lsda;
    uint8_t lp_enc = *p++;
    if (lp_enc != 0xff) p += 4;
    uint8_t tt_enc = *p++;
    uint64_t tt_base = 0;
    if (tt_enc != 0xff) tt_base = read_uleb128(&p);
    const uint8_t *type_table = p + tt_base;
    uint8_t cs_enc = *p++;
    uint64_t cs_len = read_uleb128(&p);
    const uint8_t *cs_end = p + cs_len;

    int found = 0;
    uint64_t lp_off = 0, action = 0;
    const uint8_t *cs_p = p;
    while (cs_p < cs_end) {
        uint64_t s = read_enc_val(&cs_p, cs_enc);
        uint64_t l = read_enc_val(&cs_p, cs_enc);
        lp_off = read_enc_val(&cs_p, cs_enc);
        action = read_uleb128(&cs_p);
        if (ip_off >= s && ip_off < s + l) { found = 1; break; }
    }
    const uint8_t *action_table = cs_end;

    if (actions & _UA_SEARCH_PHASE) {
        if (!found) return _URC_CONTINUE_UNWIND;
        if (action == 0) return _URC_CONTINUE_UNWIND;

        const uint8_t *act = action_table + action - 1;
        while (1) {
            const uint8_t *rec_start = act;
            int64_t filter = read_sleb128(&act);
            int64_t next = read_sleb128(&act);
            if (filter > 0) {
                const uint8_t *tp = type_table - filter * 4;
                uint64_t type_info = read_enc_val(&tp, tt_enc);
                uint32_t catch_ti = (uint32_t)type_info;
                if (catch_ti == 0) return _URC_HANDLER_FOUND;
                void *catch_vt = *(void**)(uintptr_t)catch_ti;
                if (vtable_isa(exn_vt, catch_vt)) return _URC_HANDLER_FOUND;
            }
            if (next == 0) break;
            act = rec_start + next;
        }
        return _URC_CONTINUE_UNWIND;
    }

    if (actions & _UA_CLEANUP_PHASE) {
        if (!found) return _URC_CONTINUE_UNWIND;
        if (lp_off == 0) return _URC_CONTINUE_UNWIND;   // no landing pad

        int is_handler = (actions & _UA_HANDLER_FRAME) != 0;
        int has_cleanup = (action == 0);

        if (!is_handler && !has_cleanup && action != 0) {
            const uint8_t *act = action_table + action - 1;
            while (1) {
                const uint8_t *rec = act;
                int64_t f = read_sleb128(&act);
                int64_t n = read_sleb128(&act);
                if (f <= 0) { has_cleanup = 1; break; }
                if (n == 0) break;
                act = rec + n;
            }
        }

        if (!is_handler && !has_cleanup)
            return _URC_CONTINUE_UNWIND;

        int selector = 0;
        if (action) {
            const uint8_t *act = action_table + action - 1;
            int slot = 0;
            while (1) {
                const uint8_t *rec = act;
                int64_t filter = read_sleb128(&act);
                int64_t next = read_sleb128(&act);
                slot++;
                if (filter > 0) {
                    const uint8_t *tp = type_table - filter * 4;
                    uint64_t type_info = read_enc_val(&tp, tt_enc);
                    uint32_t catch_ti = (uint32_t)type_info;
                    void *catch_vt = catch_ti ? *(void**)(uintptr_t)catch_ti : 0;
                    if (catch_ti == 0 || vtable_isa(exn_vt, catch_vt)) {
                        selector = slot;
                        break;
                    }
                }
                if (next == 0) break;
                act = rec + next;
            }
        }

        _Unwind_SetIP(ctx, region + lp_off);
        _Unwind_SetGR(ctx, 10, (uint64_t)wrap);
        _Unwind_SetGR(ctx, 11, selector);
        return _URC_INSTALL_CONTEXT;
    }

    return _URC_CONTINUE_UNWIND;
}

void partic_throw(void *exn_obj, void *vtable) {
    ParticExnWrap *wrap = (ParticExnWrap *)kr_malloc(sizeof(ParticExnWrap));
    if (!wrap) while (1) {}
    memset(wrap, 0, sizeof(ParticExnWrap));
    wrap->unwind.exception_class = PARTIC_EXN_CLASS;
    wrap->exn_obj = exn_obj;
    wrap->vtable = vtable;
    _Unwind_RaiseException(&wrap->unwind);
    while (1) {}
}
