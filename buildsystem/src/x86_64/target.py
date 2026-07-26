from pathlib import PurePath
from typing import List

from ..abstract.target import Target
from ..abstract.registry import TargetRegistry
from .toolchain import ClangToolchain


class X86_64Target(Target):
    def __init__(self, toolchain: ClangToolchain | None = None):
        self._tc = toolchain or ClangToolchain()

    def arch(self) -> str:
        return "x86_64"

    def triple(self) -> str:
        return "x86_64-unknown-windows"

    def toolchain(self) -> ClangToolchain:
        return self._tc

    def cflags(self) -> List[str]:
        return self._tc.cflags()

    def ldflags(self) -> List[str]:
        return self._tc.ldflags()

    def compile_cmd(
        self, src: PurePath, obj: PurePath, includes: List[PurePath], defines: List[str]
    ) -> List[str]:
        return self._tc.compile_cmd(src, obj, includes, defines)

    def link_cmd(self, objs: List[PurePath], out: PurePath) -> List[str]:
        return self._tc.link_cmd(objs, out)

    def obj_ext(self) -> str:
        return ".obj"

    def out_ext(self) -> str:
        return ".efi"


TargetRegistry.register("x86_64", X86_64Target)
