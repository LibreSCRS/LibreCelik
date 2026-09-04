#!/usr/bin/env bash
# Runs INSIDE a fresh container. /pkg holds this repository's package,
# /pkg-<Repo> the upstream ones.
set -uo pipefail
fail=0
check() { if [ "$2" -eq 0 ]; then echo "PASS $1"; else echo "FAIL $1"; fail=1; fi; }

if [ "${FAMILY:-deb}" = deb ]; then
  export DEBIAN_FRONTEND=noninteractive
  # The base container is not a machine. Ubuntu's image ships
  # /etc/dpkg/dpkg.cfg.d/excludes with path-exclude=/usr/share/locale/*/LC_MESSAGES/*.mo
  # (Debian's does not), so a package that carries translations installs without
  # them there. Asserting on disk under that configuration measures the image,
  # not the package, so the exclusion goes before anything is installed.
  rm -f /etc/dpkg/dpkg.cfg.d/excludes
  apt-get update -qq
  apt-get install -y -qq appstream >/dev/null
  apt-get install -y --no-install-recommends \
      /pkg-LibreMiddleware/liblibrescrs5_*.deb /pkg-LibreMiddleware/librescrs-card-plugins_*.deb \
      /pkg-LibreAgent/liblibrescrs-agentclient-qt5_*.deb /pkg/*.deb >/dev/null
  check "V1 install with upstreams" $?
  LIBGLOB="/usr/lib/*"
  installed_list() { dpkg-query -W -f='${Package}\n'; }
  remove_ours() { apt-get purge -y $(installed_list | grep -E 'librescrs|liblibrescrs|librecelik') >/dev/null; }
  validate_meta() { appstreamcli validate --no-net /usr/share/metainfo/org.librescrs.librecelik.metainfo.xml; }
else
  dnf -y -q install libappstream-glib >/dev/null
  dnf -y -q install /pkg-LibreMiddleware/librescrs-middleware-5*.rpm \
      /pkg-LibreMiddleware/librescrs-card-plugins-5*.rpm \
      /pkg-LibreAgent/librescrs-agent-client-qt-5*.rpm /pkg/*.rpm >/dev/null
  check "V1 install with upstreams" $?
  LIBGLOB="/usr/lib64"
  installed_list() { rpm -qa --qf '%{NAME}\n'; }
  remove_ours() { dnf -y -q remove $(installed_list | grep -E '^(librescrs|liblibrescrs|librecelik)') >/dev/null; }
  validate_meta() { appstream-util validate-relax --nonet /usr/share/metainfo/org.librescrs.librecelik.metainfo.xml; }
fi

test -x /usr/bin/LibreCelik; check "V2 the application is on PATH" $?
test -f /usr/share/applications/librecelik.desktop; check "V2b desktop entry" $?
test -f /usr/share/metainfo/org.librescrs.librecelik.metainfo.xml
check "V2c AppStream metainfo installed" $?

# The metainfo is validated as a gate, not as a note: without a valid component
# no software centre lists the application at all.
validate_meta; check "V2d AppStream metainfo validates" $?

n=$(ls $LIBGLOB/gui-plugins/*.so 2>/dev/null | wc -l)
test "$n" -ge 6; check "V3 six GUI plugins installed (counted $n)" $?

miss=0
for f in /usr/bin/LibreCelik $LIBGLOB/gui-plugins/*.so; do
  [ -e "$f" ] || continue
  if ldd "$f" 2>/dev/null | grep -q 'not found'; then echo "  not found in $f"; miss=1; fi
done
test "$miss" -eq 0; check "V11 no unresolved shared-library dependency" $?

remove_ours
find /usr -iname '*librecelik*' > /tmp/leftover.txt
test ! -s /tmp/leftover.txt; check "V9 nothing left under /usr after removal" $?
[ -s /tmp/leftover.txt ] && cat /tmp/leftover.txt
exit $fail
