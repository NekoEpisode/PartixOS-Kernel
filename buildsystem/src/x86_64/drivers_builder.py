from pathlib import Path, PurePath
from typing import List, Optional

from ..abstract.builder import Builder
from ..abstract.target import Target
from ..abstract.registry import BuilderRegistry
from ..common.runner import run_cmd, rel
from ..components.drivers import DriversSources, PROJECT_ROOT as COMPONENT_ROOT
from ..components.hal import PROJECT_ROOT as HAL_ROOT


DRIVERS_CFLAGS = [
    "--target=x86_64-unknown-none",
    "-ffreestanding", "-nostdlib", "-mno-red-zone",
    "-mgeneral-regs-only",
]


class X86_64DriversBuilder(Builder):
    def __init__(
        self,
        target: Target,
        build_root: Optional[PurePath] = None,
        sources: DriversSources | None = None,
    ):
        super().__init__(target, build_root)
        self._sources = sources or DriversSources()
        self._prj_root = Path(COMPONENT_ROOT)
        self._debug = False

    def source_files(self) -> List[PurePath]:
        return []

    def include_dirs(self) -> List[PurePath]:
        return []

    def defines(self) -> List[str]:
        return []

    def output_name(self) -> str:
        return "drivers"

    def build(self) -> List[Path]:
        arch = self.target.arch()
        tc = self.target.toolchain()
        build_root = Path(self.build_root)
        obj_dir = build_root / self.target.build_dir() / "obj" / "drivers"
        obj_dir.mkdir(parents=True, exist_ok=True)

        cflags = DRIVERS_CFLAGS + (["-O0", "-g"] if self._debug else ["-O2"])
        cflags += [f"-I{HAL_ROOT}/Kernel/hal/src/main/c/x86_64/include"]

        all_objs: List[Path] = []

        for src_rel in self._sources.c_sources(arch):
            src_abs = self._prj_root / src_rel
            obj = obj_dir / src_rel.with_suffix(".o").name
            cmd = [tc.compiler(), *cflags, "-c", str(src_abs), "-o", str(obj)]
            if self._needs_rebuild(src_abs, obj):
                run_cmd(cmd, desc=f"compile {rel(src_rel)}")
            all_objs.append(obj)

        if all_objs:
            print(f"[OK] drivers ({len(all_objs)} object(s))")
        return all_objs

    @staticmethod
    def _needs_rebuild(src: Path, obj: Path) -> bool:
        if not obj.exists():
            return True
        return src.stat().st_mtime > obj.stat().st_mtime


BuilderRegistry.register("x86_64", "drivers", X86_64DriversBuilder)
