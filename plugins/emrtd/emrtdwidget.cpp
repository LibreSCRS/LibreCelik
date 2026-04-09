// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright hirashix0@proton.me

#include "emrtdwidget.h"

#include "utils/collapsiblesection.h"
#include "utils/fieldsectionbuilder.h"
#include "utils/securitystatuswidget.h"

#include <QHBoxLayout>
#include <QIcon>
#include <QPainter>
#include <QToolButton>

#include <plugin/carddatautils.h>
#include <plugin/security_check.h>

#include <QLabel>
#include <QLineEdit>
#include <QPainter>
#include <QPixmap>
#include <QVBoxLayout>

using plugin::getFieldValue;

namespace {

// Translation maps as functions — qtTrId() must be called at runtime (not static init)
// to support runtime language switching.
std::map<std::string, QString> documentTranslationMap()
{
    return {
        {"document_number", qtTrId("lc-emrtd-doc-number")},      {"document_code", qtTrId("lc-emrtd-doc-code")},
        {"issuing_state", qtTrId("lc-emrtd-issuing-state")},     {"date_of_expiry", qtTrId("lc-emrtd-date-of-expiry")},
        {"personal_number", qtTrId("lc-emrtd-personal-number")},
    };
}

std::map<std::string, QString> documentExtraTranslationMap()
{
    return {
        {"issuing_authority", qtTrId("lc-emrtd-issuing-authority")},
        {"date_of_issue", qtTrId("lc-emrtd-date-of-issue")},
        {"endorsements", qtTrId("lc-emrtd-endorsements")},
        {"tax_exit", qtTrId("lc-emrtd-tax-exit")},
    };
}

std::map<std::string, QString> personalTranslationMap()
{
    return {
        {"given_names", qtTrId("lc-emrtd-given-names")},
        {"surname", qtTrId("lc-emrtd-surname")},
        {"nationality", qtTrId("lc-emrtd-nationality")},
        {"date_of_birth", qtTrId("lc-emrtd-date-of-birth")},
    };
}

std::map<std::string, QString> additionalTranslationMap()
{
    return {
        {"full_name", qtTrId("lc-emrtd-full-name")},
        {"other_names", qtTrId("lc-emrtd-other-names")},
        {"personal_number", qtTrId("lc-emrtd-personal-number")},
        {"place_of_birth", qtTrId("lc-emrtd-place-of-birth")},
        {"address", qtTrId("lc-emrtd-address")},
        {"telephone", qtTrId("lc-emrtd-telephone")},
        {"profession", qtTrId("lc-emrtd-profession")},
        {"title", qtTrId("lc-emrtd-title")},
        {"custody_info", qtTrId("lc-emrtd-custody-info")},
    };
}

std::map<std::string, QString> contactsTranslationMap()
{
    return {
        {"name", qtTrId("lc-emrtd-contact-name")},
        {"telephone", qtTrId("lc-emrtd-telephone")},
        {"address", qtTrId("lc-emrtd-address")},
    };
}

std::map<std::string, QString> nationalTranslationMap()
{
    return {
        {"tag", qtTrId("lc-emrtd-national-tag")},
        {"value", qtTrId("lc-emrtd-national-value")},
    };
}

} // namespace

EMRTDWidget::EMRTDWidget(const plugin::CardData& cardData, QWidget* parent) : EMRTDWidget(parent)
{
    data.cardType = cardData.cardType;
    for (const auto& group : cardData.groups)
        addGroup(group);
}

EMRTDWidget::EMRTDWidget(QWidget* parent) : QWidget(parent)
{
    outerLayout = new QVBoxLayout(this);
    outerLayout->setContentsMargins(0, 0, 0, 0);

    // Navy outer CollapsibleSection — shell for Phase 2 travel document display
    static const QColor navy(34, 86, 117);
    outerSection = new CollapsibleSection(qtTrId("lc-emrtd-travel-document"), navy, this);
    outerSection->setHeaderHeight(56);

    sectionLayout = new QVBoxLayout();
    sectionLayout->setSpacing(6);

    outerSection->setLayout(sectionLayout);
    outerSection->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    outerLayout->addWidget(outerSection);

    // Print button — disabled (dimmed) until all streaming completes
    printBtn = new QToolButton(this);
    {
        QIcon icon(QStringLiteral(":/images/printer-header.svg"));
        auto normalPix = icon.pixmap(24, 24);
        QPixmap dimPix(normalPix.size());
        dimPix.fill(Qt::transparent);
        QPainter p(&dimPix);
        p.setOpacity(0.3);
        p.drawPixmap(0, 0, normalPix);
        p.end();
        icon.addPixmap(dimPix, QIcon::Disabled);
        printBtn->setIcon(icon);
    }
    printBtn->setIconSize(QSize(24, 24));
    printBtn->setToolTip(qtTrId("lc-print-tooltip"));
    printBtn->setAutoRaise(true);
    printBtn->setEnabled(false);
    connect(printBtn, &QToolButton::clicked, this, [this]() { emit printRequested(data); });
    outerSection->addHeaderWidget(printBtn);
}

void EMRTDWidget::addGroup(const plugin::CardFieldGroup& group)
{
    const auto& key = group.groupKey;

    // Phase 1 groups are handled by createWidget — ignore here
    if (key == "auth_required" || key == "error")
        return;

    // Store the group for cardData() access
    data.groups.push_back(group);

    if (key == "personal") {
        auto* photoRow = new QHBoxLayout();
        photoRow->setSpacing(10);

        photoLabel = new QLabel(outerSection);
        QPixmap placeholder(190, 250);
        placeholder.fill(QColor(220, 220, 220));
        {
            QPixmap userIcon(QStringLiteral(":/images/user.png"));
            auto scaled = userIcon.scaled(QSize(95, 125), Qt::KeepAspectRatio, Qt::SmoothTransformation);
            QPainter painter(&placeholder);
            painter.drawPixmap((190 - scaled.width()) / 2, (250 - scaled.height()) / 2, scaled);
        }
        photoLabel->setPixmap(placeholder);
        photoLabel->setFixedSize(190, 250);
        photoRow->addWidget(photoLabel, 0, Qt::AlignTop);

        auto* personalSec = LibreSCRS::FieldSectionBuilder::build(qtTrId("lc-personal-data-title"), group,
                                                                  personalTranslationMap(), {}, outerSection);
        personalSec->setCollapsible(false);
        photoRow->addWidget(personalSec, 1);

        sectionLayout->insertLayout(0, photoRow);
    } else if (key == "document") {
        auto* docSection =
            LibreSCRS::FieldSectionBuilder::build(qtTrId("lc-emrtd-document-data"), group, documentTranslationMap());
        LibreSCRS::FieldSectionBuilder::highlightExpiredDates(docSection, group, {"date_of_expiry"});
        sectionLayout->addWidget(docSection);
    } else if (key == "photo") {
        if (!photoLabel || group.fields.empty() || group.fields[0].value.empty())
            return;
        QPixmap photo;
        photo.loadFromData(group.fields[0].value.data(), static_cast<uint>(group.fields[0].value.size()));
        if (!photo.isNull()) {
            auto scaledPhoto = photo.scaledToHeight(250, Qt::SmoothTransformation);
            photoLabel->setFixedSize(scaledPhoto.size());
            photoLabel->setPixmap(scaledPhoto);
        }
    } else if (key == "signature") {
        if (!group.fields.empty() && !group.fields[0].value.empty()) {
            auto* sigSection = new CollapsibleSection(qtTrId("lc-emrtd-signature"), outerSection);
            auto* sigLayout = new QVBoxLayout();
            auto* sigLabel = new QLabel();
            QPixmap sigPixmap;
            sigPixmap.loadFromData(group.fields[0].value.data(), static_cast<uint>(group.fields[0].value.size()));
            sigLabel->setPixmap(sigPixmap.scaled(200, 80, Qt::KeepAspectRatio, Qt::SmoothTransformation));
            sigLabel->setAlignment(Qt::AlignCenter);
            sigLayout->addWidget(sigLabel);
            sigSection->setLayout(sigLayout);
            sectionLayout->addWidget(sigSection);
        }
    } else if (key == "additional") {
        auto* additionalSection =
            LibreSCRS::FieldSectionBuilder::build(qtTrId("lc-emrtd-additional"), group, additionalTranslationMap());
        sectionLayout->addWidget(additionalSection);
    } else if (key == "document_extra") {
        // If "additional" already added, show as separate "Issuing Information" section
        // Otherwise show as "Additional"
        bool hasAdditional = data.findGroup("additional") != nullptr;
        if (hasAdditional) {
            auto* extraSection = LibreSCRS::FieldSectionBuilder::build(qtTrId("lc-emrtd-issuing-info"), group,
                                                                       documentExtraTranslationMap());
            sectionLayout->addWidget(extraSection);
        } else {
            auto* extraSection = LibreSCRS::FieldSectionBuilder::build(qtTrId("lc-emrtd-additional"), group,
                                                                       documentExtraTranslationMap());
            sectionLayout->addWidget(extraSection);
        }
    } else if (key == "presence") {
        // Presence group — informational, no widget needed
    } else if (key == "portrait") {
        // DG5 portrait image — similar to signature display
        if (!group.fields.empty() && !group.fields[0].value.empty()) {
            auto* portraitSection = new CollapsibleSection(qtTrId("lc-emrtd-portrait"), outerSection);
            auto* portraitLayout = new QVBoxLayout();
            auto* portraitLabel = new QLabel();
            QPixmap portraitPixmap;
            portraitPixmap.loadFromData(group.fields[0].value.data(), static_cast<uint>(group.fields[0].value.size()));
            if (!portraitPixmap.isNull()) {
                portraitLabel->setPixmap(
                    portraitPixmap.scaled(200, 250, Qt::KeepAspectRatio, Qt::SmoothTransformation));
            }
            portraitLabel->setAlignment(Qt::AlignCenter);
            portraitLayout->addWidget(portraitLabel);
            portraitSection->setLayout(portraitLayout);
            sectionLayout->addWidget(portraitSection);
        }
    } else if (key == "contacts") {
        auto* contactsSection =
            LibreSCRS::FieldSectionBuilder::build(qtTrId("lc-emrtd-contacts"), group, contactsTranslationMap());
        sectionLayout->addWidget(contactsSection);
    } else if (key == "biometric_fingerprint" || key == "biometric_iris") {
        QString title = (key == "biometric_fingerprint") ? qtTrId("lc-emrtd-biometric-fingerprint")
                                                         : qtTrId("lc-emrtd-biometric-iris");
        auto* bioSection = new CollapsibleSection(title, outerSection);
        auto* bioLayout = new QVBoxLayout();
        auto* bioLabel = new QLabel(qtTrId("lc-emrtd-biometric-eac-required"));
        bioLabel->setStyleSheet("color: #E6873C; font-style: italic; padding: 8px;");
        bioLabel->setWordWrap(true);
        bioLayout->addWidget(bioLabel);
        bioSection->setLayout(bioLayout);
        sectionLayout->addWidget(bioSection);
    } else if (key == "security_status") {
        // Parse fields back into SecurityStatus struct
        plugin::SecurityStatus secStatus;
        for (const auto& field : group.fields) {
            if (field.key == "overall_integrity") {
                secStatus.overallIntegrity = plugin::statusFromString(field.asString());
            } else if (field.key == "overall_authenticity") {
                secStatus.overallAuthenticity = plugin::statusFromString(field.asString());
            } else if (field.key == "overall_genuineness") {
                secStatus.overallGenuineness = plugin::statusFromString(field.asString());
            } else if (field.key.starts_with("check_")) {
                // Individual check fields: check_N_id, check_N_category, etc.
                // Parse grouped by index
                auto suffix = field.key.substr(field.key.find('_', 6) + 1);
                auto idxStr = field.key.substr(6, field.key.find('_', 6) - 6);
                size_t idx = 0;
                try {
                    idx = std::stoul(idxStr);
                } catch (...) {
                    continue;
                }
                while (secStatus.checks.size() <= idx)
                    secStatus.checks.emplace_back();
                auto& check = secStatus.checks[idx];
                if (suffix == "id")
                    check.checkId = field.asString();
                else if (suffix == "category")
                    check.category = field.asString();
                else if (suffix == "status")
                    check.status = plugin::statusFromString(field.asString());
                else if (suffix == "label")
                    check.label = field.asString();
                else if (suffix == "detail")
                    check.detail = field.asString();
                else if (suffix == "error")
                    check.errorDetail = field.asString();
            }
        }
        if (!securityStatusWidget) {
            securityStatusWidget = new SecurityStatusWidget(outerSection);
        }
        securityStatusWidget->setSecurityStatus(secStatus);
        securityStatusWidget->setVisible(true);
        // Always insert at top of section
        sectionLayout->insertWidget(0, securityStatusWidget);
    } else if (key == "national") {
        auto* nationalSection =
            LibreSCRS::FieldSectionBuilder::build(qtTrId("lc-emrtd-national-data"), group, nationalTranslationMap());
        sectionLayout->addWidget(nationalSection);
    }
}

void EMRTDWidget::showAuthRequired(const plugin::CardFieldGroup* group)
{
    auto* layout = qobject_cast<QVBoxLayout*>(this->layout());

    auto* titleLabel = new QLabel(qtTrId("lc-emrtd-auth-required"));
    titleLabel->setStyleSheet("font-size: 16px; font-weight: bold; color: #E6873C;");
    layout->addWidget(titleLabel);

    auto* statusLabel = new QLabel(getFieldValue(group, "status"));
    layout->addWidget(statusLabel);

    auto paceSupported = getFieldValue(group, "pace_supported");
    if (paceSupported == "true") {
        auto* paceLabel = new QLabel("PACE: " + getFieldValue(group, "pace_oids"));
        paceLabel->setWordWrap(true);
        layout->addWidget(paceLabel);
    }

    auto* infoLabel = new QLabel(qtTrId("lc-emrtd-insert-mrz-hint"));
    infoLabel->setWordWrap(true);
    layout->addWidget(infoLabel);

    layout->addStretch();
}

void EMRTDWidget::showError(const plugin::CardFieldGroup* group)
{
    auto* layout = qobject_cast<QVBoxLayout*>(this->layout());

    auto* errorLabel = new QLabel(qtTrId("lc-emrtd-auth-failed"));
    errorLabel->setStyleSheet("font-size: 16px; font-weight: bold; color: #E6873C;");
    layout->addWidget(errorLabel);

    auto* detailLabel = new QLabel(getFieldValue(group, "error"));
    detailLabel->setWordWrap(true);
    layout->addWidget(detailLabel);

    layout->addStretch();
}

void EMRTDWidget::showNoDataMessage()
{
    auto* msgLabel = new QLabel(qtTrId("lc-emrtd-no-data-message"), outerSection);
    msgLabel->setWordWrap(true);
    msgLabel->setStyleSheet("color: #CC3333; font-size: 13px; padding: 12px;");
    sectionLayout->addWidget(msgLabel);
}

void EMRTDWidget::enablePrintButton()
{
    if (printBtn) {
        printBtn->setEnabled(true);
    }
}
