from dataclasses import dataclass
from pathlib import Path, PurePath
from typing import List

from ..abstract.component import Component, ComponentRegistry

PROJECT_ROOT = Path(__file__).resolve().parent.parent.parent.parent.parent

DRIVERS = Component(
    name="drivers",
    desc="device drivers (virtio, etc.)",
    depends=["hal"],
)
ComponentRegistry.register(DRIVERS)


@dataclass(frozen=True)
class DriversSources:
    base: PurePath = PurePath("Kernel/drivers/src/main")

    @property
    def c_dir(self) -> PurePath:
        return self.base / "c"

    def c_sources(self, arch: str) -> List[PurePath]:
        results = []
        for subdir in [arch, "virtio"]:
            abs_dir = Path(PROJECT_ROOT / self.c_dir / subdir)
            if abs_dir.is_dir():
                results.extend(sorted(
                    PurePath(p.relative_to(PROJECT_ROOT))
                    for p in abs_dir.rglob("*.c")
                ))
        return results
