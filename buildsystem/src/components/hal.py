from dataclasses import dataclass
from pathlib import Path, PurePath
from typing import List

from ..abstract.component import Component, ComponentRegistry

PROJECT_ROOT = Path(__file__).resolve().parent.parent.parent.parent.parent

HAL = Component(
    name="hal",
    desc="hardware abstraction layer (C runtime, asm entry, linker script)",
    depends=[],
)
ComponentRegistry.register(HAL)


@dataclass(frozen=True)
class HalSources:
    base: PurePath = PurePath("Kernel/hal/src/main")

    @property
    def c_dir(self) -> PurePath:
        return self.base / "c"

    @property
    def asm_dir(self) -> PurePath:
        return self.base / "asm"

    @property
    def linker_dir(self) -> PurePath:
        return self.base / "linker"

    def c_sources(self, arch: str) -> List[PurePath]:
        results = []
        for subdir in ["runtime", arch]:
            abs_dir = Path(PROJECT_ROOT / self.c_dir / subdir)
            if abs_dir.is_dir():
                results.extend(sorted(
                    PurePath(p.relative_to(PROJECT_ROOT))
                    for p in abs_dir.glob("*.c")
                ))
        return results

    def asm_sources(self, arch: str) -> List[PurePath]:
        abs_dir = Path(PROJECT_ROOT / self.asm_dir / arch)
        if not abs_dir.is_dir():
            return []
        return sorted(
            PurePath(p.relative_to(PROJECT_ROOT))
            for p in abs_dir.glob("*.s")
        )

    def linker_script(self, arch: str) -> PurePath:
        return self.linker_dir / arch / "linker.ld"

    def defines(self) -> List[str]:
        return []
