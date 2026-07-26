from abc import ABC, abstractmethod
from pathlib import PurePath
from typing import Optional

from .target import Target


class Builder(ABC):
    def __init__(self, target: Target, build_root: Optional[PurePath] = None):
        self.target = target
        self._build_root = build_root or PurePath(".")

    @property
    def build_root(self) -> PurePath:
        return self._build_root

    @abstractmethod
    def source_files(self) -> list[PurePath]:
        ...

    @abstractmethod
    def include_dirs(self) -> list[PurePath]:
        ...

    @abstractmethod
    def defines(self) -> list[str]:
        ...

    @abstractmethod
    def output_name(self) -> str:
        ...

    @abstractmethod
    def build(self) -> PurePath:
        ...
