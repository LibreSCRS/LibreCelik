// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0

#include "tokenwidgetplugin.h"

#include <QVBoxLayout>

QWidget* TokenWidgetPlugin::createWidget(const LibreSCRS::Plugin::CardData& /*data*/, QWidget* parent) const
{
    auto* widget = new QWidget(parent);
    auto* layout = new QVBoxLayout(widget);
    layout->setContentsMargins(0, 0, 0, 0);
    return widget;
}
