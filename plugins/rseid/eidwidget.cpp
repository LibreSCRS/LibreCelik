// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright hirashix0@proton.me

#include "eidwidget.h"
#include "ui_eid.h"

#include <plugin/carddatautils.h>

#include <QDate>
#include <QResizeEvent>

using plugin::getFieldValue;

EidWidget::EidWidget(const plugin::CardData& data, QWidget* parent) : QWidget(parent), ui(new Ui::EId), data(data)
{
    ui->setupUi(this);
    ui->verticalLayout->setStretch(0, 1);
    ui->identityGroupBox->setHeaderHeight(56);

    // Determine card type
    const auto* cardTypeField = data.findField("card_type");
    if (cardTypeField && cardTypeField->asString() == "ForeignerIF2020") {
        isForeigner = true;
    }

    applyCardTypeVisibility();

    // Populate data from groups
    if (const auto* group = data.findGroup("personal")) {
        populatePersonalData(group);
    }
    if (const auto* group = data.findGroup("address")) {
        populateAddressData(group);
    }
    if (const auto* group = data.findGroup("document")) {
        populateDocumentData(group);
    }
    if (const auto* group = data.findGroup("photo")) {
        populatePhoto(group);
    }

    // Set verification icons
    auto setVerificationIcon = [this](const std::string& key, QLabel* label) {
        const auto* field = this->data.findField(key);
        if (!field)
            return;
        auto val = field->asString();
        if (val == "valid") {
            label->setPixmap(QPixmap(":/images/green_checked.png"));
        } else if (val == "invalid") {
            label->setPixmap(QPixmap(":/images/red_exclamation.png"));
        } else {
            label->setPixmap(QPixmap(":/images/grey_question_mark.png"));
        }
    };

    setVerificationIcon("card_verification", ui->group1Label_3);
    setVerificationIcon("fixed_verification", ui->group2Label_3);
    setVerificationIcon("variable_verification", ui->group3Label_3);
}

EidWidget::~EidWidget()
{
    delete ui;
}

void EidWidget::applyCardTypeVisibility()
{
    if (isForeigner) {
        ui->identityGroupBox->setTitle(qtTrId("lc-eid-title-foreigner"));
        ui->citizenSection->setTitle(qtTrId("lc-eid-foreigner-data"));
        ui->jmbgLabel_3->setText(qtTrId("lc-eid-label-ebs"));
        ui->addressLabel_3->setText(qtTrId("lc-eid-label-address-foreigner"));
    } else {
        ui->identityGroupBox->setTitle(qtTrId("lc-eid-title-serbian"));
        ui->citizenSection->setTitle(qtTrId("lc-eid-citizen-data"));
        ui->jmbgLabel_3->setText(qtTrId("lc-eid-label-jmbg"));
        ui->addressLabel_3->setText(qtTrId("lc-eid-label-address"));
    }
    ui->parentNameWidget->setVisible(!isForeigner);
    ui->nationalityWidget->setVisible(isForeigner);
    ui->placeOfBirthWidget->setVisible(!isForeigner);
    ui->statusOfForeignerWidget->setVisible(isForeigner);
    repositionAddressSection();
}

void EidWidget::repositionAddressSection()
{
    auto* fieldsLayout = ui->verticalLayout_citizenFields;
    int photoMaxHeight = ui->pictureLabel_3->maximumHeight();

    // Temporarily move address into the fields column if not already there, so the
    // layout can compute the combined height in one consistent sizeHint() call.
    if (!addressInColumn) {
        ui->verticalLayout_citizen->removeWidget(ui->addressSection);
        fieldsLayout->insertWidget(fieldsLayout->count() - 1, ui->addressSection);
    }

    fieldsLayout->invalidate();
    bool fits = (fieldsLayout->sizeHint().height() <= photoMaxHeight);

    if (fits) {
        addressInColumn = true;
    } else {
        fieldsLayout->removeWidget(ui->addressSection);
        ui->verticalLayout_citizen->addWidget(ui->addressSection);
        addressInColumn = false;
    }

    // When address is below the photo row, distribute fields evenly next to the photo
    // by giving each item equal stretch. When address is in the column, pack tightly.
    for (int i = 0; i < fieldsLayout->count(); ++i) {
        fieldsLayout->setStretch(i, addressInColumn ? 0 : 1);
    }
}

void EidWidget::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    repositionAddressSection();
}

void EidWidget::populatePersonalData(const plugin::CardFieldGroup* group)
{
    ui->nameLineEdit_3->setText(getFieldValue(group, "given_name"));
    ui->surnameLineEdit_3->setText(getFieldValue(group, "surname"));
    ui->parentNameLineEdit_3->setText(getFieldValue(group, "parent_given_name"));
    ui->genderLineEdit_3->setText(getFieldValue(group, "sex"));
    ui->jmbgLineEdit_3->setText(getFieldValue(group, "personal_number"));
    ui->dateOfBirthLineEdit_3->setText(getFieldValue(group, "date_of_birth"));
    ui->statusOfForeignerLineEdit_3->setText(getFieldValue(group, "status_of_foreigner"));
    ui->placeOfBirthLineEdit_3->setText(assemblePlaceOfBirth(group));
    ui->nationalityLineEdit_3->setText(getFieldValue(group, "nationality"));
}

void EidWidget::populateAddressData(const plugin::CardFieldGroup* group)
{
    ui->addressLineEdit_3->setText(assembleAddress(group));

    auto addressDate = getFieldValue(group, "address_date");
    bool show = (!addressDate.isEmpty() && addressDate != "01.01.0001");
    ui->dateOfAddressChangeWidget->setVisible(show);
    if (show) {
        ui->dateOfAddressChangeLineEdit_3->setText(addressDate);
    }
    repositionAddressSection();
}

void EidWidget::populateDocumentData(const plugin::CardFieldGroup* group)
{
    ui->documentNumberLineEdit_3->setText(getFieldValue(group, "doc_reg_no"));

    auto expiryDate = getFieldValue(group, "expiry_date");
    QDate receivedDate = QDate::fromString(expiryDate, "dd.MM.yyyy");
    QDate currentDate = QDate::currentDate();
    QPalette palette = ui->validToLineEdit_3->palette();
    if (receivedDate.isValid() && receivedDate < currentDate) {
        palette.setColor(QPalette::Text, QColor(0xe6, 0x87, 0x3c));
    }
    ui->validToLineEdit_3->setPalette(palette);
    ui->validToLineEdit_3->setText(expiryDate);

    ui->dateOfIssuanceLineEdit_3->setText(getFieldValue(group, "issuing_date"));
    ui->documentIssuerLineEdit_3->setText(getFieldValue(group, "issuing_authority"));
}

void EidWidget::populatePhoto(const plugin::CardFieldGroup* group)
{
    if (!group->fields.empty() && !group->fields[0].value.empty()) {
        QPixmap pixmap;
        pixmap.loadFromData(group->fields[0].value.data(), static_cast<uint>(group->fields[0].value.size()));
        ui->pictureLabel_3->setPixmap(pixmap);
    } else {
        ui->pictureLabel_3->setPixmap(QPixmap(QString::fromUtf8(":/images/user.png")));
    }
}

QString EidWidget::assembleAddress(const plugin::CardFieldGroup* group) const
{
    QStringList parts;
    parts << getFieldValue(group, "place") << getFieldValue(group, "community") << getFieldValue(group, "street")
          << getFieldValue(group, "house_number");
    auto address = parts.join(", ");
    auto floor = getFieldValue(group, "floor");
    auto apartmentNumber = getFieldValue(group, "apartment_number");
    if (!floor.isEmpty())
        address += "/" + floor;
    if (!apartmentNumber.isEmpty())
        address += "/" + apartmentNumber;
    return address;
}

QString EidWidget::assemblePlaceOfBirth(const plugin::CardFieldGroup* group) const
{
    QStringList parts;
    parts << getFieldValue(group, "place_of_birth") << getFieldValue(group, "community_of_birth")
          << getFieldValue(group, "state_of_birth");
    return parts.join(", ");
}
