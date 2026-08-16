// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0

#pragma once

#include "utils/pluginwidgetbase.h"

#include <LibreSCRS/AgentClient/Types.h>

#include <QList>

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
    explicit EMRTDWidget(const QList<LibreSCRS::AgentClient::FieldGroup>& groups, QWidget* parent = nullptr);

    // Empty-shell constructor for progressive display
    explicit EMRTDWidget(QWidget* parent);

    // Progressive display: add one group at a time
    void addGroup(const LibreSCRS::AgentClient::FieldGroup& group);

    Q_INVOKABLE void enablePrintButton();
    void showNoDataMessage();

    const QList<LibreSCRS::AgentClient::FieldGroup>& fieldGroups() const
    {
        return groups;
    }

signals:
    void printRequested(const QList<LibreSCRS::AgentClient::FieldGroup>& groups);

protected:
    void retranslateUi() override;

private:
    void buildShell();

    QList<LibreSCRS::AgentClient::FieldGroup> groups;
    QToolButton* printBtn = nullptr;

    // Progressive-display state
    QVBoxLayout* outerLayout = nullptr;
    CollapsibleSection* outerSection = nullptr;
    QVBoxLayout* sectionLayout = nullptr;
    QLabel* photoLabel = nullptr;
    SecurityStatusWidget* securityStatusWidget = nullptr;
    bool noDataMessageShown = false;
};
