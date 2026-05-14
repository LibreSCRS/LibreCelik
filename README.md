# LibreCelik

**[librescrs.github.io](https://librescrs.github.io)**

LibreCelik (Слободни Челик) is a free and open-source smart card reader for Linux and macOS.

## Supported Cards

- **eMRTD / ePassport** — any ICAO 9303 compliant passport or national ID card
- **Serbian eID** — Gemalto 2014+, IF2020 Foreigner
- **Serbian Vehicle Registration (EU VRC)** — all EU mandatory and optional fields, print support
- **Serbian Health Insurance (RFZO)**
- **PIV (NIST SP 800-73)** — US federal ID standard
- **PKCS#15 / PKI Tokens** — generic certificate and PIN management

## Features

- Automatic card detection and identification
- Plugin architecture — extensible for new card types
- Progressive data reading with streaming display
- Formatted document printing
- Multi-PIN support and PIN management
- Document signing wizard (PAdES, XAdES, CAdES, JAdES, ASiC) with optional
  TSA timestamping and Trusted-List-driven validation
- Bilingual interface (English / Serbian Cyrillic)

## Downloads

Pre-built packages are available on the [Releases](https://github.com/LibreSCRS/LibreCelik/releases) page.
Releases are signed with cosign keyless against the LibreSCRS GitHub
Actions identity; verification instructions are on the
[security page](https://librescrs.github.io/security/).

## Building from source

LibreCelik consumes LibreMiddleware via CMake `FetchContent`. For local
development, point at a sibling LibreMiddleware checkout:

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release \
      -DFETCHCONTENT_SOURCE_DIR_LIBREMIDDLEWARE=/path/to/LibreMiddleware
cmake --build build -j4
```

Requires CMake 3.24+, C++23, Qt 6.10+, PC/SC, OpenSSL 3, libcurl, and
libxml2. Cap parallel jobs to `-j4` to avoid system saturation.

## Contributing

See [CONTRIBUTING.md](CONTRIBUTING.md) for code-formatting expectations
(clang-format-21 pin), commit conventions, and the optional pre-commit
hook.

## License

GPL-3.0-or-later — see [LICENSE](LICENSE) for details.

## Source availability

LibreCelik is licensed under GPL-3.0-or-later. It bundles statically
linked components covered by other free-software licences, including
LGPL-2.1-or-later (LibreMiddleware, OpenSC).

The complete corresponding source code for this software, including
all modified LGPL components, is publicly available at:

- https://github.com/LibreSCRS/LibreMiddleware
- https://github.com/LibreSCRS/LibreCelik

This offer is valid for as long as we distribute this software.
