from abc import ABC, abstractmethod
from pathlib import PurePath
from typing import List


class Toolchain(ABC):
    @abstractmethod
    def compiler(self) -> str:
        ...

    @abstractmethod
    def linker(self) -> str:
        ...

    @abstractmethod
    def cflags(self) -> List[str]:
        ...

    @abstractmethod
    def ldflags(self) -> List[str]:
        ...

    @abstractmethod
    def compile_cmd(
        self,
        src: PurePath,
        obj: PurePath,
        includes: List[PurePath],
        defines: List[str],
    ) -> List[str]:
        ...

    @abstractmethod
    def link_cmd(
        self,
        objs: List[PurePath],
        out: PurePath,
    ) -> List[str]:
        ...
