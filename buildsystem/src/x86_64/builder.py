from pathlib import Path, PurePath
from typing import List, Optional

from ..abstract.builder import Builder
from ..abstract.target import Target
from ..abstract.registry import BuilderRegistry
from ..common.runner import run_cmd, BuildError, rel
from ..components.bootloader import BootloaderSources, PROJECT_ROOT as COMPONENT_ROOT


class X86_64BootloaderBuilder(Builder):
    def __init__(
        self,
        target: Target,
        build_root: Optional[PurePath] = None,
        sources: BootloaderSources | None = None,
    ):
        super().__init__(target, build_root)
        self._sources = sources or BootloaderSources()
        self._prj_root = Path(COMPONENT_ROOT)

    def source_files(self) -> list[PurePath]:
        return self._sources.sources(self.target.arch())

    def include_dirs(self) -> list[PurePath]:
        return self._sources.includes(self.target.arch())

    def defines(self) -> list[str]:
        return self._sources.defines()

    def output_name(self) -> str:
        return "bootloader"

    def build(self) -> PurePath:
        arch = self.target.arch()
        build_root = Path(self.build_root)
        obj_dir = build_root / self.target.build_dir() / "obj"
        out_dir = build_root / self.target.build_dir()
        obj_dir.mkdir(parents=True, exist_ok=True)

        sources = self.source_files()
        if not sources:
            raise BuildError(f"no source files found for arch {arch}")

        includes = self.include_dirs()
        defines = self.defines()
        objs: List[Path] = []

        print(f"[BUILD] {arch} bootloader ({len(sources)} source(s))")

        for src_rel in sources:
            src_abs = self._prj_root / src_rel
            obj = obj_dir / src_rel.with_suffix(self.target.obj_ext()).name
            objs.append(obj)
            if self._needs_rebuild(src_abs, obj):
                cmd = self.target.compile_cmd(src_abs, obj, self._resolve_includes(includes), defines)
                run_cmd(cmd, desc=f"compile {rel(src_rel)}")

        output = out_dir / f"{self.output_name()}{self.target.out_ext()}"
        if self._needs_rebuild_any(objs, output):
            cmd = self.target.link_cmd(objs, output)
            run_cmd(cmd, desc=f"link -> {rel(output)}")

        print(f"[OK] {rel(output)}")
        return output

    def _resolve_includes(self, includes: List[PurePath]) -> List[PurePath]:
        return [self._prj_root / inc for inc in includes]

    @staticmethod
    def _needs_rebuild(src: Path, obj: Path) -> bool:
        if not obj.exists():
            return True
        return src.stat().st_mtime > obj.stat().st_mtime

    @staticmethod
    def _needs_rebuild_any(deps: List[Path], out: Path) -> bool:
        if not out.exists():
            return True
        out_mtime = out.stat().st_mtime
        return any(d.stat().st_mtime > out_mtime for d in deps)


BuilderRegistry.register("x86_64", "bootloader", X86_64BootloaderBuilder)
