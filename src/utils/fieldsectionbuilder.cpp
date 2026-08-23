// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0

#include "utils/fieldsectionbuilder.h"

#include <LibreSCRS/AgentClient/IdentityRows.h>

#include <algorithm>

#include <QGridLayout>
#include <QLabel>
#include <QLineEdit>
#include <QDate>
#include <QPalette>
#include <QVBoxLayout>

namespace librecelik::utils {

CollapsibleSection* FieldSectionBuilder::build(const QString& title, const LibreSCRS::AgentClient::FieldGroup& group,
                                               const std::map<QString, QString>& translationMap,
                                               const std::set<QString>& hiddenFields, QWidget* parent,
                                               const QStringList& fieldOrder)
{
    auto* section = new CollapsibleSection(title, parent);
    section->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    auto* grid = new QGridLayout();
    grid->setSpacing(4);

    int row = 0;
    int col = 0;

    // The skip-binary / stringify decision is NOT re-implemented here: it lives
    // in the client's shared flatten rule, so every consumer renders the same
    // row set. Empty values are dropped locally — the flatten deliberately
    // retains them for tabular renderers, this grid is not one.
    auto rows = LibreSCRS::AgentClient::flattenIdentityFields({group});
    if (!fieldOrder.isEmpty()) {
        // Stable, so an unlisted key keeps its delivery position relative to
        // the other unlisted ones instead of being reordered arbitrarily.
        const auto rank = [&fieldOrder](const QString& key) {
            const qsizetype at = fieldOrder.indexOf(key);
            return at < 0 ? fieldOrder.size() : at;
        };
        std::stable_sort(
            rows.begin(), rows.end(),
            [&rank](const LibreSCRS::AgentClient::IdentityRow& a, const LibreSCRS::AgentClient::IdentityRow& b) {
                return rank(a.fieldKey) < rank(b.fieldKey);
            });
    }

    for (const auto& identityRow : rows) {
        if (identityRow.value.isEmpty())
            continue;
        if (hiddenFields.count(identityRow.fieldKey))
            continue;

        auto* cellLayout = new QVBoxLayout();
        cellLayout->setContentsMargins(0, 0, 0, 0);
        cellLayout->setSpacing(0);

        QString labelText;
        auto it = translationMap.find(identityRow.fieldKey);
        if (it != translationMap.end())
            labelText = it->second;
        else if (!identityRow.labelFallback.isEmpty())
            labelText = identityRow.labelFallback;
        else
            labelText = identityRow.fieldKey;

        auto* label = new QLabel(labelText, section);
        auto* paletteSource = parent ? parent : section;
        label->setStyleSheet(QString("color: %1; font-size: 10px;")
                                 .arg(paletteSource->palette().color(QPalette::PlaceholderText).name()));

        auto* value = new QLineEdit(identityRow.value, section);
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
                                                const LibreSCRS::AgentClient::FieldGroup& group,
                                                const std::set<QString>& dateFieldKeys)
{
    if (!section || dateFieldKeys.empty())
        return;

    // Collect values of date fields that are expired
    std::set<QString> expiredValues;
    for (const auto& field : group.fields) {
        if (!dateFieldKeys.count(field.key))
            continue;
        if (field.value.isEmpty())
            continue;
        auto date = QDate::fromString(field.value, "dd.MM.yyyy");
        if (date.isValid() && date < QDate::currentDate())
            expiredValues.insert(field.value);
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

} // namespace librecelik::utils
