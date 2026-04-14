// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0

#include "utils/cardheadercard.h"

#include <QEvent>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QLineEdit>
#include <QPixmap>
#include <QVBoxLayout>

namespace LibreSCRS {

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
        label->setStyleSheet(
            QString("color: %1; font-size: 10px;").arg(palette().color(QPalette::PlaceholderText).name()));
        auto* value = new QLineEdit(field.value, this);
        value->setReadOnly(true);
        value->setCursorPosition(0);
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

void CardHeaderCard::applyLabelStyles()
{
    auto labels = findChildren<QLabel*>();
    QString style = QString("color: %1; font-size: 10px;").arg(palette().color(QPalette::PlaceholderText).name());
    for (auto* label : labels) {
        if (label->styleSheet().contains("font-size: 10px"))
            label->setStyleSheet(style);
    }
}

void CardHeaderCard::changeEvent(QEvent* event)
{
    if (event->type() == QEvent::PaletteChange)
        applyLabelStyles();
    QWidget::changeEvent(event);
}

} // namespace LibreSCRS
