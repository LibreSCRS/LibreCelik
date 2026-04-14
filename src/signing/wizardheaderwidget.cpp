// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0

#include "wizardheaderwidget.h"

#include <QEvent>
#include <QFont>
#include <QPainter>
#include <QPalette>

//: Step 1 label in wizard header
//% "Files"
static const char* const trIdStepFiles = QT_TRID_NOOP("lc-sign-step-files");

//: Step 2 label in wizard header
//% "Place"
static const char* const trIdStepPlace = QT_TRID_NOOP("lc-sign-step-place");

//: Step 3 label in wizard header
//% "Sign"
static const char* const trIdStepSign = QT_TRID_NOOP("lc-sign-step-sign");

WizardHeaderWidget::WizardHeaderWidget(QWidget* parent) : QWidget(parent)
{
    setFixedHeight(52);
    rebuildStepLabels();
    rebuildColors();
}

void WizardHeaderWidget::setTitle(const QString& t)
{
    title = t;
    update();
}

void WizardHeaderWidget::setSubtitle(const QString& s)
{
    subtitle = s;
    update();
}

void WizardHeaderWidget::setCurrentStep(int step)
{
    currentStep = qBound(0, step, static_cast<int>(steps.size()) - 1);
    update();
}

void WizardHeaderWidget::setPlacementShown(bool shown)
{
    placementShown = shown;
    update();
}

void WizardHeaderWidget::setAllComplete(bool complete)
{
    allComplete = complete;
    update();
}

void WizardHeaderWidget::rebuildStepLabels()
{
    steps = {{qtTrId(trIdStepFiles)}, {qtTrId(trIdStepPlace)}, {qtTrId(trIdStepSign)}};
    update();
}

void WizardHeaderWidget::rebuildColors()
{
    auto pal = palette();
    textColor = pal.color(QPalette::Text);
    placeholderColor = pal.color(QPalette::PlaceholderText);
    midColor = pal.color(QPalette::Mid);
}

bool WizardHeaderWidget::event(QEvent* event)
{
    if (event->type() == QEvent::PaletteChange)
        rebuildColors();
    if (event->type() == QEvent::LanguageChange)
        rebuildStepLabels();
    return QWidget::event(event);
}

void WizardHeaderWidget::paintEvent(QPaintEvent* /*event*/)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    const int w = width();
    const int h = height();

    // --- Left side: title + subtitle ---
    QFont titleFont = font();
    titleFont.setPointSize(12);
    titleFont.setBold(true);
    p.setFont(titleFont);
    p.setPen(textColor);

    const int leftMargin = 12;
    QRect titleRect(leftMargin, 4, w / 2, 22);
    p.drawText(titleRect, Qt::AlignLeft | Qt::AlignVCenter, title);

    QFont subFont = font();
    subFont.setPixelSize(11);
    p.setFont(subFont);
    p.setPen(placeholderColor);

    QRect subRect(leftMargin, 26, w / 2, 18);
    p.drawText(subRect, Qt::AlignLeft | Qt::AlignVCenter, subtitle);

    // --- Right side: step indicator ---
    // Build visible steps list (skip step 2 "Place" when placement not shown)
    struct VisibleStep
    {
        int originalIndex;
        QString label;
    };
    QVector<VisibleStep> visibleSteps;
    for (int i = 0; i < steps.size(); ++i) {
        if (i == 1 && !placementShown)
            continue; // skip "Place" step for non-PDF
        visibleSteps.append({i, steps[i].label});
    }

    const int visCount = visibleSteps.size();
    const int circleRadius = 10;
    const int stepSpacing = 70;
    const int totalWidth = (visCount - 1) * stepSpacing;
    const int rightMargin = 16;
    const int startX = w - rightMargin - totalWidth - circleRadius;
    const int centerY = h / 2 - 2;

    QFont stepFont = font();
    stepFont.setPixelSize(10);
    stepFont.setBold(true);

    for (int vi = 0; vi < visCount; ++vi) {
        const auto& vs = visibleSteps[vi];
        int cx = startX + vi * stepSpacing;

        // Draw connecting line to next step
        if (vi < visCount - 1) {
            int nextCx = startX + (vi + 1) * stepSpacing;
            p.setPen(QPen(QColor(34, 86, 117), 1.5));
            p.drawLine(cx + circleRadius + 2, centerY, nextCx - circleRadius - 2, centerY);
        }

        bool isCompleted = allComplete || vs.originalIndex < currentStep;
        bool isActive = !allComplete && vs.originalIndex == currentStep;

        if (isCompleted) {
            // Completed: teal circle with checkmark
            p.setPen(Qt::NoPen);
            p.setBrush(tealColor);
            p.drawEllipse(QPoint(cx, centerY), circleRadius, circleRadius);

            p.setPen(QPen(Qt::white, 1.5));
            p.drawLine(cx - 4, centerY, cx - 1, centerY + 3);
            p.drawLine(cx - 1, centerY + 3, cx + 4, centerY - 3);
        } else if (isActive) {
            // Active: teal filled circle with white number
            p.setPen(Qt::NoPen);
            p.setBrush(tealColor);
            p.drawEllipse(QPoint(cx, centerY), circleRadius, circleRadius);

            p.setPen(Qt::white);
            p.setFont(stepFont);
            p.drawText(QRect(cx - circleRadius, centerY - circleRadius, circleRadius * 2, circleRadius * 2),
                       Qt::AlignCenter, QString::number(vi + 1));
        } else {
            // Future: Mid color border circle
            p.setPen(QPen(midColor, 1.5));
            p.setBrush(Qt::NoBrush);
            p.drawEllipse(QPoint(cx, centerY), circleRadius, circleRadius);

            p.setPen(placeholderColor);
            p.setFont(stepFont);
            p.drawText(QRect(cx - circleRadius, centerY - circleRadius, circleRadius * 2, circleRadius * 2),
                       Qt::AlignCenter, QString::number(vi + 1));
        }

        // Step label below circle
        QFont labelFont = font();
        labelFont.setPixelSize(10);
        p.setFont(labelFont);
        p.setPen(isActive ? textColor : placeholderColor);
        QRect labelRect(cx - stepSpacing / 2, centerY + circleRadius + 2, stepSpacing, 14);
        p.drawText(labelRect, Qt::AlignHCenter | Qt::AlignTop, vs.label);
    }

    // --- 3px teal bottom border ---
    p.setPen(Qt::NoPen);
    p.setBrush(tealColor);
    p.drawRect(0, h - 3, w, 3);
}
