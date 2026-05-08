// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0

#include "buttonbox.h"

#include <QEvent>
#include <QPushButton>
#include <QtCore/QCoreApplication>

namespace librecelik {

ButtonBox::ButtonBox(QWidget* parent) : QDialogButtonBox(parent)
{
    // No standard buttons yet; retranslateUi is a no-op until callers
    // add some. changeEvent will pick up future LanguageChange events.
}

ButtonBox::ButtonBox(StandardButtons buttons, QWidget* parent) : QDialogButtonBox(buttons, parent)
{
    retranslateUi();
}

ButtonBox::ButtonBox(Qt::Orientation orientation, QWidget* parent) : QDialogButtonBox(orientation, parent) {}

ButtonBox::ButtonBox(StandardButtons buttons, Qt::Orientation orientation, QWidget* parent)
    : QDialogButtonBox(buttons, orientation, parent)
{
    retranslateUi();
}

void ButtonBox::changeEvent(QEvent* event)
{
    // Base first: lets QDialogButtonBox re-apply QPlatformTheme standard
    // text (which on Plasma is the KDE-localized form), then we override
    // with our `lc-btn-*` translations so our chosen locale wins.
    QDialogButtonBox::changeEvent(event);
    if (event->type() == QEvent::LanguageChange)
        retranslateUi();
}

void ButtonBox::retranslateUi()
{
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
        if (auto* b = button(m.sb))
            b->setText(qtTrId(m.id));
    }
}

} // namespace librecelik
