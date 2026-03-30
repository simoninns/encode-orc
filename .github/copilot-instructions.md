# encode-orc Workspace Instructions

## Environment

- This repository is developed inside a Nix environment defined by `flake.nix`.
- Before building, testing, or running project tooling, prefer `nix develop`.
- If an extra tool is needed for a task, do not assume it is globally installed and do not install it with `apt`, `pip`, or other system package managers.
- Instead, use `nix shell` for one-off tools, or update `flake.nix` when the tool should become part of the normal development environment.

## Build

- The project uses CMake and builds a single `encode-orc` executable.
- Standard local build flow:

```sh
nix develop
cmake -S . -B build
cmake --build build
```

- The standalone binary is expected at `build/encode-orc`.
- The build requires C++17 and links against SQLite3, yaml-cpp, libpng, spdlog, Threads, and FFmpeg.
- `flake.nix` already provides the main native build inputs and runtime dependencies for Linux and Darwin.

## Test Suite

- CTest is enabled in `CMakeLists.txt`.
- The automated test suite is driven by YAML project files in `test-projects/`.
- Each YAML file there is registered as a CTest case that runs `encode-orc <yaml> --quiet` from the repository root.
- Generated test outputs are written under `test-output/`.
- Preferred local test flow:

```sh
nix develop
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

- When investigating failures, inspect the matching YAML in `test-projects/` and the generated files in `test-output/`.
- Do not rewrite or delete expected fixture outputs unless the task is explicitly about updating them.

## GGV Projects

- GGV fixture projects live in `ggv-tests/`.
- These are separate from the main CTest suite and are used to generate known outputs into `ggv-output/`.
- Use `encode-ggv.sh` to run all GGV fixture encodes after the project has been built.
- Normal flow:

```sh
nix develop
cmake -S . -B build
cmake --build build
./encode-ggv.sh
```

- `encode-ggv.sh` expects the executable at `build/encode-orc`.
- The script writes outputs to `ggv-output/` by default and can be redirected via its first argument or `ENCODE_ORC_OUTPUT_ROOT`.
- The existing GGV fixtures cover PAL, PAL-M, and NTSC variants.
- Treat files in `ggv-tests/` and `ggv-output/` as fixtures unless the task specifically requires updating them.

## Documentation

- Documentation is built with MkDocs using `mkdocs.yml` and the content under `docs/`.
- Python package requirements for the docs are listed in `requirements.txt`, but in this repository the preferred way to access tooling is through Nix.
- For one-off docs work, use `nix shell` to provide MkDocs tooling instead of installing packages globally.
- Example local docs workflow:

```sh
nix shell \
  nixpkgs#python3Packages.mkdocs \
  nixpkgs#python3Packages.mkdocs-material \
  nixpkgs#python3Packages.mkdocs-awesome-nav
mkdocs serve
```

- Use `mkdocs build` for a static docs build.
- Keep documentation changes aligned with the actual repository layout and command set.

## Working Style

- Prefer repository-local commands and paths over global assumptions.
- Preserve existing generated outputs unless the task is explicitly to regenerate fixtures.
- When adding recurring tooling, prefer updating `flake.nix` instead of relying on ad hoc installation steps.