#include "../include/Base.h"
#include "../include/Uefi.h"
#include "../include/Protocol/GraphicsOutput.h"
#include "../include/Protocol/LoadedImage.h"
#include "../include/Protocol/SimpleFileSystem.h"
#include "../include/Guid/Fdt.h"

typedef struct {
    void*    framebuffer;
    UINT64   width, height, stride, format;
    UINT64   memoryMap, memoryMapSize, memoryMapDescriptorSize;
    void*    fdt_addr;
} BootInfo;

#define ET_EXEC  2
#define PT_LOAD  1

typedef struct {
    UINT8  e_ident[16]; UINT16 e_type, e_machine; UINT32 e_version;
    UINT64 e_entry, e_phoff, e_shoff; UINT32 e_flags;
    UINT16 e_ehsize, e_phentsize, e_phnum, e_shentsize, e_shnum, e_shstrndx;
} Elf64_Ehdr;

typedef struct {
    UINT32 p_type, p_flags; UINT64 p_offset, p_vaddr, p_paddr;
    UINT64 p_filesz, p_memsz, p_align;
} Elf64_Phdr;

typedef void (*KernEntry)(BootInfo*);

static void memcpy(void* d, const void* s, UINTN n) {
    UINT8 *dst = d; const UINT8 *src = s; while (n--) *dst++ = *src++;
}
static void memset(void* p, UINT8 v, UINTN n) {
    UINT8 *d = p; while (n--) *d++ = v;
}

static void print_u64_hex(EFI_SYSTEM_TABLE* st, UINT64 v) {
    CHAR16 buf[19]; int i;
    for (i = 0; i < 16; i++) {
        UINT8 nib = (UINT8)((v >> (60 - i * 4)) & 0xF);
        buf[i] = nib < 10 ? (CHAR16)('0' + nib) : (CHAR16)('a' + nib - 10);
    }
    buf[16] = L'\r'; buf[17] = L'\n'; buf[18] = 0;
    st->ConOut->OutputString(st->ConOut, buf);
}

/* FDT config table GUID（EFI_FDT_TABLE_GUID / EFI_FDT_GUID，标准 0xe0 结尾）。
 * 逐字节比较，不假设端序。 */
static int guid_is_fdt(const EFI_GUID* g) {
    const UINT8* b = (const UINT8*)g;
    static const UINT8 expect[16] = {
        0xd5,0x21,0xb6,0xb1, 0x9c,0xf1, 0xa5,0x41,
        0x83,0x0b,0xd9,0x15, 0x2c,0x69, 0xaa, 0xe0
    };
    int i;
    for (i = 0; i < 16; i++) if (b[i] != expect[i]) return 0;
    return 1;
}

static EFI_STATUS setup_gop(EFI_BOOT_SERVICES* bs, BootInfo* info) {
    EFI_GUID g = EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID;
    EFI_HANDLE* h = 0; UINTN c = 0;
    EFI_GRAPHICS_OUTPUT_PROTOCOL* gop;
    EFI_STATUS s = bs->LocateHandleBuffer(ByProtocol, &g, 0, &c, &h);
    if (EFI_ERROR(s) || c == 0) return EFI_ABORTED;
    s = bs->HandleProtocol(h[0], &g, (VOID**)&gop);
    if (EFI_ERROR(s)) { bs->FreePool(h); return EFI_ABORTED; }
    gop->SetMode(gop, 0);
    EFI_GRAPHICS_OUTPUT_PROTOCOL_MODE* m = gop->Mode;
    info->framebuffer = (VOID*)m->FrameBufferBase;
    info->width  = m->Info->HorizontalResolution;
    info->height = m->Info->VerticalResolution;
    info->stride = m->Info->PixelsPerScanLine * 4;
    info->format = m->Info->PixelFormat == PixelBlueGreenRedReserved8BitPerColor ? 0 : 1;
    bs->FreePool(h);
    return EFI_SUCCESS;
}

static EFI_STATUS open_elf(EFI_BOOT_SERVICES* bs, EFI_HANDLE img, VOID** out, UINT64* outSize) {
    EFI_GUID fsG = EFI_SIMPLE_FILE_SYSTEM_PROTOCOL_GUID;
    EFI_GUID liG = EFI_LOADED_IMAGE_PROTOCOL_GUID;
    EFI_LOADED_IMAGE_PROTOCOL* li = 0;
    EFI_SIMPLE_FILE_SYSTEM_PROTOCOL* fs = 0;
    EFI_STATUS s = bs->HandleProtocol(img, &liG, (VOID**)&li);
    if (!EFI_ERROR(s) && li)
        s = bs->HandleProtocol(li->DeviceHandle, &fsG, (VOID**)&fs);
    if (EFI_ERROR(s) || !fs) {
        EFI_HANDLE* handles = 0; UINTN count = 0;
        s = bs->LocateHandleBuffer(ByProtocol, &fsG, 0, &count, &handles);
        if (!EFI_ERROR(s) && count > 0)
            s = bs->HandleProtocol(handles[0], &fsG, (VOID**)&fs);
    }
    if (EFI_ERROR(s) || !fs) return EFI_ABORTED;
    EFI_FILE_PROTOCOL *root, *file;
    s = fs->OpenVolume(fs, &root);
    if (EFI_ERROR(s)) return s;
    s = root->Open(root, &file, L"KERNEL.ELF", EFI_FILE_MODE_READ, 0);
    if (EFI_ERROR(s)) { root->Close(root); return s; }
    UINTN sz = 16 * 1024 * 1024;
    s = bs->AllocatePool(EfiLoaderData, sz, out);
    if (EFI_ERROR(s)) { file->Close(file); root->Close(root); return s; }
    s = file->Read(file, &sz, *out);
    file->Close(file); root->Close(root);
    *outSize = sz;
    return s;
}

static UINT64 load_elf(EFI_BOOT_SERVICES* bs, EFI_HANDLE img, VOID* elf, BootInfo* info) {
    Elf64_Ehdr* eh = elf;
    Elf64_Phdr* ph = (Elf64_Phdr*)((UINT8*)elf + eh->e_phoff);
    UINT64 lo = ~0ULL, hi = 0;
    for (UINTN i = 0; i < eh->e_phnum; i++) {
        if (ph[i].p_type != PT_LOAD || ph[i].p_memsz == 0) continue;
        UINT64 s = ph[i].p_vaddr, e = s + ph[i].p_memsz;
        if (s < lo) lo = s; if (e > hi) hi = e;
    }
    if (hi <= lo) return eh->e_entry;
    UINTN pages = (hi - lo + 4095) / 4096;
    EFI_PHYSICAL_ADDRESS base = lo & ~0xFFFULL;
    bs->AllocatePages(AllocateAddress, EfiLoaderCode, pages, &base);

    // 段拷贝必须在 ExitBootServices 之前完成：u-boot 退出后 MMU/内存
    // 状态不可靠，memcpy 可能写错地方（曾导致 trap_entry 内存为全 0）。
    // 只拷文件段（filesz），bss 由 kernel 入口（kern_entry）自清——
    // 不在这里 memset，避免把仍在提供 EFI 服务的 u-boot 后部代码清掉。
    for (UINTN i = 0; i < eh->e_phnum; i++) {
        if (ph[i].p_type != PT_LOAD || ph[i].p_memsz == 0) continue;
        UINT64 d = ph[i].p_vaddr;
        if (ph[i].p_filesz > 0)
            memcpy((VOID*)d, (UINT8*)elf + ph[i].p_offset, ph[i].p_filesz);
    }

    // ExitBootServices 前完成所有 EFI 服务调用（GetMemoryMap 等）。
    UINTN sz = 0, key = 0, dsc = 0; UINT32 ver = 0;
    bs->GetMemoryMap(&sz, 0, &key, &dsc, &ver); sz += 4096;
    EFI_MEMORY_DESCRIPTOR* mm;
    bs->AllocatePool(EfiBootServicesData, sz, (VOID**)&mm);
    while (1) { bs->GetMemoryMap(&sz, mm, &key, &dsc, &ver); if (!EFI_ERROR(bs->ExitBootServices(img, key))) break; }
    info->memoryMap = (UINT64)mm; info->memoryMapSize = sz; info->memoryMapDescriptorSize = dsc;

    return eh->e_entry;
}

static void __attribute__((noreturn)) jump_kernel(UINT64 entry, BootInfo* info) {
    ((KernEntry)entry)(info); while (1) __asm__ volatile("wfi");
}

EFI_STATUS EFIAPI efi_main(EFI_HANDLE h, EFI_SYSTEM_TABLE* st) {
    EFI_BOOT_SERVICES* bs = st->BootServices;
    BootInfo info;
    memset(&info, 0, sizeof(info));   // 无 GOP 时 width/height/stride 必须为 0，不能留栈垃圾

    st->ConOut->OutputString(st->ConOut, L"Partix RV64\r\n");

    info.fdt_addr = 0;
    {
        UINTN i;
        for (i = 0; i < st->NumberOfTableEntries; i++) {
            if (guid_is_fdt(&st->ConfigurationTable[i].VendorGuid)) {
                info.fdt_addr = st->ConfigurationTable[i].VendorTable;
                break;
            }
        }
    }
    if (info.fdt_addr) {
        st->ConOut->OutputString(st->ConOut, L"[BL] FDT found 0x");
        print_u64_hex(st, (UINT64)info.fdt_addr);
    } else {
        st->ConOut->OutputString(st->ConOut, L"[BL] FDT NOT found\r\n");
    }

    setup_gop(bs, &info);

    VOID* elf; UINT64 elfSz;
    if (EFI_ERROR(open_elf(bs, h, &elf, &elfSz))) {
        st->ConOut->OutputString(st->ConOut, L"KERNEL.ELF not found\r\n");
        while (1);
    }

    UINT64 entry = load_elf(bs, h, elf, &info);
    // ExitBootServices 已在 load_elf 内完成，此后不能再调 EFI 服务
    // （包括 FreePool）；这块缓冲留给 kernel 接管后的内存池。
    jump_kernel(entry, &info);
}
