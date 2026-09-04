%global _lto_cflags %{nil}

Name:           librecelik
Version:        5.0.0
Release:        1%{?dist}
Summary:        Cross-platform reader and signer for LibreSCRS smart cards

License:        GPL-3.0-or-later
URL:            https://librescrs.github.io/
Source0:        %{name}-%{version}.tar.gz

ExclusiveArch:  x86_64

BuildRequires:  cmake >= 3.24
BuildRequires:  ninja-build
BuildRequires:  gcc-c++
BuildRequires:  make
BuildRequires:  pkgconf-pkg-config
BuildRequires:  git
BuildRequires:  qt6-qtbase-devel
BuildRequires:  qt6-qtsvg-devel
# PdfWidgets, for the visible-signature placement page. It is a separate
# package on every distribution and the signing wizard requires it at
# configuration time.
BuildRequires:  qt6-qtpdf-devel
BuildRequires:  qt6-qttools-devel
BuildRequires:  gtest-devel
BuildRequires:  dbus-daemon
BuildRequires:  librescrs-middleware-devel >= 5.0
BuildRequires:  librescrs-agent-client-qt-devel >= 5.0
BuildRequires:  desktop-file-utils
BuildRequires:  libappstream-glib

Requires:       librescrs-agent-client-qt%{?_isa} >= 5.0
Recommends:     librescrs-agent >= 5.0

%description
LibreCelik reads Serbian government smart cards -- identity, vehicle and health
-- as well as generic ICAO electronic travel documents, NIST PIV cards and
PKCS#15 cards, and signs documents in PAdES, XAdES and CAdES.

It talks to the card through the LibreSCRS agent, so the PIN is collected by
the agent's own prompter rather than inside this application.

%prep
%autosetup -n %{name}-%{version}

%build
%cmake -GNinja -DBUILD_TESTING=OFF -DINSTALL_GTEST=OFF \
    -DLIBRECELIK_USE_INSTALLED_LIBREAGENT=ON
# Left at its default this build would fetch LibreAgent from the network.
test ! -e %{_vpath_builddir}/_deps/libreagent-subbuild || \
  { echo "the build fetched LibreAgent instead of using the installed package"; exit 1; }
%cmake_build

%install
%cmake_install

%check
# Not the test suite -- there is no reader and no session bus here. These two
# validate the metadata a software centre reads, which is exactly the kind of
# thing a package build can prove and nothing else will.
desktop-file-validate %{buildroot}%{_datadir}/applications/librecelik.desktop
appstream-util validate-relax --nonet \
    %{buildroot}%{_metainfodir}/org.librescrs.librecelik.metainfo.xml

%files
%license LICENSE
%doc README.md
%{_bindir}/LibreCelik
%{_libdir}/gui-plugins/
%{_datadir}/applications/librecelik.desktop
%{_datadir}/icons/hicolor/512x512/apps/librecelik.png
%{_metainfodir}/org.librescrs.librecelik.metainfo.xml

%changelog
* Fri Sep 04 2026 LibreSCRS <packages@librescrs.org> - 5.0.0-1
- Initial RPM packaging of LibreCelik.
