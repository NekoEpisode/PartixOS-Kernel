import argparse
import shutil
import sys
from pathlib import Path, PurePath

from .abstract.registry import TargetRegistry, BuilderRegistry
from .abstract.component import ComponentRegistry
from .common.config import load_settings, save_settings, CONFIG_PATH
from .common.qemu import QemuRunConfig, create_disk_image, launch_qemu, _interactive_qemu_setup
from .common.runner import BuildError

from .x86_64.toolchain import ClangToolchain

# Triggers ComponentRegistry + BuilderRegistry registration
from .components import bootloader as _  # noqa: F401
from .components import hal as _        # noqa: F401
from .components import kernel as _     # noqa: F401
from .components import drivers as _    # noqa: F401
from .x86_64 import builder as _          # noqa: F401
from .x86_64 import hal_builder as _      # noqa: F401
from .x86_64 import kernel_builder as _    # noqa: F401
from .x86_64 import drivers_builder as _  # noqa: F401
from .riscv64 import hal_builder as _     # noqa: F401
from .riscv64 import kernel_builder as _   # noqa: F401
from .riscv64 import bootloader_builder as _  # noqa: F401
from .riscv64 import drivers_builder as _ # noqa: F401


def _get_toolchain(arch: str, clang_override: str = "", linker_override: str = ""):
    settings = load_settings()
    tc = settings.toolchains.get(arch) or settings.toolchains.get("x86_64")
    if tc is None:
        print("[FAIL] no toolchain configured. run 'python build.py setup' first.", file=sys.stderr)
        sys.exit(1)
    clang = clang_override or tc.clang
    linker = linker_override or tc.linker
    linker_mode = tc.linker_mode
    if linker_override:
        linker_mode = "lld-link" if "lld-link" in linker else "clang"
    return ClangToolchain(clang=clang, linker=linker, linker_mode=linker_mode)


def _build_component(comp_name: str, args: argparse.Namespace, dep_objects: dict = None) -> object:
    toolchain = _get_toolchain(args.target, args.clang, args.linker)
    target_cls = TargetRegistry.get(args.target)
    target = target_cls(toolchain)

    builder_cls = BuilderRegistry.get(args.target, comp_name)
    builder = builder_cls(target=target, build_root=PurePath(args.build_root))

    if hasattr(builder, '_debug'):
        builder._debug = args.g
    if hasattr(builder, '_show_stderr'):
        builder._show_stderr = args.show_partic_compiler_err

    if hasattr(builder, 'build'):
        import inspect
        sig = inspect.signature(builder.build)
        if 'dep_objects' in sig.parameters:
            return builder.build(dep_objects=dep_objects or {})
        return builder.build()

    return builder.build()


def build_all(args: argparse.Namespace) -> dict:
    order = ComponentRegistry.build_order()
    print(f"[BUILD] order: {' -> '.join(order)}")

    outputs: dict[str, object] = {}
    dep_map: dict[str, list] = {}

    for comp_name in order:
        comp = ComponentRegistry.get(comp_name)
        deps = {}
        for dep in comp.depends:
            val = outputs.get(dep)
            if isinstance(val, list):
                deps[dep] = val

        print(f"\n  [{comp.name}] {comp.desc}")
        result = _build_component(comp_name, args, deps if deps else None)
        outputs[comp_name] = result

        if isinstance(result, list):
            dep_map[comp_name] = result

    return outputs


def cmd_build(args: argparse.Namespace) -> int:
    comp = ComponentRegistry.get(args.component)
    deps = {}
    for dep in comp.depends:
        # Pick the right builder per dependency (hal -> hal builder, etc.).
        # The old code always used X86_64HalBuilder, which duplicated the hal
        # objects into every dependency slot and broke the kernel link with
        # "duplicate symbol" errors.
        toolchain = _get_toolchain(args.target, args.clang, args.linker)
        target_cls = TargetRegistry.get(args.target)
        target = target_cls(toolchain)
        builder_cls = BuilderRegistry.get(args.target, dep)
        builder = builder_cls(target=target, build_root=PurePath(args.build_root))
        if hasattr(builder, '_debug'):
            builder._debug = args.g
        if hasattr(builder, '_show_stderr'):
            builder._show_stderr = args.show_partic_compiler_err
        deps[dep] = builder.build()

    _build_component(args.component, args, deps)
    return 0


def cmd_setup() -> int:
    if CONFIG_PATH.exists():
        CONFIG_PATH.unlink()
    load_settings()
    print("\n  config saved to buildsettings.json")
    return 0


def cmd_run(args: argparse.Namespace) -> int:
    arch = args.target
    outputs = build_all(args)

    settings = load_settings()
    qc = settings.qemu_for(arch)
    if not qc.ovmf_code:
        run_cfg = _interactive_qemu_setup(arch)
        settings.qemu[arch] = _run_to_config(run_cfg)
        save_settings(settings)
    else:
        run_cfg = QemuRunConfig(
            qemu_binary=qc.qemu_binary(arch),
            memory=qc.memory,
            ovmf_code=qc.ovmf_code,
            ovmf_vars=qc.ovmf_vars,
            extra_args=qc.extra_args,
        )

    disk = Path("build") / "partix.img"
    extra = {}
    if outputs.get("kernel"):
        extra["KERNEL.ELF"] = outputs["kernel"]

    efi_name = "BOOTRISCV64.EFI" if arch == "riscv64" else "BOOTX64.EFI"
    # startup.nsh：UEFI Shell 启动时自动执行，免去每次手动输入 EFI 路径。
    # 相对路径基于当前卷（Shell 启动时 = startup.nsh 所在卷根）。
    nsh = Path("build") / "startup.nsh"
    nsh.write_text(f"\\EFI\\BOOT\\{efi_name}\n")
    extra["startup.nsh"] = nsh
    create_disk_image(outputs.get("bootloader"), disk, extra_files=extra, efi_name=efi_name)

    launch_qemu(run_cfg, disk, arch, args.qemu_args or "")
    return 0


def _run_to_config(r: QemuRunConfig):
    from .common.config import QemuConfig
    return QemuConfig(
        qemu=r.qemu_binary, memory=r.memory,
        ovmf_code=r.ovmf_code, ovmf_vars=r.ovmf_vars,
        extra_args=r.extra_args,
    )


def show_targets() -> None:
    targets = TargetRegistry.list()
    print("available targets:")
    for arch in sorted(targets):
        t = targets[arch]
        print(f"  {arch:12s} ({t.__module__}.{t.__name__})")


def show_components() -> None:
    comps = ComponentRegistry.list_all()
    print("components (sorted by dependency order):")
    try:
        order = ComponentRegistry.build_order()
    except RuntimeError as e:
        print(f"  {e}")
        return
    for name in order:
        c = ComponentRegistry.get(name)
        deps = f" (depends: {', '.join(c.depends)})" if c.depends else ""
        print(f"  {c.name:16s} {c.desc}{deps}")


def main() -> int:
    parser = argparse.ArgumentParser(prog="partix-build")
    sub = parser.add_subparsers(dest="command")

    comp_choices = [c.name for c in ComponentRegistry.list_all()] or ["bootloader"]

    build_parser = sub.add_parser("build", help="build a component")
    build_parser.add_argument("component", choices=comp_choices, help="component to build")
    build_parser.add_argument("--target", default="x86_64", help="build target architecture")
    build_parser.add_argument("--clang", default="", help="override clang path")
    build_parser.add_argument("--linker", default="", help="override linker path")
    build_parser.add_argument("--build-root", default=".", help="root for build output")
    build_parser.add_argument("--g", action="store_true", help="enable debug info")
    build_parser.add_argument("--show-partic-compiler-err", action="store_true", help="always show Partic stderr")

    run_parser = sub.add_parser("run", help="build all + create disk image + launch QEMU")
    run_parser.add_argument("--target", default="x86_64", help="build target architecture")
    run_parser.add_argument("--clang", default="", help="override clang path")
    run_parser.add_argument("--linker", default="", help="override linker path")
    run_parser.add_argument("--build-root", default=".", help="root for build output")
    run_parser.add_argument("--qemu-args", default="", help="extra QEMU arguments")
    run_parser.add_argument("--g", action="store_true", help="enable debug info for all components")
    run_parser.add_argument("--show-partic-compiler-err", action="store_true", help="always show Partic stderr")

    sub.add_parser("targets", help="list available targets")
    sub.add_parser("components", help="list registered components with dependencies")
    sub.add_parser("setup", help="re-run toolchain detection + config")
    sub.add_parser("clean", help="remove build artifacts")

    args = parser.parse_args()

    if args.command == "build":
        return cmd_build(args)
    elif args.command == "run":
        return cmd_run(args)
    elif args.command == "targets":
        show_targets()
        return 0
    elif args.command == "components":
        show_components()
        return 0
    elif args.command == "setup":
        return cmd_setup()
    elif args.command == "clean":
        for d in ["build", Path(__file__).parent.parent / "build"]:
            d = Path(d)
            if d.is_dir():
                shutil.rmtree(d)
                print(f"  removed {d}/")
        return 0
    else:
        parser.print_help()
        return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except BuildError as e:
        print(f"\n[FAIL] {e}", file=sys.stderr)
        sys.exit(1)
    except KeyboardInterrupt:
        print("\n  cancelled")
        sys.exit(130)
