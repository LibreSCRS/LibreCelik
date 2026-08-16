// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0

#pragma once

#include "utils/pluginwidgetbase.h"

#include <LibreSCRS/AgentClient/Types.h>

#include <QList>

class CollapsibleSection;
class QToolButton;
class QVBoxLayout;

class HealthWidget : public plugin_ui::PluginWidgetBase
{
    Q_OBJECT
public:
    // Full constructor — builds entire widget at once (existing behaviour)
    explicit HealthWidget(const QList<LibreSCRS::AgentClient::FieldGroup>& groups, QWidget* parent = nullptr);

    // Empty constructor — creates outer shell for progressive population via addGroup()
    explicit HealthWidget(QWidget* parent);

    // Progressive display: append a group's UI section to the widget
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
    void transformPermanentlyValid(LibreSCRS::AgentClient::FieldGroup& group);

    void addPersonalGroup(const LibreSCRS::AgentClient::FieldGroup& group);
    void addInsuranceGroup(const LibreSCRS::AgentClient::FieldGroup& group);
    void addAddressGroup(const LibreSCRS::AgentClient::FieldGroup& group);
    void addCarrierGroup(const LibreSCRS::AgentClient::FieldGroup& group);
    void addTaxpayerGroup(const LibreSCRS::AgentClient::FieldGroup& group);

    QList<LibreSCRS::AgentClient::FieldGroup> groups;
    QVBoxLayout* outerLayout = nullptr;
    CollapsibleSection* outerSection = nullptr;
    QVBoxLayout* contentLayout = nullptr;
    QToolButton* printBtn = nullptr;
};
