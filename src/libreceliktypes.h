// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright hirashix0@proton.me

#ifndef LIBRECELIKTYPES_H
#define LIBRECELIKTYPES_H

#include <QString>

namespace LibreSCRS {

enum VerificationOption : uint8_t { NoCheck = 1 << 1, CheckCard = 1 << 2, CheckSignature = 1 << 3 };
Q_DECLARE_FLAGS(VerificationOptions, VerificationOption);
Q_DECLARE_OPERATORS_FOR_FLAGS(VerificationOptions);

} // namespace LibreSCRS

#endif // LIBRECELIKTYPES_H
