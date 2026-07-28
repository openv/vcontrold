# Local quality analysis

Install the analysis tools on Ubuntu:

```sh
sudo apt-get update
sudo apt-get install clang-tidy clang-tools cppcheck valgrind
```

Run the complete local quality suite from the repository root:

```sh
sh tools/run-quality.sh
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
