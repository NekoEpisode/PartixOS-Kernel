from dataclasses import dataclass
from pathlib import Path, PurePath
from typing import List

from ..abstract.component import Component, ComponentRegistry

PROJECT_ROOT = Path(__file__).resolve().parent.parent.parent.parent.parent


BOOTLOADER = Component(
    name="bootloader",
    desc="UEFI bootloader (PE/COFF .efi)",
    depends=[],
)
ComponentRegistry.register(BOOTLOADER)


@dataclass(frozen=True)
class BootloaderSources:
    base: PurePath = PurePath("Kernel/bootloader/src/main")

    @property
    def c_dir(self) -> PurePath:
        return self.base / "c"

    @property
    def asm_dir(self) -> PurePath:
        return self.base / "asm"

    @property
    def include_dir(self) -> PurePath:
        return self.c_dir / "include"

    _ARCH_INCLUDE_MAP = {
        "x86_64": "X64",
        "aarch64": "AArch64",
        "arm": "Arm",
        "riscv64": "RiscV64",
        "loongarch64": "LoongArch64",
    }

    def sources(self, arch: str) -> List[PurePath]:
        abs_dir = Path(PROJECT_ROOT / self.c_dir / arch)
        return sorted(
            PurePath(p.relative_to(PROJECT_ROOT))
            for p in abs_dir.glob("*.c")
        )

    def includes(self, arch: str = "") -> List[PurePath]:
        dirs = [self.include_dir]
        if arch and arch in self._ARCH_INCLUDE_MAP:
            dirs.append(self.include_dir / self._ARCH_INCLUDE_MAP[arch])
        return dirs

    def defines(self) -> List[str]:
        return []
