# AGENTS.md

## Purpose

This repository contains C assignments that must:

- Compile with `gcc` on Linux (C99, portable, no nonstandard extensions).
- Run under Linux via OrbStack from macOS.
- Pass strict `diff`-based tests from Themis.

Optimize for **clarity and correctness**, not cleverness or optimization.

## Critical Rules

Always:

- Compile with the same flags as the grader: 

  ```sh
  gcc -g -std=c99 -pedantic -Wall -o a.out *.c -lm
  ```

Note: the grader uses the '-O2' flag however use '-g' 

- Run programs via `orb` so behavior matches Linux:

  ```sh
  orb ./a.out
  ```

Note: orb requires elevated permissions, make to request higher privilege for executing orb commands

- Use Themis to fetch tests (on macOS, **not** via `orb`).
- Compare output with `diff -u` against `tests/*.out`.
- Prefer small, explicit, readable fixes over refactors.

Never:

- Change the test layout or filenames.
- Edit expected `.out` files to “fix” tests.
- Run `themis` inside `orb`.
- Rely on macOS-only C behavior.

## C Style

### Variable Naming

- Use **descriptive English snake_case** names.
- Make each name self-explanatory.
- Short names only when conventional (`i`, `j` in small local loops).

Examples:

- `current_line_length` instead of `len`.
- `number_of_students` instead of `n`.
- `is_input_valid` instead of `ok`.

### Implementation Style

- Prefer the **simplest correct solution**.
- Keep control flow direct and explicit.
- Do not compress logic into clever one-liners.
- Avoid premature generalization and abstraction.
- Use small functions with clear responsibilities.
- Write portable C99; avoid compiler-specific tricks.

## Repository Layout

Each assignment lives in its own directory:

```text
os-assignments/
  os-a-04/
    os-a04-e01/
      a04-e01.c
      tests/
        1.in
        1.out
        2.in
        2.out
        ...
```

Rules:

- Main C file (for example `a04-e01.c`) is at the assignment directory root.
- All tests live in `tests/` with matching names: `N.in` ↔ `N.out`.

## OrbStack (Linux) Usage

Run all C compilation and execution inside the Linux VM via `orb`.

Examples:

```sh
# Compile
orb gcc -O2 -std=c99 -pedantic -Wall -o a.out *.c -lm

# Run one test manually (stdin)
orb sh -c './a.out < tests/1.in'

# Run one test manually (argv)
orb sh -c './a.out tests/1.in'
```

Do **not** use `orb` for `themis`.

## Themis Configuration

Base course URL (used for discovery):

```text
THEMIS_REPO_BASE_URL=https://themis.housing.rug.nl/course/2025-2026/os/
```

Update this if the repository targets a different Themis course.

### Discovering Assignments (Optional)

Use Themis on macOS, not via `orb`:

```sh
themis list \
  --discover \
  --root-url "$THEMIS_REPO_BASE_URL" \
  --discover-depth 8
```

JSON mode (better for agents):

```sh
themis list \
  --discover \
  --root-url "$THEMIS_REPO_BASE_URL" \
  --discover-depth 8 \
  --json
```


Note: `themis` accesses the internet which requires elevated permissions, make to request higher privilege for executing `themis` commands

### Listing and Fetching Tests

List available tests:

```sh
themis list \
  --tests-url "https://themis.housing.rug.nl/file/2025-2026/os/practicals/practical5/5_priority_preemption/%40tests/1.in" \
  --start 1 --max 200 --max-misses 5
```

Fetch tests into `tests/` (default output is `./tests`):

```sh
themis fetch \
  --tests-url "https://themis.housing.rug.nl/file/2025-2026/os/practicals/practical5/5_priority_preemption/%40tests/1.in" \
  --out tests
```

Authentication order:

1. `--cookie-file`
2. `--cookie-env`
3. `~/.config/themis/cookie.txt` (default)

## Testing

The grader does a strict `diff` between your program output and the expected `.out` file.  
Every character (including spaces and capitalization) must match exactly.

Example for one test:

```sh
orb sh -c './a.out < tests/1.in | diff -u tests/1.out -'
```

Two-step variant:

```sh
orb sh -c './a.out < tests/1.in > actual.out && diff -u tests/1.out actual.out'
```

### Recommended Test Loop

Run all numbered tests:

```sh
orb sh -c '
gcc -O2 -std=c99 -pedantic -Wall -o a.out *.c -lm &&
for input_file in tests/*.in; do
    test_name=$(basename "$input_file" .in)
    expected_output_file="tests/${test_name}.out"

    echo "Running test ${test_name}"

    if ./a.out < "$input_file" | diff -u "$expected_output_file" -; then
        echo "Test ${test_name}: PASS"
    else
        echo "Test ${test_name}: FAIL"
    fi
done
'
```

## Agent Behavior

When modifying or generating code in this repo:

- Compile with the grader’s `gcc` flags.
- Run C code via `orb` so behavior matches Linux.
- Fetch tests with `themis` on macOS, never via `orb`.
- Validate against files in `tests/` using `diff -u`.
- Ensure output matches expected `.out` **exactly**.
- Prefer minimal, explicit changes over large refactors.
- Keep names and logic clear and descriptive.

## Teaching and Scaffolding Mode

When the user explicitly asks to **learn** (for example “teach me”, “explain this”, “walk me through it”):

- Optimize for learning over speed.
- Explain step by step in short paragraphs.
- Use small, focused helper functions and `struct` definitions.
- Implement I/O helpers; leave main control flow for the user.

In scaffolding mode, **do not**:

- Implement `main` fully.
- Implement full algorithms or full test loops unless the user asks.
- Hide key logic inside large helper abstractions.

Instead:

- Provide clear input-parsing and output-printing helpers.
- Use `TODO` comments to mark where the user should complete logic.
- Keep code compilable with simple, safe defaults in stubs.
