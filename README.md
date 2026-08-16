# LibreCelik

**[librescrs.github.io](https://librescrs.github.io)**

LibreCelik (Слободни Челик) is a free and open-source smart card reader for Linux and macOS.

## Supported Cards

- **Full PKI through OpenSC.** OpenSC is the PKI engine. It works with every
  card OpenSC supports — the Serbian CardEdge cards (eID,
  qualified-signature/PKS, health) via the `srbeid` driver, plus IAS-ECC,
  CardOS, PIV, OpenPGP and more. Where OpenSC does not cover something, a
  built-in PKCS#15 plugin fills the gap (for example, on-card SHA-256 signing
  on the NAM card).
- **Card data through plugins.** Built-in plugins read the document data:
  Serbian eID, Serbian health insurance, EU vehicle registration, and
  electronic passports (eMRTD).

## Features

- Automatic card detection and identification
- Plugin architecture — extensible for new card types
- Progressive data reading with streaming display
- Formatted document printing
- Multi-PIN support and PIN management
- Document signing wizard (PAdES, XAdES, CAdES, JAdES, ASiC-E) with optional
  TSA timestamping and Trusted-List-driven validation
- Bilingual interface (English / Serbian Cyrillic)

## Downloads

Pre-built packages are available on the [Releases](https://github.com/LibreSCRS/LibreCelik/releases) page.
Releases are signed with cosign keyless against the LibreSCRS GitHub
Actions identity; verification instructions are on the
[security page](https://librescrs.github.io/security/).

## Building from source

LibreCelik consumes LibreAgent (ClientQt) via CMake `FetchContent`, pinned
by `cmake/libreagent.pin`. For local development, point at a sibling
LibreAgent checkout:

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release \
      -DFETCHCONTENT_SOURCE_DIR_LIBREAGENT=/path/to/LibreAgent
cmake --build build -j4
```

Requires CMake 3.24+ (3.28+ for the `FetchContent` path, which builds
LibreAgent from source), C++23 and Qt 6.10+. Card access happens in the
LibreSCRS card agent, so this application itself needs no PC/SC stack.
Cap parallel jobs to `-j4` to avoid system saturation.

## Contributing

See [CONTRIBUTING.md](CONTRIBUTING.md) for code-formatting expectations
(clang-format-21 pin), commit conventions, and the optional pre-commit
hook.

## License

GPL-3.0-or-later — see [LICENSE](LICENSE) for details.

## Source availability

LibreCelik is licensed under GPL-3.0-or-later. It bundles the LibreAgent
Qt client library, which is LGPL-2.1-or-later, alongside Qt and the other
free-software libraries listed in the application's third-party notices.

The complete corresponding source code for this software, including
all LGPL components we build ourselves, is publicly available at:

- https://github.com/LibreSCRS/LibreAgent
- https://github.com/LibreSCRS/LibreCelik

This offer is valid for as long as we distribute this software.
