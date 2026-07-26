from abc import ABC, abstractmethod

class BuildBootLoader(ABC):
    @abstractmethod
    def build_bootloader(self) -> int:
        pass