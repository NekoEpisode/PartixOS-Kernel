from dataclasses import dataclass
from pathlib import Path, PurePath
from typing import List

from ..abstract.component import Component, ComponentRegistry

PROJECT_ROOT = Path(__file__).resolve().parent.parent.parent.parent.parent

USER = Component(
    name="user",
    desc="embedded user programs (freestanding ELF blobs)",
    depends=[],
)
ComponentRegistry.register(USER)


@dataclass(frozen=True)
class UserSources:
    base: PurePath = PurePath("Kernel/user")

    def asm_file(self, arch: str) -> PurePath:
        name = "user_test_riscv.S" if arch == "riscv64" else "user_test_x86.S"
        return self.base / name

    def linker_script(self, arch: str) -> PurePath:
        return self.base / f"linker_{arch}.ld"
