from dataclasses import dataclass
from pathlib import Path, PurePath
from typing import List

from ..abstract.component import Component, ComponentRegistry

PROJECT_ROOT = Path(__file__).resolve().parent.parent.parent.parent.parent

KERNEL = Component(
    name="kernel",
    desc="Partix kernel (Partic -> ELF)",
    depends=["hal", "drivers", "user"],
)
ComponentRegistry.register(KERNEL)


@dataclass(frozen=True)
class KernelSources:
    base: PurePath = PurePath("Kernel/kernel/src/main")

    @property
    def partic_dir(self) -> PurePath:
        return self.base / "partic"

    def partic_roots(self) -> List[PurePath]:
        abs_dir = Path(PROJECT_ROOT / self.partic_dir)
        return [self.partic_dir] if abs_dir.is_dir() else []

    def defines(self) -> List[str]:
        return []
