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

class EuVrcWidget : public plugin_ui::PluginWidgetBase
{
    Q_OBJECT
public:
    // Full constructor — builds entire widget from a complete field-group read
    explicit EuVrcWidget(const QList<LibreSCRS::AgentClient::FieldGroup>& groups, QWidget* parent = nullptr);

    // Empty shell constructor — for progressive display
    explicit EuVrcWidget(QWidget* parent);

    // Progressive display: add a group incrementally
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
    void buildShell();
    void addRegistrationGroup(const LibreSCRS::AgentClient::FieldGroup& group);
    void addVehicleGroup(const LibreSCRS::AgentClient::FieldGroup& group);
    void addHolderGroup(const LibreSCRS::AgentClient::FieldGroup& group);
    void addUserGroup(const LibreSCRS::AgentClient::FieldGroup& group);
    void addNationalGroup(const LibreSCRS::AgentClient::FieldGroup& group);
    void addOwnerGroup(const LibreSCRS::AgentClient::FieldGroup& group);
    CollapsibleSection* buildRegistrationSection(const LibreSCRS::AgentClient::FieldGroup* group);
    CollapsibleSection* buildVehicleSection(const LibreSCRS::AgentClient::FieldGroup* group);
    CollapsibleSection* buildEngineTechnicalSection(const LibreSCRS::AgentClient::FieldGroup* group);
    CollapsibleSection* buildHolderSection(const LibreSCRS::AgentClient::FieldGroup* group);
    CollapsibleSection* buildUserSection(const LibreSCRS::AgentClient::FieldGroup* group);
    CollapsibleSection* buildNationalSection(const LibreSCRS::AgentClient::FieldGroup* group);
    CollapsibleSection* buildOwnerSection(const LibreSCRS::AgentClient::FieldGroup* group);

    QList<LibreSCRS::AgentClient::FieldGroup> groups;
    QVBoxLayout* outerLayout = nullptr;
    QVBoxLayout* contentLayout = nullptr;
    CollapsibleSection* outerSection = nullptr;
    QToolButton* printBtn = nullptr;
    librecelik::utils::CardHeaderCard* headerCard = nullptr;
};
