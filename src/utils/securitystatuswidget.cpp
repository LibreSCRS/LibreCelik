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
    retranslateUi();
}

void SecurityStatusWidget::buildLayout()
{
    mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(4);

    // Section title is set by retranslateUi() so that LanguageChange
    // paths are the single source of truth.
    section = new CollapsibleSection(QString(), this);
    section->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);

    auto* contentLayout = new QVBoxLayout();
    contentLayout->setSpacing(6);

    // Three summary rows — created with empty labels; populated by
    // refreshSummaryRows() (called from retranslateUi()).
    auto* integrityRow = createStatusRow(QString(), librecelik::utils::SecurityCheck::Status::NotPerformed);
    integrityIcon = integrityRow->findChildren<QLabel*>("icon").value(0);
    integrityLabel = integrityRow->findChildren<QLabel*>("text").value(0);
    contentLayout->addWidget(integrityRow);

    auto* authenticityRow = createStatusRow(QString(), librecelik::utils::SecurityCheck::Status::NotPerformed);
    authenticityIcon = authenticityRow->findChildren<QLabel*>("icon").value(0);
    authenticityLabel = authenticityRow->findChildren<QLabel*>("text").value(0);
    contentLayout->addWidget(authenticityRow);

    auto* genuinenessRow = createStatusRow(QString(), librecelik::utils::SecurityCheck::Status::NotPerformed);
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

void SecurityStatusWidget::refreshSummaryRows()
{
    // Update icon colour and label text from cachedStatus (or initial
    // NotPerformed when no status has been applied yet). Reads label
    // text from qtTrId at call time so language-change repopulates.
    auto updateRow = [this](QLabel* icon, QLabel* text, const QString& label,
                            librecelik::utils::SecurityCheck::Status s) {
        if (icon)
            icon->setStyleSheet(QString("background: %1; border-radius: 8px;").arg(statusColor(s)));
        if (text)
            text->setText(label + ": " + statusText(s));
    };
    using Status = librecelik::utils::SecurityCheck::Status;
    const Status integ = hasStatus ? cachedStatus.overallIntegrity : Status::NotPerformed;
    const Status auth = hasStatus ? cachedStatus.overallAuthenticity : Status::NotPerformed;
    const Status genu = hasStatus ? cachedStatus.overallGenuineness : Status::NotPerformed;
    updateRow(integrityIcon, integrityLabel, qtTrId("lc-emrtd-security-integrity"), integ);
    updateRow(authenticityIcon, authenticityLabel, qtTrId("lc-emrtd-security-authenticity"), auth);
    updateRow(genuinenessIcon, genuinenessLabel, qtTrId("lc-emrtd-security-genuineness"), genu);
}

QWidget* SecurityStatusWidget::createStatusRow(const QString& label, librecelik::utils::SecurityCheck::Status status)
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

QString SecurityStatusWidget::statusColor(librecelik::utils::SecurityCheck::Status status) const
{
    switch (status) {
    case librecelik::utils::SecurityCheck::Status::Passed:
        return QStringLiteral("#4CAF50");
    case librecelik::utils::SecurityCheck::Status::Failed:
        return QStringLiteral("#F44336");
    case librecelik::utils::SecurityCheck::Status::NotSupported:
    case librecelik::utils::SecurityCheck::Status::Skipped:
        return QStringLiteral("#FFC107");
    case librecelik::utils::SecurityCheck::Status::NotPerformed:
        return QStringLiteral("#9E9E9E");
    }
    return QStringLiteral("#9E9E9E");
}

QString SecurityStatusWidget::statusText(librecelik::utils::SecurityCheck::Status status) const
{
    switch (status) {
    case librecelik::utils::SecurityCheck::Status::Passed:
        return qtTrId("lc-emrtd-security-passed");
    case librecelik::utils::SecurityCheck::Status::Failed:
        return qtTrId("lc-emrtd-security-failed");
    case librecelik::utils::SecurityCheck::Status::NotSupported:
        return qtTrId("lc-emrtd-security-not-supported");
    case librecelik::utils::SecurityCheck::Status::Skipped:
        return qtTrId("lc-emrtd-security-skipped");
    case librecelik::utils::SecurityCheck::Status::NotPerformed:
        return qtTrId("lc-emrtd-security-not-performed");
    }
    return qtTrId("lc-emrtd-security-not-performed");
}

void SecurityStatusWidget::setSecurityStatus(const librecelik::utils::SecurityStatusModel& status)
{
    // Cache for retranslate-on-language-change (per LM 4.0 retranslate
    // pattern: rebuild dynamic content from cached state).
    cachedStatus = status;
    hasStatus = true;

    refreshSummaryRows();
    rebuildDetailRows();
}

void SecurityStatusWidget::rebuildDetailRows()
{
    // Build detail section with individual checks. Called both from
    // setSecurityStatus() (when fresh status arrives) and from
    // retranslateUi() (so the "Details" header re-renders in the new
    // language). check.label / check.detail are strings the read itself
    // supplied and are not retranslated here.
    if (detailWidget->layout()) {
        QLayoutItem* item;
        while ((item = detailWidget->layout()->takeAt(0)) != nullptr) {
            delete item->widget();
            delete item;
        }
        delete detailWidget->layout();
    }

    if (!hasStatus || cachedStatus.checks.isEmpty()) {
        detailWidget->setVisible(false);
        return;
    }

    auto* detailLayout = new QVBoxLayout(detailWidget);
    detailLayout->setContentsMargins(8, 4, 4, 4);
    detailLayout->setSpacing(2);

    auto* detailTitle = new QLabel(qtTrId("lc-emrtd-security-details"));
    detailTitle->setObjectName(QStringLiteral("detailTitle"));
    detailTitle->setStyleSheet(
        QString("font-size: 11px; font-weight: bold; color: %1;").arg(palette().color(QPalette::Text).name()));
    detailLayout->addWidget(detailTitle);

    for (const auto& check : cachedStatus.checks) {
        auto* checkRow = new QHBoxLayout();
        checkRow->setSpacing(6);

        auto* checkIcon = new QLabel();
        checkIcon->setFixedSize(10, 10);
        checkIcon->setStyleSheet(QString("background: %1; border-radius: 5px;").arg(statusColor(check.status)));

        auto* checkLabel = new QLabel(check.label);
        checkLabel->setObjectName(QStringLiteral("checkLabel"));
        checkLabel->setStyleSheet(QString("font-size: 11px; color: %1;").arg(palette().color(QPalette::Text).name()));
        checkLabel->setWordWrap(true);

        checkRow->addWidget(checkIcon);
        checkRow->addWidget(checkLabel, 1);
        detailLayout->addLayout(checkRow);

        if (!check.detail.isEmpty()) {
            auto* detailText = new QLabel(check.detail);
            detailText->setObjectName(QStringLiteral("detailText"));
            detailText->setStyleSheet(QString("font-size: 10px; color: %1; margin-left: 16px;")
                                          .arg(palette().color(QPalette::PlaceholderText).name()));
            detailText->setWordWrap(true);
            detailLayout->addWidget(detailText);
        }
    }

    detailWidget->setVisible(true);
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
    refreshSummaryRows();
    rebuildDetailRows();
}
