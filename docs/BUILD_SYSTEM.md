# Build System and CI/CD

This document describes the build system and continuous integration/deployment setup for encode-orc.

## Overview

The project uses CMake for building and CPack for packaging. GitHub Actions workflows handle continuous integration and release deployment for Windows, macOS, and Linux (Flatpak).

## Build System Architecture

### CMake Configuration

The build system is configured to work across multiple platforms:

- **Windows**: Uses vcpkg for dependency management, generates MSI installer with WiX
- **macOS**: Uses Homebrew for dependencies, creates DMG installer
- **Linux**: Uses system packages, creates Flatpak bundle

### Dependencies

Required dependencies:
- CMake 3.15+
- C++17 compiler
- SQLite3
- yaml-cpp
- libpng
- spdlog

## GitHub Actions Workflows

### 1. CI Workflow (`.github/workflows/ci.yml`)

Runs on every push to `main`/`develop` and on pull requests.

**Jobs:**
- **build-linux** (Fedora): Builds and tests on Fedora container
- **build-ubuntu**: Builds and tests on Ubuntu
- **build-macos**: Builds and tests on macOS
- **build-windows**: Builds and tests on Windows with vcpkg
- **code-quality**: Runs code formatting checks
- **flatpak**: Builds Flatpak bundle
- **dev-packages**: Creates development packages (only on push to main/develop)

### 2. Package Builds Workflow (`.github/workflows/package-builds.yml`)

Reusable workflow that builds release packages for all platforms.

**Jobs:**
- **build-windows-package**: Creates MSI installer for Windows
- **build-macos-package**: Creates DMG installer for macOS
- **build-flatpak-package**: Creates Flatpak bundle for Linux

**Inputs:**
- `version`: Version string for package names (default: 'dev')

### 3. Release Workflow (`.github/workflows/release.yml`)

Triggered when a version tag (e.g., `v1.0.0`) is pushed.

**Process:**
1. Calls `package-builds.yml` with version from git tag
2. Downloads all generated packages
3. Creates GitHub Release with MSI, DMG, and Flatpak attachments

## Building Locally

### Linux (Ubuntu/Debian)

```bash
sudo apt-get install cmake build-essential libsqlite3-dev libspdlog-dev libyaml-cpp-dev libpng-dev
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
```

### macOS

```bash
brew install cmake pkg-config spdlog sqlite yaml-cpp libpng
cmake -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="$(brew --prefix)"
cmake --build build -j$(sysctl -n hw.ncpu)
```

### Windows (with vcpkg)

```powershell
vcpkg install spdlog sqlite3 yaml-cpp libpng --triplet x64-windows
cmake -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake
cmake --build build --config Release
```

## Creating Release Packages

### Windows MSI

The MSI installer is generated using CPack with the WiX Toolset:

```powershell
cd build
cpack -C Release -G WIX
```

**Requirements:**
- WiX Toolset 3.14+
- LICENSE.rtf file (created from LICENSE by GitHub Actions)

### macOS DMG

The DMG is created using `create-dmg`:

```bash
brew install create-dmg
# Build the application
cmake --build build
# Create DMG staging directory
mkdir -p dmg-staging/bin
cp build/encode-orc dmg-staging/bin/
create-dmg --volname "Encode Orc" encode-orc.dmg dmg-staging
```

### Linux Flatpak

Flatpak bundles are built using `flatpak-builder`:

```bash
flatpak-builder --force-clean build-flatpak io.github.simoninns.encode-orc.yml
flatpak build-bundle ~/.local/share/flatpak/repo encode-orc.flatpak io.github.simoninns.encode-orc
```

## Integration with decode-orc

When encode-orc is included as a submodule in decode-orc, the following considerations apply:

### Build Integration

decode-orc's CMakeLists.txt includes encode-orc as a subdirectory:

```cmake
add_subdirectory(external/encode-orc)
```

### Dependency Alignment

Both projects use:
- Same C++ standard (C++17)
- Same dependency versions (spdlog, yaml-cpp, SQLite)
- Compatible CMake configurations

### Package Dependencies

The GitHub Actions workflows ensure that:
1. All platforms build successfully before packages are created
2. Dependencies are properly bundled in release packages
3. No runtime dependency issues exist

## Troubleshooting

### Windows Build Issues

**Problem**: Missing DLLs in MSI package

**Solution**: Ensure vcpkg is installed and dependencies are in `C:/vcpkg/installed/x64-windows/bin/`. The install script in CMakeLists.txt copies these automatically.

### macOS Build Issues

**Problem**: Library not found errors

**Solution**: Set CMAKE_PREFIX_PATH to Homebrew prefix:
```bash
export HOMEBREW_PREFIX=$(brew --prefix)
cmake -B build -DCMAKE_PREFIX_PATH="$HOMEBREW_PREFIX"
```

### Flatpak Build Issues

**Problem**: Module not found

**Solution**: Ensure all dependencies are listed in `io.github.simoninns.encode-orc.yml` with correct URLs and SHA256 hashes.

## Continuous Deployment

### Release Process

1. Ensure all changes are committed and pushed
2. Create and push a version tag:
   ```bash
   git tag -a v1.0.0 -m "Release version 1.0.0"
   git push origin v1.0.0
   ```
3. GitHub Actions automatically:
   - Builds packages for all platforms
   - Creates a GitHub Release
   - Attaches MSI, DMG, and Flatpak files

### Development Builds

Development builds are automatically created on push to `main` or `develop` branches. These are available as GitHub Actions artifacts for 7 days.

## Package Naming Convention

- **Release builds**: `encode-orc-v1.0.0-macOS.dmg`, `encode-orc-v1.0.0.msi`, etc.
- **Development builds**: `encode-orc-macOS.dmg`, `encode-orc-windows.msi`, etc.

## References

- CMake Documentation: https://cmake.org/documentation/
- CPack Documentation: https://cmake.org/cmake/help/latest/module/CPack.html
- WiX Toolset: https://wixtoolset.org/
- Flatpak Builder: https://docs.flatpak.org/en/latest/flatpak-builder.html
- GitHub Actions: https://docs.github.com/en/actions
