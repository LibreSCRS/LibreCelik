// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0

#include "dialogs.h"

#include <QAbstractButton>
#include <QDialogButtonBox>
#include <QInputDialog>
#include <QPushButton>
#include <QtCore/QCoreApplication>

namespace librecelik::dialogs {

namespace {

/// Walk the standard buttons attached to @p box and apply our
/// `lc-btn-*` translations. Mirrors librecelik::ButtonBox::retranslateUi but
/// operates on a foreign QDialogButtonBox instance (the one Qt creates
/// internally inside QMessageBox / QInputDialog). Custom buttons added
/// via `addButton(QString, ButtonRole)` are left alone.
void applyStandardButtonTexts(QDialogButtonBox* box)
{
    if (!box)
        return;
    struct Mapping
    {
        QDialogButtonBox::StandardButton sb;
        const char* id;
    };
    static constexpr Mapping kMappings[] = {
        {QDialogButtonBox::Ok, "lc-btn-ok"},
        {QDialogButtonBox::Cancel, "lc-btn-cancel"},
        {QDialogButtonBox::Close, "lc-btn-close"},
        {QDialogButtonBox::Apply, "lc-btn-apply"},
        {QDialogButtonBox::Save, "lc-btn-save"},
        {QDialogButtonBox::Reset, "lc-btn-reset"},
        {QDialogButtonBox::RestoreDefaults, "lc-btn-restore-defaults"},
        {QDialogButtonBox::Yes, "lc-btn-yes"},
        {QDialogButtonBox::No, "lc-btn-no"},
        {QDialogButtonBox::Help, "lc-btn-help"},
        {QDialogButtonBox::Abort, "lc-btn-abort"},
        {QDialogButtonBox::Retry, "lc-btn-retry"},
        {QDialogButtonBox::Ignore, "lc-btn-ignore"},
        {QDialogButtonBox::Discard, "lc-btn-discard"},
    };
    for (const auto& m : kMappings) {
        if (auto* b = box->button(m.sb))
            b->setText(qtTrId(m.id));
    }
}

void applyMessageBoxButtonTexts(QMessageBox& box)
{
    struct Mapping
    {
        QMessageBox::StandardButton sb;
        const char* id;
    };
    static constexpr Mapping kMappings[] = {
        {QMessageBox::Ok, "lc-btn-ok"},
        {QMessageBox::Cancel, "lc-btn-cancel"},
        {QMessageBox::Close, "lc-btn-close"},
        {QMessageBox::Apply, "lc-btn-apply"},
        {QMessageBox::Save, "lc-btn-save"},
        {QMessageBox::Reset, "lc-btn-reset"},
        {QMessageBox::RestoreDefaults, "lc-btn-restore-defaults"},
        {QMessageBox::Yes, "lc-btn-yes"},
        {QMessageBox::No, "lc-btn-no"},
        {QMessageBox::Help, "lc-btn-help"},
        {QMessageBox::Abort, "lc-btn-abort"},
        {QMessageBox::Retry, "lc-btn-retry"},
        {QMessageBox::Ignore, "lc-btn-ignore"},
        {QMessageBox::Discard, "lc-btn-discard"},
    };
    for (const auto& m : kMappings) {
        if (auto* b = box.button(m.sb))
            b->setText(qtTrId(m.id));
    }
}

QMessageBox::StandardButton runMessageBox(QMessageBox::Icon icon, QWidget* parent, const QString& title,
                                          const QString& text, QMessageBox::StandardButtons buttons,
                                          QMessageBox::StandardButton defaultButton)
{
    QMessageBox box(parent);
    box.setIcon(icon);
    box.setWindowTitle(title);
    box.setText(text);
    box.setStandardButtons(buttons);
    if (defaultButton != QMessageBox::NoButton)
        box.setDefaultButton(defaultButton);
    applyMessageBoxButtonTexts(box);
    return static_cast<QMessageBox::StandardButton>(box.exec());
}

} // namespace

QMessageBox::StandardButton information(QWidget* parent, const QString& title, const QString& text,
                                        QMessageBox::StandardButtons buttons, QMessageBox::StandardButton defaultButton)
{
    return runMessageBox(QMessageBox::Information, parent, title, text, buttons, defaultButton);
}

QMessageBox::StandardButton warning(QWidget* parent, const QString& title, const QString& text,
                                    QMessageBox::StandardButtons buttons, QMessageBox::StandardButton defaultButton)
{
    return runMessageBox(QMessageBox::Warning, parent, title, text, buttons, defaultButton);
}

QMessageBox::StandardButton critical(QWidget* parent, const QString& title, const QString& text,
                                     QMessageBox::StandardButtons buttons, QMessageBox::StandardButton defaultButton)
{
    return runMessageBox(QMessageBox::Critical, parent, title, text, buttons, defaultButton);
}

QMessageBox::StandardButton question(QWidget* parent, const QString& title, const QString& text,
                                     QMessageBox::StandardButtons buttons, QMessageBox::StandardButton defaultButton)
{
    return runMessageBox(QMessageBox::Question, parent, title, text, buttons, defaultButton);
}

QString getText(QWidget* parent, const QString& title, const QString& label, QLineEdit::EchoMode echoMode,
                const QString& text, bool* ok, Qt::WindowFlags flags)
{
    QInputDialog dlg(parent, flags);
    dlg.setInputMode(QInputDialog::TextInput);
    dlg.setWindowTitle(title);
    dlg.setLabelText(label);
    dlg.setTextEchoMode(echoMode);
    dlg.setTextValue(text);
    // QInputDialog hosts an internal QDialogButtonBox; locate and override.
    if (auto* box = dlg.findChild<QDialogButtonBox*>())
        applyStandardButtonTexts(box);
    const bool accepted = (dlg.exec() == QDialog::Accepted);
    if (ok)
        *ok = accepted;
    return accepted ? dlg.textValue() : QString();
}

} // namespace librecelik::dialogs
