// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0

#include "slot_label_formatter.h"

#include <QCoreApplication>

namespace librecelik::signing {

QString formatSlotLabel(const QString& tokenLabel, const QString& rawPinLabel)
{
    // Map the canonical PKCS#15 AODF pin labels emitted by
    // LM's Pkcs15Slot to their localised Qt translation IDs.
    // Unknown labels are passed through verbatim so the formatter
    // remains robust against cards emitting non-canonical strings.
    QString localized;
    if (rawPinLabel == QStringLiteral("Authentication"))
        localized = qtTrId("lc-pin-label-auth");
    else if (rawPinLabel == QStringLiteral("Signing (QSCD)"))
        localized = qtTrId("lc-pin-label-qscd");
    else if (rawPinLabel == QStringLiteral("Signing"))
        localized = qtTrId("lc-pin-label-sign");
    else
        localized = rawPinLabel;

    if (tokenLabel.isEmpty())
        return localized;
    return QStringLiteral("%1 — %2").arg(tokenLabel, localized);
}

} // namespace librecelik::signing
