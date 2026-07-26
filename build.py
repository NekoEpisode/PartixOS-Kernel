import subprocess
import sys
from pathlib import Path

BUILDSYSTEM = Path(__file__).resolve().parent / "buildsystem"

def main():
    result = subprocess.run(
        [sys.executable, "-m", "src"] + sys.argv[1:],
        cwd=str(BUILDSYSTEM),
    )
    sys.exit(result.returncode)

if __name__ == "__main__":
    main()
