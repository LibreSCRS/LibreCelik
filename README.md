# LibreCelik

LibreCelik (Слободни Челик) is a free and open-source smart card reader for documents issued by the Republic of Serbia. It supports:

- Electronic ID cards (eID) — citizen and foreigner cards
- Vehicle registration documents
- Health insurance cards (RFZO)
- PKS qualified signature cards (Chamber of Commerce)

The application communicates directly with smart cards via PC/SC APDU commands, without relying on proprietary libraries.

## Downloads

Pre-built packages are available on the [Releases](https://github.com/LibreSCRS/LibreCelik/releases) page:

- `LibreCelik-<version>-x86_64.AppImage` — Linux (x86_64)
- `LibreCelik-<version>-macos.dmg` — macOS (Apple Silicon + Intel)

The PKCS#11 module for Firefox/browser integration is released separately from [LibreMiddleware Releases](https://github.com/LibreSCRS/LibreMiddleware/releases).

## Prerequisites

- **CMake** 3.24+
- **Qt 6** (Widgets, PrintSupport, LinguistTools)
- **PC/SC** library (`pcsclite` on Linux, built-in on macOS)
- **C++20** compiler
- **OpenSSL 3** (bundled as static library in `thirdparty/openssl-3.5.5/`)

### macOS

```bash
brew install cmake qt@6
```

### Linux (Debian/Ubuntu)

```bash
sudo apt install cmake qt6-base-dev qt6-tools-dev libpcsclite-dev uuid-dev
```

## Building

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

The build produces a `LibreCelik.app` bundle on macOS or a `LibreCelik` executable on Linux.

### Build options

- Version is derived automatically from git tags via `cmake/GitVersion.cmake`
- macOS deployment target: 15.0, universal binary (x86_64 + arm64)
- All internal libraries (`SmartCard`, `EIdCard`, `VehicleCard`, etc.) are built as static libraries
- Set `FETCHCONTENT_SOURCE_DIR_LIBREMIDDLEWARE` to use a local LibreMiddleware checkout

## Running tests

```bash
cd build && ctest
```

Tests use Google Test (fetched automatically during CMake configuration).

## Logging

LibreCelik uses Qt's `QLoggingCategory` framework with the following categories:

| Category | Description |
|---|---|
| `rs.libresc.librecelik.general` | Application lifecycle, general events |
| `rs.libresc.librecelik.smartcard` | Smart card detection, APDU communication |
| `rs.libresc.librecelik.celikapi` | Card protocol operations (eID, vehicle) |
| `rs.libresc.librecelik.printing` | Document printing |

### Configuring log output

Qt logging is controlled via the `QT_LOGGING_RULES` environment variable.

**Disable all debug output:**

```bash
export QT_LOGGING_RULES="rs.libresc.librecelik.*.debug=false"
```

**Enable only smart card logging for troubleshooting:**

```bash
export QT_LOGGING_RULES="rs.libresc.librecelik.*.debug=false;rs.libresc.librecelik.smartcard.debug=true"
```

## License

GPL-3.0-or-later
