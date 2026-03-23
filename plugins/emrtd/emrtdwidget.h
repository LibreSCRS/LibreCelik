// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright hirashix0@proton.me

#pragma once

#include <plugin/card_data.h>

#include <QWidget>

class CollapsibleSection;
class QLabel;
class QLineEdit;
class QToolButton;
class QVBoxLayout;

class EMRTDWidget : public QWidget
{
    Q_OBJECT
public:
    // Full-data constructor (existing behaviour, unchanged)
    explicit EMRTDWidget(const plugin::CardData& data, QWidget* parent = nullptr);

    // Empty-shell constructor for progressive display
    explicit EMRTDWidget(QWidget* parent);

    // Progressive display: add one group at a time
    void addGroup(const plugin::CardFieldGroup& group);

    Q_INVOKABLE void enablePrintButton();

    const plugin::CardData& cardData() const
    {
        return data;
    }

signals:
    void printRequested(const plugin::CardData& data);

private:
    void showAuthRequired(const plugin::CardFieldGroup* group);
    void showPersonalData(const plugin::CardData& cardData);
    void showError(const plugin::CardFieldGroup* group);

    plugin::CardData data;
    QToolButton* printBtn = nullptr;

    // Progressive-display state
    QVBoxLayout* outerLayout = nullptr;
    CollapsibleSection* outerSection = nullptr;
    QVBoxLayout* sectionLayout = nullptr;
    QLabel* photoLabel = nullptr;
};
