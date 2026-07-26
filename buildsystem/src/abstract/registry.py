from __future__ import annotations
from typing import Dict, Tuple, Type


class TargetRegistry:
    _targets: Dict[str, Type] = {}

    @classmethod
    def register(cls, arch: str, target_cls: Type) -> None:
        if arch in cls._targets:
            raise KeyError(f"arch '{arch}' already registered")
        cls._targets[arch] = target_cls

    @classmethod
    def get(cls, arch: str) -> Type:
        if arch not in cls._targets:
            known = ", ".join(sorted(cls._targets))
            raise KeyError(f"unknown arch '{arch}' (available: {known})")
        return cls._targets[arch]

    @classmethod
    def list(cls) -> Dict[str, Type]:
        return dict(cls._targets)


class BuilderRegistry:
    _builders: Dict[Tuple[str, str], Type] = {}

    @classmethod
    def register(cls, arch: str, component: str, builder_cls: Type) -> None:
        cls._builders[(arch, component)] = builder_cls

    @classmethod
    def get(cls, arch: str, component: str) -> Type:
        key = (arch, component)
        if key not in cls._builders:
            known = ", ".join(f"({a},{c})" for a, c in sorted(cls._builders))
            raise KeyError(f"no builder for arch='{arch}' component='{component}' (available: {known})")
        return cls._builders[key]
