// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0

#include "securitystatuswidget.h"

#include "utils/collapsiblesection.h"

#include <QEvent>
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

    section = new CollapsibleSection(qtTrId("lc-emrtd-security-status"), this);
    section->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);

    auto* contentLayout = new QVBoxLayout();
    contentLayout->setSpacing(6);

    // Three summary rows — initially NOT_PERFORMED
    auto* integrityRow =
        createStatusRow(qtTrId("lc-emrtd-security-integrity"), LibreSCRS::Plugin::SecurityCheck::Status::NotPerformed);
    integrityIcon = integrityRow->findChildren<QLabel*>("icon").value(0);
    integrityLabel = integrityRow->findChildren<QLabel*>("text").value(0);
    contentLayout->addWidget(integrityRow);

    auto* authenticityRow = createStatusRow(qtTrId("lc-emrtd-security-authenticity"),
                                            LibreSCRS::Plugin::SecurityCheck::Status::NotPerformed);
    authenticityIcon = authenticityRow->findChildren<QLabel*>("icon").value(0);
    authenticityLabel = authenticityRow->findChildren<QLabel*>("text").value(0);
    contentLayout->addWidget(authenticityRow);

    auto* genuinenessRow = createStatusRow(qtTrId("lc-emrtd-security-genuineness"),
                                           LibreSCRS::Plugin::SecurityCheck::Status::NotPerformed);
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

QWidget* SecurityStatusWidget::createStatusRow(const QString& label, LibreSCRS::Plugin::SecurityCheck::Status status)
{
    auto* row = new QWidget();
    auto* rowLayout = new QHBoxLayout(row);
    rowLayout->setContentsMargins(4, 2, 4, 2);
    rowLayout->setSpacing(8);

    auto* icon = new QLabel(row);
    icon->setObjectName("icon");
    icon->setFixedSize(16, 16);
    icon->setStyleSheet(QString("background: %1; border-radius: 8px;").arg(statusColor(status)));
    icon->setAccessibleName(statusText(status));

    auto* text = new QLabel(label + ": " + statusText(status), row);
    text->setObjectName("text");
    text->setStyleSheet("font-size: 12px;");
    text->setAccessibleName(label + ": " + statusText(status));

    rowLayout->addWidget(icon);
    rowLayout->addWidget(text, 1);

    return row;
}

QString SecurityStatusWidget::statusColor(LibreSCRS::Plugin::SecurityCheck::Status status) const
{
    switch (status) {
    case LibreSCRS::Plugin::SecurityCheck::Status::Passed:
        return QStringLiteral("#4CAF50");
    case LibreSCRS::Plugin::SecurityCheck::Status::Failed:
        return QStringLiteral("#F44336");
    case LibreSCRS::Plugin::SecurityCheck::Status::NotSupported:
    case LibreSCRS::Plugin::SecurityCheck::Status::Skipped:
        return QStringLiteral("#FFC107");
    case LibreSCRS::Plugin::SecurityCheck::Status::NotPerformed:
        return QStringLiteral("#9E9E9E");
    }
    return QStringLiteral("#9E9E9E");
}

QString SecurityStatusWidget::statusText(LibreSCRS::Plugin::SecurityCheck::Status status) const
{
    switch (status) {
    case LibreSCRS::Plugin::SecurityCheck::Status::Passed:
        return qtTrId("lc-emrtd-security-passed");
    case LibreSCRS::Plugin::SecurityCheck::Status::Failed:
        return qtTrId("lc-emrtd-security-failed");
    case LibreSCRS::Plugin::SecurityCheck::Status::NotSupported:
        return qtTrId("lc-emrtd-security-not-supported");
    case LibreSCRS::Plugin::SecurityCheck::Status::Skipped:
        return qtTrId("lc-emrtd-security-skipped");
    case LibreSCRS::Plugin::SecurityCheck::Status::NotPerformed:
        return qtTrId("lc-emrtd-security-not-performed");
    }
    return qtTrId("lc-emrtd-security-not-performed");
}

void SecurityStatusWidget::setSecurityStatus(const LibreSCRS::Plugin::SecurityStatus& status)
{
    // Update summary rows
    auto updateRow = [this](QLabel* icon, QLabel* text, const QString& label,
                            LibreSCRS::Plugin::SecurityCheck::Status s) {
        if (icon)
            icon->setStyleSheet(QString("background: %1; border-radius: 8px;").arg(statusColor(s)));
        if (text)
            text->setText(label + ": " + statusText(s));
    };

    updateRow(integrityIcon, integrityLabel, qtTrId("lc-emrtd-security-integrity"), status.overallIntegrity);
    updateRow(authenticityIcon, authenticityLabel, qtTrId("lc-emrtd-security-authenticity"),
              status.overallAuthenticity);
    updateRow(genuinenessIcon, genuinenessLabel, qtTrId("lc-emrtd-security-genuineness"), status.overallGenuineness);

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
        detailTitle->setObjectName(QStringLiteral("detailTitle"));
        detailTitle->setStyleSheet(
            QString("font-size: 11px; font-weight: bold; color: %1;").arg(palette().color(QPalette::Text).name()));
        detailLayout->addWidget(detailTitle);

        for (const auto& check : status.checks) {
            auto* checkRow = new QHBoxLayout();
            checkRow->setSpacing(6);

            auto* checkIcon = new QLabel();
            checkIcon->setFixedSize(10, 10);
            checkIcon->setStyleSheet(QString("background: %1; border-radius: 5px;").arg(statusColor(check.status)));

            auto* checkLabel = new QLabel(QString::fromStdString(check.label));
            checkLabel->setObjectName(QStringLiteral("checkLabel"));
            checkLabel->setStyleSheet(
                QString("font-size: 11px; color: %1;").arg(palette().color(QPalette::Text).name()));
            checkLabel->setWordWrap(true);

            checkRow->addWidget(checkIcon);
            checkRow->addWidget(checkLabel, 1);
            detailLayout->addLayout(checkRow);

            if (!check.detail.empty()) {
                auto* detailText = new QLabel(QString::fromStdString(check.detail));
                detailText->setObjectName(QStringLiteral("detailText"));
                detailText->setStyleSheet(QString("font-size: 10px; color: %1; margin-left: 16px;")
                                              .arg(palette().color(QPalette::PlaceholderText).name()));
                detailText->setWordWrap(true);
                detailLayout->addWidget(detailText);
            }
        }

        detailWidget->setVisible(true);
    }
}

void SecurityStatusWidget::changeEvent(QEvent* event)
{
    if (event->type() == QEvent::LanguageChange) {
        retranslateUi();
    } else if (event->type() == QEvent::PaletteChange) {
        if (detailWidget && detailWidget->isVisible()) {
            const QString textColor = palette().color(QPalette::Text).name();
            const QString placeholderColor = palette().color(QPalette::PlaceholderText).name();

            for (auto* label : detailWidget->findChildren<QLabel*>(QStringLiteral("detailTitle")))
                label->setStyleSheet(QString("font-size: 11px; font-weight: bold; color: %1;").arg(textColor));
            for (auto* label : detailWidget->findChildren<QLabel*>(QStringLiteral("checkLabel")))
                label->setStyleSheet(QString("font-size: 11px; color: %1;").arg(textColor));
            for (auto* label : detailWidget->findChildren<QLabel*>(QStringLiteral("detailText")))
                label->setStyleSheet(QString("font-size: 10px; color: %1; margin-left: 16px;").arg(placeholderColor));
        }
    }
    QWidget::changeEvent(event);
}

void SecurityStatusWidget::retranslateUi()
{
    section->setTitle(qtTrId("lc-emrtd-security-status"));
}
