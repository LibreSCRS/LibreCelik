// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0

#pragma once

#include <LibreSCRS/Secure/String.h>

#include <QMetaType>

/// @brief CAN/MRZ credentials carried from the eMRTD authentication widget
///        to the AsyncCardReader on the GUI thread.
///
/// CAN (Card Access Number) and the MRZ key seeds (document number, DOB,
/// expiry) feed directly into PACE / BAC key derivation. Their lifetime in
/// process memory must be minimised. Each field is held in
/// @ref LibreSCRS::Secure::String, which uses a cleansing allocator so the
/// bytes are zeroed via @c OPENSSL_cleanse when the credential is destroyed,
/// including across copy/move boundaries.
///
/// The struct is intentionally trivially registerable with Qt's metatype
/// system so it can travel through @c Qt::QueuedConnection signal/slot hops
/// without falling back to QString-backed storage.
///
/// Threading: the carrier itself has no thread affinity. Producers (the GUI
/// authentication widget) must `clear()` the source @c QLineEdits immediately
/// after constructing the carrier — read first, hide second.
struct EmrtdCredentials
{
    /// @brief Active credential mode ("can" → CAN-only PACE; "mrz" → BAC seeds).
    enum class Mode { CAN, MRZ };

    Mode mode = Mode::CAN;

    /// @brief 6-digit Card Access Number (PACE password). Empty when @ref mode == MRZ.
    LibreSCRS::Secure::String can;

    /// @brief 9-character document number (uppercase A-Z, 0-9). Empty when @ref mode == CAN.
    LibreSCRS::Secure::String mrzDocNumber;

    /// @brief Date of birth in YYMMDD. Empty when @ref mode == CAN.
    LibreSCRS::Secure::String mrzDob;

    /// @brief Expiry date in YYMMDD. Empty when @ref mode == CAN.
    LibreSCRS::Secure::String mrzExpiry;
};

Q_DECLARE_METATYPE(EmrtdCredentials)
