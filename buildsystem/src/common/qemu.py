import shutil
import subprocess
from dataclasses import dataclass
from pathlib import Path

from .runner import run_cmd, BuildError

PROJECT_ROOT = Path(__file__).resolve().parent.parent.parent.parent.parent

OVMF_CODE_PATHS = {
    "x86_64": [
        "/usr/share/OVMF/OVMF_CODE.fd",
        "/usr/share/OVMF/OVMF_CODE.secboot.fd",
        "/usr/share/ovmf/OVMF_CODE.fd",
        "/usr/share/edk2-ovmf/OVMF_CODE.fd",
        "/usr/share/qemu/OVMF_CODE.fd",
        "/usr/share/edk2/x64/OVMF_CODE.fd",
        "/usr/share/OVMF/OVMF_CODE_4M.fd",
    ],
    "aarch64": [
        "/usr/share/AAVMF/AAVMF_CODE.fd",
        "/usr/share/edk2-ovmf/QEMU_EFI-aarch64.fd",
        "/usr/share/qemu-efi-aarch64/QEMU_EFI.fd",
    ],
    "riscv64": [
        "/usr/share/qemu-efi-riscv64/RISCV_VIRT_CODE.fd",
        "/usr/share/edk2-ovmf/RISCV_VIRT_CODE.fd",
    ],
}

OVMF_VARS_PATHS = {
    "x86_64": [
        "/usr/share/OVMF/OVMF_VARS.fd",
        "/usr/share/ovmf/OVMF_VARS.fd",
        "/usr/share/edk2-ovmf/OVMF_VARS.fd",
        "/usr/share/qemu/OVMF_VARS.fd",
        "/usr/share/edk2/x64/OVMF_VARS.fd",
        "/usr/share/OVMF/OVMF_VARS_4M.fd",
    ],
    "aarch64": [
        "/usr/share/AAVMF/AAVMF_VARS.fd",
        "/usr/share/edk2-ovmf/QEMU_VARS-aarch64.fd",
    ],
    "riscv64": [
        "/usr/share/qemu-efi-riscv64/RISCV_VIRT_VARS.fd",
        "/usr/share/edk2-ovmf/RISCV_VIRT_VARS.fd",
    ],
}

# U-Boot 单镜像固件（-bios）候选路径，仅供 setup 时显式选择，不自动采用。
U_BOOT_PATHS = {
    "riscv64": [
        "/usr/lib/u-boot/qemu-riscv64_smode/u-boot.bin",
        "/usr/share/u-boot/u-boot.bin",
        "/usr/lib/riscv64-linux-gnu/u-boot.bin",
        "build/u-boot.bin",
    ],
}

QEMU_PER_ARCH = {
    "x86_64":   "qemu-system-x86_64",
    "aarch64":  "qemu-system-aarch64",
    "arm":      "qemu-system-arm",
    "riscv64":  "qemu-system-riscv64",
}


def _detect_qemu(arch: str) -> str:
    name = QEMU_PER_ARCH.get(arch, f"qemu-system-{arch}")
    return shutil.which(name) or name


@dataclass
class QemuRunConfig:
    qemu_binary: str
    memory: str
    ovmf_code: str
    ovmf_vars: str
    bios: str
    kernel: str
    extra_args: str


def _pick_path(label: str, search_paths: list[str]) -> str:
    found = [p for p in search_paths if Path(p).exists()]

    print(f"\n  {label}:")
    if found:
        for i, p in enumerate(found, 1):
            print(f"    [{i}] {p}")
        print(f"    [0] enter custom path")
        while True:
            try:
                choice = input("  > ").strip()
                if not choice:
                    return found[0]
                idx = int(choice)
                if idx == 0:
                    return input("  path: ").strip()
                if 1 <= idx <= len(found):
                    return found[idx - 1]
            except (ValueError, KeyboardInterrupt):
                pass
            print("  invalid")
    else:
        print(f"    (not found — install with: sudo apt install ovmf)")
        return input("  enter path manually (or blank to skip): ").strip()


def _interactive_qemu_setup(arch: str) -> QemuRunConfig:
    print(f"\n  === QEMU setup [{arch}] ===")

    qemu = _detect_qemu(arch)
    print(f"  qemu : {qemu}")
    override = input(f"  change? (enter=keep): ").strip()
    if override:
        qemu = override

    code = ""
    ovmf_vars = ""
    bios = ""

    if arch == "riscv64":
        print(f"\n  firmware:")
        print(f"    [1] EDK2 (UEFI, pflash 双镜像)  — FDT 依赖固件装 config table")
        print(f"    [2] U-Boot (随 EFI payload 传设备树)")
        while True:
            try:
                choice = input("  > ").strip()
                if choice == "2":
                    img = _pick_path("U-Boot image", U_BOOT_PATHS.get(arch, []))
                    if img:
                        print(f"  U-Boot image type:")
                        print(f"    [1] S-mode（需 OpenSBI 先行；QEMU 内置即可：-bios default + -kernel）")
                        print(f"    [2] 完整单镜像（内含固件/OpenSBI：直接 -bios）")
                        while True:
                            try:
                                t = input("  > ").strip()
                                if t == "1":
                                    kernel = img
                                    break
                                if t == "" or t == "2":
                                    bios = img
                                    break
                            except (ValueError, KeyboardInterrupt):
                                pass
                            print("  invalid")
                    break
                if choice == "" or choice == "1":
                    code = _pick_path("OVMF_CODE", OVMF_CODE_PATHS.get(arch, []))
                    ovmf_vars = _pick_path("OVMF_VARS", OVMF_VARS_PATHS.get(arch, []))
                    break
            except (ValueError, KeyboardInterrupt):
                pass
            print("  invalid")
    else:
        code = _pick_path("OVMF_CODE", OVMF_CODE_PATHS.get(arch, []))
        ovmf_vars = _pick_path("OVMF_VARS", OVMF_VARS_PATHS.get(arch, []))

    memory_options = ["128M", "256M", "512M", "1G", "2G"]
    print(f"\n  memory:")
    for i, m in enumerate(memory_options, 1):
        print(f"    [{i}] {m}")
    print(f"    [0] custom")
    memory = "256M"
    while True:
        try:
            choice = input("  > ").strip()
            idx = int(choice)
            if idx == 0:
                memory = input("  enter (e.g. 512M): ").strip()
                break
            if 1 <= idx <= len(memory_options):
                memory = memory_options[idx - 1]
                break
        except (ValueError, KeyboardInterrupt):
            pass
        print("  invalid")

    extra = input(f"\n  extra qemu args [-net none -serial stdio]: ").strip()
    if not extra:
        extra = "-net none -serial stdio"

    print(f"\n  Tip: 记得检查 firmwares 文件夹，如果你遇到了固件功能缺失或其他问题，那里的固件或许会有用")

    return QemuRunConfig(
        qemu_binary=qemu,
        memory=memory,
        ovmf_code=code,
        ovmf_vars=ovmf_vars,
        bios=bios,
        kernel=kernel,
        extra_args=extra,
    )


def create_disk_image(efi_path: Path, disk_path: Path, size_mb: int = 64,
                      extra_files: dict = None, efi_name: str = "BOOTX64.EFI") -> Path:
    efi_path = Path(efi_path).resolve()
    disk_path = Path(disk_path).resolve()

    if not shutil.which("dd") or not shutil.which("mkfs.fat"):
        raise BuildError("need 'dd' and 'mkfs.fat' (dosfstools) to create disk image")

    run_cmd(
        ["dd", "if=/dev/zero", f"of={disk_path}", "bs=1M", f"count={size_mb}"],
        desc=f"create {size_mb}M disk image",
    )

    # GPT + 单个 ESP 分区（FAT32，起始 2048 扇区 = 1MiB）。
    # 分区表是 U-Boot bootflow / EDK2 自动发现启动项的必需品（superfloppy 只能手动 load）。
    part_offset = 2048  # 扇区
    if shutil.which("sgdisk"):
        run_cmd(
            ["sgdisk", "-n", f"1:{part_offset}:0", "-t", "1:ef00", "-c", "1:ESP", str(disk_path)],
            desc="create GPT with ESP partition",
        )
    else:
        raise BuildError("need 'sgdisk' (gdisk) to create the ESP partition")

    run_cmd(
        ["mkfs.fat", "-F", "32", f"--offset={part_offset}", str(disk_path)],
        desc="format FAT32 ESP",
    )

    if shutil.which("mcopy"):
        dev = f"{disk_path}@@{part_offset}s"
        run_cmd(
            ["mmd", "-i", dev, "::/EFI"],
            desc="create EFI dir",
        )
        run_cmd(
            ["mmd", "-i", dev, "::/EFI/BOOT"],
            desc="create EFI/BOOT dir",
        )
        run_cmd(
            ["mcopy", "-i", dev, str(efi_path), f"::/EFI/BOOT/{efi_name}"],
            desc=f"copy {efi_name}",
        )

        if extra_files:
            for dest_name, src_path in extra_files.items():
                run_cmd(
                    ["mcopy", "-i", dev, str(src_path), f"::/{dest_name}"],
                    desc=f"copy {dest_name}",
                )
    else:
        print("  mtools not found, trying mount...")
        mnt = Path("/tmp/partix_mnt")
        mnt.mkdir(exist_ok=True)
        try:
            run_cmd(["sudo", "mount", "-o", f"loop,offset={part_offset * 512}", str(disk_path), str(mnt)],
                    desc="mount disk")
            efi_dir = mnt / "EFI" / "BOOT"
            efi_dir.mkdir(parents=True, exist_ok=True)
            shutil.copy2(efi_path, efi_dir / "BOOTX64.EFI")
            print(f"  copied BOOTX64.EFI")
        finally:
            subprocess.run(["sudo", "umount", str(mnt)], capture_output=True)

    return disk_path


def launch_qemu(run: QemuRunConfig, disk: Path, arch: str = "x86_64", extra: str = "") -> None:
    ovmf_vars_local = Path("build") / "ovmf_vars.fd"

    if run.ovmf_vars and Path(run.ovmf_vars).exists():
        shutil.copy2(run.ovmf_vars, ovmf_vars_local)

    is_riscv = arch == "riscv64"

    # 用户显式指定的固件：配置 bios/kernel 字段，或 --qemu-args 里传 -bios
    bios_in_args = "-bios" in (run.extra_args + " " + extra).split()
    bios_path = run.bios if (run.bios and Path(run.bios).exists()) else ""

    if is_riscv and (bios_path or run.kernel or bios_in_args):
        # U-Boot 等非 pflash 固件：
        #   bios   非空 → -bios <bios>（完整单镜像固件）
        #   kernel 非空 → -kernel <kernel>（S-mode U-Boot；bios 为空时用 QEMU
        #                内置 OpenSBI：-bios default）
        # （-bios 若来自 --qemu-args，由下面的 user_args 合并逻辑追加）
        fw = []
        if bios_path:
            fw += ["-bios", bios_path]
        elif run.kernel:
            fw += ["-bios", "default"]
        if run.kernel:
            fw += ["-kernel", run.kernel]
        cmd = [
            run.qemu_binary, "-M", "virt",
            *fw,
            "-m", run.memory,
            "-drive", f"file={disk},format=raw,if=none,id=drive0",
        ]
    elif is_riscv:
        vars_fd = Path("build") / "partix_VARS.fd"
        if run.ovmf_vars and Path(run.ovmf_vars).exists() and not vars_fd.exists():
            shutil.copy2(run.ovmf_vars, vars_fd)
        if not vars_fd.exists():
            run_cmd(["dd", "if=/dev/zero", f"of={vars_fd}", "bs=1M", "count=32"],
                    desc="create RISC-V VARS")
        if vars_fd.stat().st_size < 32 * 1024 * 1024:
            run_cmd(["truncate", "-s", "32M", str(vars_fd)],
                    desc="resize RISC-V VARS to 32M")

        cmd = [
            run.qemu_binary, "-M", "virt",
            "-bios", "default",
            "-m", run.memory,
            "-drive", f"if=pflash,format=raw,unit=0,file={run.ovmf_code},readonly=on",
            "-drive", f"if=pflash,format=raw,unit=1,file={vars_fd}",
            "-drive", f"file={disk},format=raw,if=none,id=drive0",
        ]
    else:
        cmd = [
            run.qemu_binary,
            "-m", run.memory,
        ]
        if run.ovmf_code:
            cmd += [
                "-drive", f"if=pflash,format=raw,readonly=on,file={run.ovmf_code}",
            ]
        if ovmf_vars_local.exists():
            cmd += [
                "-drive", f"if=pflash,format=raw,file={ovmf_vars_local}",
            ]
        cmd += ["-drive", f"file={disk},format=raw,if=none,id=drive0"]

    user_args = run.extra_args.split() + extra.split()
    i = 0
    while i < len(user_args):
        arg = user_args[i]
        if arg == "-nographic":
            cmd = [a for j, a in enumerate(cmd) if a not in ("-serial",)
                   and (j == 0 or cmd[j-1] != "-serial")]

        if arg.startswith("-"):
            if arg in cmd:
                idx = cmd.index(arg)
                if arg in ("-device", "-drive", "-netdev"):
                    # 可重复参数：追加而不是替换第一个实例
                    cmd.append(arg)
                    if i + 1 < len(user_args) and not user_args[i + 1].startswith("-"):
                        cmd.append(user_args[i + 1])
                        i += 2
                    else:
                        i += 1
                elif i + 1 < len(user_args) and not user_args[i + 1].startswith("-"):
                    if idx + 1 < len(cmd) and not cmd[idx + 1].startswith("-"):
                        cmd[idx + 1] = user_args[i + 1]
                    else:
                        cmd.insert(idx + 1, user_args[i + 1])
                    i += 2
                else:
                    i += 1
            else:
                cmd.append(arg)
                if i + 1 < len(user_args) and not user_args[i + 1].startswith("-"):
                    cmd.append(user_args[i + 1])
                    i += 2
                else:
                    i += 1
        else:
            i += 1

    print(f"\n  [QEMU] {' '.join(cmd)}")
    subprocess.run(cmd)
