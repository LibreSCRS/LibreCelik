// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0

#pragma once

#include <QIcon>
#include <QPainter>
#include <QPixmap>
#include <QSize>
#include <QToolButton>
#include <QWidget>

namespace iconutils {

// Create the standard plugin "header printer" toolbutton: loads
// ":/images/printer-header.svg", renders it to a 24x24 pixmap with a
// dimmed disabled state, configures autoRaise, and starts disabled.
// Caller is responsible for connecting `clicked` to whatever slot
// triggers the print action, and for adding the button to its parent
// layout / header. The button is parented (Qt ownership).
inline QToolButton* createPrinterHeaderButton(QWidget* parent)
{
    auto* btn = new QToolButton(parent);
    QIcon icon(QStringLiteral(":/images/printer-header.svg"));
    auto normalPix = icon.pixmap(24, 24);
    QPixmap dimPix(normalPix.size());
    dimPix.fill(Qt::transparent);
    {
        QPainter p(&dimPix);
        p.setOpacity(0.3);
        p.drawPixmap(0, 0, normalPix);
    }
    icon.addPixmap(dimPix, QIcon::Disabled);
    btn->setIcon(icon);
    btn->setIconSize(QSize(24, 24));
    btn->setToolTip(qtTrId("lc-print-tooltip"));
    btn->setAutoRaise(true);
    btn->setEnabled(false);
    return btn;
}

} // namespace iconutils
