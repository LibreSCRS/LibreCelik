// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0

#pragma once

#include "signingcolors.h"

#include <QWidget>

class WizardHeaderWidget : public QWidget
{
    Q_OBJECT
public:
    explicit WizardHeaderWidget(QWidget* parent = nullptr);

    void setTitle(const QString& title);
    void setSubtitle(const QString& subtitle);
    void setCurrentStep(int step);
    void setPlacementShown(bool shown);
    void setAllComplete(bool complete);

protected:
    void paintEvent(QPaintEvent* event) override;
    bool event(QEvent* event) override;

private:
    struct StepInfo
    {
        QString label;
    };

    void rebuildStepLabels();
    void rebuildColors();

    QString title;
    QString subtitle;
    int currentStep = 0;
    bool placementShown = true;
    bool allComplete = false;
    QVector<StepInfo> steps;

    QColor tealColor = signing::kTealColor;
    QColor textColor;
    QColor placeholderColor;
    QColor midColor;
};
