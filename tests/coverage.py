#!/usr/bin/env python3
"""Report and enforce vcontrold's gcov coverage thresholds."""

import argparse
import gzip
import json
import pathlib
import subprocess
import tempfile


CORE_MODULES = {
    "parser", "framer", "xmlconfig", "client", "unit", "arithmetic",
}


def collect(build_dir, source_dir):
    merged_lines = {}
    merged_branches = {}
    gcno_files = sorted(build_dir.glob("CMakeFiles/**/*.gcno"))
    if not gcno_files:
        raise RuntimeError(f"no gcov data found below {build_dir}")

    with tempfile.TemporaryDirectory(prefix="vcontrold-gcov-") as tmp:
        output_dir = pathlib.Path(tmp)
        for index, gcno in enumerate(gcno_files):
            run_dir = output_dir / str(index)
            run_dir.mkdir()
            result = subprocess.run(
                ["gcov", "--json-format", "--branch-probabilities", str(gcno)],
                cwd=run_dir,
                stdout=subprocess.DEVNULL,
                stderr=subprocess.PIPE,
                text=True,
            )
            if result.returncode:
                raise RuntimeError(
                    f"gcov failed for {gcno}: {result.stderr.strip()}")
            for report in run_dir.glob("*.gcov.json.gz"):
                with gzip.open(report, "rt", encoding="utf-8") as stream:
                    data = json.load(stream)
                for source in data["files"]:
                    path = pathlib.Path(source["file"]).resolve()
                    try:
                        relative = path.relative_to(source_dir)
                    except ValueError:
                        continue
                    if relative.parent != pathlib.Path("src"):
                        continue
                    module = relative.stem
                    for line in source["lines"]:
                        line_key = (module, line["line_number"])
                        merged_lines[line_key] = max(
                            merged_lines.get(line_key, 0), line["count"])
                        for branch_index, branch in enumerate(
                                line.get("branches", [])
                        ):
                            branch_key = (
                                module, line["line_number"], branch_index)
                            merged_branches[branch_key] = max(
                                merged_branches.get(branch_key, 0),
                                branch["count"],
                            )
    return merged_lines, merged_branches


def ratio(covered, total):
    return 100.0 * covered / total if total else 100.0


def totals(entries, modules):
    selected = [
        count for key, count in entries.items() if key[0] in modules
    ]
    return sum(count > 0 for count in selected), len(selected)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("build_dir", type=pathlib.Path)
    parser.add_argument("--core-min", type=float, default=80.0)
    parser.add_argument("--module-min", type=float, default=65.0)
    parser.add_argument("--vcontrold-min", type=float, default=65.0)
    parser.add_argument("--overall-min", type=float, default=80.0)
    args = parser.parse_args()

    source_dir = pathlib.Path(__file__).resolve().parents[1]
    lines, branches = collect(args.build_dir.resolve(), source_dir)
    modules = sorted({key[0] for key in lines})
    failed = False

    print("module             lines              branches")
    print("------------------ ------------------ ------------------")
    for module in modules:
        line_covered, line_total = totals(lines, {module})
        branch_covered, branch_total = totals(branches, {module})
        print(
            f"{module:18} "
            f"{line_covered:4}/{line_total:<4} "
            f"{ratio(line_covered, line_total):6.2f}%    "
            f"{branch_covered:4}/{branch_total:<4} "
            f"{ratio(branch_covered, branch_total):6.2f}%"
        )
        if module in CORE_MODULES and ratio(line_covered, line_total) < args.module_min:
            print(
                f"ERROR: {module} is below the per-core-module "
                f"threshold of {args.module_min:.2f}%")
            failed = True
        if (
            module == "vcontrold"
            and ratio(line_covered, line_total) < args.vcontrold_min
        ):
            print(
                f"ERROR: vcontrold is below its threshold of "
                f"{args.vcontrold_min:.2f}%")
            failed = True

    core_covered, core_total = totals(lines, CORE_MODULES)
    core_branch_covered, core_branch_total = totals(branches, CORE_MODULES)
    overall_covered, overall_total = totals(lines, set(modules))
    overall_branch_covered, overall_branch_total = totals(
        branches, set(modules))
    print(
        f"\ncore aggregate: {core_covered}/{core_total} "
        f"({ratio(core_covered, core_total):.2f}%), branches "
        f"{core_branch_covered}/{core_branch_total} "
        f"({ratio(core_branch_covered, core_branch_total):.2f}%)")
    print(
        f"overall:        {overall_covered}/{overall_total} "
        f"({ratio(overall_covered, overall_total):.2f}%), branches "
        f"{overall_branch_covered}/{overall_branch_total} "
        f"({ratio(overall_branch_covered, overall_branch_total):.2f}%)")

    if ratio(core_covered, core_total) < args.core_min:
        print(f"ERROR: core aggregate is below {args.core_min:.2f}%")
        failed = True
    if ratio(overall_covered, overall_total) < args.overall_min:
        print(f"ERROR: overall coverage is below {args.overall_min:.2f}%")
        failed = True
    return 1 if failed else 0


if __name__ == "__main__":
    raise SystemExit(main())
