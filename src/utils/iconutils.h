// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0

#pragma once

#include <QAction>
#include <QIcon>
#include <QLineEdit>
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

// Programmatic fallback used when QIcon::fromTheme returns nothing (macOS).
inline QIcon createEyeIcon(bool open)
{
    QPixmap px(16, 16);
    px.fill(Qt::transparent);
    QPainter p(&px);
    p.setRenderHint(QPainter::Antialiasing);
    const QColor col(90, 90, 90);
    p.setPen(QPen(col, 1.2));
    p.setBrush(Qt::NoBrush);
    // Outer eye ellipse (horizontal lens shape)
    p.drawEllipse(QRectF(0.5, 3.5, 15.0, 9.0));
    // Pupil
    p.setBrush(col);
    p.drawEllipse(QRectF(5.5, 5.5, 5.0, 5.0));
    if (!open) {
        // Diagonal slash: eye-hidden / password concealed
        p.setBrush(Qt::NoBrush);
        p.setPen(QPen(col, 1.5));
        p.drawLine(QPointF(2.5, 13.5), QPointF(13.5, 2.5));
    }
    return QIcon(px);
}

// Add a trailing eye-icon action to a QLineEdit that toggles password visibility.
inline void addToggleVisibilityAction(QObject* parent, QLineEdit* edit)
{
    auto hiddenIcon = QIcon::fromTheme("view-hidden", createEyeIcon(false));
    auto visibleIcon = QIcon::fromTheme("view-visible", createEyeIcon(true));

    auto* action = edit->addAction(hiddenIcon, QLineEdit::TrailingPosition);
    action->setToolTip(qtTrId("lc-changepin-show-hide"));
    QObject::connect(action, &QAction::triggered, parent, [edit, action, hiddenIcon, visibleIcon]() {
        if (edit->echoMode() == QLineEdit::Password) {
            edit->setEchoMode(QLineEdit::Normal);
            action->setIcon(visibleIcon);
        } else {
            edit->setEchoMode(QLineEdit::Password);
            action->setIcon(hiddenIcon);
        }
    });
}

} // namespace iconutils
