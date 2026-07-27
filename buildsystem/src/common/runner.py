import subprocess
from pathlib import PurePath
from typing import List, Optional


ROOT = PurePath(__file__).parent.parent.parent.parent.parent


class BuildError(RuntimeError):
    pass


def run_cmd(
    args: List[str],
    *,
    cwd: Optional[str] = None,
    desc: str = "",
) -> None:
    label = desc or " ".join(args)
    cmd = " ".join(str(a) for a in args)
    print(f"  [RUN] {label}")
    result = subprocess.run(cmd, cwd=cwd, shell=True, capture_output=True, text=True)
    if result.returncode != 0:
        stderr = result.stderr.strip()
        if stderr:
            print(f"  ── stderr ──")
            for line in stderr.splitlines():
                print(f"  {line}")
            print(f"  ────────────")
        raise BuildError(f"command failed (exit {result.returncode}): {label}")
    if result.stdout.strip():
        for line in result.stdout.strip().splitlines():
            print(f"  {line}")


def rel(p: PurePath) -> str:
    try:
        return str(PurePath(p).relative_to(ROOT))
    except ValueError:
        return str(p)
