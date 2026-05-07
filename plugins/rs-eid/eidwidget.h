// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0

#pragma once

#include "utils/pluginwidgetbase.h"

#include <LibreSCRS/Plugin/CardData.h>

class CollapsibleSection;
class QLabel;
class QToolButton;
class QVBoxLayout;

class EidWidget : public plugin_ui::PluginWidgetBase
{
    Q_OBJECT
public:
    // Full-data constructor (existing behaviour, unchanged)
    explicit EidWidget(const LibreSCRS::Plugin::CardData& data, QWidget* parent = nullptr);

    // Empty-shell constructor for progressive display
    explicit EidWidget(QWidget* parent);

    // Progressive display: add one group at a time
    void addGroup(const LibreSCRS::Plugin::CardFieldGroup& group);

    Q_INVOKABLE void enablePrintButton();

    const LibreSCRS::Plugin::CardData& cardData() const
    {
        return data;
    }

signals:
    void printRequested(const LibreSCRS::Plugin::CardData& data);

protected:
    void retranslateUi() override;

private:
    bool isForeigner() const;
    QPixmap loadPhoto() const;
    CollapsibleSection* buildAddressSection(QWidget* parent) const;
    CollapsibleSection* buildDocumentSection(QWidget* parent) const;
    CollapsibleSection* buildPersonalSection(QWidget* parent) const;
    void addVerificationBadges(CollapsibleSection* section, const LibreSCRS::Plugin::CardFieldGroup* source = nullptr);

    LibreSCRS::Plugin::CardData data;

    // Progressive-display state
    QVBoxLayout* outerLayout = nullptr;
    CollapsibleSection* outerSection = nullptr;
    QVBoxLayout* sectionLayout = nullptr;
    QLabel* photoLabel = nullptr;
    CollapsibleSection* personalSection = nullptr;
    QToolButton* printBtn = nullptr;
};
