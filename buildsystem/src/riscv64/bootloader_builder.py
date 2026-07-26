from pathlib import Path, PurePath
from typing import List, Optional

from ..abstract.builder import Builder
from ..abstract.target import Target
from ..abstract.registry import BuilderRegistry
from ..common.runner import run_cmd, BuildError, rel
from ..components.bootloader import BootloaderSources, PROJECT_ROOT as COMPONENT_ROOT


PROJECT_ROOT = Path(COMPONENT_ROOT)
GENFW = PROJECT_ROOT / "Kernel/buildsystem/tools/GenFw"


class RV64BootloaderBuilder(Builder):
    def __init__(self, target: Target, build_root: Optional[PurePath] = None,
                 sources: BootloaderSources | None = None):
        super().__init__(target, build_root)
        self._sources = sources or BootloaderSources()
        self._prj_root = PROJECT_ROOT
        self._debug = False

    def source_files(self) -> List[PurePath]:
        return self._sources.sources(self.target.arch())

    def include_dirs(self) -> List[PurePath]:
        return self._sources.includes(self.target.arch())

    def defines(self) -> List[str]:
        return self._sources.defines()

    def output_name(self) -> str:
        return "bootloader"

    def build(self) -> PurePath:
        arch = self.target.arch()
        tc = self.target.toolchain()
        build_root = Path(self.build_root)
        obj_dir = build_root / self.target.build_dir() / "obj"
        out_dir = build_root / self.target.build_dir()
        obj_dir.mkdir(parents=True, exist_ok=True)

        sources = self.source_files()
        if not sources:
            raise BuildError(f"no bootloader sources for arch {arch}")

        includes = self.include_dirs()
        defines = self.defines()

        cflags = ["--target=riscv64-unknown-elf", "-ffreestanding", "-nostdlib",
                   "-march=rv64gc", "-mabi=lp64d", "-fshort-wchar",
                   "-fPIC", "-O2"]
        if self._debug:
            cflags += ["-O0", "-g"]

        objs = []
        for src_rel in sources:
            src_abs = self._prj_root / src_rel
            obj = obj_dir / src_rel.with_suffix(".o").name
            objs.append(obj)
            if self._needs_rebuild(src_abs, obj):
                cmd = [tc.compiler(), *cflags]
                for inc in includes:
                    cmd.append(f"-I{self._prj_root / inc}")
                for d in defines:
                    cmd.append(f"-D{d}")
                cmd += ["-c", str(src_abs), "-o", str(obj)]
                run_cmd(cmd, desc=f"compile {rel(src_rel)}")

        elf = out_dir / f"{self.output_name()}.elf"
        ld_cmd = [tc.compiler(), "--target=riscv64-unknown-elf",
                  "-nostdlib", "-Wl,-e,efi_main", "-Wl,--gc-sections",
                  "-Wl,--emit-relocs",
                  "-o", str(elf)] + [str(o) for o in objs]
        run_cmd(ld_cmd, desc=f"link -> {rel(elf)}")

        efi = out_dir / f"{self.output_name()}.efi"
        if not GENFW.exists():
            raise BuildError(f"GenFw not found at {GENFW}")
        run_cmd([str(GENFW), "-e", "UEFI_APPLICATION", "-o", str(efi), str(elf)],
                desc=f"GenFw -> {rel(efi)}")

        print(f"[OK] {rel(efi)}")
        return efi

    @staticmethod
    def _needs_rebuild(src: Path, obj: Path) -> bool:
        if not obj.exists():
            return True
        return src.stat().st_mtime > obj.stat().st_mtime


BuilderRegistry.register("riscv64", "bootloader", RV64BootloaderBuilder)
