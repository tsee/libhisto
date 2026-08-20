import os
import sys
from setuptools import setup, Extension, find_packages

# Determine paths
BASE_DIR = os.path.abspath(os.path.dirname(__file__))
ROOT_DIR = os.path.abspath(os.path.join(BASE_DIR, "..", ".."))

# Check if building in-tree from root repository or standalone sdist
if os.path.exists(os.path.join(ROOT_DIR, "include", "histo", "histo.h")):
    include_dirs = [
        os.path.join(ROOT_DIR, "include"),
        os.path.join(ROOT_DIR, "tools", "include"),
        os.path.join(ROOT_DIR, "src"),
        os.path.join(ROOT_DIR, "src", "vendor", "cJSON"),
    ]
    sources = [
        os.path.join(BASE_DIR, "src", "_libhistomodule.c"),
        os.path.join(ROOT_DIR, "src", "histo.c"),
        os.path.join(ROOT_DIR, "src", "histo2d.c"),
        os.path.join(ROOT_DIR, "src", "fit.c"),
        os.path.join(ROOT_DIR, "src", "kde.c"),
        os.path.join(ROOT_DIR, "src", "sketch.c"),
        os.path.join(ROOT_DIR, "src", "serialize.c"),
        os.path.join(ROOT_DIR, "src", "serialize_2d.c"),
        os.path.join(ROOT_DIR, "src", "simd_avx2.c"),
        os.path.join(ROOT_DIR, "src", "simd_avx512.c"),
        os.path.join(ROOT_DIR, "src", "simd_neon.c"),
        os.path.join(ROOT_DIR, "src", "vendor", "cJSON", "cJSON.c"),
        os.path.join(ROOT_DIR, "tools", "src", "cli_common.c"),
        os.path.join(ROOT_DIR, "tools", "src", "cli_main.c"),
        os.path.join(ROOT_DIR, "tools", "src", "cmd_fill.c"),
        os.path.join(ROOT_DIR, "tools", "src", "cmd_plot.c"),
        os.path.join(ROOT_DIR, "tools", "src", "cmd_stats.c"),
        os.path.join(ROOT_DIR, "tools", "src", "cmd_cmp.c"),
        os.path.join(ROOT_DIR, "tools", "src", "cmd_fit.c"),
    ]
else:
    # Standalone sdist with bundled sources
    include_dirs = [
        os.path.join(BASE_DIR, "bundled", "include"),
        os.path.join(BASE_DIR, "bundled", "tools", "include"),
        os.path.join(BASE_DIR, "bundled", "src"),
        os.path.join(BASE_DIR, "bundled", "src", "vendor", "cJSON"),
    ]
    sources = [
        os.path.join(BASE_DIR, "src", "_libhistomodule.c"),
        os.path.join(BASE_DIR, "bundled", "src", "histo.c"),
        os.path.join(BASE_DIR, "bundled", "src", "histo2d.c"),
        os.path.join(BASE_DIR, "bundled", "src", "fit.c"),
        os.path.join(BASE_DIR, "bundled", "src", "kde.c"),
        os.path.join(BASE_DIR, "bundled", "src", "sketch.c"),
        os.path.join(BASE_DIR, "bundled", "src", "serialize.c"),
        os.path.join(BASE_DIR, "bundled", "src", "serialize_2d.c"),
        os.path.join(BASE_DIR, "bundled", "src", "simd_avx2.c"),
        os.path.join(BASE_DIR, "bundled", "src", "simd_avx512.c"),
        os.path.join(BASE_DIR, "bundled", "src", "simd_neon.c"),
        os.path.join(BASE_DIR, "bundled", "src", "vendor", "cJSON", "cJSON.c"),
        os.path.join(BASE_DIR, "bundled", "tools", "src", "cli_common.c"),
        os.path.join(BASE_DIR, "bundled", "tools", "src", "cli_main.c"),
        os.path.join(BASE_DIR, "bundled", "tools", "src", "cmd_fill.c"),
        os.path.join(BASE_DIR, "bundled", "tools", "src", "cmd_plot.c"),
        os.path.join(BASE_DIR, "bundled", "tools", "src", "cmd_stats.c"),
        os.path.join(BASE_DIR, "bundled", "tools", "src", "cmd_cmp.c"),
        os.path.join(BASE_DIR, "bundled", "tools", "src", "cmd_fit.c"),
    ]


extra_compile_args = ["-std=c99", "-O3"]
if sys.platform != "win32":
    extra_compile_args.extend(["-Wall", "-Wextra", "-pedantic", "-Wno-unused-parameter", "-Wno-unused-function"])

libraries = ["m"] if sys.platform != "win32" else []

ext_modules = [
    Extension(
        name="_libhisto",
        sources=sources,
        include_dirs=include_dirs,
        extra_compile_args=extra_compile_args,
        libraries=libraries,
    )
]

import shutil
from setuptools.command.sdist import sdist as _sdist

class CustomSdist(_sdist):
    """Custom sdist command that bundles libhisto C sources for standalone distribution."""
    def run(self):
        bundled_dir = os.path.join(BASE_DIR, "bundled")
        if os.path.exists(os.path.join(ROOT_DIR, "include", "histo", "histo.h")):
            if os.path.exists(bundled_dir):
                shutil.rmtree(bundled_dir)
            os.makedirs(bundled_dir, exist_ok=True)
            shutil.copytree(os.path.join(ROOT_DIR, "include"), os.path.join(bundled_dir, "include"))
            shutil.copytree(os.path.join(ROOT_DIR, "src"), os.path.join(bundled_dir, "src"))
            shutil.copytree(os.path.join(ROOT_DIR, "tools"), os.path.join(bundled_dir, "tools"))
        super().run()

setup(
    packages=find_packages(),
    ext_modules=ext_modules,
    package_data={"histo": ["py.typed"]},
    cmdclass={"sdist": CustomSdist},
)

