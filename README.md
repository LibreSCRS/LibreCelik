# LibreCelik

LibreCelik (Слободни Челик) is a free and open-source smart card reader for documents issued by the Republic of Serbia. It supports:

- Electronic ID cards (eID) — citizen and foreigner cards
- Vehicle registration documents

The application communicates directly with smart cards via PC/SC APDU commands, without relying on proprietary libraries.

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
- All internal libraries (`SmartCard`, `EIdCard`, `VehicleCard`) are built as static libraries

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

Qt logging is controlled via the `QT_LOGGING_RULES` environment variable. By default, all categories output debug, info, warning, and critical messages.

**Disable all debug output (recommended for release):**

```bash
export QT_LOGGING_RULES="rs.libresc.librecelik.*.debug=false"
```

**Disable all logging except warnings and errors:**

```bash
export QT_LOGGING_RULES="rs.libresc.librecelik.*.debug=false;rs.libresc.librecelik.*.info=false"
```

**Enable only smart card logging for troubleshooting:**

```bash
export QT_LOGGING_RULES="rs.libresc.librecelik.*.debug=false;rs.libresc.librecelik.smartcard.debug=true"
```

**Disable logging entirely:**

```bash
export QT_LOGGING_RULES="rs.libresc.librecelik.*=false"
```

Alternatively, create a `qtlogging.ini` file in the application's working directory:

```ini
[Rules]
rs.libresc.librecelik.*.debug=false
rs.libresc.librecelik.*.info=false
```

## License

GPL-3.0-or-later
