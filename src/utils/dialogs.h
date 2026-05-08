// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0

#pragma once

#include <QLineEdit>
#include <QMessageBox>
#include <QString>
#include <Qt>

class QWidget;

namespace librecelik::dialogs {

/// Drop-in replacements for `QMessageBox::*` static convenience methods
/// and `QInputDialog::getText` that apply LibreCelik's own button
/// translations (`lc-btn-*` ids).
///
/// The Qt static convenience methods construct an internal
/// `QDialogButtonBox`, which on KDE Plasma renders standard-button text
/// from `kwidgetsaddons6_qt.qm` (KDE-localized for system locale)
/// regardless of the application's selected locale. These wrappers
/// construct the dialog manually, override the standard-button text,
/// and `exec()` — yielding the same return semantics as their Qt
/// counterparts.
///
/// @par Thread-safety  Single-threaded (Qt main thread only). All
///                     wrappers internally call `exec()` and block.
QMessageBox::StandardButton information(QWidget* parent, const QString& title, const QString& text,
                                        QMessageBox::StandardButtons buttons = QMessageBox::Ok,
                                        QMessageBox::StandardButton defaultButton = QMessageBox::NoButton);

QMessageBox::StandardButton warning(QWidget* parent, const QString& title, const QString& text,
                                    QMessageBox::StandardButtons buttons = QMessageBox::Ok,
                                    QMessageBox::StandardButton defaultButton = QMessageBox::NoButton);

QMessageBox::StandardButton critical(QWidget* parent, const QString& title, const QString& text,
                                     QMessageBox::StandardButtons buttons = QMessageBox::Ok,
                                     QMessageBox::StandardButton defaultButton = QMessageBox::NoButton);

QMessageBox::StandardButton question(QWidget* parent, const QString& title, const QString& text,
                                     QMessageBox::StandardButtons buttons = QMessageBox::Yes | QMessageBox::No,
                                     QMessageBox::StandardButton defaultButton = QMessageBox::NoButton);

/// Mirrors `QInputDialog::getText`. Returns the entered text on accept;
/// empty string and `*ok = false` on reject.
QString getText(QWidget* parent, const QString& title, const QString& label,
                QLineEdit::EchoMode echoMode = QLineEdit::Normal, const QString& text = {}, bool* ok = nullptr,
                Qt::WindowFlags flags = Qt::WindowFlags());

} // namespace librecelik::dialogs
