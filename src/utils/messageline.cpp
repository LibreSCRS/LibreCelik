// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0

#include "utils/messageline.h"

#include <QLabel>
#include <QVBoxLayout>

namespace librecelik::utils {

QLabel* addMessageLine(QVBoxLayout* layout, QWidget* parent, const QString& objectName, bool bold)
{
    auto* label = new QLabel(parent);
    if (!objectName.isEmpty())
        label->setObjectName(objectName);
    label->setAlignment(Qt::AlignCenter);
    label->setWordWrap(true);
    // Plain text, always — see the header comment on why this is the one
    // place that decides it.
    label->setTextFormat(Qt::PlainText);
    if (bold)
        label->setStyleSheet(QStringLiteral("QLabel { font-size: 14px; font-weight: bold; }"));
    layout->addWidget(label);
    return label;
}

} // namespace librecelik::utils
