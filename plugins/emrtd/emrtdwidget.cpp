// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright hirashix0@proton.me

#include "emrtdwidget.h"

#include <plugin/carddatautils.h>

#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QPixmap>
#include <QVBoxLayout>

using plugin::getFieldValue;

EMRTDWidget::EMRTDWidget(const plugin::CardData& data, QWidget* parent) : QWidget(parent)
{
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(8, 8, 8, 8);

    // Phase 1: auth required — show status
    if (const auto* authGroup = data.findGroup("auth_required")) {
        showAuthRequired(authGroup);
        return;
    }

    // Error state
    if (const auto* errorGroup = data.findGroup("error")) {
        showError(errorGroup);
        return;
    }

    // Phase 2: authenticated — show passport data
    showPersonalData(data);
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

void EMRTDWidget::showPersonalData(const plugin::CardData& data)
{
    auto* layout = qobject_cast<QVBoxLayout*>(this->layout());

    // Photo + personal data side by side
    auto* topLayout = new QHBoxLayout();

    // Photo
    if (const auto* photoGroup = data.findGroup("photo")) {
        if (!photoGroup->fields.empty() && !photoGroup->fields[0].value.empty()) {
            auto* photoLabel = new QLabel();
            QPixmap pixmap;
            pixmap.loadFromData(photoGroup->fields[0].value.data(),
                                static_cast<uint>(photoGroup->fields[0].value.size()));
            photoLabel->setPixmap(pixmap.scaled(180, 240, Qt::KeepAspectRatio, Qt::SmoothTransformation));
            photoLabel->setFixedSize(190, 250);
            photoLabel->setAlignment(Qt::AlignCenter);
            topLayout->addWidget(photoLabel);
        }
    }

    // Personal data
    if (const auto* personal = data.findGroup("personal")) {
        auto* personalBox = new QGroupBox(qtTrId("lc-emrtd-personal-data"));
        auto* form = new QFormLayout(personalBox);
        addField(form, qtTrId("lc-emrtd-surname"), getFieldValue(personal, "surname"));
        addField(form, qtTrId("lc-emrtd-given-names"), getFieldValue(personal, "given_names"));
        addField(form, qtTrId("lc-emrtd-nationality"), getFieldValue(personal, "nationality"));
        addField(form, qtTrId("lc-emrtd-date-of-birth"), getFieldValue(personal, "date_of_birth"));
        addField(form, qtTrId("lc-emrtd-sex"), getFieldValue(personal, "sex"));
        topLayout->addWidget(personalBox, 1);
    }

    layout->addLayout(topLayout);

    // Document data
    if (const auto* doc = data.findGroup("document")) {
        auto* docBox = new QGroupBox(qtTrId("lc-emrtd-document-data"));
        auto* form = new QFormLayout(docBox);
        addField(form, qtTrId("lc-emrtd-doc-number"), getFieldValue(doc, "document_number"));
        addField(form, qtTrId("lc-emrtd-doc-code"), getFieldValue(doc, "document_code"));
        addField(form, qtTrId("lc-emrtd-issuing-state"), getFieldValue(doc, "issuing_state"));
        addField(form, qtTrId("lc-emrtd-date-of-expiry"), getFieldValue(doc, "date_of_expiry"));
        layout->addWidget(docBox);
    }

    // Additional info
    if (const auto* additional = data.findGroup("additional")) {
        auto* addBox = new QGroupBox(qtTrId("lc-emrtd-additional"));
        auto* form = new QFormLayout(addBox);
        for (const auto& field : additional->fields) {
            addField(form, QString::fromStdString(field.label), QString::fromStdString(field.asString()));
        }
        layout->addWidget(addBox);
    }

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

void EMRTDWidget::addField(QFormLayout* layout, const QString& label, const QString& value)
{
    if (value.isEmpty())
        return;
    auto* edit = new QLineEdit(value);
    edit->setReadOnly(true);
    layout->addRow(label + ":", edit);
}
