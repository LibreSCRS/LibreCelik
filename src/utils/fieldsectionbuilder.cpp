// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0

#include "utils/fieldsectionbuilder.h"
#include <QGridLayout>
#include <QLabel>
#include <QLineEdit>
#include <QDate>
#include <QPalette>
#include <QVBoxLayout>

namespace LibreSCRS {

CollapsibleSection* FieldSectionBuilder::build(const QString& title, const LibreSCRS::Plugin::CardFieldGroup& group,
                                               const std::map<std::string, QString>& translationMap,
                                               const std::set<std::string>& hiddenFields, QWidget* parent)
{
    auto* section = new CollapsibleSection(title, parent);
    section->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    auto* grid = new QGridLayout();
    grid->setSpacing(4);

    int row = 0;
    int col = 0;

    for (const auto& field : group.fields) {
        if (field.value.empty())
            continue;
        if (hiddenFields.count(field.key))
            continue;

        auto* cellLayout = new QVBoxLayout();
        cellLayout->setContentsMargins(0, 0, 0, 0);
        cellLayout->setSpacing(0);

        QString labelText;
        auto it = translationMap.find(field.key);
        if (it != translationMap.end())
            labelText = it->second;
        else
            labelText = QString::fromStdString(field.key);

        auto* label = new QLabel(labelText, section);
        auto* paletteSource = parent ? parent : section;
        label->setStyleSheet(QString("color: %1; font-size: 10px;")
                                 .arg(paletteSource->palette().color(QPalette::PlaceholderText).name()));

        auto textOpt = field.textValue();
        auto* value = new QLineEdit(textOpt ? QString::fromStdString(*textOpt) : QString{}, section);
        value->setReadOnly(true);
        value->setCursorPosition(0);

        cellLayout->addWidget(label);
        cellLayout->addWidget(value);

        grid->addLayout(cellLayout, row, col);
        col++;
        if (col >= 2) {
            col = 0;
            row++;
        }
    }

    // QGroupBox::setLayout respects the top margin reserved for the header
    section->setLayout(grid);
    return section;
}

void FieldSectionBuilder::highlightExpiredDates(CollapsibleSection* section,
                                                const LibreSCRS::Plugin::CardFieldGroup& group,
                                                const std::set<std::string>& dateFieldKeys)
{
    if (!section || dateFieldKeys.empty())
        return;

    // Collect values of date fields that are expired
    std::set<QString> expiredValues;
    for (const auto& field : group.fields) {
        if (!dateFieldKeys.count(field.key))
            continue;
        auto textOpt = field.textValue();
        if (!textOpt.has_value())
            continue;
        auto val = QString::fromStdString(*textOpt);
        auto date = QDate::fromString(val, "dd.MM.yyyy");
        if (date.isValid() && date < QDate::currentDate())
            expiredValues.insert(val);
    }

    if (expiredValues.empty())
        return;

    // Find QLineEdits with matching text and apply red palette
    auto lineEdits = section->findChildren<QLineEdit*>();
    for (auto* edit : lineEdits) {
        if (expiredValues.contains(edit->text())) {
            auto palette = edit->palette();
            palette.setColor(QPalette::Text, QColor(211, 47, 47)); // Material Red 700
            edit->setPalette(palette);
        }
    }
}

} // namespace LibreSCRS
