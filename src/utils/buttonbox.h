// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0

#pragma once

#include <QDialogButtonBox>

class QEvent;

namespace librecelik {

/// Drop-in replacement for QDialogButtonBox that uses LibreCelik's own
/// translations (`lc-btn-*` ids) for all standard buttons.
///
/// Background: KDE Plasma's Qt platform integration loads
/// `kwidgetsaddons6_qt.qm` which short-circuits
/// `QPlatformTheme::standardButtonText()` to KDE-localized strings (e.g.
/// Cyrillic Serbian "У реду" / "Одустани" for system locale `sr_RS`)
/// regardless of the application's selected locale. This mechanism does
/// not flow through the QTranslator chain, so installing additional
/// translators cannot override it.
///
/// ButtonBox solves this by re-applying our own `qtTrId` text on
/// every standard button after construction and on every
/// `QEvent::LanguageChange`. The class is the canonical button-box for
/// LC; do not use bare `QDialogButtonBox` directly.
///
/// Drop-in: same constructors and API as `QDialogButtonBox`. Existing
/// callsites only need `s/QDialogButtonBox/ButtonBox/` and the
/// header include.
///
/// @par Thread-safety  Single-threaded (Qt main thread only), like all
///                     widgets.
class ButtonBox : public QDialogButtonBox
{
    Q_OBJECT
public:
    explicit ButtonBox(QWidget* parent = nullptr);
    explicit ButtonBox(StandardButtons buttons, QWidget* parent = nullptr);
    explicit ButtonBox(Qt::Orientation orientation, QWidget* parent = nullptr);
    ButtonBox(StandardButtons buttons, Qt::Orientation orientation, QWidget* parent = nullptr);

protected:
    void changeEvent(QEvent* event) override;

private:
    /// Apply our `lc-btn-*` translations to every standard button currently
    /// in the box. Idempotent. Custom buttons added via
    /// `addButton(QString, ButtonRole)` are left alone.
    void retranslateUi();
};

} // namespace librecelik
