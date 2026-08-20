"""
In-process CLI execution for libhisto toolkit and pyhisto entry point.
"""

import sys
from typing import Tuple, Sequence
import _libhisto


def run(*args: str) -> Tuple[int, str, str]:
    """
    Run libhistocli in-process with captured stdout and stderr.

    Parameters:
        *args: Subcommand and CLI options (e.g. "fill", "--bins=20", "--auto-range")

    Returns:
        (exit_code, stdout_str, stderr_str)
    """
    str_args = [str(a) for a in args]
    return _libhisto.cli_run(*str_args)


def main(argv: Sequence[str] = None) -> int:
    """
    Standalone executable CLI entry point (pyhisto).
    """
    if argv is None:
        argv = sys.argv[1:]

    code, out, err = run(*argv)
    if out:
        sys.stdout.write(out)
        sys.stdout.flush()
    if err:
        sys.stderr.write(err)
        sys.stderr.flush()
    return code


if __name__ == "__main__":
    sys.exit(main())
