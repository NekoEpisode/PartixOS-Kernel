from pathlib import Path, PurePath
from typing import List, Optional

from ..abstract.builder import Builder
from ..abstract.target import Target
from ..abstract.registry import BuilderRegistry
from ..common.runner import run_cmd, rel
from ..components.hal import HalSources, PROJECT_ROOT as COMPONENT_ROOT


HAL_CFLAGS_BASE = [
    "--target=x86_64-unknown-none",
    "-ffreestanding", "-nostdlib", "-mno-red-zone",
    "-mgeneral-regs-only", "-funwind-tables",
]


def _hal_cflags(debug: bool) -> list[str]:
    return HAL_CFLAGS_BASE + (["-O0", "-g"] if debug else ["-O2"])


class X86_64HalBuilder(Builder):
    def __init__(
        self,
        target: Target,
        build_root: Optional[PurePath] = None,
        sources: HalSources | None = None,
    ):
        super().__init__(target, build_root)
        self._sources = sources or HalSources()
        self._prj_root = Path(COMPONENT_ROOT)
        self._debug = False

    def source_files(self) -> List[PurePath]:
        return []

    def include_dirs(self) -> List[PurePath]:
        return []

    def defines(self) -> List[str]:
        return []

    def output_name(self) -> str:
        return "hal"

    def build(self) -> List[Path]:
        arch = self.target.arch()
        tc = self.target.toolchain()
        build_root = Path(self.build_root)
        obj_dir = build_root / self.target.build_dir() / "obj" / "hal"
        obj_dir.mkdir(parents=True, exist_ok=True)

        cflags = _hal_cflags(self._debug)

        all_objs: List[Path] = []

        for src_rel in self._sources.c_sources(arch):
            src_abs = self._prj_root / src_rel
            obj = obj_dir / src_rel.with_suffix(".o").name

            if "partic_eh_dwarf" in str(src_rel):
                cc = "gcc"
                eh_flags = ["-ffreestanding", "-nostdlib", "-mno-red-zone"]
                eh_flags += ["-O0", "-g"] if self._debug else ["-O2"]
                cmd = [cc, *eh_flags, "-c", str(src_abs), "-o", str(obj)]
            else:
                cmd = [tc.compiler(), *cflags, "-c", str(src_abs), "-o", str(obj)]

            if self._needs_rebuild(src_abs, obj):
                run_cmd(cmd, desc=f"compile {rel(src_rel)}")
            all_objs.append(obj)

        for src_rel in self._sources.asm_sources(arch):
            src_abs = self._prj_root / src_rel
            obj = obj_dir / src_rel.with_suffix(".o").name
            cmd = [tc.compiler(), "--target=x86_64-unknown-none", "-ffreestanding", "-nostdlib", "-c", str(src_abs), "-o", str(obj)]
            if self._needs_rebuild(src_abs, obj):
                run_cmd(cmd, desc=f"asm {rel(src_rel)}")
            all_objs.append(obj)

        print(f"[OK] hal ({len(all_objs)} object(s))")
        return all_objs

    @staticmethod
    def _needs_rebuild(src: Path, obj: Path) -> bool:
        if not obj.exists():
            return True
        return src.stat().st_mtime > obj.stat().st_mtime


BuilderRegistry.register("x86_64", "hal", X86_64HalBuilder)
