// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0

#pragma once

#include <QWidget>

namespace plugin_ui {

// Base class for plugin card widgets that need to retranslate their UI
// strings when the application language changes. Centralizes the
// boilerplate `changeEvent` override that was previously duplicated in
// every plugin widget. Subclasses implement `retranslateUi()` to refresh
// any user-visible strings that were set via `tr()` / `qtTrId()` at
// construction time.
class PluginWidgetBase : public QWidget
{
    Q_OBJECT
public:
    explicit PluginWidgetBase(QWidget* parent = nullptr);

protected:
    void changeEvent(QEvent* event) override;

    // Subclasses implement this to retranslate any user-visible strings
    // that were set via tr()/qtTrId() at construction time.
    virtual void retranslateUi() = 0;
};

} // namespace plugin_ui
