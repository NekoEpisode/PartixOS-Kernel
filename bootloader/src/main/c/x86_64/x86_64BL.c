#include "../include/Base.h"
#include "../include/Uefi.h"
#include "../include/Protocol/GraphicsOutput.h"
#include "../include/Protocol/LoadedImage.h"
#include "../include/Protocol/SimpleFileSystem.h"

EFI_GUID gEfiGraphicsOutputProtocolGuid = EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID;
EFI_GUID gEfiLoadedImageProtocolGuid = EFI_LOADED_IMAGE_PROTOCOL_GUID;
EFI_GUID gEfiSimpleFileSystemProtocolGuid = EFI_SIMPLE_FILE_SYSTEM_PROTOCOL_GUID;

typedef struct {
    void*    framebuffer;
    UINT64   width;
    UINT64   height;
    UINT64   stride;
    UINT64   format;
    UINT64   memoryMap;
    UINT64   memoryMapSize;
    UINT64   memoryMapDescriptorSize;
    UINT16   cs_selector;
} BootInfo;

#define ET_EXEC  2
#define PT_LOAD  1

typedef struct {
    UINT8   e_ident[16];
    UINT16  e_type;
    UINT16  e_machine;
    UINT32  e_version;
    UINT64  e_entry;
    UINT64  e_phoff;
    UINT64  e_shoff;
    UINT32  e_flags;
    UINT16  e_ehsize;
    UINT16  e_phentsize;
    UINT16  e_phnum;
    UINT16  e_shentsize;
    UINT16  e_shnum;
    UINT16  e_shstrndx;
} Elf64_Ehdr;

typedef struct {
    UINT32  p_type;
    UINT32  p_flags;
    UINT64  p_offset;
    UINT64  p_vaddr;
    UINT64  p_paddr;
    UINT64  p_filesz;
    UINT64  p_memsz;
    UINT64  p_align;
} Elf64_Phdr;

typedef void (*KernEntry)(BootInfo*);

static EFI_SYSTEM_TABLE* ST;
static EFI_BOOT_SERVICES* BS;
static EFI_HANDLE IMAGE_HANDLE;
static EFI_GRAPHICS_OUTPUT_PROTOCOL* GOP;
static BootInfo bootInfo;

static void memcpy(void* dst, const void* src, UINTN n) {
    UINT8 *d = dst; const UINT8 *s = src;
    while (n--) *d++ = *s++;
}

static void memset(void* p, UINT8 v, UINTN n) {
    UINT8 *d = p; while (n--) *d++ = v;
}

static EFI_STATUS setup_gop(void) {
    EFI_GUID gopGuid = EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID;
    EFI_HANDLE* handles = 0;
    UINTN count = 0;

    EFI_STATUS status = BS->LocateHandleBuffer(ByProtocol, &gopGuid, 0, &count, &handles);
    if (EFI_ERROR(status) || count == 0) return EFI_ABORTED;

    status = BS->HandleProtocol(handles[0], &gopGuid, (VOID**)&GOP);
    if (EFI_ERROR(status)) { BS->FreePool(handles); return EFI_ABORTED; }

    EFI_GRAPHICS_OUTPUT_PROTOCOL_MODE* mode = GOP->Mode;
    bootInfo.framebuffer = (VOID*)mode->FrameBufferBase;
    bootInfo.width       = mode->Info->HorizontalResolution;
    bootInfo.height      = mode->Info->VerticalResolution;
    bootInfo.stride      = mode->Info->PixelsPerScanLine * 4;
    bootInfo.format      = mode->Info->PixelFormat == PixelBlueGreenRedReserved8BitPerColor ? 0 : 1;

    BS->FreePool(handles);
    return EFI_SUCCESS;
}

static EFI_STATUS load_file(CHAR16* path, VOID** out, UINT64* outSize) {
    EFI_GUID fsGuid = EFI_SIMPLE_FILE_SYSTEM_PROTOCOL_GUID;
    EFI_GUID lipGuid = EFI_LOADED_IMAGE_PROTOCOL_GUID;
    EFI_LOADED_IMAGE_PROTOCOL* loadedImage;
    EFI_SIMPLE_FILE_SYSTEM_PROTOCOL* fs;
    EFI_FILE_PROTOCOL *root, *file;
    UINTN size;

    EFI_STATUS status = BS->HandleProtocol(IMAGE_HANDLE, &lipGuid, (VOID**)&loadedImage);
    if (EFI_ERROR(status)) return status;

    status = BS->HandleProtocol(loadedImage->DeviceHandle, &fsGuid, (VOID**)&fs);
    if (EFI_ERROR(status)) return status;

    status = fs->OpenVolume(fs, &root);
    if (EFI_ERROR(status)) return status;

    status = root->Open(root, &file, path, EFI_FILE_MODE_READ, 0);
    if (EFI_ERROR(status)) { root->Close(root); return status; }

    size = 16 * 1024 * 1024;
    status = BS->AllocatePool(EfiLoaderData, size, out);
    if (EFI_ERROR(status)) { file->Close(file); root->Close(root); return status; }

    status = file->Read(file, &size, *out);
    file->Close(file);
    root->Close(root);

    *outSize = size;
    return status;
}

// 内核高半区 VMA 基址：主映像物理地址 = vaddr - KERNEL_VMA_BASE
// （与内核 physmap 映射 VA = BASE + phys 一致；不依赖 ELF 的 p_paddr，
//  避免 ld 对 LMA 的对齐行为导致物理布局漂移）。引导区 vaddr 在低地址，
//  物理地址即 vaddr 本身。
#define KERNEL_VMA_BASE 0xFFFF800000000000ULL

static UINT64 load_addr(UINT64 vaddr) {
    return vaddr >= KERNEL_VMA_BASE ? vaddr - KERNEL_VMA_BASE : vaddr;
}

static UINT64 load_elf(VOID* elfData, UINT64 elfSize) {
    Elf64_Ehdr* ehdr = elfData;
    Elf64_Phdr* phdrs = (Elf64_Phdr*)((UINT8*)elfData + ehdr->e_phoff);

    UINT64 minAddr = ~0ULL, maxAddr = 0;
    for (UINTN i = 0; i < ehdr->e_phnum; i++) {
        if (phdrs[i].p_type != PT_LOAD) continue;
        if (phdrs[i].p_memsz == 0) continue;
        UINT64 segStart = load_addr(phdrs[i].p_vaddr);
        UINT64 segEnd   = segStart + phdrs[i].p_memsz;
        if (segStart < minAddr) minAddr = segStart;
        if (segEnd   > maxAddr) maxAddr = segEnd;
    }
    if (maxAddr <= minAddr) return ehdr->e_entry;

    UINT64 totalPages = (maxAddr - minAddr + 4095) / 4096;
    EFI_PHYSICAL_ADDRESS base = minAddr & ~0xFFFULL;
    BS->AllocatePages(AllocateAddress, EfiLoaderData, totalPages, &base);

    for (UINTN i = 0; i < ehdr->e_phnum; i++) {
        if (phdrs[i].p_type != PT_LOAD) continue;
        if (phdrs[i].p_memsz == 0) continue;
        UINT64 dest = load_addr(phdrs[i].p_vaddr);
        if (phdrs[i].p_filesz > 0) {
            memcpy((VOID*)dest, (UINT8*)elfData + phdrs[i].p_offset, phdrs[i].p_filesz);
        }
        if (phdrs[i].p_memsz > phdrs[i].p_filesz) {
            memset((VOID*)(dest + phdrs[i].p_filesz), 0, phdrs[i].p_memsz - phdrs[i].p_filesz);
        }
    }

    return ehdr->e_entry;
}

static UINT16 get_cs_selector(void);

static void __attribute__((noreturn)) exit_boot_services(UINT64 kernelEntry) {
    UINTN mmapSize = 0, mmapKey = 0, descSize = 0;
    UINT32 descVer = 0;

    BS->GetMemoryMap(&mmapSize, 0, &mmapKey, &descSize, &descVer);
    mmapSize += 4096;

    EFI_MEMORY_DESCRIPTOR* mmap;
    BS->AllocatePool(EfiBootServicesData, mmapSize, (VOID**)&mmap);

    while (1) {
        BS->GetMemoryMap(&mmapSize, mmap, &mmapKey, &descSize, &descVer);
        EFI_STATUS s = BS->ExitBootServices(IMAGE_HANDLE, mmapKey);
        if (!EFI_ERROR(s)) break;
    }

    bootInfo.memoryMap              = (UINT64)mmap;
    bootInfo.memoryMapSize          = mmapSize;
    bootInfo.memoryMapDescriptorSize = descSize;
    bootInfo.cs_selector = get_cs_selector();
    KernEntry kern = (KernEntry)kernelEntry;
    kern(&bootInfo);
    while (1) __asm__("hlt");
}

UINT16 get_cs_selector(void) {
    UINT16 cs;
    asm volatile("mov %%cs, %0" : "=r"(cs));
    return cs;
}

EFI_STATUS EFIAPI efi_main(EFI_HANDLE handle, EFI_SYSTEM_TABLE* systemTable) {
    IMAGE_HANDLE = handle;
    ST = systemTable;
    BS = ST->BootServices;

    ST->ConOut->OutputString(ST->ConOut, L"Partix Boot\r\n");

    if (EFI_ERROR(setup_gop())) {
        ST->ConOut->OutputString(ST->ConOut, L"GOP failed\r\n");
        while (1);
    }

    VOID* elfData; UINT64 elfSize;
    if (EFI_ERROR(load_file(L"KERNEL.ELF", &elfData, &elfSize))) {
        ST->ConOut->OutputString(ST->ConOut, L"KERNEL.ELF not found\r\n");
        while (1);
    }

    UINT64 entry = load_elf(elfData, elfSize);
    BS->FreePool(elfData);

    exit_boot_services(entry);
}
