from pathlib import Path, PurePath
from typing import List, Optional

from ..abstract.builder import Builder
from ..abstract.target import Target
from ..abstract.registry import BuilderRegistry
from ..common.runner import run_cmd, rel
from ..components.user import UserSources, PROJECT_ROOT as COMPONENT_ROOT


class X86_64UserBuilder(Builder):
    def __init__(self, target: Target, build_root: Optional[PurePath] = None,
                 sources: UserSources | None = None):
        super().__init__(target, build_root)
        self._sources = sources or UserSources()
        self._prj_root = Path(COMPONENT_ROOT)
        self._debug = False

    def source_files(self) -> List[PurePath]:
        return []

    def include_dirs(self) -> List[PurePath]:
        return []

    def defines(self) -> List[str]:
        return []

    def output_name(self) -> str:
        return "user"

    def build(self) -> List[Path]:
        tc = self.target.toolchain()
        work = Path(self.build_root) / self.target.build_dir() / "user"
        work.mkdir(parents=True, exist_ok=True)

        # 1. assemble the user program
        src = self._prj_root / self._sources.asm_file("x86_64")
        obj = work / "user_test.o"
        run_cmd([tc.compiler(), "--target=x86_64-unknown-none", "-ffreestanding",
                 "-nostdlib", "-mno-red-zone",
                 "-c", str(src), "-o", str(obj)],
                desc="user asm (x86_64)")

        # 2. link the user ELF at its virtual base (0x400000)
        ld_script = self._prj_root / self._sources.linker_script("x86_64")
        elf = work / "user_test.elf"
        run_cmd([tc.linker(), "-T", str(ld_script), "-nostdlib",
                 "-o", str(elf), str(obj)],
                desc="user link (x86_64)")

        # 3. embed the ELF as a .incbin blob object (address + size symbols)
        blob_s = work / "user_blob.S"
        blob_s.write_text(
            ".section .rodata\n"
            ".globl embedded_user_test_start\n"
            "embedded_user_test_start:\n"
            "    .quad embedded_user_test_data\n"
            ".globl embedded_user_test_size\n"
            "embedded_user_test_size:\n"
            "    .quad embedded_user_test_data_end - embedded_user_test_data\n"
            "embedded_user_test_data:\n"
            f"    .incbin \"{elf}\"\n"
            "embedded_user_test_data_end:\n"
        )
        blob_obj = work / "user_blob.o"
        run_cmd([tc.compiler(), "--target=x86_64-unknown-none", "-ffreestanding",
                 "-nostdlib", "-mno-red-zone",
                 "-c", str(blob_s), "-o", str(blob_obj)],
                desc="user blob (x86_64)")

        print(f"[OK] {rel(blob_obj)}")
        return [blob_obj]


BuilderRegistry.register("x86_64", "user", X86_64UserBuilder)
