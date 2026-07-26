from pathlib import PurePath
from typing import List

from ..abstract.toolchain import Toolchain
from ..abstract.registry import TargetRegistry


RISCV_CFLAGS = [
    "-ffreestanding", "-nostdlib",
    "-mcmodel=medany", "-funwind-tables",
    "-march=rv64gc", "-mabi=lp64d",
]


class RV64Toolchain(Toolchain):
    def __init__(self, clang: str = "clang-18", ld: str = "ld.lld-18"):
        self._clang = clang
        self._ld = ld

    def compiler(self) -> str:
        return self._clang

    def linker(self) -> str:
        return self._ld

    def cflags(self) -> List[str]:
        return list(RISCV_CFLAGS)

    def ldflags(self) -> List[str]:
        return ["-nostdlib", "--no-rosegment", "--eh-frame-hdr", "--relax"]

    def compile_cmd(self, src, obj, includes, defines) -> List[str]:
        cmd = [self._clang, "--target=riscv64-unknown-none", *self.cflags()]
        for inc in includes:
            cmd.append(f"-I{inc}")
        for d in defines:
            cmd.append(f"-D{d}")
        cmd += ["-c", str(src), "-o", str(obj)]
        return cmd

    def link_cmd(self, objs, out) -> List[str]:
        cmd = [self._ld, *self.ldflags(), "-o", str(out)]
        for obj in objs:
            cmd.append(str(obj))
        return cmd


from ..abstract.target import Target

class RV64Target(Target):
    def __init__(self, toolchain=None):
        self._tc = toolchain or RV64Toolchain()

    def arch(self) -> str:
        return "riscv64"

    def triple(self) -> str:
        return "riscv64-unknown-none"

    def toolchain(self):
        return self._tc

    def cflags(self) -> List[str]:
        return self._tc.cflags()

    def ldflags(self) -> List[str]:
        return self._tc.ldflags()

    def compile_cmd(self, src, obj, includes, defines) -> List[str]:
        return self._tc.compile_cmd(src, obj, includes, defines)

    def link_cmd(self, objs, out) -> List[str]:
        return self._tc.link_cmd(objs, out)

    def obj_ext(self) -> str:
        return ".o"

    def out_ext(self) -> str:
        return ".elf"

    def build_dir(self) -> PurePath:
        return PurePath("build") / self.arch()


TargetRegistry.register("riscv64", RV64Target)
