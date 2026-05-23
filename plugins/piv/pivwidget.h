// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0

#pragma once

#include "utils/pluginwidgetbase.h"

#include <LibreSCRS/Plugin/CardData.h>

namespace librecelik::utils {
class CardHeaderCard;
}

class CollapsibleSection;
class QToolButton;
class QVBoxLayout;

class PIVWidget : public plugin_ui::PluginWidgetBase
{
    Q_OBJECT
public:
    explicit PIVWidget(const LibreSCRS::Plugin::CardData& data, QWidget* parent = nullptr);
    explicit PIVWidget(QWidget* parent);

    void addGroup(const LibreSCRS::Plugin::CardFieldGroup& group);

    const LibreSCRS::Plugin::CardData& cardData() const
    {
        return data;
    }

    Q_INVOKABLE void enablePrintButton();

signals:
    void printRequested(const LibreSCRS::Plugin::CardData& data);

protected:
    void retranslateUi() override;

private:
    void buildEmptyShell();
    void addChuidGroup(const LibreSCRS::Plugin::CardFieldGroup& group);
    void addCccGroup(const LibreSCRS::Plugin::CardFieldGroup& group);
    void addPrintedGroup(const LibreSCRS::Plugin::CardFieldGroup& group);
    void addDiscoveryGroup(const LibreSCRS::Plugin::CardFieldGroup& group);
    void addKeyHistoryGroup(const LibreSCRS::Plugin::CardFieldGroup& group);
    void rebuildHeader();

    LibreSCRS::Plugin::CardData data;
    QVBoxLayout* outerLayout = nullptr;
    QVBoxLayout* contentLayout = nullptr;
    CollapsibleSection* outerSection = nullptr;
    QToolButton* printBtn = nullptr;
    librecelik::utils::CardHeaderCard* headerCard = nullptr;
};
