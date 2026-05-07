// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0

#pragma once

#include <LibreSCRS/Trust/TrustConfig.h>
#include <LibreSCRS/Signing/TsaProvider.h>

#include <QString>

namespace signing {

/// @brief Reads LC's signing configuration from QSettings and exposes it as
///        the public API's TrustConfig + TsaProvider pair.
///
/// Intended to live for the lifetime of the application — constructed once in
/// LibreCelik's ctor and re-applied to the shared SigningService whenever the
/// user saves the settings dialog.
class SigningConfiguration
{
public:
    SigningConfiguration();

    /// @brief Build a TrustConfig from the current QSettings snapshot.
    LibreSCRS::Trust::TrustConfig makeTrustConfig() const;

    /// @brief Build a TsaProvider from the current QSettings snapshot.
    /// @return A callable TsaProvider; empty std::function if no TSA is configured.
    LibreSCRS::Signing::TsaProvider makeTsaProvider() const;
};

} // namespace signing
