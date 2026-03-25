// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright hirashix0@proton.me

#include "securitystatuswidget.h"

#include "utils/collapsiblesection.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QVBoxLayout>

SecurityStatusWidget::SecurityStatusWidget(QWidget* parent) : QWidget(parent)
{
    buildLayout();
}

void SecurityStatusWidget::buildLayout()
{
    mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(4);

    static const QColor headerColor(55, 71, 79); // blue-grey 800
    section = new CollapsibleSection(qtTrId("lc-emrtd-security-status"), headerColor, this);
    section->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);

    auto* contentLayout = new QVBoxLayout();
    contentLayout->setSpacing(6);

    // Three summary rows — initially NOT_PERFORMED
    auto* integrityRow = createStatusRow(qtTrId("lc-emrtd-security-integrity"),
                                         plugin::SecurityCheck::NOT_PERFORMED);
    integrityIcon = integrityRow->findChildren<QLabel*>("icon").value(0);
    integrityLabel = integrityRow->findChildren<QLabel*>("text").value(0);
    contentLayout->addWidget(integrityRow);

    auto* authenticityRow = createStatusRow(qtTrId("lc-emrtd-security-authenticity"),
                                            plugin::SecurityCheck::NOT_PERFORMED);
    authenticityIcon = authenticityRow->findChildren<QLabel*>("icon").value(0);
    authenticityLabel = authenticityRow->findChildren<QLabel*>("text").value(0);
    contentLayout->addWidget(authenticityRow);

    auto* genuinenessRow = createStatusRow(qtTrId("lc-emrtd-security-genuineness"),
                                           plugin::SecurityCheck::NOT_PERFORMED);
    genuinenessIcon = genuinenessRow->findChildren<QLabel*>("icon").value(0);
    genuinenessLabel = genuinenessRow->findChildren<QLabel*>("text").value(0);
    contentLayout->addWidget(genuinenessRow);

    // Detail widget — populated when setSecurityStatus is called
    detailWidget = new QWidget(section);
    detailWidget->setVisible(false);
    contentLayout->addWidget(detailWidget);

    section->setLayout(contentLayout);
    mainLayout->addWidget(section);
}

QWidget* SecurityStatusWidget::createStatusRow(const QString& label, plugin::SecurityCheck::Status status)
{
    auto* row = new QWidget();
    auto* rowLayout = new QHBoxLayout(row);
    rowLayout->setContentsMargins(4, 2, 4, 2);
    rowLayout->setSpacing(8);

    auto* icon = new QLabel(row);
    icon->setObjectName("icon");
    icon->setFixedSize(16, 16);
    icon->setStyleSheet(QString("background: %1; border-radius: 8px;").arg(statusColor(status)));

    auto* text = new QLabel(label + ": " + statusText(status), row);
    text->setObjectName("text");
    text->setStyleSheet("font-size: 12px;");

    rowLayout->addWidget(icon);
    rowLayout->addWidget(text, 1);

    return row;
}

QString SecurityStatusWidget::statusColor(plugin::SecurityCheck::Status status) const
{
    switch (status) {
    case plugin::SecurityCheck::PASSED: return QStringLiteral("#4CAF50");
    case plugin::SecurityCheck::FAILED: return QStringLiteral("#F44336");
    case plugin::SecurityCheck::NOT_SUPPORTED:
    case plugin::SecurityCheck::SKIPPED: return QStringLiteral("#FFC107");
    case plugin::SecurityCheck::NOT_PERFORMED: return QStringLiteral("#9E9E9E");
    }
    return QStringLiteral("#9E9E9E");
}

QString SecurityStatusWidget::statusText(plugin::SecurityCheck::Status status) const
{
    switch (status) {
    case plugin::SecurityCheck::PASSED: return qtTrId("lc-emrtd-security-passed");
    case plugin::SecurityCheck::FAILED: return qtTrId("lc-emrtd-security-failed");
    case plugin::SecurityCheck::NOT_SUPPORTED: return qtTrId("lc-emrtd-security-not-supported");
    case plugin::SecurityCheck::SKIPPED: return qtTrId("lc-emrtd-security-skipped");
    case plugin::SecurityCheck::NOT_PERFORMED: return qtTrId("lc-emrtd-security-not-performed");
    }
    return qtTrId("lc-emrtd-security-not-performed");
}

void SecurityStatusWidget::setSecurityStatus(const plugin::SecurityStatus& status)
{
    // Update summary rows
    auto updateRow = [this](QLabel* icon, QLabel* text, const QString& label,
                            plugin::SecurityCheck::Status s) {
        if (icon)
            icon->setStyleSheet(QString("background: %1; border-radius: 8px;").arg(statusColor(s)));
        if (text)
            text->setText(label + ": " + statusText(s));
    };

    updateRow(integrityIcon, integrityLabel,
              qtTrId("lc-emrtd-security-integrity"), status.overallIntegrity);
    updateRow(authenticityIcon, authenticityLabel,
              qtTrId("lc-emrtd-security-authenticity"), status.overallAuthenticity);
    updateRow(genuinenessIcon, genuinenessLabel,
              qtTrId("lc-emrtd-security-genuineness"), status.overallGenuineness);

    // Build detail section with individual checks
    if (detailWidget->layout()) {
        QLayoutItem* item;
        while ((item = detailWidget->layout()->takeAt(0)) != nullptr) {
            delete item->widget();
            delete item;
        }
        delete detailWidget->layout();
    }

    if (!status.checks.empty()) {
        auto* detailLayout = new QVBoxLayout(detailWidget);
        detailLayout->setContentsMargins(8, 4, 4, 4);
        detailLayout->setSpacing(2);

        auto* detailTitle = new QLabel(qtTrId("lc-emrtd-security-details"));
        detailTitle->setStyleSheet("font-size: 11px; font-weight: bold; color: #555;");
        detailLayout->addWidget(detailTitle);

        for (const auto& check : status.checks) {
            auto* checkRow = new QHBoxLayout();
            checkRow->setSpacing(6);

            auto* checkIcon = new QLabel();
            checkIcon->setFixedSize(10, 10);
            checkIcon->setStyleSheet(
                QString("background: %1; border-radius: 5px;").arg(statusColor(check.status)));

            auto* checkLabel = new QLabel(QString::fromStdString(check.label));
            checkLabel->setStyleSheet("font-size: 11px; color: #666;");
            checkLabel->setWordWrap(true);

            checkRow->addWidget(checkIcon);
            checkRow->addWidget(checkLabel, 1);
            detailLayout->addLayout(checkRow);

            if (!check.detail.empty()) {
                auto* detailText = new QLabel(QString::fromStdString(check.detail));
                detailText->setStyleSheet("font-size: 10px; color: #888; margin-left: 16px;");
                detailText->setWordWrap(true);
                detailLayout->addWidget(detailText);
            }
        }

        detailWidget->setVisible(true);
    }
}
