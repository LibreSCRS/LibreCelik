// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright hirashix0@proton.me

#include "utils/fieldsectionbuilder.h"
#include <QGridLayout>
#include <QLabel>
#include <QLineEdit>
#include <QVBoxLayout>

namespace LibreSCRS {

CollapsibleSection* FieldSectionBuilder::build(const QString& title, const plugin::CardFieldGroup& group,
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
        label->setStyleSheet("color: #777; font-size: 10px;");

        auto* value = new QLineEdit(QString::fromStdString(field.asString()), section);
        value->setReadOnly(true);

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

} // namespace LibreSCRS
