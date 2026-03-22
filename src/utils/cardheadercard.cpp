// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright hirashix0@proton.me

#include "utils/cardheadercard.h"

#include <QGridLayout>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QLineEdit>
#include <QPixmap>
#include <QVBoxLayout>

namespace LibreSCRS {

CardHeaderCard::CardHeaderCard(const QPixmap& photo, const QSize& photoSize, const std::vector<HeaderField>& fields,
                               QWidget* parent)
    : QWidget(parent)
{
    photoLabel = new QLabel(this);
    auto scaledPhoto = photo.scaled(photoSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    photoLabel->setFixedSize(scaledPhoto.size());
    photoLabel->setMinimumSize(scaledPhoto.size());
    photoLabel->setPixmap(scaledPhoto);
    buildLayout(photoLabel, fields);
}

CardHeaderCard::CardHeaderCard(const QIcon& icon, const QSize& iconSize, const std::vector<HeaderField>& fields,
                               QWidget* parent)
    : QWidget(parent)
{
    auto* iconLabel = new QLabel(this);
    iconLabel->setFixedSize(iconSize);
    iconLabel->setPixmap(icon.pixmap(iconSize));
    iconLabel->setAlignment(Qt::AlignCenter);
    buildLayout(iconLabel, fields);
}

void CardHeaderCard::buildLayout(QWidget* leftWidget, const std::vector<HeaderField>& fields)
{
    auto* mainLayout = new QHBoxLayout(this);
    mainLayout->setContentsMargins(10, 10, 10, 10);
    mainLayout->addWidget(leftWidget, 0, Qt::AlignTop);

    // Right column: fields grid at top, stretch, verification at bottom
    rightColumnLayout = new QVBoxLayout();
    rightColumnLayout->setContentsMargins(0, 0, 0, 0);

    fieldsGrid = new QGridLayout();
    fieldsGrid->setSpacing(4);

    int row = 0;
    int col = 0;
    for (const auto& field : fields) {
        auto* cellLayout = new QVBoxLayout();
        cellLayout->setContentsMargins(0, 0, 0, 0);
        cellLayout->setSpacing(0);
        auto* label = new QLabel(field.label, this);
        label->setStyleSheet("color: #777; font-size: 10px;");
        auto* value = new QLineEdit(field.value, this);
        value->setReadOnly(true);
        cellLayout->addWidget(label);
        cellLayout->addWidget(value);

        // Force span-2 fields to start at column 0
        if (field.columnSpan >= 2 && col != 0) {
            col = 0;
            row++;
        }
        fieldsGrid->addLayout(cellLayout, row, col, 1, field.columnSpan);
        col += field.columnSpan;
        if (col >= 2) {
            col = 0;
            row++;
        }
    }

    auto* fieldsWidget = new QWidget(this);
    fieldsWidget->setLayout(fieldsGrid);
    rightColumnLayout->addWidget(fieldsWidget, 0);

    mainLayout->addLayout(rightColumnLayout, 1);
}

void CardHeaderCard::setVerificationResults(const std::vector<VerificationStatus>& results)
{
    if (!rightColumnLayout || results.empty())
        return;

    auto* row = new QHBoxLayout();
    row->setContentsMargins(0, 2, 0, 0);
    row->setSpacing(12);

    for (const auto& r : results) {
        auto* item = new QHBoxLayout();
        item->setSpacing(3);

        auto* icon = new QLabel(this);
        auto* text = new QLabel(r.label, this);

        switch (r.result) {
        case VerificationStatus::Valid:
            icon->setText(QStringLiteral("\u2714")); // ✔
            icon->setStyleSheet("color: #4CAF50; font-size: 12px;");
            text->setStyleSheet("color: #4CAF50; font-size: 10px;");
            break;
        case VerificationStatus::Invalid:
            icon->setText(QStringLiteral("\u2718")); // ✘
            icon->setStyleSheet("color: #F44336; font-size: 12px;");
            text->setStyleSheet("color: #F44336; font-size: 10px;");
            break;
        case VerificationStatus::Unknown:
            icon->setText(QStringLiteral("?")); // ?
            icon->setStyleSheet("color: #9E9E9E; font-size: 12px;");
            text->setStyleSheet("color: #9E9E9E; font-size: 10px;");
            break;
        }

        item->addWidget(icon);
        item->addWidget(text);
        row->addLayout(item);
    }

    row->addStretch();
    rightColumnLayout->addLayout(row);
}

void CardHeaderCard::setPhoto(const QPixmap& photo, const QSize& photoSize)
{
    if (!photoLabel)
        return;
    auto scaledPhoto = photo.scaled(photoSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    photoLabel->setFixedSize(scaledPhoto.size());
    photoLabel->setMinimumSize(scaledPhoto.size());
    photoLabel->setPixmap(scaledPhoto);
}

} // namespace LibreSCRS
