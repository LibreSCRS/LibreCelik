// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright hirashix0@proton.me

#include "pkswidget.h"

#include "plugin/carddatautils.h"
#include "utils/cardheadercard.h"
#include "utils/collapsiblesection.h"

#include <QIcon>
#include <QVBoxLayout>

using plugin::getFieldValue;

PksWidget::PksWidget(const plugin::CardData& data, QWidget* parent) : QWidget(parent), data(data)
{
    auto* outerLayout = new QVBoxLayout(this);
    outerLayout->setContentsMargins(0, 0, 0, 0);

    // Navy header section
    auto* headerSection = new CollapsibleSection(qtTrId("lc-pks-header-title"), // "Qualified Electronic Certificate"
                                                 QColor(34, 86, 117), this);
    headerSection->setHeaderHeight(56);

    // Build header card fields from PKI group
    std::vector<LibreSCRS::HeaderField> headerFields;

    if (const auto* group = data.findGroup("pki")) {
        auto certCount = getFieldValue(group, "certificate_count");
        if (!certCount.isEmpty()) {
            headerFields.push_back({qtTrId("lc-pks-cert-count"), certCount}); // "Certificates"
        }

        // Show certificate labels
        for (const auto& field : group->fields) {
            if (field.key == "cert_label") {
                headerFields.push_back(
                    {qtTrId("lc-pks-cert-label"), QString::fromStdString(field.asString())}); // "Certificate"
            }
        }
    }

    // If no fields were found, show a placeholder
    if (headerFields.empty()) {
        headerFields.push_back({qtTrId("lc-pks-card-type"), QStringLiteral("PKS")}); // "Card Type"
    }

    auto* headerCard = new LibreSCRS::CardHeaderCard(QIcon(":/images/certificate-icon.png"), QSize(80, 80),
                                                     headerFields, headerSection);
    auto* sectionLayout = new QVBoxLayout();
    sectionLayout->addWidget(headerCard);
    headerSection->setLayout(sectionLayout);
    headerSection->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);

    outerLayout->addWidget(headerSection);
}
