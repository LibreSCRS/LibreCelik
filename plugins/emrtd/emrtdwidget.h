// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0

#pragma once

#include "utils/pluginwidgetbase.h"

#include <LibreSCRS/AgentClient/Types.h>

#include <QHash>
#include <QList>
#include <QString>

#include <map>

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

    /// The annex label table, exposed so a gate can COUNT it. A key with no
    /// entry silently falls back to the plugin's English label, which is the
    /// exact complaint this table answers, so "it renders" is not enough of a
    /// check — the count is what catches an entry dropped later.
    [[nodiscard]] static std::map<QString, QString> annexTranslationMapForTest();

signals:
    void printRequested(const QList<LibreSCRS::AgentClient::FieldGroup>& groups);

protected:
    void retranslateUi() override;

private:
    void buildShell();

    /// Build (or find) the section for annex @p id, and flush any verdict that
    /// arrived before it existed.
    void addAnnexPersonal(const QString& id, const LibreSCRS::AgentClient::FieldGroup& group);
    /// Attach @p group's verdict to annex @p id's section, or hold it until the
    /// section exists.
    void addAnnexSecurity(const QString& id, const LibreSCRS::AgentClient::FieldGroup& group);

    QList<LibreSCRS::AgentClient::FieldGroup> groups;
    QToolButton* printBtn = nullptr;

    // Progressive-display state
    QVBoxLayout* outerLayout = nullptr;
    CollapsibleSection* outerSection = nullptr;
    QVBoxLayout* sectionLayout = nullptr;
    QLabel* photoLabel = nullptr;
    SecurityStatusWidget* securityStatusWidget = nullptr;
    bool noDataMessageShown = false;

    // Keyed by annex id, never single members: the group keys are DERIVED from
    // the annex's id precisely so two annexes on one card cannot collide, and a
    // lone member here would hand that collision straight back.
    QHash<QString, CollapsibleSection*> annexSections;
    QHash<QString, LibreSCRS::AgentClient::FieldGroup> pendingAnnexVerdicts;
};
