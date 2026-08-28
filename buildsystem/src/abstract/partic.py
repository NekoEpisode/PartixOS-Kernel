import shutil
import subprocess
import sys
from pathlib import Path
from typing import List

from ..common.runner import run_cmd, BuildError
from ..common.config import load_settings, ParticConfig


def _run_partic(cmd: List[str], desc: str, show_stderr: bool) -> None:
    """Run the Partic compiler. With show_stderr, stream stderr live so a
    hung compiler still shows its progress (subprocess.run buffers until exit,
    which is useless when the JVM spins forever)."""
    if not show_stderr:
        run_cmd(cmd, desc=desc)
        return
    print(f"  [RUN] {desc}")
    proc = subprocess.Popen(cmd, shell=True,
                            stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                            text=True, bufsize=1)
    assert proc.stdout is not None
    for line in proc.stdout:
        sys.stdout.write(line)
        sys.stdout.flush()
    rc = proc.wait()
    if rc != 0:
        raise BuildError(f"partic compiler failed with exit code {rc}")


def _interactive_partic_setup() -> ParticConfig:
    print("\n  === Partic compiler setup ===")

    java = shutil.which("java") or "java"
    print(f"  java : {java}")
    override = input(f"  change? (enter=keep): ").strip()
    if override:
        java = override

    jar = ""
    while not jar:
        jar = input("  Partic JAR path: ").strip()
        if jar and not Path(jar).exists():
            print(f"  file not found: {jar}")
            jar = ""

    stdlib = input("  additional stdlib dir [none]: ").strip()
    if stdlib and not Path(stdlib).is_dir():
        print(f"  not a directory: {stdlib}")
        stdlib = ""

    return ParticConfig(java=java, jar=jar, stdlib=stdlib)


def compile_partic(
    sources: List[Path],
    out_dir: Path,
    main_class: str = "BspKrMain",
    allocator: str = "KrAlloc",
    target: str = "freestanding",
    debug: bool = False,
    show_stderr: bool = False,
) -> Path:
    settings = load_settings()
    pc = settings.partic
    if not pc.ready:
        pc = _interactive_partic_setup()
        settings.partic = pc
        from ..common.config import save_settings
        save_settings(settings)

    stage = out_dir / ".partic_src"
    if stage.exists():
        shutil.rmtree(stage)
    stage.mkdir(parents=True)

    for src in sources:
        dest = stage / src.relative_to(src.anchor) if src.is_absolute() else stage / src
        dest.parent.mkdir(parents=True, exist_ok=True)
        shutil.copytree(src, dest, dirs_exist_ok=True) if src.is_dir() else shutil.copy2(src, dest)

    partic_files = list(stage.rglob("*.partic"))
    if not partic_files:
        raise BuildError("no .partic source files found")

    cmd = [
        pc.java, "-jar", pc.jar,
        "--main", main_class,
        "--allocator", allocator,
        "--target", target,
    ]
    if debug:
        cmd.append("--g")
    if pc.stdlib:
        cmd += ["--rtdir", pc.stdlib]
    cmd.append(str(stage))

    _run_partic(cmd, f"partic -> LLVM IR ({len(partic_files)} files)", show_stderr)

    ll_files = list(stage.rglob("*.ll")) + list(stage.parent.rglob("*.ll"))
    if not ll_files:
        raise BuildError("partic compiler did not produce .ll output")

    ll_path = ll_files[0]

    if target == "freestanding" or target.endswith("-none"):
        # Rewrite the staged source paths in DWARF to the real absolute paths,
        # so debuggers (gdb) can find the .partic sources after the build.
        _fixup_ll(ll_path, str(stage) + "/")

    return ll_path


def _fixup_ll(path: Path, strip_dir: str) -> None:
    text = path.read_text()
    text = text.replace("\r\n", "\n").replace("\r", "\n")
    text = text.replace(") personality ", ") noredzone personality ")
    import re
    text = re.sub(r"(define[^)]+\))\s*\{", r"\1 noredzone {", text)
    text = text.replace(strip_dir, "/")
    path.write_text(text)
