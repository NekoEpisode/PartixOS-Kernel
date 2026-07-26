from abc import ABC, abstractmethod
from pathlib import PurePath
from typing import List

from .toolchain import Toolchain


class Target(ABC):
    @abstractmethod
    def arch(self) -> str:
        ...

    @abstractmethod
    def triple(self) -> str:
        ...

    @abstractmethod
    def toolchain(self) -> Toolchain:
        ...

    @abstractmethod
    def cflags(self) -> List[str]:
        ...

    @abstractmethod
    def ldflags(self) -> List[str]:
        ...

    @abstractmethod
    def compile_cmd(
        self, src: PurePath, obj: PurePath, includes: List[PurePath], defines: List[str]
    ) -> List[str]:
        ...

    @abstractmethod
    def link_cmd(self, objs: List[PurePath], out: PurePath) -> List[str]:
        ...

    @abstractmethod
    def obj_ext(self) -> str:
        ...

    @abstractmethod
    def out_ext(self) -> str:
        ...

    def build_dir(self) -> PurePath:
        return PurePath("build") / self.arch()
