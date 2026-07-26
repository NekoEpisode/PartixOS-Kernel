import subprocess
from pathlib import Path, PurePath
from typing import Dict, List, Optional

from ..abstract.builder import Builder
from ..abstract.target import Target
from ..abstract.registry import BuilderRegistry
from ..abstract.partic import compile_partic
from ..common.runner import run_cmd, rel
from ..components.hal import HalSources
from ..components.kernel import KernelSources, PROJECT_ROOT as COMPONENT_ROOT


KERNEL_LDFLAGS = [
    "--eh-frame-hdr",
    "-nostdlib",
]


class X86_64KernelBuilder(Builder):
    def __init__(
        self,
        target: Target,
        build_root: Optional[PurePath] = None,
        sources: KernelSources | None = None,
    ):
        super().__init__(target, build_root)
        self._sources = sources or KernelSources()
        self._prj_root = Path(COMPONENT_ROOT)
        self._debug = False

    def source_files(self) -> List[PurePath]:
        return []

    def include_dirs(self) -> List[PurePath]:
        return []

    def defines(self) -> List[str]:
        return []

    def output_name(self) -> str:
        return "kernel"

    def build(self, dep_objects: Dict[str, List[Path]] | None = None) -> Path:
        arch = self.target.arch()
        tc = self.target.toolchain()
        build_root = Path(self.build_root)
        work_dir = build_root / self.target.build_dir() / "kernel"
        obj_dir  = work_dir / "obj"
        obj_dir.mkdir(parents=True, exist_ok=True)

        hal_sources = HalSources()
        linker_script = self._prj_root / hal_sources.linker_script(arch)

        all_objs: List[Path] = []
        if dep_objects:
            for objs in dep_objects.values():
                all_objs.extend(objs)

        print(f"[BUILD] {arch} kernel")

        partic_roots = self._sources.partic_roots()
        if partic_roots:
            cflags = [
                "--target=x86_64-unknown-none",
                "-ffreestanding", "-nostdlib",
                "-O2" if not self._debug else "-O0",
            ]
            if self._debug:
                cflags.append("-g")

            partic_ll = compile_partic(
                sources=[self._prj_root / r for r in partic_roots],
                out_dir=work_dir,
                main_class="BspKrMain",
                allocator="KrAlloc",
                target="freestanding",
                debug=self._debug,
            )
            partic_o = obj_dir / "partic.o"
            cmd = [tc.compiler(), *cflags, "-c", str(partic_ll), "-o", str(partic_o)]
            run_cmd(cmd, desc="partic.ll -> partic.o")
            all_objs.append(partic_o)

        libunwind = self._prj_root / "Kernel/lib" / arch / "unwind" / "libunwind.a"
        if libunwind.exists():
            all_objs.append(libunwind)

        crt = _find_compiler_rt(self._prj_root, arch, tc.compiler())
        if crt:
            all_objs.append(crt)

        output = work_dir / f"{self.output_name()}.elf"
        ld_cmd = [
            tc.linker(), *KERNEL_LDFLAGS,
            "-T", str(linker_script),
            "-o", str(output),
        ]
        for obj in all_objs:
            ld_cmd.append(str(obj))

        if self._needs_rebuild_any([o for o in all_objs if not str(o).endswith('.a')], output):
            run_cmd(ld_cmd, desc=f"link -> {rel(output)}")

        print(f"[OK] {rel(output)}")
        return output

    @staticmethod
    def _needs_rebuild_any(deps: List[Path], out: Path) -> bool:
        if not out.exists():
            return True
        out_mtime = out.stat().st_mtime
        return any(d.stat().st_mtime > out_mtime for d in deps)


def _find_compiler_rt(prj_root: Path, arch: str, clang_path: str) -> Path | None:
    rt_name = "libclang_rt.builtins-x86_64.a"

    local = prj_root / "Kernel/lib" / arch / "compiler-rt" / rt_name
    if local.exists():
        return local

    try:
        result = subprocess.run(
            [clang_path, "-print-resource-dir"],
            capture_output=True, text=True,
        )
        res_dir = Path(result.stdout.strip())
        for candidate in [
            res_dir / "lib" / "linux" / rt_name,
            res_dir / "lib" / rt_name,
        ]:
            if candidate.exists():
                return candidate
    except Exception:
        pass

    return None


BuilderRegistry.register("x86_64", "kernel", X86_64KernelBuilder)
