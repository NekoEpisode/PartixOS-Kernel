import json
import shutil
from dataclasses import dataclass, field
from pathlib import Path
from typing import Dict, List

PROJECT_ROOT = Path(__file__).resolve().parent.parent.parent.parent
CONFIG_PATH = PROJECT_ROOT / "buildsettings.json"

CLANG_CANDIDATES = [
    "clang-19", "clang-18", "clang-17", "clang-16", "clang-15",
    "clang",
]
LLD_LINK_CANDIDATES = [
    "lld-link-19", "lld-link-18", "lld-link-17", "lld-link-16",
    "lld-link", "ld.lld-19", "ld.lld-18", "ld.lld-17",
    "ld.lld",
]

QEMU_PER_ARCH = {
    "x86_64":   "qemu-system-x86_64",
    "aarch64":  "qemu-system-aarch64",
    "arm":      "qemu-system-arm",
    "riscv64":  "qemu-system-riscv64",
}

OVMF_CODE_PATHS = {
    "x86_64": [
        "/usr/share/OVMF/OVMF_CODE.fd",
        "/usr/share/ovmf/OVMF_CODE.fd",
        "/usr/share/edk2-ovmf/OVMF_CODE.fd",
        "/usr/share/qemu/OVMF_CODE.fd",
    ],
    "aarch64": [
        "/usr/share/AAVMF/AAVMF_CODE.fd",
        "/usr/share/edk2-ovmf/QEMU_EFI-aarch64.fd",
        "/usr/share/qemu-efi-aarch64/QEMU_EFI.fd",
    ],
}

OVMF_VARS_PATHS = {
    "x86_64": [
        "/usr/share/OVMF/OVMF_VARS.fd",
        "/usr/share/ovmf/OVMF_VARS.fd",
        "/usr/share/edk2-ovmf/OVMF_VARS.fd",
        "/usr/share/qemu/OVMF_VARS.fd",
    ],
    "aarch64": [
        "/usr/share/AAVMF/AAVMF_VARS.fd",
        "/usr/share/edk2-ovmf/QEMU_VARS-aarch64.fd",
    ],
}


@dataclass
class ToolchainConfig:
    clang: str = "clang-18"
    linker: str = "lld-link"
    linker_mode: str = "lld-link"


@dataclass
class ParticConfig:
    java: str = "java"
    jar: str = ""
    stdlib: str = ""

    @property
    def ready(self) -> bool:
        return bool(self.jar and Path(self.jar).exists())


@dataclass
class QemuConfig:
    qemu: str = ""
    memory: str = "256M"
    ovmf_code: str = ""
    ovmf_vars: str = ""
    extra_args: str = "-net none -serial stdio"

    def qemu_binary(self, arch: str) -> str:
        if self.qemu and Path(self.qemu).exists():
            return self.qemu
        name = QEMU_PER_ARCH.get(arch, f"qemu-system-{arch}")
        found = shutil.which(name)
        return found or name

    def resolve_ovmf(self, arch: str) -> None:
        if not self.ovmf_code or not Path(self.ovmf_code).exists():
            for p in OVMF_CODE_PATHS.get(arch, []):
                if Path(p).exists():
                    self.ovmf_code = p
                    break
        if not self.ovmf_vars or not Path(self.ovmf_vars).exists():
            for p in OVMF_VARS_PATHS.get(arch, []):
                if Path(p).exists():
                    self.ovmf_vars = p
                    break


@dataclass
class BuildSettings:
    toolchains: Dict[str, ToolchainConfig] = field(default_factory=dict)
    qemu: Dict[str, QemuConfig] = field(default_factory=dict)
    partic: ParticConfig = field(default_factory=ParticConfig)

    def qemu_for(self, arch: str) -> QemuConfig:
        qc = self.qemu.get(arch, QemuConfig())
        qc.resolve_ovmf(arch)
        return qc


def _find_executables(candidates: List[str]) -> List[str]:
    found = []
    for name in candidates:
        path = shutil.which(name)
        if path:
            found.append(name)
    return found


def _ask_choice(prompt: str, options: List[str], allow_custom: bool = True) -> str:
    print(f"\n{prompt}")
    for i, opt in enumerate(options, 1):
        print(f"  [{i}] {opt}")
    if allow_custom:
        print(f"  [0] enter custom path")
    while True:
        try:
            choice = input("> ").strip()
            idx = int(choice)
            if allow_custom and idx == 0:
                return input("  path: ").strip()
            if 1 <= idx <= len(options):
                return options[idx - 1]
        except (ValueError, KeyboardInterrupt):
            pass
        print("  invalid choice, try again")


def _auto_detect_toolchain() -> ToolchainConfig:
    print("\n  === Partix build toolchain setup ===")

    found_clang = _find_executables(CLANG_CANDIDATES)
    found_lld = _find_executables(LLD_LINK_CANDIDATES)

    if not found_clang:
        print("\n  no clang found in PATH. install clang-18+ first.")
        return ToolchainConfig()

    clang = _ask_choice("select clang compiler:", found_clang)
    linker = _ask_choice("select linker:", (found_lld or ["(none)"]), allow_custom=bool(found_lld))

    mode = "lld-link"
    if linker.startswith("ld.lld"):
        mode = "clang"

    return ToolchainConfig(clang=clang, linker=linker, linker_mode=mode)


def load_settings() -> BuildSettings:
    if CONFIG_PATH.exists():
        try:
            data = json.loads(CONFIG_PATH.read_text())
            settings = BuildSettings()

            for arch, tc in data.get("toolchains", {}).items():
                settings.toolchains[arch] = ToolchainConfig(
                    clang=tc.get("clang", "clang-18"),
                    linker=tc.get("linker", "lld-link"),
                    linker_mode=tc.get("linker_mode", "lld-link"),
                )

            for arch, qd in data.get("qemu", {}).items():
                settings.qemu[arch] = QemuConfig(
                    qemu=qd.get("qemu", ""),
                    memory=qd.get("memory", "256M"),
                    ovmf_code=qd.get("ovmf_code", ""),
                    ovmf_vars=qd.get("ovmf_vars", ""),
                    extra_args=qd.get("extra_args", "-net none -serial stdio"),
                )

            pd = data.get("partic", {})
            settings.partic = ParticConfig(
                java=pd.get("java", "java"),
                jar=pd.get("jar", ""),
                stdlib=pd.get("stdlib", ""),
            )
            return settings
        except (json.JSONDecodeError, KeyError):
            pass

    settings = BuildSettings()
    tc = _auto_detect_toolchain()
    settings.toolchains["x86_64"] = tc
    _save(settings)
    return settings


def save_settings(settings: BuildSettings) -> None:
    _save(settings)


def _save(settings: BuildSettings) -> None:
    data = {
        "toolchains": {},
        "qemu": {},
        "partic": {
            "java": settings.partic.java,
            "jar": settings.partic.jar,
            "stdlib": settings.partic.stdlib,
        },
    }
    for arch, tc in settings.toolchains.items():
        data["toolchains"][arch] = {
            "clang": tc.clang,
            "linker": tc.linker,
            "linker_mode": tc.linker_mode,
        }
    for arch, qc in settings.qemu.items():
        data["qemu"][arch] = {
            "qemu": qc.qemu,
            "memory": qc.memory,
            "ovmf_code": qc.ovmf_code,
            "ovmf_vars": qc.ovmf_vars,
            "extra_args": qc.extra_args,
        }
    CONFIG_PATH.write_text(json.dumps(data, indent=2))
