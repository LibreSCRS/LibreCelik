// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright hirashix0@proton.me

#include "healthwidget.h"
#include "ui_health.h"

#include <plugin/carddatautils.h>

using plugin::getFieldValue;

HealthWidget::HealthWidget(const plugin::CardData& data, QWidget* parent)
    : QWidget(parent), ui(new Ui::Health), data(data)
{
    ui->setupUi(this);
    ui->healthCardSection->setTitle(qtTrId("lc-health-title"));
    ui->verticalLayout->setStretch(0, 1);
    ui->healthCardSection->setHeaderHeight(56);

    // Personal
    if (const auto* group = data.findGroup("personal")) {
        populatePersonalData(group);
    }

    // Insurance
    if (const auto* group = data.findGroup("insurance")) {
        populateInsuranceData(group);
    }

    // Address
    if (const auto* group = data.findGroup("address")) {
        populateAddressData(group);
    }

    // Carrier — show section only when carrier data is present or carrier_family_member is "true"
    if (const auto* group = data.findGroup("carrier")) {
        auto familyMember = getFieldValue(group, "carrier_family_member");
        bool hasData = !group->fields.empty();
        if (hasData || familyMember == "true") {
            populateCarrierData(group);
        } else {
            ui->carrierSection->setVisible(false);
        }
    } else {
        ui->carrierSection->setVisible(false);
    }

    // Taxpayer
    if (const auto* group = data.findGroup("taxpayer")) {
        populateTaxpayerData(group);
    }
}

HealthWidget::~HealthWidget()
{
    delete ui;
}

void HealthWidget::populatePersonalData(const plugin::CardFieldGroup* group)
{
    ui->givenNameLineEdit->setText(getFieldValue(group, "given_name"));
    ui->familyNameLineEdit->setText(getFieldValue(group, "family_name"));
    ui->givenNameLatLineEdit->setText(getFieldValue(group, "given_name_latin"));
    ui->familyNameLatLineEdit->setText(getFieldValue(group, "family_name_latin"));
    ui->parentNameLineEdit->setText(getFieldValue(group, "parent_name"));
    ui->parentNameLatLineEdit->setText(getFieldValue(group, "parent_name_latin"));
    ui->dobLineEdit->setText(getFieldValue(group, "date_of_birth"));
    ui->genderLineEdit->setText(getFieldValue(group, "gender"));
    ui->jmbgLineEdit->setText(getFieldValue(group, "personal_number"));
    ui->lboLineEdit->setText(getFieldValue(group, "insurant_number"));
}

void HealthWidget::populateInsuranceData(const plugin::CardFieldGroup* group)
{
    ui->insurerLineEdit->setText(getFieldValue(group, "insurer_name"));
    ui->insurerIdLineEdit->setText(getFieldValue(group, "insurer_id"));
    ui->cardIdLineEdit->setText(getFieldValue(group, "card_id"));
    ui->issueDateLineEdit->setText(getFieldValue(group, "date_of_issue"));
    ui->expiryLineEdit->setText(getFieldValue(group, "date_of_expiry"));
    ui->validUntilLineEdit->setText(getFieldValue(group, "valid_until"));

    auto permanentVal = getFieldValue(group, "permanently_valid");
    if (permanentVal == "true") {
        ui->permanentLineEdit->setText(qtTrId("lc-health-val-yes"));
    } else {
        ui->permanentLineEdit->setText(QString());
    }

    ui->insuranceBasisLineEdit->setText(getFieldValue(group, "insurance_basis_rzzo"));
    ui->insuranceDescLineEdit->setText(getFieldValue(group, "insurance_description"));
    ui->insuranceStartLineEdit->setText(getFieldValue(group, "insurance_start_date"));
}

void HealthWidget::populateAddressData(const plugin::CardFieldGroup* group)
{
    ui->streetLineEdit->setText(getFieldValue(group, "street"));
    ui->addressNumberLineEdit->setText(getFieldValue(group, "address_number"));
    ui->apartmentLineEdit->setText(getFieldValue(group, "apartment"));
    ui->placeLineEdit->setText(getFieldValue(group, "place"));
    ui->municipalityLineEdit->setText(getFieldValue(group, "municipality"));
    ui->countryLineEdit->setText(getFieldValue(group, "country"));
}

void HealthWidget::populateCarrierData(const plugin::CardFieldGroup* group)
{
    ui->carrierGivenNameLineEdit->setText(getFieldValue(group, "carrier_given_name"));
    ui->carrierFamilyNameLineEdit->setText(getFieldValue(group, "carrier_family_name"));
    ui->carrierRelLineEdit->setText(getFieldValue(group, "carrier_relationship"));
    ui->carrierIdLineEdit->setText(getFieldValue(group, "carrier_id_number"));
    ui->carrierLboLineEdit->setText(getFieldValue(group, "carrier_insurant_number"));
}

void HealthWidget::populateTaxpayerData(const plugin::CardFieldGroup* group)
{
    ui->taxpayerNameLineEdit->setText(getFieldValue(group, "taxpayer_name"));
    ui->taxpayerIdLineEdit->setText(getFieldValue(group, "taxpayer_id_number"));
    ui->taxpayerResLineEdit->setText(getFieldValue(group, "taxpayer_residence"));
    ui->taxpayerActLineEdit->setText(getFieldValue(group, "taxpayer_activity_code"));
}
