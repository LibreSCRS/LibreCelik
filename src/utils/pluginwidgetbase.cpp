// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0

#include "pluginwidgetbase.h"

#include <QEvent>

namespace plugin_ui {

PluginWidgetBase::PluginWidgetBase(QWidget* parent) : QWidget(parent) {}

void PluginWidgetBase::changeEvent(QEvent* event)
{
    QWidget::changeEvent(event);
    if (event->type() == QEvent::LanguageChange)
        retranslateUi();
}

} // namespace plugin_ui
