#!/usr/bin/env python3
"""
test_doc_examples.py - Extract, compile, and execute inline C and CLI examples from libhisto documentation.
"""

import argparse
import os
import re
import subprocess
import sys
import tempfile

def extract_code_blocks(filepath):
    """Extract code blocks with line numbers and languages from a markdown file."""
    with open(filepath, "r", encoding="utf-8") as f:
        lines = f.readlines()

    in_block = False
    block_lang = ""
    block_lines = []
    start_line = 0
    blocks = []

    for i, line in enumerate(lines):
        if line.startswith("```"):
            if not in_block:
                in_block = True
                block_lang = line.strip().lstrip("`").strip()
                block_lines = []
                start_line = i + 1
            else:
                in_block = False
                blocks.append({
                    "file": filepath,
                    "line": start_line,
                    "lang": block_lang,
                    "code": "".join(block_lines)
                })
        elif in_block:
            block_lines.append(line)

    return blocks

def test_c_block(block, source_dir, build_dir, compiler, cflags, verbose):
    """Compile and run a C code block."""
    code = block["code"]
    filepath = block["file"]
    line = block["line"]

    include_dir = os.path.join(source_dir, "include")
    lib_path = os.path.join(build_dir, "src", "libhisto.a")
    if not os.path.exists(lib_path):
        lib_path = os.path.join(build_dir, "libhisto.a")

    has_main = "int main(" in code or "int main (void)" in code or "int main(void)" in code

    if not has_main:
        if "my_sine_model" in code:
            wrapped = f"""#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <stdbool.h>
#include <histo/histo.h>
#include <histo/fit.h>

{code}

int main(void) {{
    histo_t *h = histo_create_uniform(20, 0.0, 10.0, HISTO_FLAG_NONE);
    for (int i = 0; i < 100; i++) histo_fill(h, (double)i * 0.1);
    histo_fit_options_t opts;
    histo_fit_options_init(&opts);
    opts.grad_fn = my_sine_gradient;
    double initial_params[2] = {{10.0, 0.1}};
    histo_fit_result_t *res = NULL;
    histo_fit_custom(h, my_sine_model, 2, initial_params, &opts, &res);
    histo_fit_result_destroy(res);
    histo_destroy(h);
    return 0;
}}
"""
        elif "lower_bounds" in code and "histo_fit_model" in code:
            wrapped = f"""#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <stdbool.h>
#include <histo/histo.h>
#include <histo/fit.h>

int main(void) {{
    histo_t *h = histo_create_uniform(50, 20.0, 80.0, HISTO_FLAG_TRACK_SUMW2);
    for (int i = 0; i < 500; i++) histo_fill(h, 50.0);
    {code}
    histo_fit_result_destroy(res);
    histo_destroy(h);
    return 0;
}}
"""
        elif "histo2d_project_x" in code:
            wrapped = f"""#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <stdbool.h>
#include <histo/histo.h>
#include <histo/histo2d.h>

int main(void) {{
    histo2d_t *h2d = histo2d_create_uniform(20, -5.0, 5.0, 20, -5.0, 5.0, HISTO_FLAG_NONE);
    histo2d_fill(h2d, 0.0, 0.0);
    {code}
    histo2d_destroy(h2d);
    return 0;
}}
"""
        elif "histo2d_serialize_binary_alloc" in code:
            wrapped = f"""#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <stdbool.h>
#include <histo/histo.h>
#include <histo/histo2d.h>

int main(void) {{
    histo2d_t *h2d = histo2d_create_uniform(10, 0.0, 10.0, 10, 0.0, 10.0, HISTO_FLAG_NONE);
    histo2d_fill(h2d, 1.0, 2.0);
    {code}
    histo2d_destroy(h2d);
    return 0;
}}
"""
        elif "N_SAMPLES" in code:
            wrapped = f"""#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <stdbool.h>
#include <histo/histo.h>

int main(void) {{
    histo_t *h = histo_create_uniform(100, 0.0, 100.0, HISTO_FLAG_NONE);
    {code}
    histo_destroy(h);
    return 0;
}}
"""
        else:
            wrapped = f"""#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <stdbool.h>
#include <histo/histo.h>
#include <histo/histo2d.h>
#include <histo/fit.h>
#include <histo/sketch.h>

int main(void) {{
    {code}
    return 0;
}}
"""
        code = wrapped

    with tempfile.TemporaryDirectory() as tmpdir:
        src_file = os.path.join(tmpdir, "example.c")
        bin_file = os.path.join(tmpdir, "example_bin")
        with open(src_file, "w", encoding="utf-8") as f:
            f.write(code)

        cmd = [compiler] + cflags + [
            f"-I{include_dir}",
            src_file,
            lib_path,
            "-lm",
            "-o", bin_file
        ]

        if verbose:
            print(f"  Compiling {filepath}:{line} -> {' '.join(cmd)}")

        res = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
        if res.returncode != 0:
            err_lower = res.stderr.lower()
            if "tsan" in err_lower or "__tsan" in err_lower:
                cmd_san = [compiler] + cflags + ["-fsanitize=thread", f"-I{include_dir}", src_file, lib_path, "-lm", "-pthread", "-o", bin_file]
                res = subprocess.run(cmd_san, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
            elif "asan" in err_lower or "ubsan" in err_lower or "__asan" in err_lower:
                cmd_san = [compiler] + cflags + ["-fsanitize=address,undefined", f"-I{include_dir}", src_file, lib_path, "-lm", "-o", bin_file]
                res = subprocess.run(cmd_san, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
            elif "msan" in err_lower or "__msan" in err_lower:
                cmd_san = [compiler] + cflags + ["-fsanitize=memory", f"-I{include_dir}", src_file, lib_path, "-lm", "-o", bin_file]
                res = subprocess.run(cmd_san, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)

            if res.returncode != 0:
                print(f"\n[FAIL] Compilation error in {filepath}:{line}")
                print(res.stderr)
                print("Source code was:")
                for i, l in enumerate(code.splitlines(), 1):
                    print(f"{i:4d}: {l}")
                return False


        run_res = subprocess.run([bin_file], stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
        if run_res.returncode != 0:
            print(f"\n[FAIL] Execution error in {filepath}:{line} (exit code {run_res.returncode})")
            print(run_res.stderr)
            return False

        if verbose:
            print(f"  [PASS] C Example {filepath}:{line}")
        return True

def test_cli_block(block, source_dir, build_dir, verbose):
    """Execute runnable CLI bash commands."""
    code = block["code"].strip()
    filepath = block["file"]
    line = block["line"]

    if any(k in code for k in ["git clone", "make", "cmake", "sudo ", "doxygen"]):
        return True

    tools_dir = os.path.abspath(os.path.join(build_dir, "tools"))
    histo_bin = os.path.join(tools_dir, "histo")
    if not os.path.exists(histo_bin):
        print(f"[FAIL] CLI binary not found at {histo_bin}")
        return False

    env = os.environ.copy()
    env["PATH"] = f"{tools_dir}:{env.get('PATH', '')}"

    pipelines = []
    current_cmd = []
    for l in code.splitlines():
        trimmed = l.strip()
        if not trimmed or trimmed.startswith("#"):
            continue
        if trimmed.endswith("\\"):
            current_cmd.append(trimmed[:-1].strip())
        else:
            current_cmd.append(trimmed)
            pipelines.append(" ".join(current_cmd))
            current_cmd = []

    with tempfile.TemporaryDirectory() as tmpdir:
        # Create dummy sample files
        with open(os.path.join(tmpdir, "data.txt"), "w") as f:
            f.write("\n".join(str(i) for i in range(100)) + "\n")
        with open(os.path.join(tmpdir, "sensor_data.csv"), "w") as f:
            f.write("\n".join(f"{i},{i*1.5}" for i in range(100)) + "\n")

        # Create sample background.json / data.json / data.bin / sparse_counts.bin
        subprocess.run(
            [histo_bin, "fill", "--min", "0", "--max", "100", "-n", "50", "-o", "json", "-f", os.path.join(tmpdir, "background.json")],
            input="\n".join(str(i) for i in range(100)),
            text=True
        )
        subprocess.run(
            [histo_bin, "fill", "--min", "0", "--max", "100", "-n", "50", "-o", "json", "-f", os.path.join(tmpdir, "data.json")],
            input="\n".join(str(i) for i in range(100)),
            text=True
        )
        subprocess.run(
            [histo_bin, "fill", "--min", "0", "--max", "100", "-n", "50", "-o", "binary", "-f", os.path.join(tmpdir, "data.bin")],
            input="\n".join(str(i) for i in range(100)),
            text=True
        )
        subprocess.run(
            [histo_bin, "fill", "--min", "0", "--max", "100", "-n", "50", "-o", "binary", "-f", os.path.join(tmpdir, "sparse_counts.bin")],
            input="\n".join(str(i) for i in range(100)),
            text=True
        )

        for cmd in pipelines:
            if "histo" not in cmd:
                continue
            if verbose:
                print(f"  Testing CLI {filepath}:{line} -> {cmd}")
            res = subprocess.run(
                ["bash", "-c", cmd],
                cwd=tmpdir,
                env=env,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                text=True
            )
            if res.returncode != 0:
                print(f"\n[FAIL] CLI execution failed in {filepath}:{line}")
                print(f"Command: {cmd}")
                print(f"Stderr: {res.stderr}")
                return False

    if verbose:
        print(f"  [PASS] CLI Pipeline {filepath}:{line}")
    return True

def run_task(task, source_dir, build_dir, compiler, cflags, verbose):
    task_type, block = task
    if task_type == "c":
        ok = test_c_block(block, source_dir, build_dir, compiler, cflags, verbose)
        return ("c", block, ok)
    else:
        ok = test_cli_block(block, source_dir, build_dir, verbose)
        return ("cli", block, ok)

def main():
    parser = argparse.ArgumentParser(description="Test inline C and CLI examples from libhisto documentation.")
    parser.add_argument("--source-dir", default=os.path.abspath("."), help="Path to libhisto repository root")
    parser.add_argument("--build-dir", default=os.path.abspath("build"), help="Path to CMake build directory")
    parser.add_argument("--compiler", default=os.environ.get("CC", "gcc"), help="C compiler to use")
    parser.add_argument("--jobs", "-j", type=int, default=os.cpu_count() or 4, help="Number of parallel workers")
    parser.add_argument("--verbose", "-v", action="store_true", help="Verbose output")
    args = parser.parse_args()

    cflags = ["-std=c99", "-Wall", "-Wextra", "-Werror", "-O2"]

    doc_files = [
        "README.md",
        "docs/manual.md",
        "docs/curve_fitting_guide.md",
        "docs/histo2d_guide.md"
    ]

    tasks = []
    for rel_path in doc_files:
        filepath = os.path.join(args.source_dir, rel_path)
        if not os.path.exists(filepath):
            continue

        blocks = extract_code_blocks(filepath)
        for block in blocks:
            lang = block["lang"].lower()
            if lang in ["c", "c99"]:
                tasks.append(("c", block))
            elif lang in ["bash", "sh"]:
                code = block["code"]
                if "histo" in code and not any(k in code for k in ["git clone", "make", "cmake", "sudo "]):
                    tasks.append(("cli", block))

    total_c = sum(1 for t in tasks if t[0] == "c")
    total_cli = sum(1 for t in tasks if t[0] == "cli")
    passed_c = 0
    passed_cli = 0
    failed = False

    print("======================================================================")
    print(f" TESTING INLINE DOCUMENTATION CODE EXAMPLES & CLI PIPELINES (-j {args.jobs})")
    print("======================================================================")

    import concurrent.futures
    with concurrent.futures.ThreadPoolExecutor(max_workers=args.jobs) as executor:
        futures = [executor.submit(run_task, t, args.source_dir, args.build_dir, args.compiler, cflags, args.verbose) for t in tasks]
        for f in concurrent.futures.as_completed(futures):
            task_type, block, ok = f.result()
            if task_type == "c":
                if ok:
                    passed_c += 1
                else:
                    failed = True
            else:
                if ok:
                    passed_cli += 1
                else:
                    failed = True

    print("======================================================================")
    print(f" SUMMARY: C Examples: {passed_c}/{total_c} passed | CLI Examples: {passed_cli}/{total_cli} passed")
    if failed or passed_c != total_c or passed_cli != total_cli:
        print(" RESULT: FAILED")
        print("======================================================================")
        return 1
    else:
        print(" RESULT: ALL INLINE DOCUMENTATION EXAMPLES PASSED SUCCESSFULLY")
        print("======================================================================")
        return 0


if __name__ == "__main__":
    sys.exit(main())
