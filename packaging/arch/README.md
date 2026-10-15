# Arch packaging — librecelik

`librecelik` packages the Qt 6 desktop reader and signer: `bin/LibreCelik`, its
GUI plugins, the desktop entry, the icon, the AppStream metainfo and the man
page.

LibreCelik is a **thin D-Bus client** of the LibreSCRS agent — it opens no card
itself and links no LibreMiddleware target. It depends on **`librelinux`**
(`librescrs-agent`) and on **`librescrs-agent-client-qt`**, the Qt client
library built from the agent library's own repository. The agent is a hard
dependency, as it is for `librekde`: without it LibreCelik starts and finds no
card, and pacman installs no `optdepends` on its own.

The `PKGBUILD` is **release-shaped**: it builds the signed release tag of this
repository (`git+https://…#tag=$pkgver?signed`), and makepkg checks the tag's
signature against `validpgpkeys` -- the release key's primary fingerprint, the
one `KEYS` carries -- before it builds anything. `pkgver` is the first line of
the repository's `VERSION` file.

`build()` touches no network: the tests (which fetch googletest) are off, and
LibreAgent comes from the installed `librescrs-agent-client-qt`, never from a
FetchContent copy — `build()` fails if anything was fetched. `check()`
validates the desktop entry and the AppStream metainfo, as the RPM `%check`
does.

## Release build (after the `5.0.0` tag is published)

```sh
gpg --import KEYS   # once, from the repository root: makepkg verifies the tag with it
cd packaging/arch
makepkg -si
```

## Local dogfood build (no remote, no tag — build from this checkout)

```sh
REPO="$(git rev-parse --show-toplevel)"
mkdir -p /var/tmp/lc-arch && cp packaging/arch/PKGBUILD /var/tmp/lc-arch/
cd /var/tmp/lc-arch
# Replace the multi-line release `source=(...)` array with a single local-git
# entry. A git source named exactly LibreCelik checks out to
# $srcdir/LibreCelik, matching the `cd` lines. No #tag and no ?signed: this
# builds the working tree's HEAD, which carries no release signature.
sed -i -e "/^source=(/,/^)/c\\source=(\"LibreCelik::git+file://$REPO\")" PKGBUILD
makepkg -si
```

> `librelinux` (`librescrs-agent`) and `librescrs-agent-client-qt` must both be
> installed first. The whole chain has **two roots, not one**:
>
>     librescrs-middleware  <=  librelinux  <=  librecelik
>     librescrs-agent-client-qt  <=  librecelik
