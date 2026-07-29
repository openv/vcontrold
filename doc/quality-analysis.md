# Local quality analysis

Install the analysis tools on Ubuntu:

```sh
sudo apt-get update
sudo apt-get install clang-tidy clang-tools cppcheck valgrind \
    libxml2-utils shellcheck python3-docutils
```

Run the complete local quality suite from the repository root:

```sh
sh tools/run-quality.sh
sh tools/run-build-matrix.sh
```

The script creates `build-quality`, builds and tests with Clang, and then runs:

- Cppcheck warning, performance, and portability checks;
- clang-tidy's low-noise core correctness checks;
- the Clang Static Analyzer through `scan-build`;
- Valgrind against every native unit-test executable.

No automatic analyzer fixes are applied. A finding must either be corrected or
suppressed narrowly with a documented reason. The hardware-free integration
tests remain part of CTest, while Valgrind intentionally targets native unit
tests so it does not report allocations owned by the Python interpreter.

The build matrix covers minimal, client-only, simulator, full test and
manpage-enabled configurations. CI additionally enforces strict warnings,
ASan/UBSan, line-coverage thresholds, GCC ``-fanalyzer``, XML validation,
ShellCheck and patch-whitespace checks.
