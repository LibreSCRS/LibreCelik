// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0

#pragma once

#include "utils/pluginwidgetbase.h"

#include <LibreSCRS/Plugin/CardData.h>

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
    explicit EMRTDWidget(const LibreSCRS::Plugin::CardData& data, QWidget* parent = nullptr);

    // Empty-shell constructor for progressive display
    explicit EMRTDWidget(QWidget* parent);

    // Progressive display: add one group at a time
    void addGroup(const LibreSCRS::Plugin::CardFieldGroup& group);

    Q_INVOKABLE void enablePrintButton();
    void showNoDataMessage();

    const LibreSCRS::Plugin::CardData& cardData() const
    {
        return data;
    }

signals:
    void printRequested(const LibreSCRS::Plugin::CardData& data);

protected:
    void retranslateUi() override;

private:
    void buildShell();

    LibreSCRS::Plugin::CardData data;
    QToolButton* printBtn = nullptr;

    // Progressive-display state
    QVBoxLayout* outerLayout = nullptr;
    CollapsibleSection* outerSection = nullptr;
    QVBoxLayout* sectionLayout = nullptr;
    QLabel* photoLabel = nullptr;
    SecurityStatusWidget* securityStatusWidget = nullptr;
    bool noDataMessageShown = false;
};
