from pathlib import PurePath
from typing import List

from ..abstract.toolchain import Toolchain


EFI_CFLAGS = [
    "-ffreestanding",
    "-fno-stack-protector",
    "-fshort-wchar",
    "-mno-red-zone",
    "-mabi=ms",
]

EFI_LDFLAGS = [
    "/subsystem:efi_application",
    "/entry:efi_main",
    "/dll",
    "/nodefaultlib",
    "/machine:x64",
]


class ClangToolchain(Toolchain):
    def __init__(
        self,
        clang: str = "clang-18",
        linker: str = "lld-link",
        linker_mode: str = "lld-link",
    ):
        self._clang = clang
        self._linker = linker
        self._linker_mode = linker_mode

    def compiler(self) -> str:
        return self._clang

    def linker(self) -> str:
        return self._linker

    def cflags(self) -> List[str]:
        return list(EFI_CFLAGS)

    def ldflags(self) -> List[str]:
        return list(EFI_LDFLAGS)

    def compile_cmd(
        self,
        src: PurePath,
        obj: PurePath,
        includes: List[PurePath],
        defines: List[str],
    ) -> List[str]:
        cmd = [
            self._clang,
            "--target=x86_64-unknown-windows",
            *self.cflags(),
        ]
        for inc in includes:
            cmd.append(f"-I{inc}")
        for d in defines:
            cmd.append(f"-D{d}")
        cmd += ["-c", str(src), "-o", str(obj)]
        return cmd

    def link_cmd(self, objs: List[PurePath], out: PurePath) -> List[str]:
        if self._linker_mode == "lld-link":
            cmd = [self._linker, *self.ldflags(), f"/out:{out}"]
        else:
            cmd = [
                self._clang,
                "--target=x86_64-unknown-windows",
                "-ffreestanding",
                "-nostdlib",
                "-fuse-ld=lld",
                *[f"-Wl,{flag}" for flag in self.ldflags()],
                "-o", str(out),
            ]
        for obj in objs:
            cmd.append(str(obj))
        return cmd
