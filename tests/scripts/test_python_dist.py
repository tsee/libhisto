#!/usr/bin/env python3
"""
Hermetic test script for Python bindings distribution tarball (sdist).
Builds sdist, extracts in a clean temporary directory, builds/installs, and runs test suite.
"""

import os
import sys
import glob
import shutil
import tempfile
import subprocess

REPO_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
PYTHON_DIR = os.path.join(REPO_ROOT, "bindings", "python")


def run_cmd(cmd, cwd=None, env=None):
    print(f"        >> {' '.join(cmd) if isinstance(cmd, list) else cmd}")
    res = subprocess.run(cmd, cwd=cwd, env=env, check=True)
    return res.returncode


def main():
    print("=" * 70)
    print(" TESTING STANDALONE PYTHON DISTRIBUTION TARBALL (HERMETIC BUILD)")
    print("=" * 70)

    # 1. Build sdist
    print("  [1/3] Building Python distribution tarball (python3 setup.py sdist)...")
    dist_dir = os.path.join(PYTHON_DIR, "dist")
    if os.path.exists(dist_dir):
        shutil.rmtree(dist_dir)

    run_cmd([sys.executable, "setup.py", "sdist"], cwd=PYTHON_DIR)

    tarballs = glob.glob(os.path.join(dist_dir, "histo-*.tar.gz"))
    if not tarballs:
        print("[FAIL] No histo-*.tar.gz sdist found in dist/", file=sys.stderr)
        sys.exit(1)
    sdist_tar = tarballs[0]
    print(f"        Generated: {os.path.basename(sdist_tar)}")

    # 2. Extract and test in isolated temp environment
    with tempfile.TemporaryDirectory() as tmpdir:
        print(f"  [2/3] Extracting tarball in isolated scratch environment {tmpdir}...")
        shutil.unpack_archive(sdist_tar, tmpdir)
        extracted_dirs = [os.path.join(tmpdir, d) for d in os.listdir(tmpdir) if os.path.isdir(os.path.join(tmpdir, d))]
        if not extracted_dirs:
            print("[FAIL] No extracted directory found!", file=sys.stderr)
            sys.exit(1)
        extracted_pkg = extracted_dirs[0]

        # 3. Build in-place in extracted folder (which only has bundled/ and no access to root CMake/repo)
        print("  [3/3] Compiling C extension and executing test suite from standalone extracted tarball...")
        run_cmd([sys.executable, "setup.py", "build_ext", "--inplace"], cwd=extracted_pkg)
        run_cmd([sys.executable, "-m", "unittest", "discover", "-s", "tests", "-v"], cwd=extracted_pkg)

    print("======================================================================")
    print(" PYTHON STANDALONE DISTRIBUTION TARBALL TEST PASSED CLEANLY")
    print("======================================================================")


if __name__ == "__main__":
    main()
