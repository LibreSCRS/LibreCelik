// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0

#pragma once

#include "utils/pluginwidgetbase.h"

#include <plugin/card_data.h>

namespace LibreSCRS {
class CardHeaderCard;
}

class CollapsibleSection;
class QToolButton;
class QVBoxLayout;

class PIVWidget : public plugin_ui::PluginWidgetBase
{
    Q_OBJECT
public:
    explicit PIVWidget(const plugin::CardData& data, QWidget* parent = nullptr);
    explicit PIVWidget(QWidget* parent);

    void addGroup(const plugin::CardFieldGroup& group);

    const plugin::CardData& cardData() const
    {
        return data;
    }

    Q_INVOKABLE void enablePrintButton();

signals:
    void printRequested(const plugin::CardData& data);

protected:
    void retranslateUi() override;

private:
    void buildEmptyShell();
    void addChuidGroup(const plugin::CardFieldGroup& group);
    void addCccGroup(const plugin::CardFieldGroup& group);
    void addPrintedGroup(const plugin::CardFieldGroup& group);
    void addDiscoveryGroup(const plugin::CardFieldGroup& group);
    void addKeyHistoryGroup(const plugin::CardFieldGroup& group);
    void rebuildHeader();

    plugin::CardData data;
    QVBoxLayout* contentLayout = nullptr;
    CollapsibleSection* outerSection = nullptr;
    QToolButton* printBtn = nullptr;
    LibreSCRS::CardHeaderCard* headerCard = nullptr;
};
