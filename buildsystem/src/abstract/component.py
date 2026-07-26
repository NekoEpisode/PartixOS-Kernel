from __future__ import annotations
from collections import deque
from dataclasses import dataclass, field
from typing import Dict, List


@dataclass
class Component:
    name: str
    desc: str = ""
    depends: List[str] = field(default_factory=list)


class ComponentRegistry:
    _components: Dict[str, Component] = {}

    @classmethod
    def register(cls, comp: Component) -> None:
        if comp.name in cls._components:
            raise KeyError(f"component '{comp.name}' already registered")
        cls._components[comp.name] = comp

    @classmethod
    def get(cls, name: str) -> Component:
        if name not in cls._components:
            known = ", ".join(sorted(cls._components))
            raise KeyError(f"unknown component '{name}' (available: {known})")
        return cls._components[name]

    @classmethod
    def list_all(cls) -> List[Component]:
        return list(cls._components.values())

    @classmethod
    def build_order(cls) -> List[str]:
        for comp in cls._components.values():
            for dep in comp.depends:
                if dep not in cls._components:
                    raise KeyError(
                        f"component '{comp.name}' depends on unknown '{dep}'"
                    )

        in_degree = {name: 0 for name in cls._components}
        adj = {name: [] for name in cls._components}
        for comp in cls._components.values():
            for dep in comp.depends:
                adj[dep].append(comp.name)
                in_degree[comp.name] += 1

        queue = deque(name for name, deg in in_degree.items() if deg == 0)
        order = []
        while queue:
            name = queue.popleft()
            order.append(name)
            for nxt in adj[name]:
                in_degree[nxt] -= 1
                if in_degree[nxt] == 0:
                    queue.append(nxt)

        if len(order) != len(cls._components):
            remaining = set(cls._components) - set(order)
            raise RuntimeError(f"circular dependency in components: {remaining}")

        return order
