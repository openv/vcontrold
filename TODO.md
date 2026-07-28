# vcontrold hardening TODO

This document records the findings from the full repository scan performed on
2026-07-28. It covers the daemon, client, simulator, protocol framing, XML
configuration, build system, tests, CI, documentation, and example files.

Production configuration and binaries were not changed as part of this scan.
No heating-system set command was executed.

## Finding states

- **Reproduced:** Triggered in an isolated ASan/UBSan test.
- **Confirmed by inspection:** The unsafe path is reachable and follows
  directly from the code.
- **Hardening:** Not currently known to cause a production failure, but should
  be improved for maintainability, portability, or defense in depth.
- **Done:** A fix and an appropriate regression test have been completed.

An item must not be marked done merely because the implementation changed. A
regression test and the relevant build checks must pass first.

## P0 — Memory-safety and daemon-availability issues

### [ ] Reject or safely ignore empty TCP command lines

- State: **Reproduced**
- Area: `src/vcontrold.c`, `interactive()`
- Problem: Control-character removal starts at `readBuf + rcount` and walks
  backwards without checking the beginning of the buffer. A line containing
  only `\n` walks before `readBuf`.
- Impact: Stack-buffer-underflow and termination of the connection worker.
  This was reproduced with AddressSanitizer by sending an empty line to an
  isolated daemon on port 33002.
- Required fix:
  - Strip CR/LF using a bounded length operation.
  - Handle an empty command as a no-op or return the normal prompt.
  - Never decrement a pointer below `readBuf`.
- Required tests:
  - Empty LF line.
  - Empty CRLF line.
  - Line containing only whitespace.
  - Normal command after an empty line.
  - EOF without a preceding command.

### [ ] Bound result accumulation in `readCmdFile()`

- State: **Confirmed by inspection**
- Area: `src/vcontrold.c`, `readCmdFile()` and `rawModus()`
- Problem: The caller passes the result buffer capacity through `resultLen`,
  but `readCmdFile()` immediately resets it to zero and then appends bytes
  without enforcing a capacity.
- Impact: Multiple command results can overflow the 4096-byte result buffer.
  The command-file interface and remotely accessible raw mode are affected.
- Additional defects in the same path:
  - `result = "\0"` only changes the local pointer and does not clear the
    caller's buffer.
  - Failure of `vcontrol_semget()` is ignored.
  - Negative command results are not handled explicitly.
- Required fix:
  - Pass capacity and used length separately.
  - Reject accumulation that would exceed capacity.
  - Always terminate textual output where applicable.
  - Propagate lock, device, parsing, and execution failures.
- Required tests:
  - Result exactly filling the buffer.
  - Result one byte too large.
  - Multiple commands whose combined output is too large.
  - Failed semaphore acquisition.
  - Failed command execution.

### [ ] Replace the fixed 100-byte receive dump buffer

- State: **Confirmed by inspection**
- Area: `src/io.c`, `dump()` and `receive_nb()`
- Problem: `dump()` uses unbounded `sprintf()` calls into a 100-byte local
  buffer. Each input byte needs approximately three output characters, while
  receive paths can request far more than about 30 bytes.
- Impact: Stack-buffer-overflow while formatting diagnostic output.
- Required fix:
  - Give `dump()` the destination size and use bounded formatting, or stream
    the dump directly to the logger.
  - Truncate explicitly with a visible marker if a diagnostic limit is used.
- Required tests:
  - Zero, one, 30, 31, 255, and maximum supported receive lengths.
  - Debug logging enabled and disabled.

### [ ] Validate the complete minimum P300 frame before field access

- State: **Confirmed by inspection**
- Area: `src/framer.c`, `framer_receive()`
- Problem: The length byte is trusted sufficiently to calculate a total frame
  length, but the code can access function, address, response-length, error,
  and checksum fields before proving that the frame is large enough to contain
  them.
- Impact: Out-of-bounds or uninitialized stack reads on malformed, truncated,
  or hostile serial/simulator responses. Invalid frames may also be interpreted
  incorrectly.
- Required fix:
  - Define minimum sizes for normal responses, error responses, reads, and
    writes.
  - Validate payload and total lengths before every fixed-offset access.
  - Validate response type and function before interpreting the payload.
  - Check the received byte count before checksum or payload processing.
- Required tests:
  - Every truncated frame length from zero through the minimum valid size.
  - Oversized frame.
  - Invalid type and function.
  - Error report without an error-code byte.
  - Declared length differing from received length.
  - Bad checksum and mismatched address.

### [ ] Make configuration reload fully transactional

- State: **Confirmed by inspection**
- Areas: `src/vcontrold.c`, `src/xmlconfig.c`, `src/parser.c`
- Problem:
  - Parsing can replace the active global configuration before command
    expansion and bytecode compilation are known to succeed.
  - `compileCommand()` ignores failures from `expand()` and
    `buildByteCode()`.
  - Some expansion errors call `exit(3)` and can terminate a worker or daemon.
- Impact: A syntactically valid but semantically invalid reload can leave an
  incomplete active configuration or terminate the service.
- Required fix:
  - Parse, validate, expand, and compile into an isolated configuration object.
  - Swap active configuration only after every stage succeeds.
  - Return structured errors instead of calling `exit()` in library paths.
  - Keep the previous configuration untouched on any failure.
  - Define which runtime settings can change on reload. Port, listener,
    logfile, user, group, and device changes must either be applied safely or
    reported as requiring a restart.
- Required tests:
  - Successful reload.
  - XML syntax failure.
  - Missing protocol command.
  - Unknown macro or unit.
  - Oversized expansion.
  - Bytecode compilation failure.
  - Verify the old configuration still answers after every failed reload.

## P1 — Client, parser, simulator, and XML correctness

### [ ] Reject empty `vclient` command lists

- State: **Reproduced**
- Area: `src/client.c`, `sendCmds()`
- Reproducer: `vclient -h 127.0.0.1 -p 3002 -c ','`
- Problem: `strtok()` returns `NULL`, but the `do` loop passes it to
  `strlen()`.
- Impact: Client-side NULL dereference. UBSan reproduced the failure.
- Required fix:
  - Validate the token before creating a result node.
  - Define behavior for empty items such as `a,,b`, leading commas, and
    trailing commas.
- Required tests:
  - Empty string, comma only, repeated commas, leading/trailing comma, and a
    normal list.

### [ ] Make command-file line handling lossless and bounded

- State: **Confirmed by inspection**
- Areas: `src/vcontrold.c`, `readCmdFile()`; `src/client.c`, `sendCmdFile()`
- Problems:
  - Daemon command files always remove the final character, even when the last
    line has no newline.
  - Overlong physical lines are split and interpreted as multiple commands.
  - Client command transmission silently truncates individual commands to its
    fixed request buffer.
- Required fix:
  - Strip only actual CR/LF characters.
  - Detect and reject overlong lines.
  - Reject commands that cannot be transmitted completely.
- Required tests:
  - Last line with and without newline.
  - CRLF input.
  - Empty lines.
  - Maximum-length and overlong lines.

### [ ] Guarantee termination and validate lengths in `parseLine()`

- State: **Confirmed by inspection**
- Area: `src/parser.c`, `parseLine()`
- Problem: `strncpy()` can fill the complete Unit destination without adding a
  null byte. Numeric values are parsed with `atoi()` and do not receive full
  syntax or range validation.
- Required fix:
  - Use an explicitly bounded copy with guaranteed termination.
  - Parse lengths and pauses with checked conversion and defined ranges.
  - Reject negative and overflowing values.
- Required tests:
  - Unit names below, at, and above the limit.
  - Negative, nonnumeric, zero, maximum, and overflowing lengths.

### [ ] Prevent `vsim` input-buffer overflow

- State: **Confirmed by inspection**
- Area: `src/vsim.c`, global `input`/`inpidx` and `handle()`
- Problem:
  - `inpidx` is a plain `char` and is used as an array index.
  - Unknown or partially matching streams can grow indefinitely without reset.
- Impact: Out-of-bounds writes and eventually negative indexing in the optional
  simulator.
- Required fix:
  - Use `size_t` for lengths and indices.
  - Enforce the input capacity.
  - Reset or resynchronize when no command can still match the prefix.
- Required tests:
  - Every known command.
  - Unknown prefix.
  - Valid prefix followed by invalid data.
  - More than 100 unmatched bytes.

### [ ] Handle empty and missing XML text safely

- State: **Confirmed by inspection**
- Area: `src/xmlconfig.c`, especially `parseConfig()`
- Problem: Empty elements such as `<readonly/>`, `<debug/>`, and `<syslog/>`
  can make `getTextNode()` return `NULL`, followed by dereferencing
  `*chrPtr`.
- Required fix:
  - Centralize required/optional text extraction.
  - Report the XML element and line number on invalid content.
  - Define accepted boolean values and reject everything else.
- Required tests:
  - Missing, empty, valid, and invalid boolean fields.
  - Whitespace-only content.

### [ ] Use exact XML element-name comparisons

- State: **Confirmed by inspection**
- Area: `src/xmlconfig.c`
- Problem: Many parser branches use `strstr(node->name, "expected")`.
  Unrelated names containing the substring can be accepted as real
  configuration fields.
- Required fix:
  - Use `xmlStrEqual()` or exact `strcmp()` consistently.
  - Reject unknown elements at the appropriate nesting level with a useful
    diagnostic.
- Required tests:
  - Expected names.
  - Prefixes and suffixes such as `notreadonly`, `serialBackup`, and
    `mycommands`.
  - Unknown elements.

### [ ] Clean up every failed XML parse and XInclude path

- State: **Confirmed by inspection**
- Area: `src/xmlconfig.c`, `parseXMLFile()` and subordinate parsers
- Problem: Numerous early returns do not free the XML document or partially
  constructed temporary protocol, unit, device, command, and config lists.
- Impact: Repeated failed reloads can leak memory.
- Required fix:
  - Use one failure-cleanup path.
  - Add cleanup helpers for temporary parse state.
  - Ensure XInclude errors free the document.
- Required tests:
  - Repeatedly reload each class of malformed configuration under LeakSanitizer.

### [ ] Check every allocation and remove recursive list operations

- State: **Hardening**
- Area: Primarily `src/xmlconfig.c`; also template and parser paths
- Problem:
  - Many `calloc()` results are used immediately without checking.
  - Linked-list append, lookup, and destruction functions recurse and can
    exhaust the stack with very large configurations.
- Required fix:
  - Introduce checked string duplication/allocation helpers.
  - Propagate allocation failures instead of exiting from library helpers.
  - Convert list traversal and deletion to iterative implementations.
- Required tests:
  - Allocation-failure injection.
  - Large generated XML lists.

### [ ] Fix integer-expression numeric conversion

- State: **Confirmed by compiler diagnostics**
- Area: `src/arithmetic.c`, `execIFactor()`
- Problem: Decimal text is parsed using `atof()` and implicitly converted to
  `int`. Fractional, overflowing, NaN, and infinite results are not handled
  deliberately.
- Required fix:
  - Decide whether integer expressions permit fractional literals.
  - Use checked `strtol()` for integers or explicit finite/range-checked
    conversion if fractions are intentional.
  - Change token variables from `char` to `int` to match `nextToken()`.
- Required tests:
  - Integer, fraction, negative, overflow, malformed, NaN, and infinity cases.

### [ ] Produce valid escaped JSON from `vclient`

- State: **Confirmed by inspection**
- Area: `src/vclient.c`, short and long JSON output
- Problem: Command, raw, and error strings are written directly inside JSON
  strings without escaping quotes, backslashes, or control characters.
- Impact: Invalid JSON and possible downstream parsing/injection problems.
- Required fix:
  - Add one JSON string encoder.
  - Represent failed or nonnumeric values consistently, preferably with
    `null` plus an error field.
- Required tests:
  - Quotes, backslashes, tabs, CR/LF, control bytes, UTF-8, and server errors.
  - Parse generated output with a real JSON parser.

### [ ] Make `vclient` template indexing size-safe

- State: **Confirmed by inspection**
- Area: `src/vclient.c`, template output
- Problems:
  - Result count and indices use `unsigned short`.
  - `calloc()` is not checked.
  - Large numeric variable names are parsed with `atoi()` and narrowed.
- Required fix:
  - Use `size_t`.
  - Use checked numeric parsing.
  - Check allocation and define an upper bound if appropriate.
- Required tests:
  - Zero results.
  - Boundary indices.
  - Invalid and overflowing variables.
  - A result count above 65535 in an isolated mock test.

## P2 — I/O, lifecycle, and API hardening

### [ ] Check all TTY configuration operations

- State: **Hardening**
- Area: `src/io.c`, `opentty()`
- Problem: `tcsetattr()`, `tcflush()`, and some `ioctl()` calls are unchecked.
  Several failures call `exit()` from low-level I/O code rather than returning
  an error.
- Required fix:
  - Check every operation.
  - Preserve and report `errno`.
  - Return failures to the daemon so locks and descriptors can be cleaned up.

### [ ] Avoid changing descriptor blocking mode repeatedly

- State: **Hardening**
- Area: `src/io.c`, `receive_nb()`; `src/client.c`, `sendServer()`
- Problem: Blocking flags are toggled around each operation, and restoration
  failures are ignored. `sendServer()` also drains input opportunistically,
  which can discard legitimate protocol data if state assumptions change.
- Required fix:
  - Prefer `poll()`/`select()` without changing persistent descriptor flags.
  - Check flag restoration.
  - Document and test the input-drain protocol.

### [ ] Make socket helper return semantics consistent

- State: **Hardening**
- Areas: `src/socket.c`, `src/client.c`
- Problems:
  - Some wrappers return zero for errors even though zero can be a valid file
    descriptor or EOF.
  - `openCliSocket()` exits on some resolution errors but returns `-1` on
    connection errors.
  - Port parsing uses unchecked `atoi()`.
- Required fix:
  - Return `-1` with `errno`/diagnostic for failures.
  - Never call `exit()` from reusable socket helpers.
  - Validate port range.

### [ ] Make logger and I/O APIs const-correct

- State: **Hardening**
- Areas: `src/common.*`, `src/socket.*`, `src/io.*`, callers
- Problem: Format strings and write buffers are declared mutable, generating a
  large share of strict-build warnings and hiding more useful diagnostics.
- Required fix:
  - `logIT(int, const char *, ...)`
  - `Writen(int, const void *, size_t)`
  - Apply `const` to input-only strings and buffers throughout.

### [ ] Replace narrow length types

- State: **Hardening**
- Area: Repository-wide
- Problem: Buffer lengths and byte counts mix `char`, `short`, `int`,
  `ssize_t`, and `size_t`, producing potentially lossy conversions.
- Required fix:
  - Use `size_t` for capacities and in-memory lengths.
  - Use `ssize_t` for read/write results.
  - Use fixed-width unsigned types for protocol fields.
  - Validate before narrowing to protocol bytes.

### [ ] Audit cleanup and descriptor ownership

- State: **Hardening**
- Areas: daemon, client, raw mode, PID file, semaphore, socket helpers
- Points to address:
  - Avoid calling device-close logic with `fd == -1`.
  - Ensure all early returns release the semaphore exactly once.
  - Close/free partial client result lists on communication or allocation
    failures.
  - Do not leave stale PID files or temporary simulator/raw files.
  - Check `semctl()`, `chmod()`, `close()`, and `unlink()` where failure matters.

### [ ] Define access control for administrative TCP commands

- State: **Hardening / deployment decision**
- Area: `src/vcontrold.c`, TCP command interface
- Problem: `reload`, `debug on`, and—when readonly is disabled—`raw` and set
  commands are available to every host that can reach the listener. There is
  no protocol authentication.
- Required decision:
  - Keep the trusted-LAN model and document it prominently, or
  - Add address restrictions/authentication, or
  - Split administrative/write access onto a local Unix socket.
- Note: This is not currently treated as an unexpected defect in the user's
  trusted home-LAN deployment, but it must remain explicit.

## Tests to add

### [ ] TCP command parser integration test

- Start an isolated daemon with a fake device.
- Exercise empty, whitespace, malformed, maximum-length, overlong, and normal
  commands.
- Verify prompts, errors, connection survival, and clean shutdown.

### [ ] Mock transport for protocol tests

- Add a socket-pair or scripted fake transport.
- Test partial reads/writes, EOF, EINTR, timeouts, malformed frames, bad
  checksums, incorrect addresses, and reconnect/reinitialization behavior.

### [ ] XML parser and reload test suite

- Cover every required and optional element.
- Cover invalid numeric ranges, booleans, enums, macros, units, protocols, and
  includes.
- Verify rollback after failed reload.
- Run repeated failures under LeakSanitizer.

### [ ] Client output test suite

- Plain, CSV, Munin, Cacti, short JSON, long JSON, and template output.
- Mixed successful and failed server responses.
- Very long command lists and responses.
- Verify documented exit codes.

### [ ] Raw/command-file tests

- Capacity accounting.
- Last line without newline.
- Overlong lines.
- Invalid bytecode.
- Readonly enforcement.
- Confirm that tests never issue writes to real hardware.

### [ ] Simulator tests

- Known command/response pairs.
- Invalid and excessively long streams.
- Disconnect/reconnect and SIGPIPE behavior.

### [ ] Allocation-failure tests

- Exercise parsing, compilation, client result creation, and output indexing
  with injected allocation failures.
- Verify clean errors without leaks or partial global state.

## Build and CI

### [ ] Run CTest in normal CI

- Current build workflows compile the project but do not execute `ctest`.
- Configure with `BUILD_TESTING=ON`.
- Run `ctest --output-on-failure` for supported Linux targets.

### [ ] Add a strict compiler job

- Start with:
  - `-Wall`
  - `-Wextra`
  - `-Wpedantic`
  - `-Wformat=2`
  - `-Wshadow`
  - `-Wconversion`
  - `-Wsign-conversion`
- Establish a warning baseline and move toward `-Werror`.
- The full scan produced 764 diagnostics with the broad strict set. Many are
  duplicate const/conversion warnings across targets, but real defects were
  found among them.

### [ ] Add sanitizer CI

- Build and test with AddressSanitizer and UndefinedBehaviorSanitizer.
- Add a separate LeakSanitizer-enabled XML/reload run.
- Include `VSIM=ON` so the optional simulator is not excluded.

### [ ] Add static-analysis CI

- Keep CodeQL, but ensure its workflow and action versions are supported.
- Add GCC `-fanalyzer`, Clang Static Analyzer, or clang-tidy where practical.
- GCC's analyzer did not report an actionable defect in this scan, but it
  bailed out on many complex XML-list paths; this is not proof that those paths
  are safe.

### [ ] Update GitHub Actions

- Several workflows still use old major versions such as
  `actions/checkout@v3`, `actions/upload-artifact@v3`, and CodeQL v2 actions.
- Pin supported versions, preferably to reviewed commit SHAs where supply-chain
  policy requires it.
- Ensure the release and cross-platform workflows still run after updates.

### [ ] Make CI workflows deterministic and safe

- Remove unnecessary `git pull` from the test workflow.
- Quote shell variables and paths.
- Avoid printing the complete environment.
- Ensure dependency installation runs after `apt-get update`.
- Validate generated packages and archives in CI.

### [ ] Add formatting and repository checks

- `git diff --check`
- `xmllint --noout` for all tracked XML files.
- Shell syntax or ShellCheck for examples.
- Optional C formatting check once a project style is selected.

## Build-system and packaging cleanup

### [ ] Use target-scoped CMake properties

- Replace global `include_directories()`, `add_definitions()`, and global
  `CMAKE_C_FLAGS` manipulation with `target_include_directories()`,
  `target_compile_definitions()`, and `target_compile_options()`.
- Declare required C language standard and extensions explicitly.

### [ ] Review install-prefix behavior

- CMake overrides its normal default prefix from `/usr/local` to `/usr`.
- Decide whether this is intentional for all direct source builds.
- Document the behavior and keep service/example paths consistent.

### [ ] Install or package runtime assets consistently

- Decide whether example configuration, systemd unit, init script, and
  documentation should be installed by CMake or only packaged separately.
- Ensure binary paths in service/init examples match the selected prefix.

### [ ] Improve feature-option coverage

- Test combinations of:
  - `VCLIENT=ON/OFF`
  - `VSIM=ON/OFF`
  - `MANPAGES=ON/OFF`
  - `BUILD_TESTING=ON/OFF`
- Check Linux, Raspberry Pi/ARM, Cygwin, and ideally another POSIX platform.

## Documentation and examples

### [ ] Document trusted-network and write-access implications

- Explain listener exposure, lack of authentication, readonly behavior, raw
  mode, set commands, and recommended network isolation.

### [ ] Document exit codes and output contracts

- Include the new `vclient` server-error exit code.
- Define output behavior for partial success and each output format.
- Document maximum command/configuration sizes once limits are formalized.

### [ ] Repair or clearly mark obsolete examples

- `doc/examples/vupdate.sh` has an invalid shebang (`#/bin/sh`).
- Several RRD scripts have no shebang and contain hard-coded historical paths.
- The init script refers to `/usr/local/sbin` while CMake defaults to `/usr`.
- Generated/template examples should be distinguished from directly usable
  scripts.

### [ ] Review spelling and terminology

- Examples include `deamon`, `aquire`, `Socker`, and mixed German/English
  diagnostics.
- Standardize user-facing messages without changing protocol output
  compatibility unintentionally.

## Verification baseline from the 2026-07-28 scan

The following checks completed successfully before this TODO was created:

- Normal CMake build.
- Build with `VSIM=ON`.
- Existing CTest suite: 2 of 2 tests passed.
- AddressSanitizer/UndefinedBehaviorSanitizer build and existing tests.
- GCC `-fanalyzer` build; no actionable analyzer diagnostic, but many
  `too-complex` analysis bailouts in XML cleanup paths.
- All tracked XML files passed `xmllint --noout`.
- All tracked `*.sh` files passed `bash -n`.
- The repository working tree was clean before adding this file.
- The production daemon remained active with zero systemd restarts during the
  isolated crash reproductions.

Reproductions performed during the scan:

1. Empty TCP line:
   - Isolated ASan daemon on port 33002.
   - Result: stack-buffer-overflow/underflow report in
     `src/vcontrold.c`, `interactive()`.
2. Empty comma-separated client command:
   - Sanitized `vclient -c ','` against the local daemon.
   - Result: runtime NULL-pointer diagnostic in `src/client.c`,
     `sendCmds()`.

## Suggested implementation order

1. Add failing regression tests for the empty TCP line and empty client list.
2. Fix those two reproduced crashes.
3. Add bounded dump and command-file result tests, then fix both overflows.
4. Add malformed-frame tests and harden P300 parsing.
5. Refactor configuration loading into a transactional parse/compile/swap.
6. Harden XML cleanup, exact element matching, and empty-value handling.
7. Fix simulator, JSON, template, and arithmetic issues.
8. Normalize length types, const correctness, cleanup, and I/O return handling.
9. Enable the expanded test suite, strict warnings, sanitizers, and static
   analysis in CI.
10. Update packaging, service examples, and security documentation.
