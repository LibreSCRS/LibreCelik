// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0

#pragma once

#include "utils/pluginwidgetbase.h"

#include <plugin/card_data.h>

class CollapsibleSection;
class QLabel;
class QLineEdit;
class QToolButton;
class QVBoxLayout;
class SecurityStatusWidget;

class EMRTDWidget : public plugin_ui::PluginWidgetBase
{
    Q_OBJECT
public:
    // Full-data constructor (existing behaviour, unchanged)
    explicit EMRTDWidget(const plugin::CardData& data, QWidget* parent = nullptr);

    // Empty-shell constructor for progressive display
    explicit EMRTDWidget(QWidget* parent);

    // Progressive display: add one group at a time
    void addGroup(const plugin::CardFieldGroup& group);

    Q_INVOKABLE void enablePrintButton();
    void showNoDataMessage();

    const plugin::CardData& cardData() const
    {
        return data;
    }

signals:
    void printRequested(const plugin::CardData& data);

protected:
    void retranslateUi() override;

private:
    void showAuthRequired(const plugin::CardFieldGroup* group);
    void showError(const plugin::CardFieldGroup* group);

    plugin::CardData data;
    QToolButton* printBtn = nullptr;

    // Progressive-display state
    QVBoxLayout* outerLayout = nullptr;
    CollapsibleSection* outerSection = nullptr;
    QVBoxLayout* sectionLayout = nullptr;
    QLabel* photoLabel = nullptr;
    SecurityStatusWidget* securityStatusWidget = nullptr;
};
