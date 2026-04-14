// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0

#pragma once

#include <QColor>
#include <QLatin1String>

namespace signing {

// Brand teal used throughout the signing wizard UI
inline constexpr QColor kTealColor{61, 140, 149};

// Hex form for use in stylesheets
inline constexpr QLatin1String kTealHex{"#3D8C95"};

// Result status colors
inline constexpr QLatin1String kSuccessHex{"#4CAF50"};
inline constexpr QLatin1String kErrorHex{"#F44336"};

} // namespace signing
