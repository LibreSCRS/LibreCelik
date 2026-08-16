// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0

#pragma once

#include "utils/pluginwidgetbase.h"

#include <LibreSCRS/AgentClient/Types.h>

#include <QList>

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
    explicit PIVWidget(const QList<LibreSCRS::AgentClient::FieldGroup>& groups, QWidget* parent = nullptr);
    explicit PIVWidget(QWidget* parent);

    void addGroup(const LibreSCRS::AgentClient::FieldGroup& group);

    const QList<LibreSCRS::AgentClient::FieldGroup>& fieldGroups() const
    {
        return groups;
    }

    Q_INVOKABLE void enablePrintButton();

signals:
    void printRequested(const QList<LibreSCRS::AgentClient::FieldGroup>& groups);

protected:
    void retranslateUi() override;

private:
    void buildEmptyShell();
    void addChuidGroup(const LibreSCRS::AgentClient::FieldGroup& group);
    void addCccGroup(const LibreSCRS::AgentClient::FieldGroup& group);
    void addPrintedGroup(const LibreSCRS::AgentClient::FieldGroup& group);
    void addDiscoveryGroup(const LibreSCRS::AgentClient::FieldGroup& group);
    void addKeyHistoryGroup(const LibreSCRS::AgentClient::FieldGroup& group);
    void rebuildHeader();

    QList<LibreSCRS::AgentClient::FieldGroup> groups;
    QVBoxLayout* outerLayout = nullptr;
    QVBoxLayout* contentLayout = nullptr;
    CollapsibleSection* outerSection = nullptr;
    QToolButton* printBtn = nullptr;
    librecelik::utils::CardHeaderCard* headerCard = nullptr;
};
