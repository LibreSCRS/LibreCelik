// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0

#pragma once

#include "utils/pluginwidgetbase.h"

#include <LibreSCRS/AgentClient/Types.h>

#include <QList>

class CollapsibleSection;
class QLabel;
class QToolButton;
class QVBoxLayout;

class EidWidget : public plugin_ui::PluginWidgetBase
{
    Q_OBJECT
public:
    // Full-data constructor (existing behaviour, unchanged)
    explicit EidWidget(const QList<LibreSCRS::AgentClient::FieldGroup>& groups, QWidget* parent = nullptr);

    // Empty-shell constructor for progressive display
    explicit EidWidget(QWidget* parent);

    // Progressive display: add one group at a time
    void addGroup(const LibreSCRS::AgentClient::FieldGroup& group);

    Q_INVOKABLE void enablePrintButton();

    const QList<LibreSCRS::AgentClient::FieldGroup>& fieldGroups() const
    {
        return groups;
    }

signals:
    void printRequested(const QList<LibreSCRS::AgentClient::FieldGroup>& groups);

protected:
    void retranslateUi() override;

private:
    bool isForeigner() const;
    QPixmap loadPhoto() const;
    CollapsibleSection* buildAddressSection(QWidget* parent) const;
    CollapsibleSection* buildDocumentSection(QWidget* parent) const;
    CollapsibleSection* buildPersonalSection(QWidget* parent) const;
    void addVerificationBadges(CollapsibleSection* section, const LibreSCRS::AgentClient::FieldGroup* source = nullptr);
    void applyVerificationFromMeta();

    QList<LibreSCRS::AgentClient::FieldGroup> groups;

    // Progressive-display state
    QVBoxLayout* outerLayout = nullptr;
    CollapsibleSection* outerSection = nullptr;
    QVBoxLayout* sectionLayout = nullptr;
    QLabel* photoLabel = nullptr;
    CollapsibleSection* personalSection = nullptr;
    QToolButton* printBtn = nullptr;
};
